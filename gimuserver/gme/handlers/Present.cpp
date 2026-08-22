#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/BraveSlots.hpp>

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <string>

// The present box — the deferred half of the reward system.
//
// Rewards that are not handed over immediately are queued as rows in
// user_presents (gme::addUserPresent) and paid out when the player opens the
// box and claims them.  That is what makes the tutorial-completion dialog
// truthful: "Gifts have been awarded to you.  Visit your presents box to
// receive them."
//
// Wire shapes, all confirmed from the IDA exports rather than inferred:
//   PresentList    — PresentListRequest::createBody (0x13ABBB0) adds no body
//                    beyond the identity envelope, so the request is bare.
//                    Response is sEA41vFK, whose 11 named fields come straight
//                    from the UserPresentInfoResponse readParam export
//                    (func_addr 0x1400740).
//   PresentReceipt — PresentReceiptRequest::createBody (0x13ABC7C) sends
//                    {"o6uWU0Z7":[{"i1WQkh4G":<receiptType>,
//                                  "S1B82FHK":"<presentId>"}]}.
//
// GroupId / AES key:
//     PresentList    nhjvB52R / 6F9sMzBxEv8jXpau
//     PresentReceipt bV5xa0ZW / X2QFqAKfomPIg3rG
//
// Recovered with tools/ida/groupid_key_pair_audit.py.  NOTE the client's
// "Unsupported request: YPBU7MD8" on the gift tab was NOT this handler —
// YPBU7MD8 / AKjzyZ81 is GetAchievementInfoRequest, which is still unbuilt.
// Registering the present box does not by itself silence that error.

namespace
{

// The two *DateStr fields are display forms of their epoch siblings and are
// never stored — formatting here is what keeps the pair from disagreeing.
// Empty for a zero epoch so an unclaimed present shows a blank receipt date
// rather than the 1970 epoch.
std::string formatPresentDate(int64_t epochSeconds)
{
	if (epochSeconds <= 0)
		return {};

	const auto asTime = static_cast<std::time_t>(epochSeconds);
	std::tm tm{};
#ifdef _WIN32
	if (localtime_s(&tm, &asTime) != 0)
		return {};
#else
	if (!localtime_r(&asTime, &tm))
		return {};
#endif

	char buffer[32] = {};
	if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm) == 0)
		return {};

	return buffer;
}

// Reads the whole present box for a user, newest first.
drogon::Task<std::vector<::UserPresentInfo>> readPresents(
	const db::Database database,
	const std::string userId)
{
	std::vector<::UserPresentInfo> presents;

	const auto rows = co_await database->execSqlCoro(
		"SELECT present_id, present_type, target_id, target_cnt, target_param,"
		" receipt_type, description, present_date, receipt_date, is_receipt"
		" FROM user_presents WHERE user_id = $1"
		" ORDER BY present_date DESC, present_id DESC;",
		userId);

	presents.reserve(rows.size());
	for (const auto& row : rows)
	{
		const auto presentDate = row["present_date"].as<int64_t>();
		const auto receiptDate = row["receipt_date"].as<int64_t>();
		const auto isReceipt   = row["is_receipt"].as<int32_t>() != 0;

		presents.push_back(::UserPresentInfo{
			// Required — omitting this key made the client drop every row and
			// render an empty box despite a fully populated response.
			.user_id          = userId,
			.present_id       = std::to_string(row["present_id"].as<int64_t>()),
			.present_type     = row["present_type"].as<int32_t>(),
			.target_id        = row["target_id"].as<std::string>(),
			.target_cnt       = row["target_cnt"].as<int32_t>(),
			.target_param     = row["target_param"].as<std::string>(),
			.receipt_type     = row["receipt_type"].as<int32_t>(),
			.present_date     = static_cast<int32_t>(presentDate),
			.present_date_str = formatPresentDate(presentDate),
			.receipt_date     = static_cast<int32_t>(receiptDate),
			.receipt_date_str = formatPresentDate(receiptDate),
			.description      = row["description"].as<std::string>(),
			// The list operation, derived from claim state:
			//   1 = add-or-update — an unclaimed present belongs in the box.
			//   3 = delete        — a claimed one is removed from the client's
			//       list, which is what makes the tile disappear after a claim.
			// Sending 1 unconditionally left claimed tiles on screen, because
			// readParam re-added them instead of removing them.
			.list_op          = isReceipt ? 3 : 1,
			.is_receipt       = isReceipt,
		});
	}

	co_return presents;
}

} // namespace

