#pragma once

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/EventTokens.hpp>
#include <gimuserver/gme/common/SummonTickets.hpp>

#include <algorithm>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Frontier Gate run state and the end of a run, shared by the battles
// (MissionStart / MissionEnd) and Retire (FrontierGateEnd).
//
// A run is a string of battles, and the client keeps its floors and score
// between them only when the server hands them back: FrontierBattleInfoResponse
// (eIQ79KO2) is the sole setter of FrontierBattleInfo's progress, running score
// and support.  So each Frontier Gate MissionEnd's report is stored on the run
// row, echoed, and handed to the next battle in its MissionStart.
//
// Rewards pay THE MOMENT the run reaches them (every battle checks), not when
// the run ends: starting a gate again abandons the open run, and a run that had
// cleared paying floors would otherwise lose them.  Each F_FROGATE_REWARD_MST
// row of the gate pays once per user (cond_type 1 by floors, 2 by score), and
// the run remembers what it was paid so the result screen can list the whole
// run.  A run ends in the result scene either way it ends — Retire
// (FrontierGateEnd) or a lost battle (MissionEnd).  See FrontierEndInfo and
// FrontierResRewardInfo in net/frontier_gate.kdl.

namespace gme
{

/*!
* The run as the server holds it (user_frontier_gate_run).
*/
struct FrontierRun
{
	int32_t gateId = 0;
	int32_t supportId = 0;
	int64_t score = 0;     // the run's running score (PYbfxpTp)
	int32_t progress = 0;  // floors cleared (pG2n1A28)
	std::string note;      // the bonus tallies (7cev0fzQ)
	std::string odInfo;    // overdrive info (H7f9NAXu)
	std::vector<std::string> paid;  // reward ids this run has already been paid

