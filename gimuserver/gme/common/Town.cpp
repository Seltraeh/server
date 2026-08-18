#include "Town.hpp"

#include "App.hpp"
#include "Common.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/utils/Random.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace gme
{
namespace
{

int64_t nowSeconds()
{
	return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
}

/// Splits on a single delimiter, keeping empty pieces.
///
/// Matches CommonUtils::split, which is what the client runs over the same
/// strings — an off-by-one in either direction would misalign drop_item_info.
std::vector<std::string> split(std::string_view in, const char sep)
{
	std::vector<std::string> out;
	size_t start = 0;
	while (true)
	{
		const auto at = in.find(sep, start);
		if (at == std::string_view::npos)
		{
			out.emplace_back(in.substr(start));
			return out;
		}
		out.emplace_back(in.substr(start, at - start));
		start = at + 1;
	}
}

/// std::stoi that answers a fallback instead of throwing.
///
/// Every caller here parses MST or client-supplied text, and the dispatcher
/// closes the session on an escaped exception (handbook 6.7).
int32_t toInt(const std::string& in, const int32_t fallback = 0)
{
	try
	{
		return std::stoi(in);
	}
	catch (...)
	{
		return fallback;
	}
}

/// Lowest level present in a level master for one id.
template <typename Rows>
int32_t baseLevel(const Rows& rows, const int32_t id)
{
	int32_t best = -1;
	for (const auto& row : rows)
	{
		if (row.id == id && (best < 0 || row.lv < best))
		{
			best = row.lv;
		}
	}

	return best < 0 ? 1 : best;
}

/// One weighted entry of a tile's drop pool.
struct DropChance
{
	int32_t itemId = 0;
	int32_t rate = 0;
};

/// The drop pool a tile offers at a given level.
///
/// TownLocationLvMst.item_info is CUMULATIVE: each level row lists only what
/// that upgrade ADDS, so the pool is the union of every row up to the current
/// level.  Zero-rate entries are padding in the shipped data and are dropped.
std::vector<DropChance> dropPool(const int32_t locationId, const int32_t lv)
{
	std::vector<DropChance> pool;

	for (const auto& row : theServer()->cache().initializeResp().town_location_lv)
	{
		if (row.id != locationId || row.lv > lv)
		{
			continue;
		}

		for (const auto& pair : split(row.item_info, ','))
		{
			if (pair.empty())
			{
				continue;
			}

			const auto parts = split(pair, ':');
			if (parts.size() < 2)
			{
				continue;
			}

			const DropChance chance{ toInt(parts[0]), toInt(parts[1]) };
			if (chance.itemId > 0 && chance.rate > 0)
			{
				pool.push_back(chance);
			}
		}
	}

	return pool;
}

/// The level row a tile is currently on, or nullptr when the level has none.
const TownLocationLvMst* locationLevelRow(const int32_t locationId, const int32_t lv)
{
	for (const auto& row : theServer()->cache().initializeResp().town_location_lv)
	{
		if (row.id == locationId && row.lv == lv)
		{
			return &row;
		}
	}

	return nullptr;
}

/// Rolls one tap's worth of loot as the client's "<itemId>:<zel>:<karma>".
///
/// The item is a weighted pick across the pool with a no-drop remainder of
/// 100 - sum(rate).  That remainder is real at low levels (Mountain lv 1 totals
/// 36) and vanishes as the tile is upgraded, where the sums pass 100 and the
/// rates become purely relative — which is exactly the "more and rarer
/// materials every time you collect" the upgrade screen promises.
///
/// Zel and karma are independent: their rate is a percent chance to pay out at
/// all, then a uniform amount between start and end.  Item id 0 is the client's
/// own "nothing this tap" — ItemMstList::getObject misses and it skips the
/// warehouse credit.
std::string rollTap(const TownLocationLvMst& level, const std::vector<DropChance>& pool)
{
	int32_t itemId = 0;
	int32_t poolWeight = 0;
	for (const auto& chance : pool)
	{
		poolWeight += chance.rate;
	}

	if (poolWeight > 0)
	{
		auto roll = static_cast<int32_t>(RandomUInt(1, static_cast<uint32_t>(std::max(poolWeight, 100))));
		for (const auto& chance : pool)
		{
			if (roll <= chance.rate)
			{
				itemId = chance.itemId;
				break;
			}
			roll -= chance.rate;
		}
	}

	int32_t zel = 0;
	if (level.zel_rate > 0 && static_cast<int32_t>(RandomUInt(1, 100)) <= level.zel_rate)
	{
		const auto lo = std::min(level.zel_start, level.zel_end);
		const auto hi = std::max(level.zel_start, level.zel_end);
		zel = static_cast<int32_t>(RandomUInt(
			static_cast<uint32_t>(std::max(lo, 0)), static_cast<uint32_t>(std::max(hi, 0))));
	}

	int32_t karma = 0;
	if (level.karma_rate > 0 && static_cast<int32_t>(RandomUInt(1, 100)) <= level.karma_rate)
	{
		const auto lo = std::min(level.karma_start, level.karma_end);
		const auto hi = std::max(level.karma_start, level.karma_end);
		karma = static_cast<int32_t>(RandomUInt(
			static_cast<uint32_t>(std::max(lo, 0)), static_cast<uint32_t>(std::max(hi, 0))));
	}

	return std::to_string(itemId) + ':' + std::to_string(zel) + ':' + std::to_string(karma);
}

/// Rolls a whole harvest period: a tap allowance plus its pre-rolled loot.
void rollPeriod(const int32_t locationId, const int32_t lv, int32_t& tapCnt, std::string& dropInfo)
{
	tapCnt = 0;
	dropInfo.clear();

	const auto* level = locationLevelRow(locationId, lv);
	if (!level)
	{
		LOG_WARN << "Town: no TownLocationLvMst row for location " << locationId
			<< " lv " << lv << "; tile will not sparkle";
		return;
	}

	const auto lo = std::max(std::min(level->tap_cnt_start, level->tap_cnt_end), 0);
	const auto hi = std::max(level->tap_cnt_start, level->tap_cnt_end);
	if (hi <= 0)
	{
		return;
	}

	tapCnt = static_cast<int32_t>(RandomUInt(
		static_cast<uint32_t>(std::max(lo, 1)), static_cast<uint32_t>(hi)));

	const auto pool = dropPool(locationId, lv);
	for (int32_t i = 0; i < tapCnt; ++i)
	{
		if (i)
		{
			dropInfo += ',';
		}
		dropInfo += rollTap(*level, pool);
	}
}

/// Formats a Unix timestamp the way the client's datetime codec reads it.
pkg::chrono_time toChrono(const int64_t epochSeconds)
{
	return pkg::chrono_time(std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::seconds(epochSeconds)));
}

} // namespace

