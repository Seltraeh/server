#pragma once

#include <gimuserver/archive/UnitArchiver.hpp>
#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Energy.hpp>
#include <gimuserver/gme/common/Town.hpp>
#include <gimuserver/utils/Random.hpp>

#include <drogon/orm/DbClient.h>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gme
{

/*!
* Local user identity resolved from a Gumi Live login.
*/
struct UserIdentity
{
	std::string gumiUserId;
	std::string userId;
};

/*!
* Builds a user-unit packet from curated unit archive data.
*
* @param unit_id Unit id to look up in the archive.
* @param unit_type_id Unit type to use when populating stats.
* @return Populated packet, or std::nullopt when archive data is missing or
* unsupported.
*/
inline std::optional<UserUnitInfo> fromArchivedUnit(uint32_t unit_id, uint32_t unit_type_id)
{
	const auto unitRecord = UnitArchiver::instance().lookup(unit_id);
	if (!unitRecord)
	{
		return std::nullopt;
	}

	UserUnitInfo unit{};
	if (!UnitArchiver::populatePacket(*unitRecord, unit_type_id, unit))
	{
		return std::nullopt;
	}

	return unit;
}

/*!
* Adds a user-owned unit row and updates the packet with database-owned fields.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity that owns the unit.
* @param unit Unit packet to persist.
* @param isNew Whether the inserted unit should be marked new for the client.
* @return Persisted unit packet populated with database-owned fields.
*/
inline drogon::Task<db::InterfaceResult<UserUnitInfo>> addUserUnit(
	const db::Database database,
	const UserIdentity identity,
	const UserUnitInfo unit,
	const bool isNew = true)
{
	auto packet = unit;
	packet.user_id = identity.userId;
	packet.is_new = isNew;
	const auto result = co_await db::PacketInterfaceFor<UserUnitInfo>::insert(
		database,
		"user_units",
		packet);

	packet.user_unit_id = result.front<uint32_t>("user_unit_id");
	packet.received_order = packet.user_unit_id;

	// OR IGNORE: the dictionary is one row per SPECIES, keyed
	// (user_id, unit_id), so obtaining a second copy of a unit you already own
	// collides on the primary key.  A plain insert would throw and take the
	// whole grant down with it — the dictionary is a record of what you have
	// seen, and failing to re-record it must never cost you the unit.
	co_await database->execSqlCoro(
		"INSERT OR IGNORE INTO user_unit_dictionary (user_id, unit_id)"
		" VALUES ($1, $2);",
		identity.userId, packet.unit_id);

	co_return db::InterfaceResult<UserUnitInfo>{
		.data = std::move(packet),
		.affected = result.affected,
	};
}

/*!
* Queues a present in the user's present box.
*
* Presents are the deferred half of the reward system: the grant is recorded
* now and the payout happens when the player claims it in the present box
* (PresentReceipt), which is what lets the client say "Gifts have been awarded
* to you.  Visit your presents box to receive them."
*
* presentType follows the same vocabulary CampaignReceipt dispatches on (wire
* hash 30Kw4WBa): 3 = zel, 8 = gem, 6 = unit, 4/5/7 = item/material/sphere.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to grant to.
* @param presentType What the present pays out.
* @param targetId Rewarded entity id, interpreted per presentType ("" for currency).
* @param targetCnt Quantity, or the currency amount.
* @param receiptType Claim-processing selector echoed back by PresentReceipt.
*/
inline drogon::Task<void> addUserPresent(
	const db::Database database,
	const UserIdentity identity,
	const int32_t presentType,
	const std::string targetId,
	const int32_t targetCnt,
	const int32_t receiptType = 0,
	const std::string description = {})
{
	const auto now = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());

	co_await database->execSqlCoro(
		"INSERT INTO user_presents"
		" (user_id, present_type, target_id, target_cnt, receipt_type, description, present_date)"
		" VALUES ($1, $2, $3, $4, $5, $6, $7);",
		identity.userId, presentType, targetId, targetCnt, receiptType, description, now);

	co_return;
}

