#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/archive/MissionArchiver.hpp>
#include <gimuserver/gme/common/BattleItems.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/DailyTask.hpp>
#include <gimuserver/gme/common/FriendPoints.hpp>
#include <gimuserver/gme/common/Friends.hpp>
#include <gimuserver/gme/common/MissionBreak.hpp>
#include <gimuserver/gme/common/MissionRuns.hpp>
#include <gimuserver/gme/common/FrontierGate.hpp>
#include <gimuserver/gme/common/PermitPlace.hpp>
#include <gimuserver/gme/common/ResearchLab.hpp>

#include <algorithm>
#include <chrono>
#include <charconv>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace
{
constexpr uint32_t kFirstTutorialMission = 1;
constexpr uint32_t kSecondTutorialMission = 2;

// Tutorial CHAPTER ids (9sQM2XcN), not script numbers — the client maps one to
// the other in GameUtils::getTutorialScriptFile (0x1EB8758), and the mapping is
// NOT the identity:
//
//     chapter 1-9   -> tuto1-9.txt
//     chapter 50-54 -> tuto10-14.txt
//     chapter 10    -> tuto15.txt   (the free-summon tutorial)
//     chapter 11    -> tuto16.txt   (Tilith's farewell)
//     chapter 12    -> tuto17.txt   (does not exist -> nothing plays = done)
//
// ⚠ THE SECOND CHECKPOINT USED TO BE 10, AND THAT SKIPPED FIVE SCRIPTS.
// Chapter 10 is tuto15, so clearing mission 2 jumped the player straight from
// tuto2 to the free-summon tutorial and the whole 50..54 band -- tuto10 through
// tuto14 -- never ran.  The cost was not cosmetic: **tuto14 is the one that
// issues `battle_ui_on`**, so the BB/SBB/UBB/DBB commands were never enabled
// and manual play was left with Guard.  It also fires the summon tutorial ahead
// of the ones meant to precede it, which is why fresh saves see tutorials out
// of order.
//
// These checkpoints are RESUME points, not the driver: the client walks the
// sequence itself and reports each step through TutorialUpdate, which stores
// what it is told.  A resume point ahead of the player therefore does not
// "skip ahead harmlessly" -- it tells the client it is further along than it
// is, and everything in between is lost.
//
// 50 is the chapter that follows tuto9, so the client resumes at tuto10 and
// walks 50->54 then 10, 11, 12 (tuto17 does not exist = done) in order.
constexpr uint8_t kFirstTutorialCheckpoint = 2;
constexpr uint8_t kSecondTutorialCheckpoint = 50;

// What Karl hands over in tuto15.txt ("You received 5 Gems"), which is exactly
// the cost of summon gate 2000 — the gate the client itself labels
// "Tutorial Gacha" (type 20000, GachaActionScene::initConnect @ 0x16988C0).
constexpr uint32_t kTutorialSummonGems = 5;

// Clearing every mission in a Grand Gaia quest dungeon pays this, once — the
// "1 Gem from fully completing a stage in a Quest map" on the wiki's In-Game
// Credits page.  203 dungeons across the campaign, so ~1 Gem per 5 missions.
// See ServerCache::missionQuestDungeon for why the dungeon and not the area.
//
// This is a SERVER-SIDE rule, not MST data: F_MISSION_MST carries exp, zel and
// karma per mission and no gem column at all, and neither AreaMst nor
// DungeonMst has a reward field — so the live server is what paid it, and this
// is where we pay it.  See tools/wiki_gem_scrape.py for the source.
constexpr int64_t kDungeonClearGems = 1;

// Matches CampaignReceipt / DailySpin — the client's counter is 4 digits.
constexpr int64_t kMaxGems = 9'999LL;

/*!
* Pays the clear Gem when a first clear completes a Grand Gaia quest dungeon.
*
* Called only on a FIRST clear, which is what makes it pay once per dungeon
* with no extra bookkeeping: the grant needs the last uncleared mission in the
* dungeon to become cleared, and once that has happened there are no first
* clears left in that dungeon to trigger it again.  Replaying a mission is not
* a first clear, so it cannot pay twice.
*
* @param database Transaction the surrounding MissionEnd is running in.
* @param identity Resolved caller.
* @param missionId The mission just cleared.
*/
// ---------------------------------------------------------------------------
// First-clear rewards
//
// F_MISSION_MST::clear_rewards (SiYs27Cj) carries them on 787 missions as
// comma-separated `type:id:amount:?:?`, in the SAME reward vocabulary the
// present box and CampaignReceipt already dispatch on -- 3 zel, 8 gem, 6 unit,
// 4/5/7 item/material/sphere, 11 karma, 17 summoner SP.
//
// They are delivered as PRESENTS rather than granted inline.  That is the
// deferred half of the reward system this server already has (addUserPresent),
// it reuses one tested payout path instead of adding a second per-type switch,
// and it matches what the game told the player: "Gifts have been awarded to
// you. Visit your presents box to receive them."
// ---------------------------------------------------------------------------
struct ClearReward { int32_t type; std::string id; int32_t amount; };

static std::vector<ClearReward> parseClearRewards(const std::string& raw)
{
	std::vector<ClearReward> out;
	size_t start = 0;
	while (start <= raw.size())
	{
		const auto comma = raw.find(',', start);
		const auto entry = raw.substr(start, comma == std::string::npos
		                                     ? std::string::npos : comma - start);
		if (!entry.empty())
		{
			// type:id:amount, plus two trailing fields nothing reads.
			std::vector<std::string> parts;
			size_t f = 0;
			while (f <= entry.size() && parts.size() < 5)
			{
				const auto colon = entry.find(':', f);
				parts.push_back(entry.substr(f, colon == std::string::npos
				                                ? std::string::npos : colon - f));
				if (colon == std::string::npos) break;
				f = colon + 1;
			}
			if (parts.size() >= 3)
			{
				try
				{
					const auto type   = std::stoi(parts[0]);
					const auto amount = std::stoi(parts[2]);
					if (type > 0 && amount > 0)
						out.push_back({ type, parts[1] == "0" ? std::string() : parts[1], amount });
				}
				catch (const std::exception&) { /* malformed entry: skip it */ }
			}
		}
		if (comma == std::string::npos) break;
		start = comma + 1;
	}
	return out;
}

// The result screen's own bonus line.  MissionResultBaseScene::getRewardBonusZel
// @0x18B9780 parses this as comma-separated `type:_:amount` triples and sums
// the entries matching its type -- and it only ever asks for 3 (zel), 11
// (karma) and 17 (SP).  Everything else in the reward list is delivered by
// present and would simply be ignored here, so it is not emitted.
static std::string encodeClearBonus(const std::vector<ClearReward>& rewards)
{
	std::string out;
	for (const auto& r : rewards)
	{
		if (r.type != 3 && r.type != 11 && r.type != 17)
			continue;
		if (!out.empty()) out += ',';
		out += std::to_string(r.type) + ":0:" + std::to_string(r.amount);
	}
	return out;
}

// Called on a FIRST clear only.  Returns the quest dungeon this clear completed
// (every one of its missions now cleared), or 0 -- which is also exactly the
// dungeon MissionEnd reports as newly cleared (4sQ8vBXm).
drogon::Task<int32_t> grantDungeonClearGem(
	const db::Database database,
	const gme::UserIdentity identity,
	const uint32_t missionId)
{
	const auto& cache = theServer()->cache();
	const auto dungeonIt = cache.missionQuestDungeon().find(static_cast<int32_t>(missionId));
	if (dungeonIt == cache.missionQuestDungeon().end())
		co_return 0; // Vortex, Frontier Gate, event content — no clear Gem.

	const auto missionsIt = cache.missionsByDungeon().find(dungeonIt->second);
	if (missionsIt == cache.missionsByDungeon().end())
		co_return 0;

	const auto& dungeonMissions = missionsIt->second;

	// One query rather than a per-mission loop: MissionEnd already runs a long
	// chain of awaits inside a transaction on a single-connection SQLite pool,
	// and that is exactly the shape that wedged the pool in §6.14.
	//
	// The ids are QUOTED because user_campaign_missions.mission_id is TEXT (the
	// insert above writes std::to_string(serial_id)).  SQLite would in fact
	// apply the column's TEXT affinity to a bare integer in an IN list and
	// match anyway, but that is a rule worth not depending on.  These are our
	// own MST ints, so there is nothing to escape.
	std::string idList;
	idList.reserve(dungeonMissions.size() * 10);
	for (size_t i = 0; i < dungeonMissions.size(); ++i)
	{
		if (i)
			idList += ',';
		idList += '\'';
		idList += std::to_string(dungeonMissions[i]);
		idList += '\'';
	}

	const auto rows = co_await database->execSqlCoro(
		"SELECT COUNT(DISTINCT mission_id) AS cleared FROM user_campaign_missions"
		" WHERE user_id = $1 AND state = 2 AND mission_id IN (" + idList + ");",
		identity.userId);
	if (rows.empty())
		co_return 0;

	const auto cleared = rows[0]["cleared"].as<int64_t>();
	if (cleared < static_cast<int64_t>(dungeonMissions.size()))
		co_return 0;

	co_await database->execSqlCoro(
		"UPDATE user_info SET gems = MIN(gems + $1, $2)"
		" WHERE gumi_user_id = $3 AND id = $4;",
		kDungeonClearGems,
		kMaxGems,
		identity.gumiUserId,
		identity.userId);

	LOG_INFO << "MissionEnd: quest dungeon " << dungeonIt->second << " fully cleared by "
	         << identity.userId << " (" << dungeonMissions.size()
	         << " missions) — awarded " << kDungeonClearGems << " gem";
	co_return dungeonIt->second;
}

// Fully consume decimal fields; std::stoul accepted suffixes and signed input.
uint32_t dropNumber(const std::string& text)
{
	uint32_t value = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc{} || end != text.data() + text.size() || value == 0)
		throw std::runtime_error("Invalid mission drop number: " + text);
	return value;
}