// PresentList (nhjvB52R) — returns the present box.  Claimed
// rows are sent with list_op 3 so the client drops them from its list;
// unclaimed rows use list_op 1 to add or update the tile.
HANDLEF(PresentList)
{
	(void)session;

	PresentListReq req = {};
	{
		glz::context ctx{};
		if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "PresentList: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	PresentListResp resp = {};
	resp.presents = co_await readPresents(theDb(), identity.userId);

	LOG_INFO << "PresentList: " << resp.presents.size() << " present(s) for " << identity.userId;

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		const auto& glze = glz::format_error(ec, buffer);
		LOG_DEBUG << "Gme PresentList Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}

// PresentReceipt — claims one present and pays it out.
//
// The payout switch is deliberately identical to CampaignReceipt's: both read
// the same present_type vocabulary off the same wire hash (30Kw4WBa), so they
// must stay in agreement.  Anything unrecognised is logged and the present is
// left UNCLAIMED, so a reward we cannot pay is never silently consumed.
HANDLEF(PresentReceipt)
{
	(void)session;

	PresentReceiptReq req = {};
	{
		glz::context ctx{};
		if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
		{
			const auto& fmte = glz::format_error(ec, json);
			LOG_DEBUG << "Gme PresentReceipt Error during JSON read: " << fmte;
			co_return HandleResult::error("Deserialization error", fmte);
		}
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	PresentReceiptResp resp = {};
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			for (const auto& node : req.nodes)
			{
				if (node.present_id.empty())
					continue;

				int64_t presentId = 0;
				try { presentId = std::stoll(node.present_id); }
				catch (...)
				{
					LOG_WARN << "PresentReceipt: non-numeric present id " << node.present_id;
					continue;
				}

				// Scoped to this user AND to unclaimed rows, so a replayed
				// claim finds nothing and pays out nothing.
				const auto rows = co_await transaction->execSqlCoro(
					"SELECT present_type, target_id, target_cnt FROM user_presents"
					" WHERE present_id = $1 AND user_id = $2 AND is_receipt = 0;",
					presentId, identity.userId);
				if (rows.empty())
				{
					LOG_INFO << "PresentReceipt: present " << presentId
						<< " already claimed or not owned by " << identity.userId;
					continue;
				}

				const auto presentType = rows[0]["present_type"].as<int32_t>();
				const auto targetId    = rows[0]["target_id"].as<std::string>();
				const auto targetCnt   = rows[0]["target_cnt"].as<int32_t>();

				bool granted = false;
				switch (presentType)
				{
				case 3:  // zel
				case 8:  // gem
				{
					const char* column = (presentType == 3) ? "zel" : "gems";
					co_await transaction->execSqlCoro(
						std::string("UPDATE user_info SET ") + column + " = " + column +
						" + $1 WHERE gumi_user_id = $2 AND id = $3;",
						targetCnt, identity.gumiUserId, identity.userId);
					granted = true;
					break;
				}
				case 6:  // unit
				{
					uint32_t unitId = 0;
					try { unitId = static_cast<uint32_t>(std::stoul(targetId)); }
					catch (...) { break; }

					auto unit = gme::fromArchivedUnit(unitId, UnitArchiver::getRandomType());
					if (!unit)
					{
						LOG_WARN << "PresentReceipt: unit " << targetId
							<< " has no archive record — present left unclaimed";
						break;
					}
					for (int32_t i = 0; i < std::max(targetCnt, 1); ++i)
						(co_await gme::addUserUnit(transaction, identity, *unit)).nonEmpty();
					granted = true;
					break;
				}
				case 4:  // item
				case 5:  // material
				case 7:  // sphere
				{
					uint32_t itemId = 0;
					try { itemId = static_cast<uint32_t>(std::stoul(targetId)); }
					catch (...) { break; }

					(co_await gme::addUserItem(
						transaction, identity, itemId,
						static_cast<uint32_t>(std::max(targetCnt, 1)))).nonEmpty();
					granted = true;
					break;
				}
				case 12:  // medal
				{
					// PresentCommon::createThumbnail @0x11C4900 dispatches
					// present_type - 2 through a jump table, and 12 is the
					// entry that reaches GameUtils::getMedalThumbnail with the
					// target_id — so target_id is the MEDAL id, not an item id.
					// (2 friend pts, 3 zel, 8 gem, 11 karma, 13 achievement pts,
					// 16 colosseum pts, 17 summoner sp, 14 sub-dispatches on
					// target_id for tickets/orbs.)
					co_await transaction->execSqlCoro(
						"INSERT INTO user_brave_medals (user_id, medal_id, possession)"
						" VALUES ($1, $2, MIN($3, $4))"
						" ON CONFLICT(user_id, medal_id) DO UPDATE SET"
						" possession = MIN(possession + $3, $4);",
						identity.userId, targetId, targetCnt, gme::kBraveSlotMedalCap);
					granted = true;
					break;
				}
				default:
					LOG_WARN << "PresentReceipt: present_type " << presentType
						<< " (target " << targetId << " x" << targetCnt
						<< ") not supported — present left unclaimed";
					break;
				}

				if (!granted)
					continue;

				const auto now = static_cast<int64_t>(
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now().time_since_epoch()).count());
				co_await transaction->execSqlCoro(
					"UPDATE user_presents SET is_receipt = 1, receipt_date = $1"
					" WHERE present_id = $2 AND user_id = $3;",
					now, presentId, identity.userId);

				LOG_INFO << "PresentReceipt: claimed present " << presentId
					<< " (type " << presentType << ", target " << targetId
					<< " x" << targetCnt << ")";
			}

			resp.presents = co_await readPresents(transaction, identity.userId);
			resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
			// Medals are a currency the HUD does not carry, so without this the
			// Brave Slots counter keeps the pre-claim number until some later
			// call happens to bring UserInfo along.
			resp.medal_info = co_await gme::loadBraveMedals(transaction, identity);
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		const auto& glze = glz::format_error(ec, buffer);
		LOG_DEBUG << "Gme PresentReceipt Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}