int32_t Town::facilityBaseLevel(const int32_t facilityId)
{
	return baseLevel(theServer()->cache().initializeResp().town_facility_lv, facilityId);
}

int32_t Town::locationBaseLevel(const int32_t locationId)
{
	return baseLevel(theServer()->cache().initializeResp().town_location_lv, locationId);
}

bool Town::isUnlocked(const int32_t needMissionId, const std::set<int32_t>& clearedMissions)
{
	return needMissionId == 0 || clearedMissions.count(needMissionId) > 0;
}

drogon::Task<std::vector<::UserTownFacilityInfo>> Town::facilityState(
	const db::Database database,
	const UserIdentity identity)
{
	std::vector<::UserTownFacilityInfo> out;

	const auto rows = co_await database->execSqlCoro(
		"SELECT facility_id, lv, karma FROM user_town_facilities WHERE user_id = $1;",
		identity.userId);

	const auto& levels = theServer()->cache().initializeResp().town_facility_lv;
	std::string repairSql;

	out.reserve(rows.size());
	for (const auto& row : rows)
	{
		const auto facilityId = row["facility_id"].as<int32_t>();
		auto lv = row["lv"].as<int32_t>();

		// A level the master has no row for cannot be rendered: the upgrade
		// screen's getObjectWithKey(facilityId, lv) returns null and the detail
		// panel comes up blank.  Accounts provisioned before the base level was
		// read from the master carry exactly that — facilities 3-6 stored at lv 1
		// when their only row is lv 0 — so clamp and repair rather than serve it.
		const bool known = std::any_of(levels.begin(), levels.end(),
			[facilityId, lv](const auto& l) { return l.id == facilityId && l.lv == lv; });
		if (!known)
		{
			const auto base = facilityBaseLevel(facilityId);
			LOG_WARN << "Town: facility " << facilityId << " stored at lv " << lv
				<< " which has no TownFacilityLvMst row; repairing to lv " << base;
			lv = base;

			// One upsert for the whole repair set.  Drogon's SQLite client
			// rejects semicolon-separated statements, so batching has to be
			// multi-VALUES rather than multi-statement.
			repairSql += repairSql.empty()
				? "INSERT INTO user_town_facilities (user_id, facility_id, lv) VALUES "
				: ",";
			repairSql += "('" + identity.userId + "'," + std::to_string(facilityId) + ","
				+ std::to_string(base) + ")";
		}

		out.push_back(::UserTownFacilityInfo{
			.user_id = identity.userId,
			.facility_id = facilityId,
			.lv = lv,
			.karma = row["karma"].as<int32_t>(),
		});
	}

	if (!repairSql.empty())
	{
		co_await database->execSqlCoro(
			repairSql + " ON CONFLICT(user_id, facility_id) DO UPDATE SET lv=excluded.lv;");
	}

	co_return out;
}