/*!
* Provisions this user's town: one base-level row per town facility and location.
*
* The town is NOT created lazily anywhere else.  UserInfo reports
* town_facility_info / town_location_info / town_location_detail straight from
* these two tables, and the three arrays must travel together with matching
* cardinality or the town scene loader null-derefs (handbook §6.8).  Before this
* existed the only writers were TownFacilityUpdate — which needs a working town
* already — and the debug CLI's `unlocktown`, so a natural playthrough reached
* the town with zero rows and the client crashed on entry with no server-side
* error.  That also blocks the summon tutorial: tuto15.txt routes through
* `change_town_top_scene` on its way to the summon gate.
*
* EVERY facility and location is seeded, including the ones the player has not
* unlocked yet.  The mission gate is enforced client-side against
* need_mission_id (MyTownTopScene::isOpen / setLocationInfo), and a location
* with no row is skipped by the render loop outright — so withholding rows
* hides those tiles permanently instead of locking them.  Town::locationState
* is what keeps a locked tile inert.
*
* The base level is per-facility, not a flat 1: facilities 1 and 2 (sphere and
* item synthesis) have level rows from 1, but 3-6 only ever have a lv 0 row, and
* seeding those at 1 makes TownFacilityLvMstList::getObjectWithKey miss so the
* upgrade screen draws an empty detail panel.
*
* Facilities with id >= 1000 are Event Bazaar entries the client has no bundled
* sprites for — seeding them crashes the town scene on load (handbook §3.3), so
* they are skipped here exactly as the CLI skips them.
*
* Idempotent: INSERT OR IGNORE against (user_id, facility_id) /
* (user_id, location_id), so re-running never disturbs upgrades already made.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to provision.
*/
inline drogon::Task<void> provisionTown(
	const db::Database database,
	const UserIdentity identity)
{
	const auto& init = theServer()->cache().initializeResp();

	// Batched: one statement each rather than one await per row, so a fresh
	// account does not make eleven sequential round trips on the
	// single-connection SQLite pool (handbook §6.14).  Every value is MST data
	// or a server-minted id, so there is no injection surface.
	std::string facilitySql;
	for (const auto& facility : init.town_facility)
	{
		if (facility.id >= 1000)
			continue;

		facilitySql += facilitySql.empty()
			? "INSERT OR IGNORE INTO user_town_facilities (user_id, facility_id, lv) VALUES "
			: ",";
		facilitySql += "('" + identity.userId + "'," + std::to_string(facility.id) + ","
			+ std::to_string(Town::facilityBaseLevel(facility.id)) + ")";
	}

	if (!facilitySql.empty())
	{
		co_await database->execSqlCoro(facilitySql + ";");
	}

	std::string locationSql;
	for (const auto& location : init.town_location)
	{
		locationSql += locationSql.empty()
			? "INSERT OR IGNORE INTO user_town_locations (user_id, location_id, lv) VALUES "
			: ",";
		locationSql += "('" + identity.userId + "'," + std::to_string(location.id) + ","
			+ std::to_string(Town::locationBaseLevel(location.id)) + ")";
	}

	if (!locationSql.empty())
	{
		co_await database->execSqlCoro(locationSql + ";");
	}

	co_return;
}

/*!
* Credits an item stack to the owning user's warehouse.
*
* Stacks are keyed by (user_id, item_id): a repeat drop increments item_num
* rather than creating a second row.  Returns the resulting quantity.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity that owns the item.
* @param itemId Item master id to credit.
* @param quantity Amount to add (default 1).
* @return Number of affected rows.
*/
inline drogon::Task<db::InterfaceResult<>> addUserItem(
	const db::Database database,
	const UserIdentity identity,
	const uint32_t itemId,
	const uint32_t quantity = 1)
{
	if (!database || identity.userId.empty() || itemId == 0)
	{
		LOG_ERROR << "Invalid addUserItem call: "
			<< "db=" << static_cast<bool>(database)
			<< ", user_id_empty=" << identity.userId.empty()
			<< ", item_id=" << itemId;
		throw std::invalid_argument("Invalid addUserItem call");
	}

	// Bump the stack if it exists, else insert a new one.  item_num accumulates
	// rather than being replaced, so repeated grants stack.
	co_return co_await db::DatabaseInterface::upsert(
		database,
		"user_items",
		{
			db::Data("user_id", identity.userId),
			db::Data("item_id", itemId),
			db::Data("item_num", quantity),
		},
		{ "user_id", "item_id" },
		{ "item_num" });
}

