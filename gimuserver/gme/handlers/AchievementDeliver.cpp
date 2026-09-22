#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Achievements.hpp>
#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

// AchievementDeliver (vsaXI4M0 / 2Lj5hIEG) - hand goods to Randall for Merit
// Points.  One request serves all four Dedicate screens; which one it came from
// is the achievement cond_type it carries, not a field of its own:
//
//     4 Zel     5 Karma     6 Sphere     8 Unit
//
// Proved by RandallAchievementDedicateKarmaScene::setBaseData @0x1A35FB0, which
// asks getDataObject(3, 5) - category 3 is the Trade category, and its ten rows
// are exactly the four flows' "Traded N ... Total" achievements.
//
// THE CLIENT'S OWN NUMBER IS NOT A PRICE.  idfCDG70 is what
// GameUtils::getSellAchievePoint worked out on the client, and it is sent so the
// screen can show it; the payout here is recomputed from the same tables
// (gme::unitDeliverPoints / gme::itemDeliverPoints, which mirror @0x1176B70 and
// @0x1176EAC) and the claim is only logged when the two disagree.
//
// EVERY FLOW IS CAPPED PER DAY.  DefineMst carries a separate ceiling for each
// (max_achieve_point_zel_per_day and friends), measured in POINTS EARNED that
// day rather than goods handed over, and user_achievement_deliver is the ledger.
// A delivery that would earn nothing - because the cap is already reached, or
// because the goods price at zero - is REFUSED before anything is consumed, so
// goods are never destroyed for no credit.

namespace
{

/*! Splits the request's comma-separated id lists.  Blank entries are dropped. */
std::vector<std::string> splitIds(const std::string& packed)
{
	std::vector<std::string> out;
	size_t at = 0;
	while (at <= packed.size())
	{
		const auto comma = packed.find(',', at);
		auto piece = packed.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
		while (!piece.empty() && std::isspace(static_cast<unsigned char>(piece.front())))
			piece.erase(piece.begin());
		while (!piece.empty() && std::isspace(static_cast<unsigned char>(piece.back())))
			piece.pop_back();
		if (!piece.empty())
			out.push_back(std::move(piece));
		if (comma == std::string::npos)
			break;
		at = comma + 1;
	}
	return out;
}

/*! user_units.unit_id carries an "_100" suffix on evolved forms. */
int32_t mstIdOf(const std::string& raw)
{
	const auto sep = raw.find('_');
	try { return std::stoi(sep == std::string::npos ? raw : raw.substr(0, sep)); }
	catch (const std::exception&) { return 0; }
}

/*! A plain integer id, refused rather than defaulted when it is not one. */
bool parseId(const std::string& text, int64_t& out)
{
	try { out = std::stoll(text); }
	catch (const std::exception&) { return false; }
	return out > 0;
}

} // namespace

