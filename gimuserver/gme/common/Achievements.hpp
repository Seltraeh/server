#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

// Merit Points — the Randall Achievement currency.
//
// The balance lives on user_info.achieve_point and reaches the client as the
// `Bnc4LpM8` singleton (UserAchievementInfo).  It is NOT part of user_info on
// the wire: RandallAchievementDedicateScene::setAchievePoint @0x1A39D58 prints
// UserAchievementInfo +0x18, which is written by exactly one thing —
// UserAchievementInfoResponse::readParam @0x13FBB58.  So every handler that
// moves the balance has to send this block or the number the player is looking
// at keeps the value the last UserInfo gave it.
//
// That is the Frontier Gate bug reported 2026-09-12: retiring a run credited
// score/100 merit points and the result screen even showed the credit, but the
// running total in the corner never moved, because FrontierGateEnd's reply
// carried the currencies (fEi17cnx) and not this singleton.

namespace gme
{

/*!
* The player's Merit Point balance as the `Bnc4LpM8` singleton.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return The achievement singleton; its two unknown fields stay 0, which is
* what every capture of this block shows.
*/
inline drogon::Task<::UserAchievementInfo> loadAchievementInfo(
	const db::Database database,
	const UserIdentity identity)
{
	::UserAchievementInfo info{};
	info.id = (co_await db::DatabaseInterface::read(
		database, "user_info", { db::Data("achieve_point"), db::Lookup("id", identity.userId) }))
		.front<int32_t>("achieve_point");
	co_return info;
}


namespace detail
{

/*!
* Splits a cond_param on one separator, dropping empties.
*
* cond_param carries four different shapes across the table — a bare number, a
* comma list of item ids, "<count>,<percent>" and "<item>:<count>" — so every
* reader that is not expecting a bare number goes through here rather than
* calling stoll on the whole string and silently taking its first field.
*/
inline std::vector<std::string> splitCsv(const std::string& value, const char sep = ',')
{
	std::vector<std::string> out;
	size_t start = 0;
	while (start <= value.size())
	{
		const auto at = value.find(sep, start);
		auto piece = value.substr(start, at == std::string::npos ? std::string::npos : at - start);
		if (!piece.empty())
			out.push_back(std::move(piece));
		if (at == std::string::npos)
			break;
		start = at + 1;
	}
	return out;
}

/*! Everything one pass over the achievement list needs to measure progress. */
struct AchievementCounters
{
	::UserTeamArchive archive{};
	int64_t meritPoints = 0;
	int32_t playerLevel = 1;
	int64_t itemSpecies = 0;
	int64_t unitSpecies = 0;
	int64_t favouritedUnits = 0;
	int64_t ownedSounds = 0;
	// Lifetime goods handed to the Merit Point Dedicate screens, summed out of
	// user_achievement_deliver.  Zel and Karma are amounts; units and spheres
	// are counts.
	int64_t deliveredZel = 0;
	int64_t deliveredKarma = 0;
	int64_t deliveredUnits = 0;
	int64_t deliveredSpheres = 0;
	std::set<std::string> clearedMissions;

	// --- added 2026-09-20, closing bands that were reported as -1 -----------
	int64_t friendCount = 0;
	int64_t favouritedFriends = 0;

	/*! Town building levels, by the id the MST gives them. */
	std::map<int32_t, int64_t> facilityLevel;
	std::map<int32_t, int64_t> locationLevel;

	/*! The highest total cost of any squad this player has formed. */
	int64_t maxSquadCost = 0;

	/*! Grand Quest missions cleared to 100 percent. */
	int64_t grandQuestFullClears = 0;

	/*! Every item id the player holds, for the "Item Guide Entry Filled" rows. */
	std::set<int64_t> heldItems;

	/*! item id -> how many of it this player has ever crafted. */
	std::map<int64_t, int64_t> craftedItems;

	/*! Missions this player has cleared without ever spending a continue. */
	std::set<std::string> noContinueMissions;

	/*! Best single quest, for the three "in one Quest" bands. */
	int64_t bestBattleCrystals = 0;
	int64_t bestHeartCrystals = 0;
	int64_t bestSparks = 0;