/*!
* Returns any spheres equipped on soon-to-be-consumed units to the owner's
* warehouse.
*
* UnitSell / UnitMix / UnitEvo delete user_units rows (sold units, fusion
* fodder, evo materials).  Spheres equipped on those units are owned items —
* deleting the row without this call would destroy them silently.  Call BEFORE
* the DELETE, with the same pre-validated integer id list its IN clause uses.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity that owns the units.
* @param userUnitIdList Comma-joined user_unit_id list (validated integers).
*/
inline drogon::Task<void> returnEquippedSpheres(
	const db::Database database,
	const UserIdentity identity,
	const std::string& userUnitIdList)
{
	if (!database || identity.userId.empty() || userUnitIdList.empty())
		co_return;

	// Callers hand us the same comma-joined id string their DELETE uses, so
	// split it back into bound values rather than splicing it into SQL.  When
	// those DELETEs move onto the typed interface this should take the id
	// vector directly and the round trip disappears.
	db::Values userUnitIds;
	for (size_t start = 0; start <= userUnitIdList.size();)
	{
		const auto end = userUnitIdList.find(',', start);
		const auto token = userUnitIdList.substr(
			start, end == std::string::npos ? std::string::npos : end - start);
		if (!token.empty())
		{
			userUnitIds.emplace_back(
				static_cast<uint64_t>(std::stoull(token)));
		}

		if (end == std::string::npos)
		{
			break;
		}

		start = end + 1;
	}

	if (userUnitIds.empty())
		co_return;

	const auto result = co_await db::DatabaseInterface::read(
		database,
		"user_units",
		{
			db::Data("eqip_item_id"),
			db::Data("eqip_item_id2"),
			db::Lookup("user_id", identity.userId),
			db::LookupIn("user_unit_id", userUnitIds),
		});
	for (const auto& row : result.data)
	{
		for (const auto col : { "eqip_item_id", "eqip_item_id2" })
		{
			const auto itemId = row[col].as<uint32_t>();
			if (itemId != 0)
			{
				co_await addUserItem(database, identity, itemId, 1);
			}
		}
	}
}

/*!
* Maps a numeric element id (UnitMst.element) to the string form stored in
* user_units.element.
*
* @param id Element id 1-6.
* @return Element name; "fire" for out-of-range ids.
*/
inline std::string_view elementIdToString(const int32_t id)
{
	switch (id)
	{
	case 2: return "water";
	case 3: return "earth";
	case 4: return "thunder";
	case 5: return "light";
	case 6: return "dark";
	default: return "fire";
	}
}