HANDLEF(AchievementDeliver)
{
	(void)session;

	::AchievementDeliverReq req{};
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		LOG_WARN << "AchievementDeliver: parse error: " << glz::format_error(ec, json);
		co_return HandleResult::error("Deserialization error");
	}
	if (req.nodes.empty() || req.goods.empty())
		co_return HandleResult::error("Invalid deliver request", "no node or goods");

	const auto& node = req.nodes.front();
	const auto& goods = req.goods.front();
	if (!gme::isDeliverKind(node.cond_type))
	{
		LOG_WARN << "AchievementDeliver: cond_type " << node.cond_type
			<< " is not one of the four deliver flows";
		co_return HandleResult::error("Invalid deliver request", "unknown deliver flow");
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
	const auto today = gme::deliverToday();
	const auto cap = gme::deliverDailyCap(node.cond_type);

	::AchievementDeliverResp resp{};
	resp.signal_key.key = "5EdKHavF";
	std::string body;
	int32_t paidPoints = 0;
	int64_t deliveredAmount = 0;

	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// What this flow has already earned today, which is what the
			// ceiling is measured against.
			int32_t earnedToday = 0;
			{
				const auto rows = co_await transaction->execSqlCoro(
					"SELECT points FROM user_achievement_deliver"
					" WHERE user_id = $1 AND kind = $2 AND day = $3;",
					identity.userId, node.cond_type, today);
				if (!rows.empty())
					earnedToday = rows[0]["points"].as<int32_t>();
			}
			const auto headroom = std::max(cap - earnedToday, 0);
			if (headroom <= 0)
			{
				transaction->rollback();
				LOG_INFO << "AchievementDeliver: " << identity.userId
					<< " is at the daily ceiling for flow " << node.cond_type
					<< " (" << earnedToday << "/" << cap << "); nothing taken";
				co_return HandleResult::error("Invalid deliver request",
					"daily Merit Point limit reached");
			}

			// ---- price it, and take the goods -------------------------------
			std::string unitIdList;     // filled on the unit flow only
			bool touchedWarehouse = false;

			if (node.cond_type == static_cast<int32_t>(gme::DeliverKind::Zel)
				|| node.cond_type == static_cast<int32_t>(gme::DeliverKind::Karma))
			{
				const auto isZel = node.cond_type == static_cast<int32_t>(gme::DeliverKind::Zel);
				const auto& defines = theServer()->cache().initializeResp().defines;
				const auto perPoint = isZel
					? defines.zel_per_achieve_point
					: defines.karma_per_achieve_point;
				if (perPoint <= 0 || goods.amount <= 0)
				{
					transaction->rollback();
					co_return HandleResult::error("Invalid deliver request", "nothing to trade");
				}

				// Only whole points are bought, and only the currency that buys
				// them is taken - a remainder below one point stays with the
				// player instead of vanishing into rounding.
				auto points = goods.amount / perPoint;
				points = std::min(points, headroom);
				if (points <= 0)
				{
					transaction->rollback();
					co_return HandleResult::error("Invalid deliver request",
						"not enough to earn a Merit Point");
				}
				const auto charged = static_cast<int64_t>(points) * perPoint;

				const char* column = isZel ? "zel" : "karma";
				const auto spent = co_await transaction->execSqlCoro(
					std::string("UPDATE user_info SET ") + column + " = " + column +
					" - $1 WHERE id = $2 AND " + column + " >= $1 RETURNING id;",
					charged, identity.userId);
				if (spent.empty())
				{
					transaction->rollback();
					LOG_WARN << "AchievementDeliver: " << identity.userId
						<< " cannot afford " << charged << ' ' << column;
					co_return HandleResult::error("Invalid deliver request",
						"not enough currency");
				}

				paidPoints = points;
				deliveredAmount = charged;
			}
			else if (node.cond_type == static_cast<int32_t>(gme::DeliverKind::Unit))
			{
				const auto wanted = splitIds(goods.user_unit_ids);
				std::string inClause;
				for (const auto& raw : wanted)
				{
					int64_t id = 0;
					if (!parseId(raw, id))
						continue;
					if (!inClause.empty()) inClause += ',';
					inClause += std::to_string(id);
				}
				if (inClause.empty())
				{
					transaction->rollback();
					co_return HandleResult::error("Invalid deliver request", "no unit named");
				}

				// A LOCKED UNIT IS NOT DELIVERABLE.  favorite_flg is the in-game
				// lock, and this is the one place that would destroy the row, so
				// the filter belongs in the read the delete is built from.
				const auto rows = co_await transaction->execSqlCoro(
					"SELECT user_unit_id, unit_id, unit_lvl FROM user_units"
					" WHERE user_id = $1 AND favorite_flg = 0"
					" AND user_unit_id IN (" + inClause + ");",
					identity.userId);
				if (rows.empty())
				{
					transaction->rollback();
					co_return HandleResult::error("Invalid deliver request",
						"no deliverable unit in that list");
				}

				const auto& unitMst = theServer()->cache().unitMst();
				int64_t points = 0;
				int64_t counted = 0;
				for (const auto& row : rows)
				{
					const auto mstId = mstIdOf(row["unit_id"].as<std::string>());
					const auto it = std::find_if(unitMst.begin(), unitMst.end(),
						[mstId](const ::UnitMst& u) { return u.id == mstId; });
					if (it == unitMst.end())
						continue;
					points += gme::unitDeliverPoints(*it, row["unit_lvl"].as<int32_t>());
					++counted;

					if (!unitIdList.empty()) unitIdList += ',';
					unitIdList += std::to_string(row["user_unit_id"].as<int64_t>());
				}
				points = std::min<int64_t>(points, headroom);
				if (points <= 0 || unitIdList.empty())
				{
					transaction->rollback();
					LOG_INFO << "AchievementDeliver: " << identity.userId
						<< " offered units worth no Merit Points; nothing taken";
					co_return HandleResult::error("Invalid deliver request",
						"those units are worth no Merit Points");
				}

				// Spheres come off first or they die with the row.
				co_await gme::returnEquippedSpheres(transaction, identity, unitIdList);
				co_await transaction->execSqlCoro(
					"DELETE FROM user_units WHERE user_id = $1"
					" AND user_unit_id IN (" + unitIdList + ");",
					identity.userId);

				paidPoints = static_cast<int32_t>(points);
				deliveredAmount = counted;
				touchedWarehouse = true;
			}
			else   // Sphere
			{
				// n6E8iMf3 is the sphere screen's id list, but no store to that
				// offset was read, so take whichever of the two string fields
				// actually arrived and say which it was - the first live
				// delivery settles it.
				const char* which = "n6E8iMf3";
				auto packed = goods.item_ids;
				if (packed.empty())
				{
					packed = goods.user_unit_ids;
					which = "edy7fq3L";
				}
				const auto wanted = splitIds(packed);
				if (wanted.empty())
				{
					transaction->rollback();
					co_return HandleResult::error("Invalid deliver request", "no sphere named");
				}
				LOG_INFO << "AchievementDeliver: sphere ids arrived under " << which;

				// A repeated id is a second copy of the same sphere.
				std::map<int64_t, int32_t> byId;
				for (const auto& raw : wanted)
				{
					int64_t id = 0;
					if (parseId(raw, id))
						++byId[id];
				}

				const auto& itemMst = theServer()->cache().itemMst();
				int64_t points = 0;
				int64_t taken = 0;
				for (const auto& entry : byId)
				{
					const auto itemId = entry.first;
					const auto count = entry.second;
					const auto it = std::find_if(itemMst.begin(), itemMst.end(),
						[itemId](const ::ItemMst& i) { return i.id == itemId; });
					if (it == itemMst.end())
						continue;

					// Held-and-unlocked only, decremented in the statement that
					// checks the stack, so two taps cannot both pass.
					const auto spent = co_await transaction->execSqlCoro(
						"UPDATE user_items SET item_num = item_num - $1"
						" WHERE user_id = $2 AND item_id = $3 AND favorite_flg = 0"
						" AND item_num >= $1 RETURNING item_num;",
						count, identity.userId, itemId);
					if (spent.empty())
						continue;      // not held in that quantity, or locked

					points += static_cast<int64_t>(gme::itemDeliverPoints(*it)) * count;
					taken += count;
				}

				points = std::min<int64_t>(points, headroom);
				if (points <= 0 || taken <= 0)
				{
					transaction->rollback();
					LOG_INFO << "AchievementDeliver: " << identity.userId
						<< " offered spheres worth no Merit Points; nothing taken";
					co_return HandleResult::error("Invalid deliver request",
						"those spheres are worth no Merit Points");
				}

				co_await transaction->execSqlCoro(
					"DELETE FROM user_items WHERE user_id = $1 AND item_num <= 0;",
					identity.userId);

				paidPoints = static_cast<int32_t>(points);
				deliveredAmount = taken;
				touchedWarehouse = true;
			}

			// ---- pay, and record it against today's ceiling -----------------
			const auto maxPoint = theServer()->cache().initializeResp().defines.max_achieve_point;
			co_await transaction->execSqlCoro(
				"UPDATE user_info SET achieve_point = MIN(achieve_point + $1, $2)"
				" WHERE id = $3;",
				paidPoints, maxPoint > 0 ? maxPoint : 999999, identity.userId);

			co_await transaction->execSqlCoro(
				"INSERT INTO user_achievement_deliver (user_id, kind, day, points, amount)"
				" VALUES ($1, $2, $3, $4, $5)"
				" ON CONFLICT(user_id, kind, day) DO UPDATE SET"
				" points = points + $4, amount = amount + $5;",
				identity.userId, node.cond_type, today, paidPoints, deliveredAmount);

			// ---- and tell the client what moved -----------------------------
			// ALWAYS, not only on the currency flows.  The field is a
			// std::optional and an unset one serializes as `[null]`, not as an
			// absent key, so leaving it out hands the client a null where a
			// UserTeamInfo should be.  Refreshing the header is cheap and is
			// never wrong.
			resp.team_info = std::move(
				(co_await gme::getTeamInfo(transaction, identity)).nonEmpty());

			resp.achievement_info = co_await gme::loadAchievementInfo(transaction, identity);
			resp.subjects = co_await gme::loadAchievementSubjects(
				transaction, identity, node.category, node.cond_type);
			resp.badge.badge_data = co_await gme::achievementBadgeData(transaction, identity);

			if (!unitIdList.empty())
			{
				// FULL REPLACE.  The delivered units are gone, and the
				// insert-if-absent sibling of this key can never remove one -
				// nor does the Dedicate scene drop them from its own list.
				resp.unit_refresh = std::move((co_await db::PacketInterfaceFor<::UserUnitInfo>::read(
					transaction, "user_units",
					{ db::Lookup("user_id", identity.userId) })).data);
			}
			if (touchedWarehouse)
			{
				auto snapshot = co_await gme::loadWarehouseSnapshot(transaction, identity);
				resp.warehouse_info = std::move(snapshot.warehouse);
			}

			// Serialize before the transaction closes: the goods are already
			// gone, and a reply that could not be written after the commit would
			// leave the player short with nothing on screen to show for it.
			if (const auto error = glz::write_json(resp, body); error)
				throw std::runtime_error(glz::format_error(error, body));
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	if (goods.claimed_point != paidPoints)
	{
		LOG_INFO << "AchievementDeliver: client claimed " << goods.claimed_point
			<< " point(s), server priced it at " << paidPoints
			<< " (flow " << node.cond_type << ")";
	}
	LOG_INFO << "AchievementDeliver: " << identity.userId << " delivered "
		<< deliveredAmount << " to flow " << node.cond_type << " for "
		<< paidPoints << " Merit Point(s); balance " << resp.achievement_info.id;

	co_return HandleResult::success(body);
}