std::vector<UserUnitInfo> parseUnitDrops(const std::string& unitDrops)
{
	std::vector<UserUnitInfo> units;
	std::istringstream drops(unitDrops);
	for (std::string drop; std::getline(drops, drop, ',');)
	{
		if (drop.empty())
		{
			continue;
		}

		std::istringstream parts(drop);
		std::string id;
		std::string level;
		std::string type;
		if (!std::getline(parts, id, ':')
			|| !std::getline(parts, level, ':')
			|| !std::getline(parts, type, ':'))
		{
			throw std::runtime_error("Invalid mission drop unit entry: " + drop);
		}

		const auto dropType = dropNumber(type);
		if (dropType > 6)
			throw std::runtime_error("Invalid mission drop unit type: " + type);
		auto unit = gme::fromArchivedUnit(dropNumber(id), dropType);
		if (!unit)
		{
			throw std::runtime_error("Unknown mission drop unit or type: " + drop);
		}

		// A drop can specify a level above 1, and base_* must follow it.
		// fromArchivedUnit hands back the LEVEL-1 statline, and the client
		// displays user_units.base_* verbatim, so setting the level alone
		// produced a "Lv 30" unit with level-1 stats.
		const auto dropLevel = dropNumber(level);
		const auto& catalogue = theServer()->cache().unitMst();
		const auto mst = std::find_if(catalogue.begin(), catalogue.end(),
			[&](const auto& row) { return row.id == static_cast<int32_t>(unit->unit_id); });
		if (mst == catalogue.end() || mst->max_lv < 1 || dropLevel > static_cast<uint32_t>(mst->max_lv))
			throw std::runtime_error("Mission drop level exceeds unit master: " + drop);

		unit->unit_lvl = dropLevel;
		gme::scaleUnitBaseStats(*unit, static_cast<int>(dropLevel));
		units.push_back(std::move(*unit));
	}

	return units;
}

// Parses the client-reported item drops from MissionBattleResultInfo.item_rewards
// (wire key 4T0Q2Bh5).
//
// APK BattleRewardList::getItemCsv @0x10DFA24 emits itemId:count pairs,
// comma-separated. MissionEndRequest sends them under 4T0Q2Bh5 @0x13A7AE4.
// Reject an invalid batch before any grants are committed. The producer emits
// exactly two positive decimal fields; a bare id is not an observed wire form.
std::vector<std::pair<uint32_t, uint32_t>> parseItemDrops(const std::string& itemDrops)
{
	std::vector<std::pair<uint32_t, uint32_t>> items;
	const auto& catalogue = theServer()->cache().itemMst();
	if (itemDrops.empty()) return items;
	size_t at = 0;
	while (at < itemDrops.size())
	{
		const auto comma = itemDrops.find(',', at);
		const auto entry = itemDrops.substr(at, comma == std::string::npos ? comma : comma - at);
		const auto colon = entry.find(':');
		if (colon == std::string::npos || entry.find(':', colon + 1) != std::string::npos)
			throw std::runtime_error("Invalid mission item drop: " + entry);
		const auto id = dropNumber(entry.substr(0, colon));
		const auto count = dropNumber(entry.substr(colon + 1));
		if (id > INT32_MAX || count > INT32_MAX || std::none_of(catalogue.begin(), catalogue.end(),
			[id](const auto& row) { return row.id == static_cast<int32_t>(id); }))
			throw std::runtime_error("Unknown or oversized mission item drop: " + entry);
		items.emplace_back(id, count);
		if (comma == std::string::npos) break;
		at = comma + 1;
		if (at == itemDrops.size()) throw std::runtime_error("Trailing mission item delimiter");
	}
	return items;
}

std::string encodeUnitDrops(
	const std::vector<UserUnitInfo>& unitDrops,
	const std::vector<bool>& newFlags)
{
	std::string encoded;
	for (size_t idx = 0; idx < unitDrops.size(); ++idx)
	{
		if (!encoded.empty())
		{
			encoded += ',';
		}

		const auto& drop = unitDrops[idx];

		// reward_units is formatted as:
		// unit_id:user_unit_id:play_acquired_animation:show_reward_row
		encoded += std::to_string(drop.unit_id);
		encoded += ':';
		encoded += std::to_string(drop.user_unit_id);
		encoded += ':';
		encoded += (
			idx < newFlags.size() && newFlags[idx]
				? '1'
				: '0');
		encoded += ":1";
	}

	return encoded;
}

/*!
* The player level cap: DefineMst max_team_lv (Kt8H4LN7, 999 in this data).
*
* ⚠ NOT "the last UserLevelMst row".  The level table carries a row for the
* level AFTER the cap: MissionResultScene::updateEvent seeds its EXP counter
* with GameUtils::getNeedUserExp(beforeLv + 1, true) @0x11762A0, which skips
* the max-level check and returns -1 when the row is missing.  At level 999
* that -1 becomes a NEGATIVE per-frame step, and MissionResultBaseScene::countUp
* @0x18B8784 compares `counted + step < total`, which then never becomes false
* -- the result screen froze for every level-999 player.  The row for 1000 is
* the upstream capture's own (see deploy/mst/user_level_mst.json), so the cap
* has to come from the define the client itself compares against.
*/
uint32_t maxPlayerLevel()
{
	const auto cap = theServer()->cache().initializeResp().defines.max_team_lv;
	return cap > 0 ? static_cast<uint32_t>(cap) : 999u;
}