/*!
* Grants a fresh level-1 unit to the user from its UnitMst row.
*
* Column mapping mirrors the debug CLI's InsertUnitFromMst (the proven insert
* shape for this schema): base stats from the MST minimums, skill levels 10
* when the unit has the skill, and a random unit type (1-6) — types are rolled
* on acquisition, matching live behaviour.  Reward flows (CampaignReceipt) use
* this for present_type=6 unit rewards.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity that receives the unit.
* @param unit Unit master row to instantiate.
*/
inline drogon::Task<void> addUserUnit(
	const db::Database database,
	const UserIdentity identity,
	const UnitMst& unit)
{
	if (!database || identity.userId.empty())
	{
		LOG_ERROR << "Invalid addUserUnit call: "
			<< "db=" << static_cast<bool>(database)
			<< ", user_id_empty=" << identity.userId.empty();
		throw std::invalid_argument("Invalid addUserUnit call");
	}

	const int32_t skillLv      = unit.skill_id       > 0 ? 10 : 0;
	const int32_t extraSkillLv = unit.extra_skill_id > 0 ? 10 : 0;

	int32_t unitType = 1;
	{
		std::lock_guard lock(RandomMutex());
		unitType = std::uniform_int_distribution<int32_t>(1, 6)(RandomEngine());
	}

	co_await database->execSqlCoro(
		"INSERT INTO user_units "
		"(user_id, unit_id, unit_lvl,"
		" base_hp,  add_hp,  ext_hp,  limit_over_hp,"
		" base_atk, add_atk, ext_atk, limit_over_atk,"
		" base_def, add_def, ext_def, limit_over_def,"
		" base_rec, add_rec, ext_rec, limit_over_rec,"
		" exp, total_exp,"
		" skill_id, skill_lv, extra_skill_id, extra_skill_lv,"
		" element, unit_type_id, \"new\") "
		"VALUES ($1,$2,1,"
		" $3,0,0,0, $4,0,0,0, $5,0,0,0,"
		" $6,0,0,0,"
		" 1,1,"
		" $7,$8,$9,$10,"
		" $11,$12,1);",
		identity.userId, std::to_string(unit.id),
		unit.min_hp, unit.min_atk, unit.min_def, unit.min_rec,
		unit.skill_id, skillLv, unit.extra_skill_id, extraSkillLv,
		std::string(elementIdToString(unit.element)),
		unitType);

	// ⚠ THIS OVERLOAD USED TO DO NEITHER OF THE TWO THINGS ABOVE AND BELOW.
	//
	// The UserUnitInfo overload sets is_new and records the species in
	// user_unit_dictionary; this one is raw SQL and did neither, so every unit
	// granted through it — slot prizes, Journal rewards, CampaignReceipt —
	// arrived with no NEW badge and never appeared in the in-game unit
	// dictionary.  That is why an account with 29 units had 8 distinct species
	// owned and only 6 listed, the missing five being exactly the ones the
	// reward paths handed out.
	//
	// OR IGNORE because the dictionary is one row per SPECIES: obtaining a
	// second Burst Frog must not fail the grant on the primary key.
	co_await database->execSqlCoro(
		"INSERT OR IGNORE INTO user_unit_dictionary (user_id, unit_id)"
		" VALUES ($1, $2);",
		identity.userId, unit.id);
}

/*!
* Looks up player progression MST data for a specific user level.
*
* The progression cache is expected to be ordered by level, with level 1 at
* index 0. This helper centralizes that assumption and validates the row before
* returning it.
*
* @param level Player level to look up.
* @return Progression row for the level, or std::nullopt if unavailable.
*/
inline std::optional<UserLevelMst> getLevelMst(uint32_t level)
{
	const auto& progression = theServer()->cache().initializeResp().progression;
	if (level == 0 || level > progression.size())
	{
		LOG_ERROR << "Unable to find user level MST for level " << level;
		return std::nullopt;
	}

	const auto& mst = progression[level - 1];
	if (mst.level != level)
	{
		LOG_ERROR << "User level progression is not ordered at level " << level;
		return std::nullopt;
	}

	return mst;
}

/*!
* Seeds the default normal decks when the owning user has no deck rows yet.
* The starter unit is placed in the middle party position for decks 0
* through 9. Existing deck rows are ignored and return zero affected rows.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity that owns the deck rows.
* @param starter_unit Starter unit to place in the default decks.
* @return Number of affected rows.
*/
inline drogon::Task<db::InterfaceResult<>> addDefaultDecks(
	const db::Database database,
	const UserIdentity identity,
	const UserUnitInfo starter_unit)
{
	if (!database || identity.userId.empty() || starter_unit.user_unit_id == 0)
	{
		LOG_ERROR << "Invalid addDefaultDecks call: "
			<< "db=" << static_cast<bool>(database)
			<< ", user_id_empty=" << identity.userId.empty()
			<< ", user_unit_id=" << starter_unit.user_unit_id;
		throw std::invalid_argument("Invalid addDefaultDecks call");
	}

	auto result = co_await database->execSqlCoro(
		"WITH RECURSIVE deck_nums(deck_num) AS ("
		"SELECT 0 "
		"UNION ALL "
		"SELECT deck_num + 1 FROM deck_nums WHERE deck_num < 9"
		") "
		"INSERT INTO user_decks ("
		"user_id, "
		"user_unit_id, "
		"deck_type, "
		"deck_num, "
		"member_type, "
		"disp_order"
		") "
		"SELECT "
		"$1, "
		"$2, "
		"1, "
		"deck_nums.deck_num, "
		// The client treats member_type 0 as the party leader.
		"0, "
		"2 "
		"FROM deck_nums "
		"WHERE NOT EXISTS ("
		"SELECT 1 "
		"FROM user_decks "
		"WHERE user_decks.user_id = $1"
		");",
		identity.userId,
		starter_unit.user_unit_id);

	co_return db::InterfaceResult<>{
		.data = {},
		.affected = result.affectedRows(),
	};
}