	// --- the Summoner Avatar arc, closed 2026-09-20 -------------------------
	int64_t summonerLevel = 1;
	int64_t summonerTrainingPoints = 0;   //!< `friend_point` on the wire block
	int64_t summonerSkillPoints = 0;      //!< `sp`
	int64_t summonerPedestals = 1;        //!< `summon_limit`
	int64_t summonerParameterLevel = 0;   //!< highest unlocked ability level
	int64_t summonerBestArmLevel = 0;     //!< best weapon level ever reached
	int64_t summonerLsSpheres = 0;        //!< LS Spheres CREATED, a lifetime count
	/*! The six Summoning Arts levels, indexed 1..6 Fire..Dark. */
	std::map<int32_t, int64_t> summonerElementLevel;
};

/*!
* Which town building a levelling band is about.
*
* ⚠ THE BAND NAMES THE BUILDING, and that is the whole difficulty this used to
* have: the achievement text says "Sphere House" while the MST calls the
* facility "Spheres", so a note here once concluded "neither MST resolves its
* name" and left all 22 rows dark.  The id was never in the text — it is in the
* band, exactly as the condition itself is.  Facility and location are separate
* id spaces, hence the pair.
*/
struct TownBandTarget
{
	int32_t facilityId = 0;   //!< user_town_facilities.facility_id, or 0
	int32_t locationId = 0;   //!< user_town_locations.location_id, or 0
};

inline TownBandTarget townBandTarget(const int32_t band)
{
	switch (band)
	{
	case 8000:  return { 2, 0 };   // "Synthesis House"  -> facility 2, Synthesis
	case 9000:  return { 1, 0 };   // "Sphere House"     -> facility 1, Spheres
	case 10000: return { 0, 4 };   // Forest
	case 11000: return { 0, 3 };   // Farm
	case 12000: return { 0, 2 };   // River
	case 13000: return { 0, 1 };   // Mountain
	default:    return {};
	}
}

/*!
* The mission a Trial-of-the-Gods achievement is asking about.
*
* Band 37000 addresses trials by their own id — cond_param 2000000 for Trial
* No. 001, 2000001 for 002 and so on — while the archive holds them as missions
* 8380000, 8381000, ...  The 16 rows outrun the six Trials that exist here; the
* other ten resolve to nothing and stay unattained rather than being faked.
*
* @param condParam The achievement's cond_param.
* @return Mission id, or 0 when that trial does not exist on this server.
*/
inline int32_t trialMissionFor(const int64_t condParam)
{
	constexpr int64_t kTrialIdBase = 2000000;
	constexpr int32_t kTrialMissionBase = 8380000;
	constexpr int32_t kTrialsBuilt = 6;
	const auto index = condParam - kTrialIdBase;
	if (index < 0 || index >= kTrialsBuilt)
		return 0;
	return kTrialMissionBase + static_cast<int32_t>(index) * 1000;
}

/*! Defined below; subjectProgress needs it for the Trial band. */
inline int64_t subjectTarget(const ::AchievementSubjectMst& subject);

/*!
* How far this player is along one achievement's condition.
*
* ⚠ THE CONDITION IS THE ID BAND, NOT `cond_type`.  `3rhygS9K` is the screen's
* filter and groups several unrelated conditions under one value -- cond_type 1
* alone covers logins, Honor Points, player level, friends added, favouriting a
* friend and editing a comment.  Reading progress off cond_type made "25 Friends
* Added" report the login count and claim itself finished.  What actually
* identifies a condition is the thousand-band of the subject id: 1000 logins,
* 2000 Honor Points, 3000 player level, and so on for eighty bands.
*
* Bands with no counter behind them return -1, which the caller reports as 0
* rather than inventing a number.  Same discipline as the trophy grades: a mode
* this server does not track stays unattained instead of being faked.  Left at
* -1 on purpose:
*
* ⚠ THE LIST BELOW WAS HALF WRONG, and every wrong line cost real achievements.
* Four of the bands it called impossible were answerable from state this server
* already had -- the Trials had been BUILT since that note was written, the town
* MSTs do resolve their names, Grand Quest completion is on
* user_campaign_missions.attain_percent, and the guide rows just wanted the
* held-item set.  Re-check a "cannot" before trusting it.
*
* Closed 2026-09-20: 4000/5000 friends, 8000-13000 town, 17000+18100+99900000
* guide entries, 23000 squad cost, 37000 Trials, 41000 Grand Quest, 403000
* crafted, 401000/402000 no-continue, 620000 per-quest maxima, and all 94 of
* the 52000-65000 SUMMONER bands.
*
* ⚠ THE SUMMONER ENTRY WAS THE WORST OF THE WRONG ONES.  It said the subsystem
* "has no content folder in this drop so it cannot draw at all".  There are 33
* layout_summoner_*.csv files in _dlcbundle, UserSummonerInfo was already
* modelled AND already on the wire, and the only thing missing was that
* user_summoner had one column.  Three separate "cannot"s in this block turned
* out to be false in one afternoon.
*
* Still -1, and each for a reason that is a whole feature rather than a counter:
*
*   6000         edited player's comment                      (no comment field
*                                                              exists on user_info)
*   28000-33000  arena                                        (not simulated)
*   38000        Hunter Rank                                  (not built)
*   39000        raid                                         (not simulated)
*   42000-43000  Frontier Hunter                              (not built)
*   44000        Frontier Gate training points                (FG stores a score,
*                                                              not training points)
*   45000-51000  colosseum                                    (not simulated)
*   400000,630000  "cleared in N turns"                       (MissionEnd does not
*                                                              report a turn count --
*                                                              only ChallengeArena
*                                                              has getTurnCnt)
*   401000-402000  "cleared with no continues"                (MissionContinue is a
*                                                              registered request, so
*                                                              this IS observable --
*                                                              it needs a per-battle
*                                                              flag, not a new mode)
*   600000-620000  the remaining SP tabs
*/
inline int64_t subjectProgress(
	const ::AchievementSubjectMst& subject,
	const AchievementCounters& counters)
{
	const auto& a = counters.archive;

	// Two rows sit in a band whose condition is not theirs.  Named rather than
	// banded, because that is what they are.
	if (subject.id == 7500)      // "600 Mission Records Cleared", in the merit band
		return a.quest_clear_cnt;
	// "Item Guide Entry Filled" rows, in three bands.  cond_param is an ITEM ID
	// — or, for id 18100 and 17000, a COMMA LIST of six of them, which is why
	// those two used to be special-cased to -1.  A list is just "own them all",
	// so one reader covers both shapes.
	if (subject.id == 18100 || subject.id / 1000 * 1000 == 17000
		|| (subject.id / 1000 * 1000 == 99900000 && subject.cond_type == 6))
	{
		int64_t have = 0, want = 0;
		for (const auto& piece : splitCsv(subject.cond_param))
		{
			int64_t itemId = 0;
			try { itemId = std::stoll(piece); }
			catch (const std::exception&) { continue; }
			++want;
			if (counters.heldItems.contains(itemId))
				++have;
		}
		// Reported as "all of them or none", because the threshold on these
		// rows is 1 and the text promises every entry in the list.
		return want > 0 && have == want ? 1 : 0;
	}
	if (subject.id == 99900010)  // "[Ezra Special] Player Level 500 Reached" sits
		return counters.playerLevel;  // alone in a band of eight unrelated rows


	switch (subject.id / 1000 * 1000)
	{
	case 1000:  return a.login_cnt;             // "Log in for a total of N days"
	case 2000:  return a.friend_p_get;          // "N Honor Points Accumulated" --
	                                            // the LIFETIME total, which is
	                                            // why the current balance could
	                                            // not answer it: that falls when
	                                            // an Honor Summon spends it
	case 3000:  return counters.playerLevel;    // "Player Level N Reached"

	// THE LEVEL UP CAMPAIGN -- 152 rows, "[Level Up Campaign] Reached Level N",
	// cond_param 1..500.  Reported as "the Level Up Campaign has a Receive All
	// button but nothing is claimable": the band fell through to the default and
	// every row read progress 0 forever, at any player level.  Verified
	// homogeneous before mapping -- all 152 share one name shape and a numeric
	// cond_param, unlike band 99900000 whose eight rows are eight different
	// conditions and are handled by id.
	//
	// These pay NO merit points (point is 0 on every row); the reward is the
	// packed JQ23rIvk entry, which is why claiming them needs
	// grantAchievementReward below.
	case 88010000: return counters.playerLevel;
	case 7000:  return counters.meritPoints;    // "N Merit Points Accumulated"
	case 15000: return a.item_mix_cnt;          // "Synthesized N Times"
	case 16000: return counters.itemSpecies;    // "N Item Guide Entries Filled"
	case 18000: return a.sphere_mix_cnt;        // "N Spheres Created"
	case 19000: return counters.unitSpecies;    // "N Unit Guide Entries Filled"
	case 20000: return a.unit_mix_cnt;          // "N Fusions Performed"
	case 21000: return a.unit_evo_cnt;          // "N Evolutions Performed"
	case 14000: return counters.ownedSounds;    // "Bought a Song from the Music
	                                            // House" -- one row, threshold 1
	case 22000: return counters.favouritedUnits;// "Favorited a Unit"

	// The Merit Point Dedicate screens' own achievements -- category 3, the
	// four cond_types the Deliver flow is told apart by.  Each counts the
	// LIFETIME goods handed over, which is the sum of the daily ledger.
	case 9000000: return counters.deliveredZel;     // "Traded N Zel Total"
	case 9100000: return counters.deliveredKarma;   // "Traded N Karma Total"
	case 9200000: return counters.deliveredUnits;   // "Traded Unit"
	case 9300000: return counters.deliveredSpheres; // "Trade Sphere"
	case 24000: return a.battle_spark_cnt;      // "N Sparks Produced"
	case 25000: return a.b_crystal;             // "N Battle Crystals Collected"
	case 26000: return a.h_crystal;             // "N Heart Crystals Collected"
	case 27000: return a.battle_skill_cnt;      // "Activated BB/SBB N Times"
	case 34000: return a.quest_clear_cnt;       // "N Overall Quests Cleared"

	// Not counters: cond_param names ONE mission, and the achievement is done
	// the moment it is cleared.  35000/36000 are ordinary quests and 40000 a
	// Grand Quest; both clear-histories live in user_campaign_missions.
	case 35000:
	case 36000:
	case 40000:
		return counters.clearedMissions.contains(subject.cond_param) ? 1 : 0;

	// --- closed 2026-09-20 -------------------------------------------------
	case 4000: return counters.friendCount;        // "N Friends Added"
	case 5000: return counters.favouritedFriends;  // "Favorited a Friend"

	// Town levelling.  The BAND says which building; see townBandTarget.
	case 8000:
	case 9000:
	case 10000:
	case 11000:
	case 12000:
	case 13000:
	{
		const auto target = townBandTarget(subject.id / 1000 * 1000);
		const auto& table = target.facilityId ? counters.facilityLevel : counters.locationLevel;
		const auto it = table.find(target.facilityId ? target.facilityId : target.locationId);
		return it == table.end() ? 0 : it->second;
	}

	case 23000: return counters.maxSquadCost;      // "Formed Squad with more than N Cost"

	// Trial of the Gods.  Built during the mission pass, so the note that
	// called this "not built" is stale; only the ten trials that do not exist
	// here stay unattained, via trialMissionFor returning 0.
	case 37000:
	{
		const auto mission = trialMissionFor(subjectTarget(subject));
		return mission != 0
			&& counters.clearedMissions.contains(std::to_string(mission)) ? 1 : 0;
	}

	// "N Quests 100% Completed in Grand Quest".  cond_param is "<count>,100" —
	// the percentage is the second field, not part of the count.
	case 41000: return counters.grandQuestFullClears;

	// "Cleared (No Continues)".  ⚠ BANDS 401000 AND 402000 ARE MIXED: ten rows
	// name a bare mission id and mean "without continuing", eight pack
	// "<mission>,0,<turns>" and mean "within N turns".  MissionEnd reports no
	// turn count at all (only ChallengeArena has getTurnCnt), so the turn rows
	// stay -1 while the no-continue rows are answerable.  The SHAPE of
	// cond_param is what tells them apart, not the band.
	case 401000:
	case 402000:
	{
		const auto parts = splitCsv(subject.cond_param);
		if (parts.size() != 1)
			return -1;                     // a turn requirement; not observable
		return counters.noContinueMissions.contains(parts[0]) ? 1 : 0;
	}

	// THE SUMMONER AVATAR ARC.  Every one of these reads the block the client
	// already receives (UserSummonerInfo), which is why they were answerable
	// once user_summoner stopped being a single `sp` column.
	case 52000: return counters.summonerLevel;           // "Summoner Level N Reached"
	case 53000: return counters.summonerTrainingPoints;  // "N Summoner Training Points"
	case 54000: return counters.summonerSkillPoints;     // "N Summoner Skill Points"
	case 55000: return counters.summonerBestArmLevel;    // "Weapon Level N Reached"
	case 62000: return counters.summonerLsSpheres;       // "N LS Spheres Created"
	case 63000: return counters.summonerPedestals;       // "Unlocked N Summoning Pedestal"
	case 65000: return counters.summonerParameterLevel;  // "Parameter Level N Reached"

	// The six Summoning Arts.  ⚠ THE ELEMENT IS IN cond_param, not only in the
	// band: "5,1" is level 5 of element 1.  The bands run Fire..Dark in the
	// same order, so the two agree, but the parameter is the authority.
	case 56000:
	case 57000:
	case 58000:
	case 59000:
	case 60000:
	case 61000:
	{
		const auto parts = splitCsv(subject.cond_param);
		if (parts.size() < 2)
			return -1;
		try
		{
			const auto it = counters.summonerElementLevel.find(std::stoi(parts[1]));
			return it == counters.summonerElementLevel.end() ? 0 : it->second;
		}
		catch (const std::exception&) { return -1; }
	}

	// The three "in ONE Quest" bands, told apart by the hundred within 620000.
	// Each is a high-water mark, not a running total.
	case 620000:
		switch (subject.id / 100 * 100)
		{
		case 620000: return counters.bestBattleCrystals;
		case 620100: return counters.bestHeartCrystals;
		case 620200: return counters.bestSparks;
		default:     return -1;
		}

	// "<Item> Crafted".  cond_param is "<item id>:<count>".
	case 403000:
	{
		const auto parts = splitCsv(subject.cond_param, ':');
		if (parts.empty())
			return -1;
		try
		{
			const auto it = counters.craftedItems.find(std::stoll(parts[0]));
			return it == counters.craftedItems.end() ? 0 : it->second;
		}
		catch (const std::exception&) { return -1; }
	}

	default: return -1;
	}
}

/*!
* The threshold `progress` is measured against.
*
* cond_param is a string because the "clear this one mission" bands put an id
* there rather than a count, and the SP bands pack "<mission>,<x>,<turns>".
*/
inline int64_t subjectTarget(const ::AchievementSubjectMst& subject)
{
	// The item-guide rows carry a LIST of ids, not a count; subjectProgress
	// answers them 1 or 0, so the threshold is 1.
	if (subject.id == 18100 || subject.id / 1000 * 1000 == 17000
		|| (subject.id / 1000 * 1000 == 99900000 && subject.cond_type == 6))
		return 1;

	switch (subject.id / 1000 * 1000)
	{
	case 35000:
	case 36000:
	case 40000:
	case 37000:          // cond_param is a TRIAL ID; cleared is cleared
	case 401000:         // a mission id, answered 1 or 0
	case 402000:
		return 1;

	// "0,<threshold>" — ⚠ the leading 0 is a mission filter meaning "any", so a
	// bare stoll would make every one of these a target of ZERO and mark all
	// twelve instantly complete.
	case 620000:
	{
		const auto parts = splitCsv(subject.cond_param);
		if (parts.size() < 2)
			return 0;
		try { return std::stoll(parts[1]); }
		catch (const std::exception&) { return 0; }
	}

	// "<item id>:<count>" — ⚠ the threshold is the field AFTER the colon.  A
	// bare stoll here would read the item id as the target and demand 38,601
	// Malice Jewels.
	case 403000:
	{
		const auto parts = splitCsv(subject.cond_param, ':');
		if (parts.size() < 2)
			return 1;
		try { return std::stoll(parts[1]); }
		catch (const std::exception&) { return 1; }
	}

	default:
		// stoll stops at the first non-digit, which is what the "<count>,100"
		// Grand Quest rows want -- the 100 is the percentage, not the target.
		try { return std::stoll(subject.cond_param); }
		catch (const std::exception&) { return 0; }
	}
}

/*!
* Reads everything subjectProgress measures against, once per request.
*/
inline drogon::Task<AchievementCounters> achievementCounters(
	const db::Database database,
	const UserIdentity identity)
{
	AchievementCounters counters{};

	const auto archiveRows = co_await gme::loadTeamArchive(database, identity);
	if (!archiveRows.empty())
		counters.archive = archiveRows.front();

	const auto info = co_await database->execSqlCoro(
		"SELECT achieve_point, level FROM user_info WHERE id = $1;", identity.userId);
	if (!info.empty())
	{
		counters.meritPoints = info[0]["achieve_point"].as<int64_t>();
		counters.playerLevel = info[0]["level"].as<int32_t>();
	}

	const auto units = co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n FROM user_unit_dictionary WHERE user_id = $1;", identity.userId);
	counters.unitSpecies = units.empty() ? 0 : units[0]["n"].as<int64_t>();

	const auto items = co_await database->execSqlCoro(
		"SELECT COUNT(DISTINCT item_id) AS n FROM user_items WHERE user_id = $1;", identity.userId);
	counters.itemSpecies = items.empty() ? 0 : items[0]["n"].as<int64_t>();

	const auto favourites = co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n FROM user_units WHERE user_id = $1 AND favorite_flg <> 0;",
		identity.userId);
	counters.favouritedUnits = favourites.empty() ? 0 : favourites[0]["n"].as<int64_t>();

