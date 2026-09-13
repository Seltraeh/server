#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <map>
#include <string>
#include <vector>

// Trophy GRADES — the star tiers on the Records screen.
//
// The counters were already reported (UserTeamArchive), but the grade a trophy
// has reached is NOT computed by the client: PlayerInfoBattleResultScene::
// getTrophyGrade @0x1799F34 walks that trophy's TrophyGradeMst rows and asks
// UserTrophyGradeInfoList::exist(gradeId) for each, returning the highest one
// present — so with the list empty (we never sent it) every trophy sat at grade
// 0 no matter how high its counter ran.
//
// This derives the attained grades from the same counters the archive reports:
// a grade row (trophy id, grade, clear_cond) is attained when the trophy's
// counter has reached clear_cond.  UserTrophyGradeInfo carries only user_id and
// the grade id (UserTrophyGradeInfoResponse::readParam @0x1404870), and the
// list is a full replace.
//
// Only the cumulative-record trophies (groups 100 and 200) are derivable here;
// the mode-specific groups (300 arena, 400+ raid/guild/colosseum...) count
// things this server does not track yet, and are left unattained rather than
// being faked.

namespace gme
{

namespace detail
{

/*!
* Trophy id -> the UserTeamArchive counter that feeds it.
*
* The names in the table are Japanese (F_TROPHY_MST x1evqVH6), so the pairing is
* by meaning: 100010 ログイン日数 = login days, 100030 累計獲得ゼル = zel earned,
* 200030 総合戦闘勝利数 = battle wins, and so on.  Anything absent from this map
* has no counter behind it yet.
*/
inline int64_t trophyCounter(const int32_t trophyId, const ::UserTeamArchive& a)
{
	switch (trophyId)
	{
	case 100010: return a.login_cnt;              // login days
	case 100020: return a.serial_login_day;       // consecutive login days
	case 100030: return a.zel_get;                // zel earned
	case 100040: return a.zel_use;                // zel spent
	case 100050: return a.zel_unit_sale;          // zel from unit sales
	case 100060: return a.zel_item_sale;          // zel from item sales
	case 100070: return a.karma_get;              // karma earned
	case 100080: return a.karma_use;              // karma spent
	case 100090: return a.friend_p_get;           // bond points earned
	case 100100: return a.friend_p_use;           // bond points spent
	case 100190: return a.unit_sum_cnt;           // units acquired (every copy)
	case 100220: return a.unit_mix_cnt;           // fusions
	case 100230: return a.unit_mix_elem_cnt;      // fusion materials used
	case 100240: return a.unit_evo_cnt;           // evolutions
	case 100250: return a.item_mix_cnt;           // syntheses
	case 100260: return a.item_mix_elem_cnt;      // synthesis materials used
	case 100270: return a.sphere_mix_cnt;         // spheres crafted
	case 100280: return a.town_harvest_cnt;       // town harvest taps
	case 100290: return a.b_crystal_max;          // most battle crystals at once
	case 100300: return a.h_crystal_max;          // most heart crystals at once
	case 100310: return a.b_crystal;              // battle crystals, cumulative
	case 100320: return a.h_crystal;              // heart crystals, cumulative
	case 100325: return a.turn_max_unit_damage;   // biggest hit by one unit
	case 100330: return a.battle_turn_max_damage; // biggest turn
	case 100340: return a.battle_turn_max_spark;  // most sparks in a turn
	case 100350: return a.battle_spark_cnt;       // sparks, cumulative
	case 100360: return a.battle_skill_cnt;       // BBs used
	case 200010: return a.quest_challenge_cnt;    // quests attempted
	case 200020: return a.quest_clear_cnt;        // quests cleared
	case 200030: return a.quest_win_cnt;          // battles won
	case 200040: return a.quest_mimic_cnt;        // mimics met
	default:     return -1;                       // no counter behind it yet
	}
}

} // namespace detail

/*!
* Every trophy grade the player has attained, as `H18CjPKI` rows.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @param archive  The counters this is measured against (the caller usually has
*                 them already — UserInfo reports them in the same reply).
* @return One row per attained grade, in grade-id order.
*/
inline drogon::Task<std::vector<::UserTrophyGradeInfo>> loadTrophyGrades(
	const db::Database database,
	const UserIdentity identity,
	const ::UserTeamArchive& archive)
{
	// The dictionary trophies (100110 total species, 100120-100170 per element,
	// 100180 items) are answered by the dictionaries rather than a counter.
	const auto unitRows = co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n FROM user_unit_dictionary WHERE user_id = $1;",
		identity.userId);
	const int64_t unitSpecies = unitRows.empty() ? 0 : unitRows[0]["n"].as<int64_t>();

	const auto itemRows = co_await database->execSqlCoro(
		"SELECT COUNT(DISTINCT item_id) AS n FROM user_items WHERE user_id = $1;",
		identity.userId);
	const int64_t itemSpecies = itemRows.empty() ? 0 : itemRows[0]["n"].as<int64_t>();

	std::vector<::UserTrophyGradeInfo> grades;
	for (const auto& row : theServer()->cache().initializeResp().trophy_grade)
	{
		int64_t have = detail::trophyCounter(row.trophy_id, archive);
		if (row.trophy_id == 100110)
			have = unitSpecies;
		else if (row.trophy_id == 100180)
			have = itemSpecies;
		// 100120-100170 are the per-element species counts; the dictionary
		// table holds no element, so they wait for a join that does.

		if (have < 0 || have < row.clear_cond)
			continue;

		::UserTrophyGradeInfo grade{};
		grade.user_id = identity.userId;
		grade.grade_id = row.id;
		grades.push_back(std::move(grade));
	}
	co_return grades;
}

} // namespace gme