/*!
* Persists deck rows posted by DeckEditRequest.
* For each deck included in the request, existing rows are replaced with the
* occupied slots sent by the client. Missing slots are treated as removed.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity that owns the deck rows.
* @param decks Deck slot packets to persist.
* @return Number of affected rows.
*/
inline drogon::Task<db::InterfaceResult<>> updateDecks(
	const db::Database database,
	const UserIdentity identity,
	const std::vector<UserPartyDeckInfo> decks)
{
	if (!database || identity.userId.empty())
	{
		LOG_ERROR << "Invalid updateDecks call: "
			<< "db=" << static_cast<bool>(database)
			<< ", user_id_empty=" << identity.userId.empty();
		throw std::invalid_argument("Invalid updateDecks call");
	}

	// For each modified party, the client sends the full current party state.
	// Any missing slot should therefore be treated as a removed unit.
	//
	// The simplest way to persist that is to clear each affected deck first,
	// then insert the UserPartyDeckInfo rows the client sent.
	std::set<std::pair<int32_t, int32_t>> affectedDecks;
	for (const auto& deck : decks)
	{
		affectedDecks.emplace(deck.deck_type, deck.deck_num);
	}

	size_t affected = 0;
	for (const auto& [deckType, deckNum] : affectedDecks)
	{
		const auto result = co_await db::DatabaseInterface::remove(
			database,
			"user_decks",
			{
				db::Lookup("user_id", identity.userId),
				db::Lookup("deck_type", deckType),
				db::Lookup("deck_num", deckNum),
			});
	}

	for (const auto& deck : decks)
	{
		const auto insert = co_await db::PacketInterfaceFor<UserPartyDeckInfo>::insert(
			database,
			"user_decks",
			deck,
			{ db::Data("user_id", identity.userId) });
		affected += insert.affected;
	}

	co_return db::InterfaceResult<>{
		.data = {},
		.affected = affected,
	};
}

/*!
* Builds a login-info packet from the persisted user row.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return Login info packet populated from the database.
*/
inline drogon::Task<db::InterfaceResult<LoginInfoResp>> getLoginInfo(
	const db::Database database,
	const UserIdentity identity)
{
	auto result = co_await db::PacketInterfaceFor<LoginInfoResp>::read(
		database,
		"user_info",
		{
			db::Lookup("gumi_user_id", identity.gumiUserId),
			db::Lookup("id", identity.userId),
		});
	auto packet = std::move(result.nonEmpty().front());

	// tutorial_status (9sQM2XcN) and tutorial_end_flag (sv6BEI8X) are both read
	// straight from user_info now.  The flag used to be derived here as
	// `tutorial_status >= 12`, which ended the tutorial four scripts early and
	// made the status pointer impossible to test independently; TutorialUpdate
	// persists what the client reports instead (14082026_AddTutorialEndFlag).

	co_return db::InterfaceResult<LoginInfoResp>{
		.data = std::move(packet),
		.affected = result.affected,
	};
}

/*!
* Builds a team-info packet from the persisted user row.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return Team info packet populated from the database and derived cache data.
*/
inline drogon::Task<db::InterfaceResult<UserTeamInfo>> getTeamInfo(
	const db::Database database,
	const UserIdentity identity)
{
	auto result = co_await db::PacketInterfaceFor<UserTeamInfo>::read(
		database,
		"user_info",
		{ db::Lookup("id", identity.userId) });
	auto packet = std::move(result.nonEmpty().front());

	packet.reinforcement_deck.emplace_back(0);
	packet.reinforcement_deck.emplace_back(0);
	packet.reinforcement_deck.emplace_back(0);
	packet.add_unit_count = 100;

	if (const auto mst = getLevelMst(packet.level))
	{
		packet.deck_cost = mst->deck_cost;
		packet.max_action_point = mst->energy;
		packet.max_friend_count = mst->friend_count;
		packet.add_friend_count = mst->add_friend_count;
	}

	// Calculate the current energy points of the user.
	const auto energyFullTs = (co_await db::DatabaseInterface::read(
		database,
		"user_info",
		{
			db::Data("energy_full_ts"),
			db::Lookup("id", identity.userId),
		})).front<uint64_t>("energy_full_ts");
	packet.energy_full_seconds = UserEnergy::derive(
		packet.level,
		energyFullTs,
		packet.energy);

	co_return db::InterfaceResult<UserTeamInfo>{
		.data = std::move(packet),
		.affected = result.affected,
	};
}