	// Band 14000, "Bought a Song from the Music House" -- answerable since the
	// jukebox got a table behind it (gme/common/SoundRoom.hpp).
	const auto sounds = co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n FROM user_sounds WHERE user_id = $1;", identity.userId);
	counters.ownedSounds = sounds.empty() ? 0 : sounds[0]["n"].as<int64_t>();

	// One pass over the deliver ledger; the per-day rows are what the ceiling
	// reads, and their sum is the lifetime total these achievements measure.
	for (const auto& row : co_await database->execSqlCoro(
		"SELECT kind, SUM(amount) AS total FROM user_achievement_deliver"
		" WHERE user_id = $1 GROUP BY kind;", identity.userId))
	{
		const auto total = row["total"].as<int64_t>();
		switch (row["kind"].as<int32_t>())
		{
		case 4: counters.deliveredZel = total; break;
		case 5: counters.deliveredKarma = total; break;
		case 6: counters.deliveredSpheres = total; break;
		case 8: counters.deliveredUnits = total; break;
		default: break;
		}
	}

	for (const auto& row : co_await database->execSqlCoro(
		"SELECT mission_id FROM user_campaign_missions WHERE user_id = $1 AND state >= 2;",
		identity.userId))
	{
		counters.clearedMissions.insert(row["mission_id"].as<std::string>());
	}