/*!
* Player EXP Boost (#26): the percent a unit's LEADER SKILL adds to quest EXP.
*
* Passive/process 97 is "Player EXP Boost" (Global wiki, Player EXP Boost rev
* 653737: buff 79, passive 97), and its first parameter is the percent --
* Perpetual Flaw Roglizer's Historical Epilogue (LS 10168) carries "97" with
* "20", which its wiki page notes as "20% EXP" (rev 642770); Holy End
* Roglizer's End of the World (LS 10167) carries 15.  The client has no
* consumer for it: the result screen shows reward_info.inc_exp as the server
* sent it, so the server owns this bonus.
*
* The leader is read from UnitMst by species, as the battle does
* (BattleParty::setPartyPassiveList @0x10B3E10 -> UnitMst::getLeaderSkillID),
* not from user_units.leader_skill_id, which is 0 on units granted before that
* column was written.
*
* LeaderSkillMst.process_param is modelled as a comma list, but its per-process
* separator is '@' ("80,80,80,0,80@15@110,0,0,0,0,0@15" for LS 10167), so the
* list is re-joined and split on '@' to line up with process_id.
*/
int32_t leaderExpBoostPercent(const int32_t unitId)
{
	constexpr int32_t kPlayerExpBoost = 97;
	const auto& cache = theServer()->cache();
	const auto unit = std::find_if(cache.unitMst().begin(), cache.unitMst().end(),
		[unitId](const auto& u) { return u.id == unitId; });
	if (unit == cache.unitMst().end() || unit->leader_skill_id <= 0)
		return 0;
	const auto ls = std::find_if(cache.leaderSkillMst().begin(), cache.leaderSkillMst().end(),
		[&](const auto& l) { return l.leader_skill_id == unit->leader_skill_id; });
	if (ls == cache.leaderSkillMst().end())
		return 0;

	std::string joined;
	for (size_t i = 0; i < ls->process_param.size(); ++i)
		joined += (i ? "," : "") + ls->process_param[i];
	std::vector<std::string> params;
	std::stringstream split(joined);
	for (std::string part; std::getline(split, part, '@');)
		params.push_back(part);

	int32_t percent = 0;
	for (size_t i = 0; i < ls->process_id.size() && i < params.size(); ++i)
	{
		if (ls->process_id[i] != kPlayerExpBoost)
			continue;
		const auto first = params[i].substr(0, params[i].find(','));
		int32_t value = 0;
		const auto [end, error] = std::from_chars(first.data(), first.data() + first.size(), value);
		if (error == std::errc{} && value > 0)
			percent += value;
	}
	return percent;
}

/*!
* Applies pending EXP to the current user level.
*
* The caller should pass exp after adding the mission reward. This function
* treats UserLevelMst::exp as the per-level chunk required to reach level + 1,
* then mutates level and exp so exp remains the residual progress at the new
* level.  It never levels past maxLevel, and at the cap nothing carries over:
* the client draws an empty bar there (MissionResultScene picks 0 when
* getMaxTeamLv() == beforeLv @0x18CCE4C) and there is no level left to spend
* it on.
*
* @return True if at least one level was gained.
*/
bool levelUp(uint32_t& level, uint32_t& exp, const uint32_t maxLevel)
{
	bool leveled = false;
	while (level < maxLevel)
	{
		const auto mst = gme::getLevelMst(level + 1);
		if (!mst || exp < mst->exp)
			break;

		// Advance a level.
		level++;
		exp -= mst->exp;
		leveled = true;
	}

	if (level >= maxLevel)
		exp = 0;

	return leveled;
}
}

