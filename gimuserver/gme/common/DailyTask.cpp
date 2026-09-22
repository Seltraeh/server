#include "DailyTask.hpp"

#include "App.hpp"
#include "Common.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <chrono>

namespace gme
{

namespace
{

// Grand Gaia areas only.  The special-mode id space (Vortex, Frontier Gate,
// Grand Quest) is above this and is not what "an Area" means on the quest map.
constexpr int32_t kDailyTaskSpecialIdFloor = 100000;

int64_t utcDayNow()
{
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::chrono::duration_cast<std::chrono::seconds>(now).count() / 86400;
}

/*!
* An area's display name, falling back to "Area <id>" when it has none.
*/
std::string areaName(const int32_t areaId)
{
	const auto& areas = theServer()->cache().areaMst();
	const auto it = std::find_if(areas.begin(), areas.end(),
		[areaId](const ::AreaMst& a) { return a.area_id == areaId; });
	if (it != areas.end() && !it->name.empty())
		return it->name;
	return "Area " + std::to_string(areaId);
}

/*!
* The MST row for a task code, or null when the table does not offer it.
*/
const ::DailyTaskMst* taskRow(const std::string& code)
{
	const auto& tasks = theServer()->cache().initializeResp().daily_tasks;
	const auto it = std::find_if(tasks.begin(), tasks.end(),
		[&code](const ::DailyTaskMst& row) { return row.key == code; });
	return it == tasks.end() ? nullptr : &*it;
}

} // namespace

std::vector<std::string> dailyTaskRotation(const int64_t utcDay)
{
	// Only codes the table actually describes can be offered: a rotation that
	// named a code with no row would draw a blank tile.
	std::vector<std::string> pool;
	for (const auto* code : kDailyTaskCodes)
	{
		if (dailyTaskCountable(code) && taskRow(code) != nullptr)
			pool.emplace_back(code);
	}
	if (pool.size() <= kDailyTasksPerDay)
		return pool;

	// A shuffle seeded on the day, so every request in the same day agrees
	// without the choice having to be stored anywhere.  Deliberately not
	// std::random_device: the point is reproducibility.
	auto seed = static_cast<uint64_t>(utcDay) * 6364136223846793005ULL + 1442695040888963407ULL;
	const auto next = [&seed]() {
		seed ^= seed << 13;
		seed ^= seed >> 7;
		seed ^= seed << 17;
		return seed;
	};
	for (size_t i = pool.size(); i > 1; --i)
		std::swap(pool[i - 1], pool[next() % i]);

	pool.resize(kDailyTasksPerDay);
	return pool;
}

drogon::Task<std::vector<int32_t>> dailyTaskQuestAreas(
	const db::Database database,
	const UserIdentity& identity,
	const int64_t utcDay)
{
	// ⚠ THE POOL IS WHERE THIS PLAYER HAS ALREADY BEEN.  Picking from the whole
	// map handed a level-25 save a task in Vilanciel, an area it had never
	// reached — uncompletable, and indistinguishable from a bug.  An area with
	// a cleared mission in it is reachable by construction.
	std::map<int32_t, int32_t> cleared;
	for (const auto& row : co_await database->execSqlCoro(
			"SELECT mission_id FROM user_campaign_missions"
			" WHERE user_id = $1 AND state = 2;",
			identity.userId))
	{
		int32_t missionId = 0;
		try { missionId = std::stoi(row["mission_id"].as<std::string>()); }
		catch (const std::exception&) { continue; }
		if (const auto areaId = missionAreaId(missionId); areaId > 0)
			++cleared[areaId];
	}

	std::vector<int32_t> pool;
	for (const auto& [areaId, count] : cleared)
	{
		(void)count;
		pool.push_back(areaId);
	}

	// A save with nothing cleared is mid-tutorial; give it the starting area
	// rather than nothing, so the tile still reads as a real objective.
	if (pool.empty())
	{
		for (const auto& area : theServer()->cache().areaMst())
		{
			if (area.area_id > 0 && area.area_id < kDailyTaskSpecialIdFloor && area.land_id > 0)
			{
				pool.push_back(area.area_id);
				break;
			}
		}
	}
	if (pool.size() <= 2)
		co_return pool;

	std::sort(pool.begin(), pool.end());
	auto seed = static_cast<uint64_t>(utcDay) * 2862933555777941757ULL + 3037000493ULL;
	const auto next = [&seed]() {
		seed ^= seed << 13;
		seed ^= seed >> 7;
		seed ^= seed << 17;
		return seed;
	};
	for (size_t i = pool.size(); i > 1; --i)
		std::swap(pool[i - 1], pool[next() % i]);
	pool.resize(2);
	std::sort(pool.begin(), pool.end());
	co_return pool;
}

int32_t missionAreaId(const int32_t missionId)
{
	const auto& cache = theServer()->cache();
	const auto dungeon = cache.missionQuestDungeon().find(missionId);
	if (dungeon == cache.missionQuestDungeon().end())
		return 0;
	const auto& dungeons = cache.dungeonMst();
	const auto it = std::find_if(dungeons.begin(), dungeons.end(),
		[&dungeon](const ::DungeonMst& d) { return d.dungeon_id == dungeon->second; });
	return it == dungeons.end() ? 0 : it->area_id;
}

drogon::Task<std::vector<DailyTaskProgress>> loadDailyTasks(
	const db::Database database,
	const UserIdentity& identity)
{
	const auto today = utcDayNow();
	const auto rotation = dailyTaskRotation(today);

	std::vector<DailyTaskProgress> out;
	out.reserve(rotation.size());
	for (const auto& code : rotation)
	{
		const auto rows = co_await database->execSqlCoro(
			"SELECT times_completed, utc_day, rewarded FROM user_daily_tasks"
			" WHERE user_id = $1 AND task_key = $2;",
			identity.userId, code);

		DailyTaskProgress progress{};
		progress.key = code;
		// A stale row is reported as zero rather than rewritten here: this is a
		// READ, and advanceDailyTask does the rollover on the write side.  Doing
		// it in both places would have two clocks to keep in step.
		if (!rows.empty() && rows[0]["utc_day"].as<int64_t>() == today)
		{
			progress.timesCompleted = rows[0]["times_completed"].as<int32_t>();
			progress.rewarded = rows[0]["rewarded"].as<int32_t>() != 0;
		}
		out.push_back(std::move(progress));
	}
	co_return out;
}

drogon::Task<void> advanceDailyTask(
	const db::Database database,
	const UserIdentity& identity,
	const std::string code,
	const int32_t amount)
{
	if (amount <= 0 || identity.userId.empty())
		co_return;

	const auto today = utcDayNow();
	const auto rotation = dailyTaskRotation(today);
	if (std::find(rotation.begin(), rotation.end(), code) == rotation.end())
		co_return;   // not offered today; nothing to advance

	const auto* row = taskRow(code);
	if (row == nullptr)
		co_return;

	// Roll the day over in the same statement that counts, so a task cannot be
	// advanced against yesterday's total.  `rewarded` clears with it.
	co_await database->execSqlCoro(
		"INSERT INTO user_daily_tasks (user_id, task_key, times_completed, utc_day, rewarded)"
		" VALUES ($1, $2, $3, $4, 0)"
		" ON CONFLICT(user_id, task_key) DO UPDATE SET"
		"   times_completed = CASE WHEN user_daily_tasks.utc_day = excluded.utc_day"
		"                          THEN user_daily_tasks.times_completed + excluded.times_completed"
		"                          ELSE excluded.times_completed END,"
		"   rewarded        = CASE WHEN user_daily_tasks.utc_day = excluded.utc_day"
		"                          THEN user_daily_tasks.rewarded ELSE 0 END,"
		"   utc_day         = excluded.utc_day;",
		identity.userId, code, amount, today);

	// Pay the Brave Points on the transition to complete, and only once.  The
	// UPDATE is its own guard: it matches only a row that is complete, today's,
	// and not yet paid, so a replayed event cannot pay twice.
	const auto paid = co_await database->execSqlCoro(
		"UPDATE user_daily_tasks SET rewarded = 1"
		" WHERE user_id = $1 AND task_key = $2 AND utc_day = $3"
		"   AND rewarded = 0 AND times_completed >= $4"
		" RETURNING times_completed;",
		identity.userId, code, today, row->task_count);
	if (paid.empty())
		co_return;

	co_await database->execSqlCoro(
		"UPDATE user_info SET total_brave_points = total_brave_points + $1,"
		" avail_brave_points = avail_brave_points + $1 WHERE id = $2;",
		row->task_brave_pts, identity.userId);

	LOG_INFO << "DailyTask: " << identity.userId << " completed " << code
		<< " (" << row->title << ") for " << row->task_brave_pts << " BP";

	// THE ALL-TASKS BONUS.  The client has the copy for it —
	// DAILYTASK_TASK_COMPLETE_BONUS "Bonus Completion Reward!" and
	// DAILYTASK_TASK_COMPLETE_BONUS2 "You have completed all Tasks for today!"
	// — and daily_task_bonus_mst carries the single figure it pays
	// (`bonus_brave_points`).  Nothing else in the game reads that table, so
	// this is what it is for.
	//
	// Counted off `rewarded` rather than a flag of its own: the day's tasks are
	// exactly the rotation, and every one of them has been paid iff all of
	// their rows are rewarded for today.  The row just written is included, so
	// this fires on the LAST completion and only then.
	const auto rotation2 = dailyTaskRotation(today);
	const auto done = co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n FROM user_daily_tasks"
		" WHERE user_id = $1 AND utc_day = $2 AND rewarded = 1;",
		identity.userId, today);
	if (!done.empty()
		&& done[0]["n"].as<size_t>() == rotation2.size()
		&& !rotation2.empty())
	{
		const auto bonus = theServer()->cache().initializeResp().daily_task_bonuses.bonus_brave_points;
		if (bonus > 0)
		{
			co_await database->execSqlCoro(
				"UPDATE user_info SET total_brave_points = total_brave_points + $1,"
				" avail_brave_points = avail_brave_points + $1 WHERE id = $2;",
				bonus, identity.userId);
			LOG_INFO << "DailyTask: " << identity.userId
				<< " cleared every task today — bonus " << bonus << " BP";
		}
	}
	co_return;
}