drogon::Task<void> Town::locationState(
	const db::Database database,
	const UserIdentity identity,
	const std::set<int32_t> clearedMissions,
	std::vector<::UserTownLocationInfo>& info,
	std::vector<::UserTownLocationDetail>& detail)
{
	const auto rows = co_await database->execSqlCoro(
		"SELECT location_id, lv, karma, period_start, tap_cnt, drop_info"
		" FROM user_town_locations WHERE user_id = $1 ORDER BY location_id;",
		identity.userId);

	const auto now = nowSeconds();

	// Tiles whose period elapsed are re-rolled in one batched upsert.  A
	// per-tile await loop here would be four sequential round trips on the
	// single-connection SQLite pool for every UserInfo (handbook 6.14), and
	// Drogon's SQLite client rejects semicolon-separated statements, so the
	// batch has to be multi-VALUES rather than multi-statement.  The rows all
	// exist already so the INSERT arm never fires, but lv and karma still have
	// to be supplied: they are echoed back and left alone by the conflict clause.
	std::string refreshSql;

	for (const auto& row : rows)
	{
		const auto locationId = row["location_id"].as<int32_t>();
		const auto lv = row["lv"].as<int32_t>();

		auto tapCnt = row["tap_cnt"].as<int32_t>();
		auto dropInfo = row["drop_info"].as<std::string>();
		auto periodStart = row["period_start"].as<int64_t>();

		const auto& locations = theServer()->cache().initializeResp().town_location;
		const auto found = std::find_if(locations.begin(), locations.end(),
			[locationId](const auto& m) { return m.id == locationId; });

		const bool unlocked = found != locations.end()
			&& isUnlocked(found->need_mission_id, clearedMissions);

		// Roll a new period once the old one has aged out.  Locked tiles are
		// left empty: the client hides them, and a stockpile waiting behind the
		// gate would dump a full period the instant the mission is cleared.
		if (unlocked && now - periodStart >= kHarvestPeriodSeconds)
		{
			periodStart = now;
			rollPeriod(locationId, lv, tapCnt, dropInfo);

			refreshSql += refreshSql.empty()
				? "INSERT INTO user_town_locations"
				  " (user_id, location_id, lv, karma, period_start, tap_cnt, drop_info) VALUES "
				: ",";
			refreshSql += "('" + identity.userId + "'," + std::to_string(locationId) + ","
				+ std::to_string(lv) + "," + std::to_string(row["karma"].as<int32_t>()) + ","
				+ std::to_string(periodStart) + "," + std::to_string(tapCnt) + ",'"
				+ dropInfo + "')";
		}
		else if (!unlocked)
		{
			tapCnt = 0;
			dropInfo.clear();
		}

		info.push_back(::UserTownLocationInfo{
			.user_id = identity.userId,
			.location_id = locationId,
			.lv = lv,
			.karma = row["karma"].as<int32_t>(),
		});

		// Always emitted, locked or not: setLocationInfo skips a location with
		// no detail row entirely, so an omitted row hides the tile rather than
		// locking it.
		detail.push_back(::UserTownLocationDetail{
			.user_id = identity.userId,
			.location_id = locationId,
			.start_date = toChrono(periodStart),
			.tap_cnt = tapCnt,
			.drop_item_info = dropInfo,
		});
	}

	// drop_info is server-rolled digits and separators and userId is a server
	// minted id, so there is nothing here to escape (handbook 6.14).
	if (!refreshSql.empty())
	{
		co_await database->execSqlCoro(refreshSql
			+ " ON CONFLICT(user_id, location_id) DO UPDATE SET"
			  " period_start=excluded.period_start, tap_cnt=excluded.tap_cnt,"
			  " drop_info=excluded.drop_info;");
	}

	co_return;
}