	// What payFrontierRewards credited, so the reply can refresh exactly those
	// client caches.  Not persisted — per call.
	bool tokensChanged = false;   // event tokens (l234vdKs)
	bool ticketsChanged = false;  // V2 summon tickets (a3d5d12i)
};

/*!
* Reads the user's run, if one is open.
*/
inline drogon::Task<std::optional<FrontierRun>> loadFrontierRun(
	const db::Database database,
	const UserIdentity identity)
{
	const auto rows = co_await database->execSqlCoro(
		"SELECT frogate_id, sel_support_id, run_score, run_progress, run_note, run_od_info,"
		" run_rewards FROM user_frontier_gate_run WHERE user_id = $1;",
		identity.userId);
	if (rows.empty())
		co_return std::nullopt;

	FrontierRun run{};
	run.gateId = rows[0]["frogate_id"].as<int32_t>();
	run.supportId = rows[0]["sel_support_id"].as<int32_t>();
	run.score = rows[0]["run_score"].as<int64_t>();
	run.progress = rows[0]["run_progress"].as<int32_t>();
	run.note = rows[0]["run_note"].as<std::string>();
	run.odInfo = rows[0]["run_od_info"].as<std::string>();
	{
		std::stringstream stream(rows[0]["run_rewards"].as<std::string>());
		std::string id;
		while (std::getline(stream, id, ','))
		{
			if (!id.empty())
				run.paid.push_back(id);
		}
	}
	co_return run;
}

/*!
* Stores what a Frontier Gate MissionEnd reported about the run.
*/
inline drogon::Task<void> storeFrontierRun(
	const db::Database database,
	const UserIdentity identity,
	const FrontierRun& run)
{
	co_await database->execSqlCoro(
		"UPDATE user_frontier_gate_run SET run_score = $1, run_progress = $2, run_note = $3,"
		" run_od_info = $4 WHERE user_id = $5;",
		run.score, run.progress, run.note, run.odInfo, identity.userId);
}

/*!
* The user's best score on a gate (0 before the first run ends).
*/
inline drogon::Task<int64_t> frontierBestScore(
	const db::Database database,
	const UserIdentity identity,
	const int32_t gateId)
{
	const auto rows = co_await database->execSqlCoro(
		"SELECT score FROM user_frontier_gates WHERE user_id = $1 AND frogate_id = $2;",
		identity.userId, gateId);
	co_return rows.empty() ? 0 : rows[0]["score"].as<int64_t>();
}

/*!
* The eIQ79KO2 block for a run: where the next battle starts from.  The
* support goes out only when one was picked — FrontierBattleInfoResponse turns
* the value into an id with StrToInt, so a 0 would name support "0".
*/
inline ::FrontierBattleInfo frontierBattleInfo(const FrontierRun& run, const int64_t bestScore)
{
	::FrontierBattleInfo info{};
	info.high_score = bestScore;
	info.note = run.note;
	info.progress = run.progress;
	if (run.supportId != 0)
		info.support_id = std::to_string(run.supportId);
	info.od_info = run.odInfo;
	info.now_score = run.score;
	return info;
}

/*!
* The result grade for a run's score, "1" (D) .. "7" (SSS).
*
* Frontier Gate ships no grade table.  The result scene draws the grade with
* Frontier Hunter's art (HunterResult<rank>.sam, setRankSam @0x15F2CAC), and
* the only grade ladder in the data is Frontier Hunter's
* F_CHLNG_MISSION_GRADE_MST_Ver1 (mission 1000000): grades 1..7 at
* 0 / 50,000 / 150,000 / 250,000 / 400,000 / 550,000 / 700,000 points.  The
* grade is display-only — no reward depends on it.
*/
inline std::string frontierGrade(const int64_t score)
{
	static constexpr int64_t kGradeFloor[] = { 0, 50'000, 150'000, 250'000, 400'000, 550'000, 700'000 };
	int grade = 1;
	for (int i = 0; i < 7; ++i)
	{
		if (score >= kGradeFloor[i])
			grade = i + 1;
	}
	return std::to_string(grade);
}

/*!
* What a finished run hands the result scene.
*/
struct FrontierResult
{
	::FrontierEndInfo end{};
	std::vector<::FrontierResRewardInfo> rewards;
	// Which currency lists the payout touched, so the reply can refresh them.
	bool tokensChanged = false;
	bool ticketsChanged = false;
};

namespace detail
{

/*!
* The gate's floor (cond_type 1) and score (2) rewards, lowest threshold first —
* the order the result scene shows them in.  cond_type 8001 is FG+ and is left
* out, the same way the client's own reward list hides it.
*/
inline std::vector<const ::FrontierGateRewardMst*> frontierGateRewards(const int32_t gateId)
{
	std::vector<const ::FrontierGateRewardMst*> rewards;
	for (const auto& reward : theServer()->cache().frontierGateRewardMst())
	{
		if (reward.frogate_id == gateId && (reward.cond_type == 1 || reward.cond_type == 2))
			rewards.push_back(&reward);
	}
	std::sort(rewards.begin(), rewards.end(), [](const auto* a, const auto* b) {
		if (a->cond_type != b->cond_type)
			return a->cond_type < b->cond_type;
		if (a->cond_param != b->cond_param)
			return a->cond_param < b->cond_param;
		return a->reward_id < b->reward_id;
	});
	return rewards;
}

/*!
* Comma-joins reward ids in numeric order (it reads better than string order).
*/
inline std::string joinIds(std::vector<std::string> ids)
{
	std::sort(ids.begin(), ids.end(), [](const std::string& a, const std::string& b) {
		return a.size() != b.size() ? a.size() < b.size() : a < b;
	});
	std::string out;
	for (const auto& id : ids)
	{
		if (!out.empty())
			out += ',';
		out += id;
	}
	return out;
}

/*!
* One QXFCkE67 row for a reward the run was paid.
*/
inline ::FrontierResRewardInfo frontierRewardEntry(const ::FrontierGateRewardMst& reward)
{
	::FrontierResRewardInfo entry{};
	entry.tar_type = reward.present_type;
	entry.tar_id = reward.target_id;
	entry.tar_cnt = std::max(reward.target_cnt, 1);
	entry.tar_param = reward.target_param;
	entry.reward_type = reward.cond_type;
	entry.reward_param = std::to_string(reward.cond_param);
	// Currency is credited on the spot; the rest waits in the present box.
	// The gate's own currencies count as currency: event tokens (8004 — the
	// Rift Token) and both summon-ticket kinds (8000 plain, 8001 per-type) have
	// stores of their own, so they are credited rather than gifted.
	entry.present_flg = (reward.present_type == 3 || reward.present_type == 8
		|| reward.present_type == 11 || reward.present_type == 8000
		|| reward.present_type == 8001 || reward.present_type == 8004) ? 0 : 1;
	return entry;
}

} // namespace detail

/*!
* Pays every reward the run has now reached and has not been paid before.
*
* Called after each battle, so a run that is abandoned (the player starts the
* gate again, or closes the game) keeps what it cleared.  Each reward pays once
* per user: the ids live in user_frontier_gates.rewards_got, which is also what
* the gate's reward list marks as obtained.  The run remembers its own share in
* run_rewards so the result screen can list the whole run.
*
* Zel, karma, gems, event tokens (8004) and both summon-ticket kinds (8000,
* 8001) are credited at once — they are currencies with stores of their own.
* Everything else goes to the present box, where PresentReceipt pays it and
* refreshes the caches it changes: the route first-clear quest rewards take.
*
* @return how many rewards this call paid.
*/
inline drogon::Task<size_t> payFrontierRewards(
	const db::Database database,
	const UserIdentity identity,
	FrontierRun& run)
{
	constexpr int64_t kMaxZelKarma = 99'999'999LL;

	std::set<std::string> got;
	const auto gateRows = co_await database->execSqlCoro(
		"SELECT rewards_got FROM user_frontier_gates WHERE user_id = $1 AND frogate_id = $2;",
		identity.userId, run.gateId);
	if (!gateRows.empty())
	{
		std::stringstream stream(gateRows[0]["rewards_got"].as<std::string>());
		std::string id;
		while (std::getline(stream, id, ','))
		{
			if (!id.empty())
				got.insert(id);
		}
	}

	std::string gateName = "Frontier Gate";
	for (const auto& gate : theServer()->cache().frontierGateMst())
	{
		if (gate.id == run.gateId && !gate.name.empty())
		{
			gateName = gate.name;
			break;
		}
	}

	size_t paid = 0;
	int64_t zel = 0, karma = 0, gems = 0;
	for (const auto* reward : detail::frontierGateRewards(run.gateId))
	{
		const auto rewardId = std::to_string(reward->reward_id);
		if (got.contains(rewardId))
			continue;
		const bool reached = reward->cond_type == 1
			? run.progress >= reward->cond_param
			: run.score >= static_cast<int64_t>(reward->cond_param);
		if (!reached)
			continue;

		const int32_t count = std::max(reward->target_cnt, 1);
		switch (reward->present_type)
		{
		case 3:  zel += count; break;
		case 11: karma += count; break;
		case 8:  gems += count; break;
		case 8000:
			// The plain summon-ticket counter, capped where the Daily Spin caps it.
			co_await database->execSqlCoro(
				"UPDATE user_info SET summon_tickets = MIN(summon_tickets + $1, 99) WHERE id = $2;",
				count, identity.userId);
			break;
		case 8001:
			co_await grantSummonTicketV2(database, identity, reward->target_id, count);
			run.ticketsChanged = true;
			break;
		case 8004:
			// The gate's own currency (token 8 is the Rift Token).  An id with
			// no name would render as a blank tile, so it falls through to the
			// box instead, where the same guard refuses it.
			if (isKnownEventToken(reward->target_id))
			{
				co_await grantEventToken(database, identity, reward->target_id, count);
				run.tokensChanged = true;
				break;
			}
			co_await addUserPresent(database, identity, reward->present_type, reward->target_id,
				count, 0, gateName + " reward");
			break;
		default:
			co_await addUserPresent(database, identity, reward->present_type, reward->target_id,
				count, 0, gateName + " reward");
			break;
		}
		got.insert(rewardId);
		run.paid.push_back(rewardId);
		++paid;

		LOG_INFO << "FrontierGate: gate " << run.gateId << " reward " << rewardId << " reached ("
			<< (reward->cond_type == 1 ? "floor " : "score ") << reward->cond_param
			<< ") — present type " << reward->present_type << " " << reward->target_id
			<< " x" << count;
	}

	if (paid == 0)
		co_return 0;

	if (zel > 0 || karma > 0 || gems > 0)
	{
		// $N in first-appearance order (the sqlite binding gotcha).
		co_await database->execSqlCoro(
			"UPDATE user_info SET zel = MIN(zel + $1, $2), karma = MIN(karma + $3, $4), gems = gems + $5"
			" WHERE id = $6;",
			zel, kMaxZelKarma, karma, kMaxZelKarma, gems, identity.userId);
	}

	// The gate's row records what has been paid.  A new row is state 1, the
	// "available" tile (FrontierGateInfo) — 0 is not a state the gate list was
	// ever seen to draw.  The best score is written when the run ENDS, not here:
	// the run measures itself against the previous best.
	co_await database->execSqlCoro(
		"INSERT INTO user_frontier_gates (user_id, frogate_id, state, score, rewards_got)"
		" VALUES ($1, $2, 1, 0, $3)"
		" ON CONFLICT(user_id, frogate_id) DO UPDATE SET state = MAX(state, 1), rewards_got = $3;",
		identity.userId, run.gateId,
		detail::joinIds(std::vector<std::string>(got.begin(), got.end())));

	co_await database->execSqlCoro(
		"UPDATE user_frontier_gate_run SET run_rewards = $1 WHERE user_id = $2;",
		detail::joinIds(run.paid), identity.userId);

	co_return paid;
}

/*!
* Ends the user's run: pays anything it reached on the way out, records the
* gate's best score, pays the run's Merit Points and closes the run.
*
* MERIT POINTS (idfCDG70) are the Randall achieve-point currency — the balance
* RandallAchievementDedicateScene::setAchievePoint @0x1A39D58 prints from
* UserAchievementInfo +0x18, which is this same wire key.  Nothing in the data
* says how many a Frontier Gate run is worth: the gate MST's score parameters
* are all client-side scoring rates (GameUtils::setFrontierGateSetting feeds
* them to ChallengeBattleSetting) and its FixPt column is empty on every row.
* So this server pays one point per 100 points of the run's score, capped by
* DefineMst max_achieve_point (1JFcDr05 = 999,999).  That rate is a decided
* rule, not a decoded one — change it here if the real one turns up.
*/
inline drogon::Task<FrontierResult> finishFrontierRun(
	const db::Database database,
	const UserIdentity identity,
	FrontierRun run)
{
	constexpr int64_t kScorePerMeritPoint = 100;
	constexpr int64_t kMaxAchievePoint = 999'999LL;  // DefineMst 1JFcDr05

	co_await payFrontierRewards(database, identity, run);

	const auto merit = run.score / kScorePerMeritPoint;
	if (merit > 0)
	{
		co_await database->execSqlCoro(
			"UPDATE user_info SET achieve_point = MIN(achieve_point + $1, $2) WHERE id = $3;",
			merit, kMaxAchievePoint, identity.userId);
	}

	co_await database->execSqlCoro(
		"INSERT INTO user_frontier_gates (user_id, frogate_id, state, score) VALUES ($1, $2, 1, $3)"
		" ON CONFLICT(user_id, frogate_id) DO UPDATE SET state = MAX(state, 1), score = MAX(score, $3);",
		identity.userId, run.gateId, run.score);

	co_await database->execSqlCoro(
		"DELETE FROM user_frontier_gate_run WHERE user_id = $1;", identity.userId);

	// The result screen lists everything this run was paid, in threshold order.
	FrontierResult result{};
	result.tokensChanged = run.tokensChanged;
	result.ticketsChanged = run.ticketsChanged;
	const std::set<std::string> paid(run.paid.begin(), run.paid.end());
	int64_t zel = 0, karma = 0;
	for (const auto* reward : detail::frontierGateRewards(run.gateId))
	{
		if (!paid.contains(std::to_string(reward->reward_id)))
			continue;
		if (reward->present_type == 3)
			zel += std::max(reward->target_cnt, 1);
		else if (reward->present_type == 11)
			karma += std::max(reward->target_cnt, 1);
		result.rewards.push_back(detail::frontierRewardEntry(*reward));
	}

	result.end.grade_id = frontierGrade(run.score);
	result.end.zel = static_cast<int32_t>(zel);
	result.end.karma = static_cast<int32_t>(karma);
	result.end.score = static_cast<int32_t>(std::min<int64_t>(run.score, INT32_MAX));
	result.end.achieve_point = static_cast<int32_t>(std::min<int64_t>(merit, kMaxAchievePoint));
	result.end.bonus_rate = 1.0f;
	result.end.prestige_points = 0;

	LOG_INFO << "FrontierGate: run on gate " << run.gateId << " ended at " << run.progress
		<< " floor(s), " << run.score << " points, grade " << result.end.grade_id << ", "
		<< merit << " merit point(s), " << result.rewards.size() << " reward(s) this run";

	co_return result;
}

} // namespace gme