	// --- the bands closed on 2026-09-20 ------------------------------------
	const auto friends = co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n, COALESCE(SUM(favorite <> 0), 0) AS fav"
		" FROM user_friends WHERE user_id = $1;", identity.userId);
	if (!friends.empty())
	{
		counters.friendCount = friends[0]["n"].as<int64_t>();
		counters.favouritedFriends = friends[0]["fav"].as<int64_t>();
	}

	for (const auto& row : co_await database->execSqlCoro(
		"SELECT facility_id, lv FROM user_town_facilities WHERE user_id = $1;", identity.userId))
	{
		counters.facilityLevel[row["facility_id"].as<int32_t>()] = row["lv"].as<int64_t>();
	}
	for (const auto& row : co_await database->execSqlCoro(
		"SELECT location_id, lv FROM user_town_locations WHERE user_id = $1;", identity.userId))
	{
		counters.locationLevel[row["location_id"].as<int32_t>()] = row["lv"].as<int64_t>();
	}

	// SQUAD COST.  Summed per deck from the units actually slotted, then the
	// best deck wins — the achievement is "Formed Squad with more than N Cost",
	// so it is the high-water mark across squads, not the active one.
	{
		std::map<int32_t, const ::UnitMst*> unitById;
		for (const auto& unit : theServer()->cache().unitMst())
			unitById.emplace(unit.id, &unit);

		std::map<int32_t, int64_t> costByDeck;
		for (const auto& row : co_await database->execSqlCoro(
			"SELECT d.deck_num AS deck_num, u.unit_id AS unit_id"
			" FROM user_campaign_decks d"
			" JOIN user_units u ON u.user_unit_id = d.user_unit_id AND u.user_id = d.user_id"
			" WHERE d.user_id = $1 AND d.user_unit_id <> 0;", identity.userId))
		{
			const auto it = unitById.find(row["unit_id"].as<int32_t>());
			if (it != unitById.end())
				costByDeck[row["deck_num"].as<int32_t>()] += it->second->cost;
		}
		for (const auto& [deck, cost] : costByDeck)
		{
			(void)deck;
			counters.maxSquadCost = std::max(counters.maxSquadCost, cost);
		}
	}