HANDLEF(MissionEnd)
{
	MissionEndReq req = {};
	const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json);
	if (ec)
	{
		const auto error = glz::format_error(ec, json);
		LOG_ERROR << "MissionEndReq deserialization failed:\n" << error;
		co_return HandleResult::error("Deserialization error", error);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// THE SERIAL NAMES THE BATTLE, NOT THE MISSION (gme/common/MissionRuns.hpp).
	// Resolved once here; everything below uses missionId.  A run serial this
	// user was never issued cannot be settled against any mission.
	const uint32_t wireSerial = req.mission_num.serial_id;
	const bool issuedSerial = wireSerial >= gme::kMissionRunSerialFloor;
	const auto resolvedMission = co_await gme::missionForSerial(theDb(), identity, wireSerial);
	if (!resolvedMission)
	{
		LOG_WARN << "MissionEnd: " << identity.userId << " sent battle serial " << wireSerial
			<< ", which was never issued to them";
		co_return HandleResult::error("Unknown battle", "No battle was started with that serial");
	}
	const uint32_t missionId = *resolvedMission;

	// Same battle-content fallback as MissionStart — without it a mission that
	// STARTS on template content would fail on completion instead, which is a
	// worse failure: the player fights the battle and then loses the result.
	// See the block in MissionStart for why only content is substituted.
	auto missionRecord = MissionArchiver::instance().lookup(missionId);
	if (!missionRecord)
	{
		missionRecord = MissionArchiver::instance().lookup(10);
		if (!missionRecord)
		{
			co_return HandleResult::error("Archive error", "Unable to find mission record");
		}
		// Relabelled for the same reason as MissionStart — the result screen
		// reads the mission back out of the response.
		missionRecord->id = missionId;

		LOG_WARN << "MissionEnd: mission " << missionId
		         << " has no authored battle content; using the template record "
		            "relabelled as this mission";
	}

	MissionEndResp resp{};
	std::string buffer;

	// A Frontier Gate battle sends MissionEnd win or lose, and 2 is a win
	// (UserState::getMissionState — the client subtracts the lost floor on
	// anything else).  A lost battle is not a clear: it pays only what the
	// client picked up, records no clear, and ends the run (below).
	const bool frontierBattle = req.frontier_battle && !req.frontier_battle->empty();
	const bool frontierLost = frontierBattle && req.mission_num.mission_status.has_value()
		&& *req.mission_num.mission_status != 2;

	// The same status decides a NORMAL mission: 103 of the 104 captured ends
	// carry "2", the other "3" (a loss or a give-up).  Honor is paid for a
	// cleared mission only, so a wipe cannot farm the helper.
	const bool missionLost = req.mission_num.mission_status.has_value()
		&& *req.mission_num.mission_status != 2;

	// This needs to be wrapped as a transaction to prevent cases where we issue
	// partial rewards upon mission completion.
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			auto userInfo = co_await db::DatabaseInterface::read(
				transaction,
				"user_info",
				{
					db::Data("level"),
					db::Data("exp"),
					db::Data("zel"),
					db::Data("karma"),
					db::Data("brave_coin"),
					db::Data("friend_points"),
					db::Data("reinforce_user_id"),
					db::Lookup("gumi_user_id", identity.gumiUserId),
					db::Lookup("id", identity.userId),
			});

			// Fetch the current user state.
			const auto currentZel = userInfo.front<uint64_t>("zel");
			const auto currentKarma = userInfo.front<uint64_t>("karma");
			const auto currentLevel = userInfo.front<uint32_t>("level");
			const auto currentExp = userInfo.front<uint32_t>("exp");
			const auto currentBraveCoin = userInfo.front<int32_t>("brave_coin");
			const auto currentHonor = userInfo.front<int32_t>("friend_points");
			const auto helperUserId = userInfo.front<std::string>("reinforce_user_id");

			// ONE RESULT PER BATTLE.  MissionStart records the battle it opened
			// (user_info.open_mission_id) and this clears it to 0, so a second
			// MissionEnd for the same start -- a client retry after a lost
			// reply, or a replayed request -- finds nothing open.  It must not
			// pay, record, drop or spend anything a second time; it gets the
			// current state with zero rewards instead of an error, because
			// every handler error closes the client's session.  NULL is a
			// battle opened before this column existed and is honoured once.
			//
			// EXACT FOR ISSUED SERIALS.  The client echoes the serial MissionStart
			// gave it, and that serial settles only while it is the open battle's
			// (open_mission_serial) -- so a repeat of a settled result, or a
			// late result from an earlier run of the SAME mission after a new
			// one started, both pay nothing.
			//
			// A MISSION-ID SERIAL SETTLES ONLY A BATTLE THAT PREDATES SERIALS.
			// open_mission_serial is NULL until the first MissionStart after the
			// upgrade (29092026_MissionRunSerials adds it without a default), and
			// every start and every settled result writes it; so NULL, and only
			// NULL, means the open battle -- open_mission_id, or nothing known for
			// a save older than that column too -- was started by a server that
			// sent the mission id as the serial.  That battle may settle once
			// with the mission-id serial its client holds.  Once any run has been
			// issued, the open battle has a serial of its own, and a mission-id
			// serial is at best a delayed result from before the upgrade: letting
			// it through would pay the new run and leave its genuine result
			// nothing to settle (QC 2026-09-30, P1).
			{
				const auto open = co_await transaction->execSqlCoro(
					"SELECT open_mission_id, open_mission_serial FROM user_info WHERE id = $1;",
					identity.userId);
				const bool serialEra = !open.empty() && !open[0]["open_mission_serial"].isNull();
				const bool openUnknown = open.empty() || open[0]["open_mission_id"].isNull();
				const auto openMission = openUnknown ? int64_t{ -1 } : open[0]["open_mission_id"].as<int64_t>();
				const auto openSerial = serialEra ? open[0]["open_mission_serial"].as<int64_t>() : int64_t{ 0 };
				const bool settles = issuedSerial
					? serialEra && openSerial == static_cast<int64_t>(wireSerial)
					: !open.empty() && !serialEra
						&& (openUnknown || openMission == static_cast<int64_t>(missionId));
				if (!settles)
				{
					LOG_WARN << "MissionEnd: " << identity.userId << " ended mission "
						<< missionId << " (serial " << wireSerial << ") but the open battle is "
						<< openMission << " (serial " << openSerial << "); answering with no rewards";

					resp.login_info = std::move((co_await gme::getLoginInfo(transaction, identity)).nonEmpty());
					gme::omitClientScenarioMarkers(resp.login_info);
					resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
					resp.energy_recover.action_point_threshold = resp.team_info.max_action_point;
					// Nothing was cleared by THIS reply, so it says so in the
					// client's own sentinels (see "WHAT THIS RESULT NEWLY
					// CLEARED" below): a mission id here would replay that
					// mission's end script, and an empty area id resets the map.
					resp.reward_info.clear_mission_id = 0;
					resp.reward_info.clear_dungeon_id.clear();
					resp.reward_info.clear_area_id = "0";
					resp.reward_info.before_level = currentLevel;
					resp.clear_mission_info = co_await gme::getClearedMissions(transaction, identity);
					const auto permit = co_await gme::buildPermitPlace(transaction, identity);
					if (const auto error = glz::write_json(resp, buffer); error)
						throw std::runtime_error(glz::format_error(error, buffer));
					gme::injectPermitPlace(buffer, permit);
					co_return HandleResult::success(buffer);
				}
				co_await transaction->execSqlCoro(
					"UPDATE user_info SET open_mission_id = 0, open_mission_serial = 0 WHERE id = $1;",
					identity.userId);
			}

			// Parsed before anything is paid: a malformed log fails the whole
			// result inside this transaction rather than being half-applied.
			const auto usedItems = gme::parseUseItemLog(req.battle_result.use_item_log);

			// HONOR for the Summoner Helper this run borrowed.  MissionStart
			// recorded who it was; the amount is the one the card promised
			// (gme::honorForMission picks the friend or stranger rate exactly
			// as ReinforcementInfo::getFriendPoint does), and it is credited
			// straight to the balance rather than paid into the present box,
			// because the result screen counts it off team_info.
			//
			// Cleared and zeroed in the same UPDATE below whatever the outcome,
			// so one start can only ever pay once.
			//
			// The roster decides the rate, and it is read on THIS transaction:
			// the SQLite pool has one connection, so a helper that opened its
			// own would deadlock against the one already held here.
			const auto helperWasFriend =
				co_await gme::isFriend(transaction, identity, helperUserId);
			const auto honorEarned =
				missionLost ? 0 : gme::honorForMission(helperUserId, helperWasFriend);

			// THE RESEARCH LAB PAYS ONCE: "Subsequent victories won't award
			// anything" (gme/common/ResearchLab.hpp).  A repeat clear of a lab
			// mission pays no archive Zel, Karma or EXP; its first-clear
			// rewards are already once-only below.  Read before the clear is
			// recorded.
			const auto labReplay = !missionLost
				&& gme::isResearchLabMission(static_cast<int32_t>(missionId))
				&& !(co_await transaction->execSqlCoro(
					"SELECT 1 FROM user_campaign_missions"
					" WHERE user_id = $1 AND mission_id = $2 AND state = 2;",
					identity.userId,
					std::to_string(missionId))).empty();

			// A reported loss earns none of the archive clear rewards.
			const auto payClear = !missionLost && !labReplay;
			const auto rewardZel = req.battle_result.zel + (payClear ? missionRecord->zel : 0);
			const auto rewardKarma = req.battle_result.karma + (payClear ? missionRecord->karma : 0);
			// Nothing is earned at the level cap ("prevent the user from gaining
			// any more experience"): the result shows no EXP obtained, and the
			// stored progress stays at 0 (levelUp below).
			const auto maxLevel = maxPlayerLevel();
			const auto baseExp = (payClear && currentLevel < maxLevel) ? missionRecord->exp : 0;

			// PLAYER EXP BOOST from the two leaders (see leaderExpBoostPercent).
			// Global wiki, Player EXP Boost rev 653737: quest EXP x (leader skill
			// 1 + leader skill 2 + ...), the player's leader and the friend's
			// leader ADDING ("Zelnite lead and friend ... totaling up to 30%").
			// The player's leader is the party member the client flags
			// member_type 0 in this very request; the helper's is the unit the
			// picker offered for the helper MissionStart recorded.  Rounded
			// down.  NOT modelled: Extra Skill / SP enhancement / item / guild /
			// event multipliers the same formula names.
			int32_t expBoostPercent = 0;
			if (baseExp > 0)
			{
				for (const auto& member : req.party_deck_info)
				{
					if (member.member_type != 0)
						continue;
					const auto unitRows = co_await transaction->execSqlCoro(
						"SELECT unit_id FROM user_units WHERE user_id = $1 AND user_unit_id = $2;",
						identity.userId, static_cast<int64_t>(member.user_unit_id));
					if (!unitRows.empty())
					{
						const auto raw = unitRows[0]["unit_id"].as<std::string>();
						int32_t species = 0;
						std::from_chars(raw.data(), raw.data() + raw.size(), species);
						expBoostPercent += leaderExpBoostPercent(species);
					}
					break;
				}
				if (gme::borrowedHelper(helperUserId))
					expBoostPercent += leaderExpBoostPercent(
						co_await gme::helperLeaderUnit(transaction, identity, helperUserId));
			}
			const auto rewardExp = static_cast<uint32_t>(
				static_cast<uint64_t>(baseExp) * static_cast<uint64_t>(100 + expBoostPercent) / 100);
			if (expBoostPercent > 0)
			{
				LOG_INFO << "MissionEnd: leader EXP boost +" << expBoostPercent << "% -> "
					<< baseExp << " becomes " << rewardExp;
			}

			// See if we leveled up.  Summed wide: a save-edited exp near the top
			// of the column must not wrap into a small number.
			auto newLevel = currentLevel;
			auto newExp = static_cast<uint32_t>(std::min<uint64_t>(
				static_cast<uint64_t>(currentExp) + rewardExp, UINT32_MAX));
			const auto leveledUp = levelUp(newLevel, newExp, maxLevel);

			// The client expects the post-mission total, including the amount earned
			// during this mission.
			(co_await db::DatabaseInterface::update(
				transaction,
				"user_info",
				{
					db::Data("level", newLevel),
					db::Data("exp", newExp),
					db::Data("zel", currentZel + rewardZel),
					db::Data("karma", currentKarma + rewardKarma),
					db::Data("friend_points", currentHonor + honorEarned),
					db::Data("reinforce_user_id", std::string{}),
					db::Lookup("gumi_user_id", identity.gumiUserId),
					db::Lookup("id", identity.userId)
				})).nonEmpty();

			if (leveledUp)
			{
				(co_await gme::UserEnergy::refresh(transaction, identity, newLevel, true)).nonEmpty();
			}

			// Persist tutorial checkpoints so leaving and returning mid-tutorial does not
			// replay completed steps.
			if (!missionLost && missionId == kFirstTutorialMission)
			{
				co_await transaction->execSqlCoro(
					"UPDATE user_info SET tutorial_status = $1"
					" WHERE id = $2 AND gumi_user_id = $3 AND tutorial_status < $1;",
					kFirstTutorialCheckpoint, identity.userId, identity.gumiUserId);

			}
			else if (!missionLost && missionId == kSecondTutorialMission)
			{
				// Chapter 10 arms the free-summon tutorial, and tuto15.txt opens
				// with Karl handing over the gems for it ("You received 5 Gems",
				// then "Now summon yourself a strong Unit!").  The script only
				// draws that message — the balance has to come from here, or the
				// player reaches the summon gate unable to pay for it.
				//
				// ⚠ THE GUARD CANNOT BE `tutorial_status < <checkpoint>`.  The
				// chapter ids are not monotonic -- the order is 1..9, 50..54,
				// 10, 11, 12 -- so against a checkpoint of 50 a FINISHED player
				// (chapter 12) still compares "less than", and replaying
				// mission 2 would reset them into the tutorial and pay the gems
				// again.  The old `< 10` hid this only because 10 happened to
				// be low.
				//
				// Advance only a player still inside the opening band, which is
				// the one case this checkpoint is for.  MissionEnd runs again on
				// every replay of mission 2, and everyone else is left alone.
				co_await transaction->execSqlCoro(
					"UPDATE user_info"
					" SET tutorial_status = $1, gems = gems + $2"
					" WHERE gumi_user_id = $3 AND id = $4"
					"   AND tutorial_status BETWEEN 1 AND 9;",
					kSecondTutorialCheckpoint,
					kTutorialSummonGems,
					identity.gumiUserId,
					identity.userId);
			}

			// Filled in below when this turns out to be a first clear; stays
			// empty otherwise, which is what the live server sent and what the
			// result screen's length check expects.
			std::string firstClearBonus;
			// What this result newly cleared, for the reward block's clear ids
			// (see "WHAT THIS RESULT NEWLY CLEARED").  A loss clears nothing.
			bool newlyCleared = false;
			int32_t completedDungeon = 0;

			// A Frontier Gate battle reports the run's floors and score
			// (eIQ79KO2).  They are stored for the next battle and the end of
			// the run, and a LOST battle ends the run here: FG sends MissionEnd
			// win or lose, and only this reply can carry the result that moves
			// the client on (see gme/common/FrontierGate.hpp).
			std::optional<gme::FrontierRun> frontierRun;
			if (frontierBattle)
			{
				const auto& report = req.frontier_battle->front();
				frontierRun = co_await gme::loadFrontierRun(transaction, identity);
				if (frontierRun)
				{
					frontierRun->score = report.now_score;
					frontierRun->progress = report.progress;
					frontierRun->note = report.note;
					frontierRun->odInfo = report.od_info;
					co_await gme::storeFrontierRun(transaction, identity, *frontierRun);
					// Pay whatever the run has now reached.  Waiting for the
					// run to end loses it all if the player starts the gate
					// again instead of retiring.
					co_await gme::payFrontierRewards(transaction, identity, *frontierRun);
					// Those payouts include the gate's own currencies, so the
					// reply carries the lists they changed rather than leaving
					// the client on a stale balance until the next login.
					if (frontierRun->tokensChanged)
						resp.event_token_info = co_await gme::loadEventTokens(transaction, identity);
					if (frontierRun->ticketsChanged)
						resp.summon_ticket_v2_user = co_await gme::loadSummonTicketsV2(transaction, identity);
				}
				else
				{
					LOG_WARN << "MissionEnd: Frontier Gate battle with no open run for "
						<< identity.userId;
				}
			}

			// Record the clear in the mission clear-history
			// (user_campaign_missions, state=2).  UserInfo reports this set as
			// UT1SVg59 (UserClearMissionInfo) — the list the client evaluates
			// feature unlocks against (F_FUNCTION_RELEASE_MST conditions and
			// the hardcoded town/early-feature gates), so every victorious
			// MissionEnd must land here. A reported loss must not create a clear.
			if (!missionLost)
			{
				const auto clearedAt = static_cast<int64_t>(
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now().time_since_epoch()).count());

				// Whether this is the FIRST clear decides the area Gem below,
				// so it has to be read before the upsert bumps clear_count.
				const auto firstClear = (co_await transaction->execSqlCoro(
					"SELECT 1 FROM user_campaign_missions"
					" WHERE user_id = $1 AND mission_id = $2 AND state = 2;",
					identity.userId,
					std::to_string(missionId))).empty();
				newlyCleared = firstClear;

				co_await transaction->execSqlCoro(
					"INSERT INTO user_campaign_missions"
					" (user_id, mission_id, state, attain_percent, clear_count, last_cleared_at)"
					" VALUES ($1, $2, 2, 100, 1, $3)"
					" ON CONFLICT(user_id, mission_id) DO UPDATE SET"
					" state=2, attain_percent=100,"
					" clear_count=clear_count+1, last_cleared_at=$3;",
					identity.userId,
					std::to_string(missionId),
					clearedAt);

				// DAILY TASKS.  Both quest codes are fed from here because this
				// is the one place that knows a mission was WON: the client
				// reads them in MissionResultScene, but nothing reports the
				// completion back, so the count has to be derived server-side.
				// A Vortex clear is a quest clear too, so QE always advances
				// and VV additionally when the mission is in the Vortex land.
				// advanceDailyTask ignores a code that is not offered today,
				// so both calls are unconditional.
				// ⚠ QUEST EXPLORER IS SCOPED TO TWO AREAS, not to any mission:
				// "Complete 3 Missions in (any 2 randomly selected Areas)".
				// gme::dailyTaskQuestAreas picks the day's pair and
				// fillDailyTaskTables names them on the tile, so the counter
				// has to agree with what the player was told.
				{
					const auto areas = co_await gme::dailyTaskQuestAreas(
						transaction, identity,
						std::chrono::duration_cast<std::chrono::seconds>(
							std::chrono::system_clock::now().time_since_epoch()).count() / 86400);
					if (std::find(areas.begin(), areas.end(),
							gme::missionAreaId(missionId)) != areas.end())
					{
						co_await gme::advanceDailyTask(transaction, identity, "QE");
					}
				}
				if (theServer()->cache().vortexMissions().count(missionId))
					co_await gme::advanceDailyTask(transaction, identity, "VV");

				// "CLEARED (NO CONTINUES)".  A continue during this run left a
				// row behind; its absence is what earns the flag.  Sticky —
				// once a mission has been done cleanly, a later messy clear
				// must not take it back, so this only ever sets 1.
				const auto continued = co_await transaction->execSqlCoro(
					"SELECT 1 FROM user_mission_continues"
					" WHERE user_id = $1 AND mission_id = $2;",
					identity.userId, std::to_string(missionId));
				if (continued.empty())
				{
					co_await transaction->execSqlCoro(
						"UPDATE user_campaign_missions SET no_continue = 1"
						" WHERE user_id = $1 AND mission_id = $2;",
						identity.userId, std::to_string(missionId));
				}
				co_await transaction->execSqlCoro(
					"DELETE FROM user_mission_continues WHERE user_id = $1 AND mission_id = $2;",
					identity.userId, std::to_string(missionId));

				if (firstClear)
				{
					completedDungeon = co_await grantDungeonClearGem(transaction, identity, missionId);

					// Per-mission first-clear rewards, queued to the present box.
					const auto& clearRewards = theServer()->cache().missionClearRewards();
					const auto rewardIt = clearRewards.find(
						static_cast<int32_t>(missionId));
					if (rewardIt != clearRewards.end())
					{
						const auto rewards = parseClearRewards(rewardIt->second);
						for (const auto& r : rewards)
						{
							co_await gme::addUserPresent(
								transaction, identity, r.type, r.id, r.amount,
								0, "First clear reward");
						}
						// The result screen shows only the currency half.
						firstClearBonus = encodeClearBonus(rewards);
						LOG_INFO << "MissionEnd: first clear of "
							<< missionId << " -> " << rewards.size()
							<< " reward(s) queued (" << rewardIt->second << ")";
					}
				}
			}

			// Hand the run's state back (the client's only source for it), and
			// when the battle was lost, end the run and pay it — before the
			// team info below is read, so the header and the present badge
			// already include what it paid.  The best is read first so the
			// result scene measures the run against the previous best.
			if (frontierRun)
			{
				const auto best = co_await gme::frontierBestScore(transaction, identity, frontierRun->gateId);
				resp.frontier_battle = std::vector<::FrontierBattleInfo>{ gme::frontierBattleInfo(*frontierRun, best) };
				if (frontierLost)
				{
					auto result = co_await gme::finishFrontierRun(transaction, identity, *frontierRun);
					resp.frontier_end = std::vector<::FrontierEndInfo>{ std::move(result.end) };
					resp.frontier_rewards = std::move(result.rewards);
				}
			}
			else if (frontierLost)
			{
				// No run to pay (already ended, or never opened here) — still
				// send a result, or the client never leaves the battle-end scene.
				::FrontierEndInfo end{};
				end.grade_id = gme::frontierGrade(0);
				end.bonus_rate = 1.0f;
				resp.frontier_end = std::vector<::FrontierEndInfo>{ std::move(end) };
				resp.frontier_rewards = std::vector<::FrontierResRewardInfo>{};
			}

			auto loginInfo = std::move((co_await gme::getLoginInfo(transaction, identity)).nonEmpty());
			auto teamInfo = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());

			auto droppedUnits = parseUnitDrops(req.battle_result.unit_rewards);

			// We need to track which units are already seen by the player.
			std::vector<bool> newFlags;
			newFlags.reserve(droppedUnits.size());
			for (auto& dropped : droppedUnits)
			{
				// Read from unit dictionary to see if the player has already seen this unit.
				newFlags.push_back((co_await db::PacketInterfaceFor<UserUnitDictionary>::read(
					transaction,
					"user_unit_dictionary",
					{
						db::Lookup("user_id", identity.userId),
						db::Lookup("unit_id", dropped.unit_id),
					})).affected == 0);

				// Add the unit and persist it to the dictionary.
				dropped = std::move(
					(co_await gme::addUserUnit(
						transaction,
						identity,
						dropped)).nonEmpty());
			}

			// The COMPLETE dictionary, never just the dropped species: this
			// response rebuilds the reference list the summon lineup reads
			// (UserUnitDictionary in net/user.kdl).  Only when a unit dropped,
			// so a clear without drops sends exactly what it always did.
			if (!droppedUnits.empty())
				resp.unit_dictionary = co_await gme::loadUnitDictionary(transaction, identity);

			// Credit item/sphere drops reported by the client to the player's
			// warehouse.  Spheres travel the same path as any other item — they
			// just carry stat params in ItemMst.  The client already renders
			// the obtained-item cards from its own battle-result state, so we
			// only need to persist ownership here.
			for (const auto& [itemId, qty] : parseItemDrops(req.battle_result.item_rewards))
			{
				const auto stack = co_await transaction->execSqlCoro(
					"SELECT item_num FROM user_items WHERE user_id = $1 AND item_id = $2;",
					identity.userId, itemId);
				const auto owned = stack.empty() ? int64_t{0} : stack[0]["item_num"].as<int64_t>();
				if (owned < 0 || owned > INT32_MAX - static_cast<int64_t>(qty))
					throw std::runtime_error("Mission item stack exceeds wire range");
				(co_await gme::addUserItem(transaction, identity, itemId, qty));
			}

			// Items used in the battle leave the OWNED bar, win or lose -- the
			// client has already spent them from its copy (see
			// gme/common/BattleItems.hpp).  A battle that ran on a supplied
			// list (challenge item sets) spent nothing the player owns.
			if (!usedItems.empty() && gme::missionUsesOwnedLoadout(
					static_cast<int32_t>(missionId)))
			{
				co_await gme::consumeLoadoutItems(transaction, identity, usedItems);
			}

			// UserWarehouseInfoResponse @0x14060E0 clears its list. Send every
			// positive stack, preserving instance IDs and unrelated inventory.
			{
				auto warehouse = co_await gme::loadWarehouseSnapshot(transaction, identity);
				resp.warehouse_info = std::move(warehouse.warehouse);
				resp.item_favorite = std::move(warehouse.favorites);
				resp.item_dictionary_info = std::move(warehouse.dictionary);
			}

			resp.reward_info.zel = rewardZel;
			resp.reward_info.karma = rewardKarma;
			resp.reward_info.before_level = currentLevel;
			resp.reward_info.lvlup_flag = leveledUp ? 1 : 0;
			resp.reward_info.inc_exp = rewardExp;
			resp.reward_info.reward_units = encodeUnitDrops(droppedUnits, newFlags);
			resp.reward_info.clear_bonus = firstClearBonus;
			// Tell the client the mission is cleared NOW, rather than leaving
			// it to whenever UserInfo next runs.  Until this was here a
			// freshly beaten mission kept its uncleared marker on the map.
			// PermitPlace below separately refreshes what that clear unlocks.
			//
			// Merges rather than replaces: readParam @0x13FD758 addObject()s
			// without removeAllObjects, so this cannot drop clears the client
			// already knows about.
			// MUST use `transaction`, not theDb(): the SQLite pool is a single
			// connection, so querying theDb() while this transaction holds it
			// deadlocks the request.  Introduced as theDb() in dee3a1f and hung
			// every MissionEnd from then until 2026-08-24 (handbook 6.14).
			resp.clear_mission_info = co_await gme::getClearedMissions(transaction, identity);
			std::set<int32_t> cleared;
			for (const auto& done : resp.clear_mission_info) cleared.insert(done.mission_id);
			// PermitRecipeResponse @0x13E9BB8 replaces the list. Rebuild all
			// unlocked recipes, including ones gated by this newly cleared mission.
			resp.permit_receipes = co_await gme::Town::permittedRecipes(transaction, identity, cleared);
			// THE TOWN TILES, against the same set.  Missions 11 and 12 open
			// the Farm and the Mountain, and the client gates them itself on
			// the UT1SVg59 above -- but a tile locked at the last UserInfo was
			// sent with no taps, and nothing else re-sent this list, so a Farm
			// opened mid-session passed its gate and stayed empty and inert
			// until the next login (reported 2026-10-02).  locationState rolls
			// a newly opened tile's first period, and any tile whose 3 hours
			// are up.  Taps are never lost to it: the client flushes them in
			// one TownUpdate on leaving town.  See town_location_detail in
			// net/handlers.kdl.
			{
				std::vector<::UserTownLocationInfo> unusedInfo;
				std::vector<::UserTownLocationDetail> tiles;
				co_await gme::Town::locationState(transaction, identity, cleared, unusedInfo, tiles);
				resp.town_location_detail = std::move(tiles);
			}

			// WHAT THIS RESULT NEWLY CLEARED -- and nothing on a repeat.
			//
			// MissionResultFriendRequestScene::changeNextScene @0x18C301C routes
			// the player out of the result screens on these three ids, and
			// trusts them completely:
			//
			//   mauD5qZ1 clear_mission_id -- "12" first offers the Randall
			//     presentation; then, if that mission has an END script
			//     (GameUtils::existMissionEvent(id, "1")), it is PLAYED, with no
			//     once-only check of its own.  Mission 10's end script is the
			//     Farm announcement, and it ends on script op 41 = 0: return
			//     point 0, which MissionEventScene::updateEvent maps to Home.
			//   4sQ8vBXm clear_dungeon_id -- the same for the DUNGEON's end
			//     script (map1-ending.txt on dungeon 80), and
			//     GameUtils::getAppearDungeon @0x1196FE4 animates
			//     DungeonMstList::getNext of it on the dungeon list.
			//   NgPQbA46 clear_area_id -- anything but "0", an EMPTY string
			//     included, resets UserState's last area so the map shows the
			//     newly opened one.
			//
			// With all three neutral a story clear goes to scene 61,
			// DungeonSelectScene2 -- the area's dungeon list, where the next
			// stage is picked (scene ids resolved through getGameScene's table).
			//
			// All three used to be sent on EVERY clear, so each repeat of
			// mission 10 replayed the Farm scene and then went Home (reported
			// 2026-10-02).  The client never resets them between results --
			// MissionRewardInfo::init @0x1268830 leaves them alone and readParam
			// stores the raw string -- so "nothing" must be sent explicitly, in
			// the client's own sentinels: "0" for the mission (row 0 has no
			// script) and the area, "" for the dungeon (what
			// GameUtils::deleteAppearDungeon @0x1197188 writes).
			resp.reward_info.clear_mission_id = 0;
			resp.reward_info.clear_dungeon_id.clear();
			resp.reward_info.clear_area_id = "0";
			if (newlyCleared)
			{
				resp.reward_info.clear_mission_id = missionId;
				// The dungeon only when this clear completed it -- the same
				// fact the clear Gem pays on.  Quest dungeons only, as before:
				// special content keeps its end scripts unplayed.
				if (completedDungeon > 0)
					resp.reward_info.clear_dungeon_id = std::to_string(completedDungeon);
				// The area only when this clear OPENED another story area.  Not
				// "every dungeon done": Mistral's EX dungeon opens at mission 365,
				// long after Morgan, and the area presentation is for the
				// opening (85 -> Morgan, 265 -> St. Lamia, ...).
				const auto& cache = theServer()->cache();
				const auto dungeonIt = cache.missionQuestDungeon().find(static_cast<int32_t>(missionId));
				if (dungeonIt != cache.missionQuestDungeon().end()
					&& gme::storyAreaOpenedBy(static_cast<int32_t>(missionId), cleared))
				{
					const auto& dungeons = cache.dungeonMst();
					const auto row = std::find_if(dungeons.begin(), dungeons.end(),
						[&dungeonIt](const ::DungeonMst& d) { return d.dungeon_id == dungeonIt->second; });
					if (row != dungeons.end() && row->area_id > 0)
						resp.reward_info.clear_area_id = std::to_string(row->area_id);
				}
				LOG_INFO << "MissionEnd: first clear of " << missionId
					<< " (dungeon cleared: " << (resp.reward_info.clear_dungeon_id.empty() ? "-" : resp.reward_info.clear_dungeon_id)
					<< ", area opened by: " << resp.reward_info.clear_area_id << ")";
			}


			// Fold this battle's statistics into the lifetime trophy archive.
			// The client already reports all eight in rXvA1E5y with the same
			// keys UserTeamArchive stores them under; before this they were
			// parsed and dropped, which is why every battle trophy read 0.
			co_await gme::accumulateBattleArchive(transaction, identity, req.battle_result);

			// Lifetime totals the handler observes rather than receives.
			// rewardZel/rewardKarma are the server-computed grant, not the
			// client's claim, so these track what we actually paid out.
			co_await gme::bumpArchiveCounters(transaction, identity, {
				{ "zel_get",         static_cast<int64_t>(rewardZel)   },
				{ "karma_get",       static_cast<int64_t>(rewardKarma) },
				// Losses in either normal quests or Frontier Gate are not wins.
				{ "quest_clear_cnt", missionLost ? 0 : 1 },
				// TROPHY 200030 (total battle wins).  A cleared mission is a
				// won battle.
				{ "quest_win_cnt",   missionLost ? 0 : 1 },
				// TROPHY 100090 (Honor earned).  Its counterpart 100100 is
				// bumped where Honor is spent, in Gacha.
				{ "friend_p_get",    static_cast<int64_t>(honorEarned) },
			});

			// THE BATTLE IS OVER, won or lost, so it is no longer resumable.
			// Inside the reward transaction: a rolled-back MissionEnd has to
			// leave the resume offer standing, or a failed result would lose
			// the run outright.
			co_await gme::clearMissionBreak(transaction, identity);

			resp.login_info = std::move(loginInfo);
			// Never echo the cutscene markers here: the client re-parses them
			// from any reply and this one would roll back events it has just
			// played (see omitClientScenarioMarkers).
			gme::omitClientScenarioMarkers(resp.login_info);
			resp.team_info = std::move(teamInfo);
			// A level-up raises max energy, and with it the header's
			// energy-refill threshold (see UserInfo).
			resp.energy_recover.action_point_threshold = resp.team_info.max_action_point;
			resp.unit_info = std::move(droppedUnits);

			// Build and serialize before committing rewards. Any failure rolls
			// back the clear too; all reads use the held SQLite transaction.
			const auto permit = co_await gme::buildPermitPlace(transaction, identity);
			if (const auto error = glz::write_json(resp, buffer); error)
			    throw std::runtime_error(glz::format_error(error, buffer));
			gme::injectPermitPlace(buffer, permit);

		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	co_return HandleResult::success(buffer);
}