/*!
* Resolves and validates the current local user identity.
*
* The server stores one current Gumi Live user id, then maps it to the local
* user_info id. Requests must send the same Gumi Live id, and normally must also
* send the same user id. During user creation, the client may not have a local
* user id yet, so allowUnspecifiedUser lets callers accept an empty request
* user_id and use the database value instead.
*
* @param database Database client or transaction to use.
* @param req Login info from the request packet.
* @param allowUnspecifiedUser True when an empty request user_id is allowed.
* @return Resolved Gumi Live id and local user id.
*/
inline drogon::Task<db::InterfaceResult<UserIdentity>> getUserIdentity(
	const db::Database database,
	const LoginInfoReq req,
	const bool allowUnspecifiedUser=false)
{
	const auto gumiUserId = (co_await db::DatabaseInterface::read(
		database,
		"gumi_live_users",
		{ db::Data("id") })).front<std::string>("id");
	if (gumiUserId != req.gumi_live_userid)
	{
		LOG_ERROR << "Gumi Live user ID mismatch: client sent " << req.gumi_live_userid
			<< ", database has " << gumiUserId;
		throw std::runtime_error("Gumi Live user ID mismatch: client sent "
			+ req.gumi_live_userid + ", database has " + gumiUserId);
	}

	// Upon user creation, the client might not have a user Id stored locally yet,
	// so we allow the client to omit it in the request, and instead fallback to
	// whatever user ID we have in the database.
	if (!allowUnspecifiedUser && req.user_id.empty())
	{
		LOG_ERROR << "Missing user_id; request did not include user_id";
		throw std::runtime_error("Missing user_id; request did not include user_id");
	}

	auto user = co_await db::DatabaseInterface::read(
		database,
		"user_info",
		{
			db::Data("id"),
			db::Lookup("gumi_user_id", gumiUserId),
	});
	std::string userId;
	if (user.affected > 0)
	{
		userId = user.front<std::string>("id");
	}

	// If the client gave us a user_id, we require it to match the database.
	if (!req.user_id.empty() && userId != req.user_id)
	{
		LOG_ERROR << "User ID mismatch for Gumi Live user " << gumiUserId
			<< ": client sent " << req.user_id
			<< ", database has " << userId;
		throw std::runtime_error("User ID mismatch for Gumi Live user "
			+ gumiUserId + ": client sent " + req.user_id + ", database has " + userId);
	}

	co_return db::InterfaceResult<UserIdentity>{
		.data = {
			.gumiUserId = gumiUserId,
			.userId = userId,
		},
		// This means we must have found a user for the nonEmpty() check to
		// succeed.
		.affected = user.affected,
	};
}