	// GRAND QUEST 100%.  attain_percent has been on the clear history all
	// along; the note that said completion "is not stored per quest" was
	// looking for a different table.
	{
		const auto& grand = theServer()->cache().grandQuestPermits().missions;
		if (!grand.empty())
		{
			std::string ids;
			for (const auto id : grand)
				ids += (ids.empty() ? "'" : ",'") + std::to_string(id) + "'";
			const auto rows = co_await database->execSqlCoro(
				"SELECT COUNT(*) AS n FROM user_campaign_missions"
				" WHERE user_id = $1 AND attain_percent >= 100 AND mission_id IN (" + ids + ");",
				identity.userId);
			counters.grandQuestFullClears = rows.empty() ? 0 : rows[0]["n"].as<int64_t>();
		}
	}

	for (const auto& row : co_await database->execSqlCoro(
		"SELECT mission_id FROM user_campaign_missions"
		" WHERE user_id = $1 AND state >= 2 AND no_continue <> 0;", identity.userId))
	{
		counters.noContinueMissions.insert(row["mission_id"].as<std::string>());
	}

	// THE SUMMONER ARC.  One row, mirroring the wire block.
	if (const auto s = co_await database->execSqlCoro(
			"SELECT sp, level, friend_point, summon_limit, ability_info,"
			" level1, level2, level3, level4, level5, level6"
			" FROM user_summoner WHERE user_id = $1;", identity.userId);
		!s.empty())
	{
		const auto n = [&](const char* c) {
			return s[0][c].isNull() ? 0 : s[0][c].as<int64_t>();
		};
		counters.summonerLevel = n("level");
		counters.summonerSkillPoints = n("sp");
		counters.summonerTrainingPoints = n("friend_point");
		counters.summonerPedestals = n("summon_limit");
		counters.summonerElementLevel[1] = n("level1");
		counters.summonerElementLevel[2] = n("level2");
		counters.summonerElementLevel[3] = n("level3");
		counters.summonerElementLevel[4] = n("level4");
		counters.summonerElementLevel[5] = n("level5");
		counters.summonerElementLevel[6] = n("level6");

		// `ability_info` packs the unlocked Parameters; the achievement asks for
		// the highest LEVEL reached, so the count of unlocked entries is what it
		// measures.  Empty means none unlocked, which is level 0, not 1.
		const auto packed = s[0]["ability_info"].isNull()
			? std::string{} : s[0]["ability_info"].as<std::string>();
		counters.summonerParameterLevel =
			static_cast<int64_t>(splitCsv(packed).size());
	}

	// Band 55000 asks for the best WEAPON level ever reached, so it reads the
	// per-arm table rather than whichever arm happens to be equipped.
	if (const auto arm = co_await database->execSqlCoro(
			"SELECT COALESCE(MAX(lv), 0) AS best FROM user_summoner_arm_levels"
			" WHERE user_id = $1;", identity.userId);
		!arm.empty())
	{
		counters.summonerBestArmLevel = arm[0]["best"].as<int64_t>();
	}

	if (const auto ls = co_await database->execSqlCoro(
			"SELECT COALESCE(ls_spheres_created, 0) AS n FROM user_summoner"
			" WHERE user_id = $1;", identity.userId);
		!ls.empty())
	{
		counters.summonerLsSpheres = ls[0]["n"].as<int64_t>();
	}

	// The two crystal maxima already existed for trophies 100290/100300; the
	// spark one was added beside them rather than in a table of its own.
	if (const auto best = co_await database->execSqlCoro(
			"SELECT b_crystal_max, h_crystal_max, spark_cnt_max"
			" FROM user_team_archive WHERE user_id = $1;", identity.userId);
		!best.empty())
	{
		counters.bestBattleCrystals = best[0]["b_crystal_max"].as<int64_t>();
		counters.bestHeartCrystals = best[0]["h_crystal_max"].as<int64_t>();
		counters.bestSparks = best[0]["spark_cnt_max"].as<int64_t>();
	}

	for (const auto& row : co_await database->execSqlCoro(
		"SELECT DISTINCT item_id FROM user_items WHERE user_id = $1 AND item_num > 0;",
		identity.userId))
	{
		counters.heldItems.insert(row["item_id"].as<int64_t>());
	}

	// CRAFTED ITEMS.  user_recipe_crafts counts RECIPES; the achievement names
	// an ITEM, so the recipe's output is what joins them.
	{
		std::map<int32_t, std::pair<int64_t, int64_t>> recipeOutput;   // recipe -> (item, per craft)
		for (const auto& recipe : theServer()->cache().initializeResp().receipe)
			recipeOutput.emplace(recipe.id, std::pair{ recipe.item_id, std::max(recipe.item_count, 1) });

		for (const auto& row : co_await database->execSqlCoro(
			"SELECT recipe_id, craft_count FROM user_recipe_crafts WHERE user_id = $1;",
			identity.userId))
		{
			const auto it = recipeOutput.find(row["recipe_id"].as<int32_t>());
			if (it == recipeOutput.end())
				continue;
			counters.craftedItems[it->second.first] +=
				row["craft_count"].as<int64_t>() * it->second.second;
		}
	}

	co_return counters;
}

} // namespace detail