HANDLEF(MissionStart)
{
	MissionStartReq req = {};
	const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json);
	if (ec)
	{
		const auto error = glz::format_error(ec, json);
		LOG_ERROR << "MissionStartReq deserialization failed:\n" << error;
		co_return HandleResult::error("Deserialization error", error);
	}

	// Enforce the same prerequisites as the map (all-of, gme::storyMissionUnlocked)
	// before spending energy or replacing an in-flight battle. A stale client
	// tile cannot bypass them.
	{
		const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
		std::set<int32_t> cleared;
		for (const auto& done : co_await gme::getClearedMissions(theDb(), identity))
			cleared.insert(done.mission_id);
		// A STALE TILE, NOT A CHEAT: the client drew this quest from a permit
		// list it has not refreshed (#30), so the player gets the reason and
		// Home instead of the app exiting.  Nothing has been spent yet.
		if (!gme::storyMissionUnlocked(req.start_info.mission_id, cleared))
			co_return HandleResult::refuseToHome(
				"This quest is still locked. Clear the quests it requires first.",
				"Mission locked: prerequisites not cleared");
	}

	// BATTLE-CONTENT FALLBACK.
	//
	// deploy/archive/mission.json supplies authored waves, monsters, and AIs.
	// Only a mission missing from that archive borrows mission 10's content;
	// an existing but malformed record must fail validation below.
	//
	// CRITICAL: only the CONTENT comes from the template.  resp.start_info is
	// echoed from the request below, so the response still names the mission the
	// client asked for.  Handbook §3.1 is the story of what happens otherwise:
	// answering mission 11 with "mission 10" crashed the client outright.
	//
	// Replace this with real authored content per mission — that is what the
	// mission editor exists for.  When the archive covers a mission, nothing
	// here runs.
	constexpr MissionArchiver::MissionId kTemplateMissionId = 10;

	auto missionRecord = MissionArchiver::instance().lookup(req.start_info.mission_id);
	const bool usingTemplate = !missionRecord;
	if (usingTemplate)
	{
		missionRecord = MissionArchiver::instance().lookup(kTemplateMissionId);
		if (!missionRecord)
		{
			// The template itself is missing, so the archive is unusable.
			co_return HandleResult::error("Archive error", "Unable to find mission record");
		}
		// RELABEL THE TEMPLATE AS THE REQUESTED MISSION.
		//
		// populatePacket stamps record.id into BattleGroupMst::mission_id and
		// MissionNumInfo::serial_id.  Left alone, the response would carry
		// start_info for mission 9010001 while every battle group and the
		// mission_num claimed mission 10 — the client resolves the mission it
		// asked for, finds battle data belonging to a different one, and dies
		// during load.  That is handbook §3.1's mismatch crash exactly, and it
		// is what echoing start_info alone did NOT fix (verified 2026-08-07: FG
		// downloaded its assets and then crashed with a clean server log).
		//
		// Safe because the archive contract is explicit that the SERVER owns
		// these ids: "Client battle group ids and encoded AI/drop wire strings
		// are generated while formatting a mission-start response"
		// (MissionArchiver.hpp).  The client does not require the group ids its
		// own MST lists — only that the response is internally consistent about
		// which mission it describes.
		missionRecord->id = req.start_info.mission_id;
		// Chest policies belong to authored content, not fallback missions.
		missionRecord->mimic_chests.reset();
		missionRecord->random_mimics.reset();

		LOG_WARN << "MissionStart: mission " << req.start_info.mission_id
		         << " has no authored battle content; serving mission "
		         << kTemplateMissionId << "'s waves relabelled as this mission";
	}

	MissionStartResp resp{};
	resp.signal_key = req.signal_key;
	resp.start_info = req.start_info;

	// A Frontier Gate battle starts from the run's floors and score, which
	// the client holds only when the server sends them (see
	// gme/common/FrontierGate.hpp): zeros on a fresh run, the last
	// MissionEnd's report after that.
	if (req.frontier_run && !req.frontier_run->empty())
	{
		const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
		if (const auto run = co_await gme::loadFrontierRun(theDb(), identity))
		{
			const auto best = co_await gme::frontierBestScore(theDb(), identity, run->gateId);
			resp.frontier_battle = std::vector<::FrontierBattleInfo>{ gme::frontierBattleInfo(*run, best) };
		}
		else
		{
			LOG_WARN << "MissionStart: Frontier Gate battle with no open run for " << identity.userId;
		}
	}

	if (!MissionArchiver::populatePacket(*missionRecord, resp.mission_num)
		|| !MissionArchiver::populatePacket(*missionRecord, resp.ais)
		|| !MissionArchiver::populatePacket(*missionRecord, resp.monsters)
		|| !MissionArchiver::populatePacket(*missionRecord, resp.monster_cgs)
		|| !MissionArchiver::populatePacket(*missionRecord, resp.battle_monster_groups)
		|| !MissionArchiver::populatePacket(*missionRecord, resp.battle_groups))
	{
		co_return HandleResult::error("Archive error", "Unable to populate mission start response");
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// THIS BATTLE'S OWN SERIAL, in place of the mission id populatePacket put
	// there (see gme/common/MissionRuns.hpp): the client echoes it in MissionEnd
	// and MissionContinue and seeds the battle from it.  Issued before the
	// preflight below so the reply carries it; it only becomes the OPEN battle
	// once the start has succeeded (open_mission_serial, further down).
	const auto runSerial = co_await gme::issueMissionSerial(
		theDb(), identity, static_cast<uint32_t>(req.start_info.mission_id));
	resp.mission_num.serial_id = runSerial;

	std::string buffer;
	const auto& writeError = glz::write_json(resp, buffer);
	if (writeError)
	{
		const auto& glze = glz::format_error(writeError, buffer);
		LOG_ERROR << "MissionStart response serialization failed: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	// Preflight the complete response before spending: malformed archive data
	// or a serialization failure must not consume energy or alter its timer.
	// Energy is only consumed for a mission we actually have data for.  The
	// template's cost belongs to mission 10, not to whatever was requested, and
	// charging a made-up amount is the §3.4 anti-pattern; the real per-mission
	// cost lives in the archive (handbook §6.15 rule 3), which by definition
	// does not have this one.  Frontier Gate in particular spends Hunter Orbs
	// (Aube) client-side rather than energy — see §6.16.
	if (!usingTemplate && missionRecord->energy_cost > 0)
	{
		(co_await gme::UserEnergy::consume(
			theDb(),
			identity,
			missionRecord->energy_cost)).nonEmpty();
	}

	// REMEMBER THE BORROWED HELPER so MissionEnd can pay the Honor for it.
	//
	// MissionEnd cannot work it out for itself: MissionEndRequest::createBody
	// @0x13A78F8 sends 75 keys and h7eY3sAK is not one of them.  This request
	// is where the helper is named -- "0" for a solo run, the helper's user id
	// otherwise -- so it is recorded here and consumed (and cleared) there.
	//
	// Written unconditionally and outside the energy branch: a template mission
	// still borrows a helper, an energy-free one still borrows a helper, and a
	// solo start has to CLEAR whatever the last start left, or the next clear
	// would pay for a helper it never took.  A start that fails validation
	// above never reaches this line.
	co_await theDb()->execSqlCoro(
		"UPDATE user_info SET reinforce_user_id = $1 WHERE id = $2;",
		gme::borrowedHelper(req.start_info.reinforce_user_id)
			? req.start_info.reinforce_user_id
			: std::string{},
		identity.userId);

	// THE BATTLE THIS START OPENS, so MissionEnd pays its result exactly once
	// (see the ONE RESULT PER BATTLE block there).  A new start supersedes any
	// battle the player walked away from, the same way the break record below
	// does -- its serial can no longer settle.
	co_await theDb()->execSqlCoro(
		"UPDATE user_info SET open_mission_id = $1, open_mission_serial = $2 WHERE id = $3;",
		static_cast<int64_t>(req.start_info.mission_id), static_cast<int64_t>(runSerial),
		identity.userId);

	// A FRESH RUN STARTS WITH A CLEAN CONTINUE RECORD, or a continue spent in an
	// earlier attempt would still be counted against this one and the "Cleared
	// (No Continues)" achievements could never be earned after a single bad run.
	co_await theDb()->execSqlCoro(
		"DELETE FROM user_mission_continues WHERE user_id = $1 AND mission_id = $2;",
		identity.userId, std::to_string(req.start_info.mission_id));

	// A NEW BATTLE SUPERSEDES ANY INTERRUPTED ONE.  The resume path never comes
	// through here -- MissionRestartScene fires MissionRestart and rebuilds from
	// the client's own suspend data -- so a MissionStart means the player walked
	// away from whatever was open.  The statement is a no-op when nothing is.
	co_await gme::clearMissionBreak(theDb(), identity);

	co_return HandleResult::success(buffer);
}