/*!
* Folds one battle's statistics into the caller's lifetime trophy archive.
*
* UserTeamArchive (zI2tJB7R) is the progress half of the trophy system: every
* one of its counters is answered by PlayerInfoBattleResultScene::getActual
* (libgame.so 0x1791EEC), a 32KB chain of TrophyMst::getTrophyID() compares that
* maps exactly one trophy id to one counter.
*
* These eight are the ones the client already reports in every MissionEnd
* request under rXvA1E5y, with byte-identical keys, each sourced from BattleLog
* — i.e. THIS BATTLE's value, never a running total, so the server accumulates.
*
* SUM vs MAX comes from each trophy's own label in deploy/mst/trophy_mst.json
* (累計 / 総合 sum, 最大 max), NOT from the setter name:
*
*   b_crystal              += hoG2ieT5   trophy 100310 累計バトルクリスタル出現数
*   h_crystal              += 6PLsn8xo   trophy 100320 累計ハートクリスタル出現数
*   battle_spark_cnt       += U8uZLA34   trophy 100350 累計スパーク回数
*   battle_skill_cnt       += rZQJF5G9   trophy 100360 累計BB使用回数
*   quest_mimic_cnt        += TW1Mrtp5   trophy 200040 総合ミミック出現数
*   battle_turn_max_damage  = max(…, 5NRJQ1LU)  trophy 100330 戦闘1ターン最大ダメージ数
*   battle_turn_max_spark   = max(…, XP06YWdT)  trophy 100340 戦闘1ターン最大スパーク回数
*   turn_max_unit_damage    = max(…, e6BKoYy9)  trophy 100325 ユニット単体最大ダメージ数
*
* Negative values are clamped to 0 before folding: the wire fields are quoted
* ints the client controls, and a negative would corrupt a lifetime total that
* nothing ever recomputes.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to update.
* @param battle   The request's rXvA1E5y block.
*/
inline drogon::Task<void> accumulateBattleArchive(
	const db::Database database,
	const UserIdentity identity,
	const ::MissionBattleResultInfo battle)
{
	const auto clamp = [](const int32_t v) { return std::max(v, 0); };

	// One UPSERT rather than read-modify-write: MissionEnd already runs inside a
	// transaction on the single-connection SQLite pool, and a second await here
	// would be another sequential round trip (handbook §6.14).  The conflict
	// clause does the summing and the max-ing in SQL, so a concurrent clear
	// cannot lose an update.
	//
	// Values are ints off a parsed struct plus a server-minted user id, so
	// inlining them carries no injection surface.
	const std::string sql =
		"INSERT INTO user_team_archive ("
		"user_id, b_crystal, h_crystal, battle_spark_cnt, battle_skill_cnt,"
		" quest_mimic_cnt, battle_turn_max_damage, battle_turn_max_spark,"
		" turn_max_unit_damage) VALUES ('"
		+ identity.userId + "',"
		+ std::to_string(clamp(battle.battle_crystal_num)) + ","
		+ std::to_string(clamp(battle.heart_crystal_num)) + ","
		+ std::to_string(clamp(battle.spark_cnt)) + ","
		+ std::to_string(clamp(battle.skill_use_cnt)) + ","
		+ std::to_string(clamp(battle.mimic_cnt)) + ","
		+ std::to_string(clamp(battle.max_turn_damage)) + ","
		+ std::to_string(clamp(battle.max_turn_spark_cnt)) + ","
		+ std::to_string(clamp(battle.one_attack_damage)) + ")"
		" ON CONFLICT(user_id) DO UPDATE SET"
		" b_crystal        = b_crystal        + excluded.b_crystal,"
		" h_crystal        = h_crystal        + excluded.h_crystal,"
		" battle_spark_cnt = battle_spark_cnt + excluded.battle_spark_cnt,"
		" battle_skill_cnt = battle_skill_cnt + excluded.battle_skill_cnt,"
		" quest_mimic_cnt  = quest_mimic_cnt  + excluded.quest_mimic_cnt,"
		" battle_turn_max_damage = MAX(battle_turn_max_damage, excluded.battle_turn_max_damage),"
		" battle_turn_max_spark  = MAX(battle_turn_max_spark,  excluded.battle_turn_max_spark),"
		" turn_max_unit_damage   = MAX(turn_max_unit_damage,   excluded.turn_max_unit_damage);";

	try
	{
		co_await database->execSqlCoro(sql);
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		// Never fail a mission clear over a statistics row.  The reward, the
		// unit drops and the clear flag matter; a missed trophy tick does not.
		LOG_WARN << "accumulateBattleArchive: " << ex.base().what();
	}
	co_return;
}