/*!
* This player's progress on the achievements the screen asked for.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @param category   AchievementSubjectMst.category to filter on; 0 = any.
* @param condType   AchievementSubjectMst.cond_type to filter on; 0 = any.
* @return One row per matching subject, in catalogue order.
*
* Every matching subject is reported, finished or not: the request CLEARS the
* client's list before it is sent (GetAchievementInfoRequest::createBody), so a
* row left out is a row the screen cannot draw at all.
*
* Four of the row's six numbers are DERIVED, not stored.  Only "has this player
* claimed the reward" is persisted; progress, the target, the state and the
* timer are computed from the live counters on every read, which is what makes
* an achievement the player finished before the feature existed show up
* already complete instead of sitting at zero.
*/
inline drogon::Task<std::vector<::UserAchievementSubjectInfo>> loadAchievementSubjects(
	const db::Database database,
	const UserIdentity identity,
	const int32_t category,
	const int32_t condType)
{
	const auto counters = co_await detail::achievementCounters(database, identity);

	// What the player has already claimed, so a finished subject stops paying.
	std::map<std::string, int32_t> claimed;
	for (const auto& row : co_await database->execSqlCoro(
		"SELECT subject_id, reward_received FROM user_achievement_subjects WHERE user_id = $1;",
		identity.userId))
	{
		claimed[row["subject_id"].as<std::string>()] = row["reward_received"].as<int32_t>();
	}

	std::vector<::UserAchievementSubjectInfo> subjects;
	for (const auto& subject : theServer()->cache().achievementSubjectMst())
	{
		// 0 AND -1 BOTH MEAN "ANY".  Only 0 was treated as a wildcard, and that
		// silently broke the whole Merit unit trade.
		//
		// RandallAchievementDedicateUnitScene::initConnect @0x1A3FD4C asks with
		// a hardcoded (mode 1, category 3) pair and `sub_category = -1` — the
		// `mov w8,#-1` at 0x1A3FD98.  Read literally, `cond_type != -1` is true
		// of every row, so the reply carried ZERO subjects.
		//
		// The client then has no (3, 8) bucket for
		// UserAchievementSubjectInfoList::getDataObject(3, 8)
		// (createSubjectMstList @0x1A3FFDC), so the scene's subject id at
		// +0x448 stays empty, AchievementSubjectMstList::find("") returns null,
		// and sellUnit @0x1A43578 bails on `cbz x0` BEFORE it ever constructs
		// AchievementDeliverRequest.
		//
		// That is why selecting units did nothing and `vsaXI4M0` never reached
		// the server: there was no request to read, and nothing server-side had
		// gone wrong at the point the trade appeared to fail.  The rate table,
		// the pricing and the handler were all fine and all innocent.
		if (category > 0 && subject.category != category)
			continue;
		if (condType > 0 && subject.cond_type != condType)
			continue;

		const auto progress = detail::subjectProgress(subject, counters);
		const auto target = detail::subjectTarget(subject);
		const auto it = claimed.find(std::to_string(subject.id));
		const bool paid = it != claimed.end() && it->second != 0;
		const bool done = paid || (progress >= 0 && target > 0 && progress >= target);

		::UserAchievementSubjectInfo row{};
		row.subject_id = std::to_string(subject.id);
		row.progress = std::max<int64_t>(progress, 0);
		row.need_count = static_cast<int32_t>(
			std::clamp<int64_t>(target, 0, std::numeric_limits<int32_t>::max()));

		// 1 = In Progress, 2 = Completed.  Never 0: 0 is "not accepted yet" and
		// draws a Start button, and there is nothing to accept here — see
		// UserAchievementSubjectInfo.state in net/achievement.kdl.
		row.state = done ? 2 : 1;

		// 0 nothing to claim / 1 claimable / 2 claimed.  The DATABASE column
		// stays a plain 0-or-1 "has this player claimed it", because that is
		// what the claim guard latches on; the three-way value is a wire
		// concern and is built here.
		row.reward_received = paid ? 2 : (done ? 1 : 0);

		// Seconds left, and 0 is the value that prints "Time Expired" on every
		// row.  Nothing here is timed, so -1 goes out and the line is skipped.
		row.time_left = -1;

		subjects.push_back(std::move(row));
	}
	co_return subjects;
}

// ---------------------------------------------------------------------------
// The Merit Point DELIVER flow -- Trade Zel / Karma / Units / Spheres.
//
// The four Dedicate screens are one request, AchievementDeliver (vsaXI4M0), and
// the flow is told apart by the achievement cond_type it carries.  See
// AchievementDeliverNode in net/achievement.kdl for where that mapping is
// proved.
// ---------------------------------------------------------------------------

/*! Deliver flows, which are the request's cond_type values verbatim. */
enum class DeliverKind : int32_t
{
	Zel    = 4,
	Karma  = 5,
	Sphere = 6,
	Unit   = 8,
};

inline bool isDeliverKind(const int32_t condType)
{
	return condType == 4 || condType == 5 || condType == 6 || condType == 8;
}

/*!
* The global multiplier on every delivery, GameUtils::getSellAchievePointRate
* @0x1176D2C.
*
* It reads GeneralEventMst type 19, splits its params on "," and takes the first
* — a "Merit Points x N" campaign — and RETURNS 1 when there is no such row.
* This data has no type-19 row, so this is 1 today; it is written out rather
* than hardcoded so a future event table changes the answer on its own.
*/
inline int32_t sellAchievePointRate()
{
	for (const auto& ev : theServer()->cache().userInfoResp().general_event)
	{
		if (ev.event_type != 19)
			continue;
		const auto& params = ev.params;
		const auto comma = params.find(',');
		try
		{
			return std::max(std::stoi(params.substr(0, comma)), 0);
		}
		catch (const std::exception&)
		{
			break;
		}
	}
	return 1;
}