drogon::Task<void> Town::applyTaps(
	const db::Database database,
	const UserIdentity identity,
	const std::string collectLog)
{
	if (collectLog.empty())
	{
		co_return;
	}

	// locationId -> taps claimed this flush.
	std::map<int32_t, int32_t> claimed;
	for (const auto& entry : split(collectLog, ','))
	{
		const auto parts = split(entry, ':');
		if (parts.size() < 2)
		{
			continue;
		}

		const auto locationId = toInt(parts[0]);
		const auto taps = toInt(parts[1]);
		if (locationId > 0 && taps > 0)
		{
			claimed[locationId] += taps;
		}
	}

	if (claimed.empty())
	{
		co_return;
	}

	const auto rows = co_await database->execSqlCoro(
		"SELECT location_id, tap_cnt, drop_info FROM user_town_locations WHERE user_id = $1;",
		identity.userId);

	std::map<int32_t, int32_t> itemGains;
	int64_t zelGain = 0;
	int64_t karmaGain = 0;
	std::string tapSql;

	for (const auto& row : rows)
	{
		const auto locationId = row["location_id"].as<int32_t>();
		const auto it = claimed.find(locationId);
		if (it == claimed.end())
		{
			continue;
		}

		const auto remaining = row["tap_cnt"].as<int32_t>();
		const auto drops = split(row["drop_info"].as<std::string>(), ',');
		const auto taken = std::min(it->second, remaining);
		if (taken <= 0)
		{
			continue;
		}

		// getCollectItemInfo reads element `count - tap_cnt`, so the taps just
		// reported are the `taken` entries starting there.
		const auto first = static_cast<int32_t>(drops.size()) - remaining;
		for (int32_t i = 0; i < taken; ++i)
		{
			const auto index = first + i;
			if (index < 0 || index >= static_cast<int32_t>(drops.size()))
			{
				continue;
			}

			const auto parts = split(drops[index], ':');
			if (parts.size() < 3)
			{
				continue;
			}

			if (const auto itemId = toInt(parts[0]); itemId > 0)
			{
				++itemGains[itemId];
			}
			zelGain += toInt(parts[1]);
			karmaGain += toInt(parts[2]);
		}

		tapSql += tapSql.empty()
			? "INSERT INTO user_town_locations (user_id, location_id, tap_cnt) VALUES "
			: ",";
		tapSql += "('" + identity.userId + "'," + std::to_string(locationId) + ","
			+ std::to_string(remaining - taken) + ")";

		LOG_INFO << "TownUpdate: location " << locationId << " taps " << taken
			<< " (" << remaining << " remaining before)";
	}

	// One batched statement per resource rather than one await per tap: a full
	// tile is up to 20 taps and the SQLite pool is single-connection.
	if (!itemGains.empty())
	{
		std::string itemSql =
			"INSERT INTO user_items (user_id, item_id, item_num) VALUES ";
		bool first = true;
		for (const auto& [itemId, count] : itemGains)
		{
			if (!first)
			{
				itemSql += ',';
			}
			first = false;
			itemSql += "('" + identity.userId + "'," + std::to_string(itemId) + ","
				+ std::to_string(count) + ")";
		}
		itemSql += " ON CONFLICT(user_id, item_id) DO UPDATE SET"
			" item_num = item_num + excluded.item_num;";

		co_await database->execSqlCoro(itemSql);
	}

	if (zelGain > 0 || karmaGain > 0)
	{
		co_await database->execSqlCoro(
			"UPDATE user_info SET zel = zel + $1, karma = karma + $2 WHERE id = $3;",
			zelGain, karmaGain, identity.userId);
	}

	if (!tapSql.empty())
	{
		co_await database->execSqlCoro(tapSql
			+ " ON CONFLICT(user_id, location_id) DO UPDATE SET tap_cnt=excluded.tap_cnt;");
	}

	co_return;
}