drogon::Task<void> fillDailyTaskTables(
	const db::Database database,
	const UserIdentity& identity,
	std::vector<::DailyTaskMst>& tasks,
	std::vector<::DailyTaskPrizeMst>& prizes)
{
	const auto& cache = theServer()->cache().initializeResp();

	int32_t available = 0;
	int32_t lifetime = 0;
	if (const auto rows = co_await database->execSqlCoro(
			"SELECT total_brave_points, avail_brave_points FROM user_info WHERE id = $1;",
			identity.userId);
		!rows.empty())
	{
		lifetime = rows[0]["total_brave_points"].as<int32_t>();
		available = rows[0]["avail_brave_points"].as<int32_t>();
	}

	tasks.clear();
	for (const auto& progress : co_await loadDailyTasks(database, identity))
	{
		const auto* row = taskRow(progress.key);
		if (row == nullptr)
			continue;
		::DailyTaskMst out = *row;
		out.times_completed = progress.timesCompleted;

		// Quest Explorer names the two areas it is scoped to, both in
		// `task_area_ids` and in the sentence — the client's own string stops
		// at "Complete N Mission in " and expects them to follow.
		if (out.key == "QE")
		{
			const auto areas = co_await dailyTaskQuestAreas(database, identity, utcDayNow());
			std::string ids;
			std::string names;
			for (const auto areaId : areas)
			{
				if (!ids.empty()) { ids += ','; names += " & "; }
				ids += std::to_string(areaId);
				names += areaName(areaId);
			}
			out.area_id = ids;
			if (!names.empty())
				out.desc = "Complete " + std::to_string(out.task_count) + " Missions in " + names;
		}
		// Repeated on every row on purpose — this is where the screen's "BP
		// Total" and "Current" headers read from, not from a separate block.
		out.brave_points = available;
		out.brave_points_total = lifetime;
		tasks.push_back(std::move(out));
	}

	std::map<int32_t, int32_t> claimed;
	for (const auto& row : co_await database->execSqlCoro(
			"SELECT prize_id, claim_count FROM user_daily_task_claims WHERE user_id = $1;",
			identity.userId))
	{
		claimed[row["prize_id"].as<int32_t>()] = row["claim_count"].as<int32_t>();
	}

	prizes = cache.daily_task_prizes;
	for (auto& prize : prizes)
	{
		const auto it = claimed.find(prize.id);
		prize.current_claim_count = it == claimed.end() ? 0 : it->second;
	}
	co_return;
}