/*!
* Loads the caller's lifetime trophy archive for zI2tJB7R.
*
* Returns a single-element vector because that is the shape UserInfoResp wants.
* Only the eight counters accumulateBattleArchive maintains are filled; the
* other 31 fields stay 0 because nothing feeds them yet, which reads on the
* client as a trophy with no progress rather than as an error.
*
* A user with no row yet yields an all-zero entry rather than an empty vector —
* UserTeamArchiveResponse is a singleton readParam, so sending the row is what
* makes the Trophy screen show 0/N instead of leaving stale values in place.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return One UserTeamArchive, ready to serialise under zI2tJB7R.
*/
inline drogon::Task<std::vector<::UserTeamArchive>> loadTeamArchive(
	const db::Database database,
	const UserIdentity identity)
{
	::UserTeamArchive archive = {};
	archive.user_id = identity.userId;

	try
	{
		const auto rows = co_await database->execSqlCoro(
			"SELECT b_crystal, h_crystal, battle_spark_cnt, battle_skill_cnt,"
			" quest_mimic_cnt, battle_turn_max_damage, battle_turn_max_spark,"
			" turn_max_unit_damage FROM user_team_archive WHERE user_id = $1;",
			identity.userId);

		if (!rows.empty())
		{
			const auto& row = rows.front();
			archive.b_crystal              = row["b_crystal"].as<int32_t>();
			archive.h_crystal              = row["h_crystal"].as<int32_t>();
			archive.battle_spark_cnt       = row["battle_spark_cnt"].as<int32_t>();
			archive.battle_skill_cnt       = row["battle_skill_cnt"].as<int32_t>();
			archive.quest_mimic_cnt        = row["quest_mimic_cnt"].as<int32_t>();
			archive.battle_turn_max_damage = row["battle_turn_max_damage"].as<int32_t>();
			archive.battle_turn_max_spark  = row["battle_turn_max_spark"].as<int32_t>();
			archive.turn_max_unit_damage   = row["turn_max_unit_damage"].as<int32_t>();
		}
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		LOG_WARN << "loadTeamArchive: " << ex.base().what();
	}

	co_return std::vector<::UserTeamArchive>{ std::move(archive) };
}

/*!
* Reads the caller's cleared-mission history (UT1SVg59).
*
* THE progression driver: the client evaluates feature unlocks against this
* list, and the server's own PermitPlace progression gate reads the same set so
* the two agree. Backed by user_campaign_missions rows with state=2, written by
* MissionEnd and CampaignBattleEnd.
*
* Shared by UserInfo and UpdateInfoLight — both have to report the identical
* set or a refresh would contradict the login snapshot.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return One entry per cleared mission, ready to serialise under UT1SVg59.
*/
inline drogon::Task<std::vector<::UserClearMissionInfo>> getClearedMissions(
	const db::Database database,
	const UserIdentity identity)
{
	std::vector<::UserClearMissionInfo> cleared{};

	const auto rows = co_await database->execSqlCoro(
		"SELECT mission_id, clear_count, last_cleared_at"
		" FROM user_campaign_missions WHERE user_id = $1 AND state = 2;",
		identity.userId);

	for (const auto& row : rows)
	{
		::UserClearMissionInfo entry = {};
		entry.user_id = identity.userId;
		try
		{
			entry.mission_id = std::stoi(row["mission_id"].as<std::string>());
		}
		catch (...)
		{
			continue;
		}
		entry.clear_cnt = row["clear_count"].as<int32_t>();
		if (const auto epoch = row["last_cleared_at"].as<int64_t>(); epoch > 0)
		{
			// setClearDate is a string setter; "YYYY-MM-DD hh:mm:ss" until a
			// capture proves otherwise (KDL doc marks it UNVERIFIED).
			std::tm tmv = {};
			const time_t t = static_cast<time_t>(epoch);
			localtime_s(&tmv, &t);
			char buf[24] = {};
			std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
			entry.clear_date = buf;
		}
		cleared.push_back(std::move(entry));
	}

	co_return cleared;
}

/*!
* Reads the caller's Vortex dungeon-key inventory, seeding any row DungeonKeyMst
* declares that the player does not have yet.
*
* The two derived response fields — receipt_possible_flg and
* next_receipt_possible_date — are computed against the local calendar on every
* call rather than stored, so the result is only valid for the request that
* asked for it.  See the 08082026_CreateUserDungeonKeysTable migration.
*
* Declared here rather than defined inline because the calendar helpers it
* needs are private to the translation unit.  Defined in
* gme/handlers/DungeonKey.cpp; consumed there and by UserInfo.
*
* @param db Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return One entry per DungeonKeyMst row, ready to serialise under eFU7Qtb0.
*/
drogon::Task<std::vector<::UserDungeonKeyInfo>> dungeonKeyState(
	const db::Database db,
	const UserIdentity identity);

}