drogon::Task<std::vector<::PermitReceipe>> Town::permittedRecipes(
	const db::Database database,
	const UserIdentity identity,
	const std::set<int32_t> clearedMissions)
{
	const auto& init = theServer()->cache().initializeResp();

	const auto rows = co_await database->execSqlCoro(
		"SELECT facility_id, lv FROM user_town_facilities WHERE user_id = $1;",
		identity.userId);

	std::set<int32_t> permitted;
	for (const auto& row : rows)
	{
		const auto facilityId = row["facility_id"].as<int32_t>();
		const auto lv = row["lv"].as<int32_t>();

		const auto found = std::find_if(init.town_facility.begin(), init.town_facility.end(),
			[facilityId](const auto& m) { return m.id == facilityId; });
		if (found == init.town_facility.end()
			|| !isUnlocked(found->need_mission_id, clearedMissions))
		{
			continue;
		}

		// release_receipe lists what each upgrade ADDS, so everything at or
		// below the current level is unlocked.
		for (const auto& level : init.town_facility_lv)
		{
			if (level.id != facilityId || level.lv > lv)
			{
				continue;
			}

			for (const auto recipeId : level.release_receipe)
			{
				permitted.insert(recipeId);
			}
		}
	}

	if (permitted.empty())
	{
		co_return {};
	}

	std::map<int32_t, int32_t> crafted;
	{
		const auto craftRows = co_await database->execSqlCoro(
			"SELECT recipe_id, craft_count FROM user_recipe_crafts WHERE user_id = $1;",
			identity.userId);
		for (const auto& row : craftRows)
		{
			crafted[row["recipe_id"].as<int32_t>()] = row["craft_count"].as<int32_t>();
		}
	}

	std::vector<::PermitReceipe> out;
	out.reserve(permitted.size());
	for (const auto recipeId : permitted)
	{
		// A release list can name a recipe the master does not carry; sending it
		// anyway would put an id in the menu that RecipeMstList cannot resolve.
		const auto found = std::find_if(init.receipe.begin(), init.receipe.end(),
			[recipeId](const auto& r) { return r.id == recipeId; });
		if (found == init.receipe.end())
		{
			continue;
		}

		const auto craftCount = crafted.count(recipeId) ? crafted.at(recipeId) : 0;

		// -1 is "permanent".  Every row of the shipped recipe master ends at
		// 9999-12-31, so every recipe takes this branch today; the elapsed-time
		// form is here for the day an event recipe with a real window lands.
		// A 0 would read as already-expired (handbook 6.12).
		int32_t availableSeconds = -1;
		{
			const auto end = std::chrono::duration_cast<std::chrono::seconds>(
				found->availability_end.time_since_epoch()).count();
			const auto now = nowSeconds();
			if (end > 0 && end < now + 0x7FFFFFFFLL)
			{
				availableSeconds = static_cast<int32_t>(std::max<int64_t>(end - now, 0));
			}
		}

		out.push_back(::PermitReceipe{
			.recipe_id = recipeId,
			.craft_count = craftCount,
			.available_seconds = availableSeconds,
		});
	}

	co_return out;
}
}