namespace detail
{

/*! The rate row for a (rarity, getting_type) pair, or null when there is none. */
inline const ::AchievementDeliverRateMst* deliverRate(
	const int32_t rarity, const int32_t gettingType)
{
	const auto& table = theServer()->cache().achievementDeliverRateMst();
	const auto it = std::find_if(table.begin(), table.end(),
		[rarity, gettingType](const ::AchievementDeliverRateMst& r)
		{ return r.rarity == rarity && r.getting_type == gettingType; });
	return it == table.end() ? nullptr : &*it;
}

/*! `base` is a StrToInt read, which stops at the first non-digit by design. */
inline int32_t deliverBase(const ::AchievementDeliverRateMst& row)
{
	std::string digits;
	for (const char c : row.base)
	{
		if (std::isdigit(static_cast<unsigned char>(c)) || (c == '-' && digits.empty()))
			digits += c;
		else
			break;
	}
	try { return digits.empty() || digits == "-" ? 0 : std::stoi(digits); }
	catch (const std::exception&) { return 0; }
}

inline float deliverMultiplier(const ::AchievementDeliverRateMst& row)
{
	try { return std::stof(row.multiplier); }
	catch (const std::exception&) { return 0.0f; }
}

} // namespace detail

/*!
* What one unit is worth, from GameUtils::getSellAchievePoint @0x1176B70.
*
*     rate * ( base + trunc( multiplier * unitLevel ) )
*
* The row is found on (UnitMst.rarity, UnitMst.getting_type), and a unit with no
* row is worth NOTHING rather than a default — the client refuses to price those
* too, which is why 304 of this data's 2291 units (every getting_type 3) cannot
* be traded at all.
*
* @param unit The unit's master row.
* @param unitLevel The owned unit's level — UserUnitInfoBase::getUnitLv, which
*        is what the client multiplies by (vtable +0xb0).
*/
inline int32_t unitDeliverPoints(const ::UnitMst& unit, const int32_t unitLevel)
{
	const auto* row = detail::deliverRate(unit.rarity, unit.getting_type);
	if (!row)
		return 0;
	const auto scaled = detail::deliverMultiplier(*row) * static_cast<float>(unitLevel);
	const auto points = detail::deliverBase(*row) + static_cast<int32_t>(scaled);
	return std::max(points, 0) * sellAchievePointRate();
}

/*!
* What one item is worth, from GameUtils::getSellAchievePoint @0x1176EAC.
*
*     rate * trunc( multiplier + base )
*
* NOT the unit formula: an item has no level, so the two columns are ADDED
* rather than multiplied (`ldp s1,s0 / scvtf / fadd` at 0x1176F28), and the row
* is found on (ItemMst.rarity, 5) — the 5 is a constant in the binary, not a
* column of the item.
*/
inline int32_t itemDeliverPoints(const ::ItemMst& item)
{
	constexpr int32_t kItemGettingType = 5;
	const auto* row = detail::deliverRate(item.rarity, kItemGettingType);
	if (!row)
		return 0;
	const auto sum = detail::deliverMultiplier(*row)
		+ static_cast<float>(detail::deliverBase(*row));
	return std::max(static_cast<int32_t>(sum), 0) * sellAchievePointRate();
}

/*! The per-day ceiling on points earned from one flow, from DefineMst. */
inline int32_t deliverDailyCap(const int32_t condType)
{
	const auto& defines = theServer()->cache().initializeResp().defines;
	switch (condType)
	{
	case 4: return defines.max_achieve_point_zel_per_day;
	case 5: return defines.max_achieve_point_karma_per_day;
	// Spheres are items and share the item ceiling.  Units have their OWN
	// ceiling, and it is now modelled: `j1zaSBm7` ->
	// DefineMst::setMaxAchievePointUnitPerDay, recovered 2026-09-15 from
	// DefineMstResponse::readParam.  Units rode the item column until then --
	// harmless, since both are 99999 in this data, but the Trade Units screen
	// was quoting the wrong field's number.
	case 6: return defines.max_achieve_point_item_per_day;
	case 8: return defines.max_achieve_point_unit_per_day;
	default: return 0;
	}
}

/*! Today, as the ledger's day key.  UTC, so a cap cannot be reset by a timezone. */
inline std::string deliverToday()
{
	const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm tm{};
#ifdef _WIN32
	gmtime_s(&tm, &now);
#else
	gmtime_r(&now, &tm);
#endif
	char buf[16] = {};
	std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	return buf;
}

/*!
* Pays one achievement's packed `JQ23rIvk` reward into the present box.
*
* FORMAT: `type:target:count`, plus two trailing fields this build does not
* decode (they are ('0','0') or ('1','0') on all but two of the 152 Level Up
* Campaign rows, and nothing here depends on them).  The leading three are the
* same shape AchievementTrade's offers use, so `parseReward`'s reading of them is
* already exercised.  // UNVERIFIED: fields 4 and 5.
*
* EVERYTHING GOES TO THE PRESENT BOX, currencies included, rather than being
* credited directly.  Receive All can settle dozens of rows in one request --
* the Level Up Campaign alone is 152 -- and a reply cannot show that many grants
* inline.  The box is the path already built for bulk rewards, its claim credits
* currencies properly, and it keeps this handler from having to refresh half a
* dozen caches it does not otherwise touch (a SQL UPDATE is not a client
* refresh).  present_count on the next header refresh lights the Presents badge.
*
* The types that occur in this data are 3 zel, 5 material, 6 unit, 7 sphere and
* 8 gem, all of which the box's vocabulary already dispatches.  An unknown type
* is logged and skipped rather than silently dropped or guessed at.
*
* @param database Caller-owned transaction; the grant must roll back with the claim.
* @param identity Resolved user identity.
* @param packed The subject's reward_info.  Empty is a no-op, not an error.
* @return True when something was queued.
*/
inline drogon::Task<bool> grantAchievementReward(
	const db::Database database,
	const UserIdentity identity,
	const std::string packed)
{
	if (packed.empty())
		co_return false;

	int32_t type = 0;
	std::string target;
	int32_t count = 1;
	{
		size_t start = 0, field = 0;
		while (start <= packed.size() && field < 3)
		{
			const auto end = packed.find(':', start);
			const auto part = packed.substr(start, end == std::string::npos ? end : end - start);
			try
			{
				if (field == 0) type = std::stoi(part);
				else if (field == 1) target = part;
				else count = std::max(std::stoi(part), 1);
			}
			catch (const std::exception&) { /* leave the default */ }
			if (end == std::string::npos)
				break;
			start = end + 1;
			++field;
		}
	}

	switch (type)
	{
	case 3:   // zel
	case 5:   // material
	case 6:   // unit
	case 7:   // sphere
	case 8:   // gem
	case 4:   // item
	case 11:  // karma
		break;
	default:
		LOG_WARN << "grantAchievementReward: unhandled reward type " << type
			<< " in \"" << packed << "\"; nothing queued";
		co_return false;
	}

	// Currency presents carry no target id, the same way CampaignReceipt writes
	// them; a unit or item present carries the entity id.
	const auto isCurrency = (type == 3 || type == 8 || type == 11);
	co_await addUserPresent(database, identity, type,
		isCurrency ? std::string{} : target, count);
	co_return true;
}

