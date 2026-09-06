#include "App.hpp"
#include "Handlers.hpp"

#include <vector>

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
			// ⚠ "RECEIVE ALL" SENDS NO PRESENT ID AT ALL.
			//
			// Captured from a real client: a single claim posts
			//     {"i1WQkh4G":"1","S1B82FHK":"7"}
			// while Receive All posts
			//     {"i1WQkh4G":"2"}
			// — mode 2, no id, meaning "everything in the box".  Skipping
			// nodes without an id therefore made Receive All claim NOTHING,
			// which is why the box only ever emptied one present at a time.
			//
			// ⚠ i1WQkh4G is a MODE in the REQUEST but carries our stored
			// receipt_type in the LISTING we send.  Same key, two meanings:
			// the client sends 1 for a single claim regardless of what the
			// listing said that present's receipt_type was.  So it is NOT
			// echoed back and must NOT be used to filter.
			std::vector<int64_t> toClaim;
			for (const auto& node : req.nodes)
			{
				if (!node.present_id.empty())
				{
					try { toClaim.push_back(std::stoll(node.present_id)); }
					catch (...)
					{
						LOG_WARN << "PresentReceipt: non-numeric present id "
							<< node.present_id;
					}
					continue;
				}

				const auto all = co_await transaction->execSqlCoro(
					"SELECT present_id FROM user_presents"
					" WHERE user_id = $1 AND is_receipt = 0 ORDER BY present_id;",
					identity.userId);
				for (const auto& row : all)
				{
					toClaim.push_back(row["present_id"].as<int64_t>());
				}
				LOG_INFO << "PresentReceipt: receive-all for " << identity.userId
					<< " — " << all.size() << " unclaimed present(s)";
			}

			for (const auto presentId : toClaim)
			{

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
				case 9:  // dungeon key
				{
					// PresentCommon::createPresentName @0x11C37F4 dispatches
					// (present_type - 2) through the jump table at 0x2328200, and
					// index 7 lands at 0x11C3EDC -> DungeonKeyMstList, so target_id
					// is a DungeonKeyMst id (1 Metal, 2 Jewel, 3 Imp).
					//
					// Capped at that key's possession_limit (50 on all three rows)
					// for the same reason DungeonKeyReceipt caps it: the client's
					// Administration Office draws "held / limit" and a present must
					// not be able to push the count past what the daily claim can.
					//
					// last_receipt_day stays 0 on a fresh row: it is the once-per-day
					// CLAIM token, and stamping it here would consume today's
					// legitimate claim.  A present is not a distribution.
					if (targetId.empty())
						break;

					int32_t keyId = 0;
					try { keyId = std::stoi(targetId); }
					catch (const std::exception&) { break; }

					const auto& keyMst = theServer()->cache().dungeonKeyMst();
					const auto key = std::find_if(keyMst.begin(), keyMst.end(),
						[keyId](const DungeonKeyMst& k) { return k.id == keyId; });
					if (key == keyMst.end())
					{
						LOG_WARN << "PresentReceipt: dungeon key " << targetId
							<< " is not in DungeonKeyMst — present left unclaimed";
						break;
					}

					co_await transaction->execSqlCoro(
						"INSERT INTO user_dungeon_keys"
						" (user_id, dungeon_key_id, possession, last_receipt_day, active_type)"
						" VALUES ($1, $2, MIN($3, $4), 0, 0)"
						" ON CONFLICT(user_id, dungeon_key_id) DO UPDATE SET"
						" possession = MIN(possession + $3, $4);",
						identity.userId, keyId, std::max(targetCnt, 1),
						static_cast<int32_t>(key->possession_limit));

					// Read back rather than reporting target_cnt: the MIN above
					// means a 100-key present on a 50 cap grants 46, and a log that
					// says 100 would hide that.  Same "held/limit" shape as
					// DungeonKeyReceipt's line.
					const auto held = co_await transaction->execSqlCoro(
						"SELECT possession FROM user_dungeon_keys"
						" WHERE user_id = $1 AND dungeon_key_id = $2;",
						identity.userId, keyId);
					LOG_INFO << "PresentReceipt: " << key->name << " present of "
						<< std::max(targetCnt, 1) << " — now holding "
						<< (held.empty() ? 0 : held[0]["possession"].as<int32_t>())
						<< "/" << static_cast<int32_t>(key->possession_limit);
					granted = true;
					break;
				}
				case 11:  // karma
				{
					// PresentCommon::createThumbnail @0x11C4900 maps
					// present_type 11 to karma_thum.png, and the Journal pays
					// karma for "Equip Sphere" and the Honor Points stand-in.
					// Without this the present was displayable but not
					// grantable - it would sit in the box logging "not
					// supported" when Receive was pressed.
					co_await transaction->execSqlCoro(
						"UPDATE user_info SET karma = MIN(karma + $1, $2)"
						" WHERE gumi_user_id = $3 AND id = $4;",
						targetCnt, static_cast<int64_t>(99'999'999),
						identity.gumiUserId, identity.userId);
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
				case 17:  // summoner SP
				{
					// PresentCommon::createPresentName @0x11C37F4, index 15 of the
					// jump table at 0x2328200.  A quantity-only reward: target_id is
					// 0 in every row of the data, target_cnt is the SP.
					//
					// Capped at DefineMst.max_summoner_sp (99999), the same way karma
					// is capped -- the client's SP label has no room to grow past the
					// value the define declares.
					//
					// The row is created on demand: nothing seeds user_summoner at
					// account creation, and an absent row reads as 0 SP in UserInfo.
					constexpr int32_t kMaxSummonerSp = 99'999;

					co_await transaction->execSqlCoro(
						"INSERT INTO user_summoner (user_id, sp) VALUES ($1, MIN($2, $3))"
						" ON CONFLICT(user_id) DO UPDATE SET sp = MIN(sp + $2, $3);",
						identity.userId, std::max(targetCnt, 1), kMaxSummonerSp);

					const auto held = co_await transaction->execSqlCoro(
						"SELECT sp FROM user_summoner WHERE user_id = $1;",
						identity.userId);
					LOG_INFO << "PresentReceipt: summoner SP present of "
						<< std::max(targetCnt, 1) << " — now holding "
						<< (held.empty() ? 0 : held[0]["sp"].as<int32_t>())
						<< "/" << kMaxSummonerSp;
					granted = true;
					break;
				}
				case 18:  // summoner arm
				{
					// PresentCommon::createPresentName @0x11C37F4 dispatches
					// (present_type - 2) through the jump table at 0x2328200,
					// and index 16 lands at 0x11C3E98 -> SummonerArmMstList,
					// so target_id is a SummonerArmMst id.  Every id the
					// mission rewards use resolves in that 23-row table.
					//
					// An arm is owned once; target_cnt is always 1 in the data
					// and a repeat grant must not create a second row.  Level
					// starts at 1 because that is what the arm list shows for
					// an unlevelled arm -- 0 would render as "Lv.0".
					if (targetId.empty())
						break;

					int32_t armId = 0;
					try { armId = std::stoi(targetId); }
					catch (const std::exception&) { break; }

					const auto& arms = theServer()->cache().summonerArmMst();
					const auto known = std::any_of(arms.begin(), arms.end(),
						[&](const SummonerArmMst& a) { return a.id == armId; });
					if (!known)
					{
						LOG_WARN << "PresentReceipt: summoner arm " << targetId
							<< " is not in SummonerArmMst — present left unclaimed";
						break;
					}

					co_await transaction->execSqlCoro(
						"INSERT INTO user_summoner_arms (user_id, summoner_arm_id, exp, level)"
						" VALUES ($1, $2, 0, 1)"
						" ON CONFLICT(user_id, summoner_arm_id) DO NOTHING;",
						identity.userId, targetId);
					granted = true;
					break;
				}
				default:
				{
					// Every type the mission/campaign reward data actually uses
					// now has a case above.  Anything reaching here is a type only
					// PresentCommon::createPresentName @0x11C37F4 knows about —
					// resolve it through the jump table at 0x2328200 (index is
					// present_type - 2) before adding a case for it.
					LOG_WARN << "PresentReceipt: present_type " << presentType
						<< " (target " << targetId << " x" << targetCnt
						<< ") not supported — present left unclaimed";
					break;
				}
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