drogon::Task<DailyTaskClaimResult> claimDailyTaskPrize(
	const db::Database database,
	const UserIdentity& identity,
	const int32_t prizeId)
{
	const auto& catalogue = theServer()->cache().initializeResp().daily_task_prizes;
	const auto it = std::find_if(catalogue.begin(), catalogue.end(),
		[prizeId](const ::DailyTaskPrizeMst& row) { return row.id == prizeId; });
	if (it == catalogue.end())
	{
		LOG_WARN << "DailyTaskClaimReward: " << identity.userId
			<< " asked for prize " << prizeId << ", which is not in the catalogue";
		co_return DailyTaskClaimResult::UnknownPrize;
	}
	const auto& prize = *it;

	const auto held = co_await database->execSqlCoro(
		"SELECT claim_count FROM user_daily_task_claims WHERE user_id = $1 AND prize_id = $2;",
		identity.userId, prizeId);
	const auto already = held.empty() ? 0 : held[0]["claim_count"].as<int64_t>();
	if (already >= static_cast<int64_t>(prize.max_claim_count))
		co_return DailyTaskClaimResult::AlreadyClaimed;

	if (prize.milestone_prize)
	{
		// Gated on the LIFETIME total, and spends nothing.  Checked in the same
		// statement that would have spent, so a concurrent claim cannot slip
		// between the test and the tick.
		const auto rows = co_await database->execSqlCoro(
			"SELECT total_brave_points FROM user_info"
			" WHERE id = $1 AND total_brave_points >= $2;",
			identity.userId, prize.brave_points_cost);
		if (rows.empty())
			co_return DailyTaskClaimResult::NotEnoughPoints;
	}
	else
	{
		const auto spent = co_await database->execSqlCoro(
			"UPDATE user_info SET avail_brave_points = avail_brave_points - $1"
			" WHERE id = $2 AND avail_brave_points >= $1 RETURNING avail_brave_points;",
			prize.brave_points_cost, identity.userId);
		if (spent.empty())
			co_return DailyTaskClaimResult::NotEnoughPoints;
	}

	co_await database->execSqlCoro(
		"INSERT INTO user_daily_task_claims (user_id, prize_id, claim_count) VALUES ($1, $2, 1)"
		" ON CONFLICT(user_id, prize_id) DO UPDATE SET claim_count = claim_count + 1;",
		identity.userId, prizeId);

	// To the PRESENT BOX, not granted inline — one reward vocabulary for every
	// payout path, and `present_type` here is already that vocabulary.
	co_await gme::addUserPresent(
		database, identity, prize.present_type,
		prize.reward_id == 0 ? std::string{} : std::to_string(prize.reward_id),
		prize.reward_count, 0,
		prize.title.empty() ? std::string{ "Brave Points reward" } : prize.title);

	LOG_INFO << "DailyTaskClaimReward: " << identity.userId << " claimed prize " << prizeId
		<< " (" << prize.title << ") "
		<< (prize.milestone_prize ? "at milestone " : "for ")
		<< prize.brave_points_cost << " BP";
	co_return DailyTaskClaimResult::Ok;
}

} // namespace gme