/*!
* The packed achievement badge string — `dUujjBBK` on the BadgeInfo block.
*
* GRAMMAR, from AchievementBadgeInfo::parseBadgeData @0x11F0284: comma-separated
* records of three colon-separated integers, `category:subCategory:count`.  The
* reader pushes a 12-byte {int,int,int} per record and CLAMPS the count to 99
* (`cmp w23,#0x63; csel`).  It is a FULL REPLACE done inline — the first thing
* parseBadgeData does is `end = begin` (0x11F02B4) — so an empty string is a
* meaningful "no badges", not a no-op, and every send must be the complete set.
* (clearBadgeData() @0x11F0690 exists but has zero callers.)
*
* WHAT THE PAIR IS: the same two filters GetAchievementInfoRequest::createBody
* sends, `KT71m8Ae` category and `3rhygS9K` cond_type — see GetAchievementQuery
* in net/achievement.kdl, which shares both keys with F_ACHIEVEMENT_SUBJECT_MST.
*
* CONFIRMED CONSUMERS, all of which read 0 for as long as this went out empty:
*   - RandallAchievementRecordSelectScene::setLayout +0x790 badges each row with
*     getBadgeCount(1, subCategory), and +0xb1c adds getBadgeCount(4).
*   - RandallAchievementTopScene::setLayout draws two counts:
*     getBadgeCount(1) + getBadgeCount(4) (+0x5e4/+0x7e0) and
*     getBadgeCount(2) + getBadgeCount(5) (+0x610/+0x80c).
*   - BadgeInfo::getSummonNum @0x11F4194 folds the no-arg getBadgeCount() total
*     into the Summoners' Hall count, which is the badge on the Randall town
*     tile — reported as "no badge showing that an achievement has been made".
*   - AchievementBadgeInfo::getActiveCategoryCountExt @0x11F00F8 counts the
*     categories 1..8 (skipping 0 and 6) that have any badge, for
*     RandallSummonScene2/3::setLayout.
*
* WHAT COUNTS: an achievement that is finished and NOT yet claimed — the same
* `done && !paid` pair loadAchievementSubjects turns into reward_received 1.  A
* claimed one is deliberately not counted, or the badge would never clear.  The
* count goes out unclamped; the client clamps it to 99 itself.
*
* Categories 2 and 4 get no rows from this data (our 873 curated subjects are
* categories 1, 3, 4 and 5, and only 1/3/4/5 ever carry a claimable row), and
* nothing here invents one.  // UNVERIFIED: which screen category 2 belongs to.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity.
* @return The packed string, empty when the player has nothing to claim.
*/
inline drogon::Task<std::string> achievementBadgeData(
	const db::Database database,
	const UserIdentity identity)
{
	const auto counters = co_await detail::achievementCounters(database, identity);

	std::map<std::string, int32_t> claimed;
	for (const auto& row : co_await database->execSqlCoro(
		"SELECT subject_id, reward_received FROM user_achievement_subjects WHERE user_id = $1;",
		identity.userId))
	{
		claimed[row["subject_id"].as<std::string>()] = row["reward_received"].as<int32_t>();
	}

	// std::map keeps the records in a stable (category, cond_type) order, which
	// keeps the string diffable between two replies.
	std::map<std::pair<int32_t, int32_t>, int32_t> pending;
	for (const auto& subject : theServer()->cache().achievementSubjectMst())
	{
		const auto it = claimed.find(std::to_string(subject.id));
		if (it != claimed.end() && it->second != 0)
			continue;   // already paid — must not keep the badge lit

		const auto progress = detail::subjectProgress(subject, counters);
		const auto target = detail::subjectTarget(subject);
		if (progress < 0 || target <= 0 || progress < target)
			continue;

		++pending[{ subject.category, subject.cond_type }];
	}

	std::string packed;
	for (const auto& [key, count] : pending)
	{
		if (!packed.empty())
			packed += ',';
		packed += std::to_string(key.first) + ':' + std::to_string(key.second)
			+ ':' + std::to_string(count);
	}
	co_return packed;
}

/*!
* Whether an achievement's condition is met, for the handler that pays it out.
* Kept beside the loader so the two cannot drift.
*/
inline drogon::Task<bool> achievementComplete(
	const db::Database database,
	const UserIdentity identity,
	const ::AchievementSubjectMst& subject)
{
	const auto counters = co_await detail::achievementCounters(database, identity);
	const auto progress = detail::subjectProgress(subject, counters);
	const auto target = detail::subjectTarget(subject);
	co_return progress >= 0 && target > 0 && progress >= target;
}

/*!
* This player's Merit Point shop purchases as `9j3ALx8I` rows.
*
* Every catalogue offer is reported, zeros included: readParam @0x13FCC68
* clears the list on row 0 and an empty array never reaches it, so an offer
* bought down to its limit has to keep saying so.
*/
inline drogon::Task<std::vector<::UserAchievementTradeInfo>> loadAchievementTrades(
	const db::Database database,
	const UserIdentity identity)
{
	std::map<std::string, int32_t> bought;
	for (const auto& row : co_await database->execSqlCoro(
		"SELECT trade_id, count FROM user_achievement_trades WHERE user_id = $1;",
		identity.userId))
	{
		bought[row["trade_id"].as<std::string>()] = row["count"].as<int32_t>();
	}

	std::vector<::UserAchievementTradeInfo> trades;
	for (const auto& offer : theServer()->cache().achievementTradeMst())
	{
		::UserAchievementTradeInfo row{};
		row.trade_id = std::to_string(offer.id);
		const auto it = bought.find(row.trade_id);
		row.count = it == bought.end() ? 0 : it->second;
		trades.push_back(std::move(row));
	}
	co_return trades;
}

} // namespace gme
