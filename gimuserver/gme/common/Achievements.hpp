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

/*! Everything one pass over the achievement list needs to measure progress. */
struct AchievementCounters
{
	::UserTeamArchive archive{};
	int64_t meritPoints = 0;
	int32_t playerLevel = 1;
	int64_t itemSpecies = 0;
	int64_t unitSpecies = 0;
	int64_t favouritedUnits = 0;
	std::set<std::string> clearedMissions;
};

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
*   4000-6000    friends added / favourited / comment edited  (no friend system)
*   8000-13000   town facility and location levels            (the achievement
*                names the building in TEXT only and neither MST resolves its
*                name here, so which id is "Sphere House" would be a guess)
*   14000        bought a song                                (no music house)
*   17000        the six Heroes Spheres guide entries         (needs a per-sphere
*                                                              guide join)
*   23000        squad cost                                   (not counted)
*   28000-33000  arena                                        (not simulated)
*   37000        Trials                                       (not built)
*   38000        Hunter Rank                                  (not built)
*   39000        raid                                         (not simulated)
*   41000        Grand Quest 100% completion                  (completion is not
*                                                              stored per quest)
*   42000-44000  Frontier Hunter / Frontier Gate training     (not built)
*   45000-51000  colosseum                                    (not simulated)
*   52000-65000  the summoner subsystems                      (user_summoner
*                holds SP and nothing else)
*   400000+      the SP tabs (categories 4 and 5)             (Trials again)
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
	if (subject.id == 18100)     // "Six Spheres' Item Guide Entries Filled": its
		return -1;               // cond_param is a LIST of six sphere ids, which
		                         // needs a per-species guide join

	switch (subject.id / 1000 * 1000)
	{
	case 1000:  return a.login_cnt;             // "Log in for a total of N days"
	case 2000:  return a.friend_p_get;          // "N Honor Points Accumulated" --
	                                            // the LIFETIME total, which is
	                                            // why the current balance could
	                                            // not answer it: that falls when
	                                            // an Honor Summon spends it
	case 3000:  return counters.playerLevel;    // "Player Level N Reached"
	case 7000:  return counters.meritPoints;    // "N Merit Points Accumulated"
	case 15000: return a.item_mix_cnt;          // "Synthesized N Times"
	case 16000: return counters.itemSpecies;    // "N Item Guide Entries Filled"
	case 18000: return a.sphere_mix_cnt;        // "N Spheres Created"
	case 19000: return counters.unitSpecies;    // "N Unit Guide Entries Filled"
	case 20000: return a.unit_mix_cnt;          // "N Fusions Performed"
	case 21000: return a.unit_evo_cnt;          // "N Evolutions Performed"
	case 22000: return counters.favouritedUnits;// "Favorited a Unit"
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
	switch (subject.id / 1000 * 1000)
	{
	case 35000:
	case 36000:
	case 40000:
		return 1;
	default:
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

	for (const auto& row : co_await database->execSqlCoro(
		"SELECT mission_id FROM user_campaign_missions WHERE user_id = $1 AND state >= 2;",
		identity.userId))
	{
		counters.clearedMissions.insert(row["mission_id"].as<std::string>());
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
		if (category != 0 && subject.category != category)
			continue;
		if (condType != 0 && subject.cond_type != condType)
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
