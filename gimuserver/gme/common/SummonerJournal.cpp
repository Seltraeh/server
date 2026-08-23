#include "SummonerJournal.hpp"

#include "Common.hpp"

#include <gimuserver/archive/archive.hpp>
#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/utils/JsonFile.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace gme
{

namespace
{

/*!
* The authored Journal, loaded once at startup.
*/
SummonerJournalArchive g_journal;

} // namespace

void loadSummonerJournalArchive(const std::string& archiveRoot)
{
	if (archiveRoot.empty())
	{
		LOG_WARN << "SummonerJournal: archive_root is empty, journal not loaded";
		return;
	}

	try
	{
		g_journal = LoadJson<SummonerJournalArchive>(archiveRoot, "summoner_journal.json");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "SummonerJournal: failed to load summoner_journal.json: " << e.what();
		return;
	}

	uint32_t total = 0;
	uint32_t untracked = 0;
	uint32_t unpayable = 0;
	uint32_t substituted = 0;
	for (const auto& task : g_journal.tasks)
	{
		total += task.points;
		if (!task.tracked)
		{
			++untracked;
		}
		if (task.present_type == 0)
		{
			++unpayable;
		}
		if (task.substituted)
		{
			++substituted;
		}
	}

	LOG_INFO << "SummonerJournal: loaded " << g_journal.tasks.size() << " mission(s) worth "
		<< total << " point(s) and " << g_journal.milestones.size() << " milestone(s)";

	// Said plainly at startup rather than left for someone to discover in the
	// client: the screen is real but inert, and roughly four fifths of the
	// rewards cannot be paid because this server has no such items or units.
	if (untracked > 0)
	{
		LOG_WARN << "SummonerJournal: " << untracked << " of " << g_journal.tasks.size()
			<< " mission(s) have no progress source yet — they display but cannot complete";
	}
	if (substituted > 0)
	{
		LOG_WARN << "SummonerJournal: " << substituted
			<< " mission reward(s) are STAND-INS — the entity the wiki names does not"
			   " exist here; reward_note records what each should have been";
	}
	if (unpayable > 0)
	{
		LOG_WARN << "SummonerJournal: " << unpayable
			<< " mission reward(s) are present_type 0 — the entity does not exist here,"
			   " so a claim would be left unclaimed rather than paid";
	}

	if (!g_journal.milestones.empty() && total < g_journal.milestones.back().points)
	{
		LOG_ERROR << "SummonerJournal: the missions are worth " << total
			<< " point(s) but the top milestone needs "
			<< g_journal.milestones.back().points << " — it can never be reached";
	}
}

drogon::Task<void> addJournalProgress(
	const db::Database database,
	const UserIdentity& identity,
	const std::string& taskKey,
	const int32_t amount)
{
	const auto task = std::find_if(g_journal.tasks.begin(), g_journal.tasks.end(),
		[&taskKey](const SummonerJournalTask& t) { return t.key == taskKey; });

	if (task == g_journal.tasks.end())
	{
		// Not fatal: the caller is in the middle of a gameplay action that has
		// nothing to do with the Journal, and a renamed archive key must not
		// take that action down with it.
		LOG_WARN << "SummonerJournal: no mission keyed '" << taskKey
			<< "' — progress dropped";
		co_return;
	}

	// Clamped to the target, so a counter cannot run past what the mission
	// will ever ask for and the [n/N] label can never read 14/10.
	co_await database->execSqlCoro(
		"INSERT INTO user_journal_tasks (user_id, task_key, progress)"
		" VALUES ($1, $2, MIN($3, $4))"
		" ON CONFLICT(user_id, task_key) DO UPDATE SET"
		" progress = MIN(progress + $3, $4);",
		identity.userId, taskKey, amount, static_cast<int32_t>(task->target));

	LOG_INFO << "SummonerJournal: " << identity.userId << " +" << amount
		<< " on '" << taskKey << "' (target " << task->target << ")";
	co_return;
}

namespace
{

/*!
* Storage key for a milestone's claim state.
*
* Milestones live in user_journal_tasks under a prefix rather than in a table
* of their own: the table is already a generic (user, key) -> progress/claimed
* counter, and five rungs do not justify a migration.
*/
std::string milestoneKey(const std::string& milestoneId)
{
	return "milestone:" + milestoneId;
}

} // namespace

drogon::Task<uint32_t> claimJournalMilestones(
	const db::Database database,
	const UserIdentity& identity)
{
	const auto journal = co_await buildSummonerJournal(database, identity);
	const int32_t points = journal.user_info.points;

	uint32_t paid = 0;
	for (const auto& milestone : g_journal.milestones)
	{
		if (points < static_cast<int32_t>(milestone.points))
		{
			continue;
		}

		const auto key = milestoneKey(milestone.milestone_id);
		const auto rows = (co_await db::DatabaseInterface::read(
			database,
			"user_journal_tasks",
			{
				db::Data("claimed"),
				db::Lookup("user_id", identity.userId),
				db::Lookup("task_key", key),
			})).data;

		if (!rows.empty() && rows[0]["claimed"].as<int32_t>() != 0)
		{
			continue;
		}

		// Marked first, then paid — same order as a task claim.
		co_await database->execSqlCoro(
			"INSERT INTO user_journal_tasks (user_id, task_key, progress, claimed)"
			" VALUES ($1, $2, $3, 1)"
			" ON CONFLICT(user_id, task_key) DO UPDATE SET claimed = 1;",
			identity.userId, key, static_cast<int32_t>(milestone.points));

		co_await gme::addUserPresent(
			database, identity,
			static_cast<int32_t>(milestone.present_type),
			milestone.target_id == 0 ? std::string{} : std::to_string(milestone.target_id),
			static_cast<int32_t>(milestone.target_cnt),
			kJournalRewardReceiptType,
			"Journal Milestone " + std::to_string(milestone.points));

		LOG_INFO << "SummonerJournal: " << identity.userId << " claimed milestone "
			<< milestone.points << " (" << milestone.reward_note << ")";
		++paid;
	}

	co_return paid;
}

drogon::Task<bool> claimJournalTask(
	const db::Database database,
	const UserIdentity& identity,
	const std::string& taskId)
{
	const auto task = std::find_if(g_journal.tasks.begin(), g_journal.tasks.end(),
		[&taskId](const SummonerJournalTask& t) { return t.task_id == taskId; });

	if (task == g_journal.tasks.end())
	{
		LOG_WARN << "SummonerJournal: claim for unknown task_id '" << taskId << "'";
		co_return false;
	}

	const auto rows = (co_await db::DatabaseInterface::read(
		database,
		"user_journal_tasks",
		{
			db::Data("progress"),
			db::Data("claimed"),
			db::Lookup("user_id", identity.userId),
			db::Lookup("task_key", task->key),
		})).data;

	const int32_t progress = rows.empty() ? 0 : rows[0]["progress"].as<int32_t>();
	const int32_t claimed = rows.empty() ? 0 : rows[0]["claimed"].as<int32_t>();

	if (progress < static_cast<int32_t>(task->target) || claimed != 0)
	{
		LOG_WARN << "SummonerJournal: refusing claim of '" << task->key
			<< "' — progress " << progress << "/" << task->target
			<< ", claimed " << claimed;
		co_return false;
	}

	// Marked BEFORE paying, like Mystery Chest: a reward that throws must not
	// leave the mission claimable a second time.
	co_await database->execSqlCoro(
		"UPDATE user_journal_tasks SET claimed = 1"
		" WHERE user_id = $1 AND task_key = $2;",
		identity.userId, task->key);

	// Straight to the present box, which is where the wiki says Journal
	// rewards land: "a reward that is sent directly to your Gift Box".
	co_await gme::addUserPresent(
		database, identity,
		static_cast<int32_t>(task->present_type),
		task->target_id == 0 ? std::string{} : std::to_string(task->target_id),
		static_cast<int32_t>(task->target_cnt),
		kJournalRewardReceiptType,
		task->name);

	LOG_INFO << "SummonerJournal: " << identity.userId << " claimed '" << task->key
		<< "' (+" << task->points << " pts), queued " << task->reward_note
		<< (task->substituted ? " [STAND-IN]" : "");
	co_return true;
}

drogon::Task<::SummonerJournalInfoResp> buildSummonerJournal(
	const db::Database database,
	const UserIdentity& identity)
{
	::SummonerJournalInfoResp resp{};

	// One read for all 45 missions; a mission that has never ticked has no row
	// and is absent here, which reads as 0.
	std::map<std::string, std::pair<int32_t, int32_t>> stored;
	for (const auto& row : (co_await db::DatabaseInterface::read(
		database,
		"user_journal_tasks",
		{
			db::Data("task_key"),
			db::Data("progress"),
			db::Data("claimed"),
			db::Lookup("user_id", identity.userId),
		})).data)
	{
		stored[row["task_key"].as<std::string>()] = {
			row["progress"].as<int32_t>(), row["claimed"].as<int32_t>()};
	}

	int32_t earned = 0;
	bool allTasksClaimed = true;
	bool allMilestonesClaimed = true;

	for (const auto& task : g_journal.tasks)
	{
		resp.tasks.push_back(::SummonerJournalTaskMst{
			.task_id = task.task_id,
			.name = task.name,
			.instructions = task.instructions,
			// Nothing gates a mission on Summoner level here, and the unlock
			// vocabulary is unverified, so all three stay 0 rather than
			// carrying a guess (§6.20).
			.locked_level = 0,
			.unlock_type = 0,
			.unlock_value = 0,
			// ⚠ These two are NOT what their setter names suggest, and the
			// first cut had them swapped.  Pinned from
			// setSummonerJournalList: the [0/N] label is built from
			// UserTaskInfo::getProgress() over TaskMst::getProgress()
			// (@0xE3B4DC-E3B4E8), while SJ_TASK_POINT — the "Journal Point
			// reward" line — is filled from TaskMst::getValue() (@0xE3BB4C).
			//
			// So progress is the TARGET and value is the POINT REWARD.
			// Swapping them printed "Journal Point reward: 50" on a 20-point
			// mission whose target was 50.
			.progress = static_cast<int32_t>(task.target),
			.value = static_cast<int32_t>(task.points),
			// moveToTaskScene @0xE3DD00 takes 1..9 and returns for anything
			// else, so an unset destination is an inert button, not a crash.
			.target_screen = static_cast<int32_t>(task.target_screen),
		});

		// ⚠ present_id IS THE REWARDED ENTITY, not an id for the reward row.
		// setSummonerJournalList @0xE3AEA0 draws the row icon with
		// PresentCommon::createThumbnailBack(getPresentType(), getPresentID()),
		// so for an item or unit reward that string goes straight into
		// ItemMstList/UnitMstList::getObject.
		//
		// Putting the TASK id here (and the entity in target_param) crashed the
		// screen the moment those 35 rows stopped being present_type 0:
		// getObject("1") returned null and the thumbnail read off it.  Currency
		// types (3 zel, 8 gem, 11 karma) hid it, because those draw a fixed
		// thumbnail and never look anything up — which is why the first cut,
		// where only the 10 currency rows had a real present_type, was fine.
		resp.rewards.push_back(::SummonerJournalRewardsMst{
			.task_id = task.task_id,
			.present_id = task.target_id == 0 ? std::string{} : std::to_string(task.target_id),
			.target_param = {},
			.message = task.reward_note,
			.present_type = static_cast<int32_t>(task.present_type),
			.target_cnt = static_cast<int32_t>(task.target_cnt),
		});

		const auto found = stored.find(task.key);
		const int32_t progress = found == stored.end() ? 0 : found->second.first;
		const int32_t claimed = found == stored.end() ? 0 : found->second.second;
		const bool complete = progress >= static_cast<int32_t>(task.target);

		if (claimed != 0)
		{
			earned += static_cast<int32_t>(task.points);
		}
		else
		{
			allTasksClaimed = false;
		}

		// Which BUTTON the row shows, from setSummonerJournalList @0xE3BCDC:
		//
		//     claim_status == 0                  -> receive_btn
		//     claim_status != 0, is_available 0  -> locked
		//     claim_status != 0, is_available !0 -> go_btn
		//
		// So claim_status 0 means "there is a reward waiting", NOT "unclaimed
		// so far" — sending 0 everywhere put Receive on 45 missions sitting at
		// zero progress.  A finished-but-unclaimed mission is the ONLY case
		// that should offer Receive.
		resp.user_tasks.push_back(::SummonerJournalUserTaskInfo{
			.user_id = identity.userId,
			.task_id = task.task_id,
			.progress = progress,
			.claim_status = (complete && claimed == 0) ? 0 : 1,
			.is_available = 1,
		});
	}

	for (const auto& milestone : g_journal.milestones)
	{
		resp.milestones.push_back(::SummonerJournalMilestoneMst{
			.milestone_id = milestone.milestone_id,
			.present_id = milestone.milestone_id,
			.target_id = milestone.target_id == 0
				? std::string{} : std::to_string(milestone.target_id),
			.target_param = {},
			.message = milestone.reward_note,
			.points = static_cast<int32_t>(milestone.points),
			.present_type = static_cast<int32_t>(milestone.present_type),
			.target_cnt = static_cast<int32_t>(milestone.target_cnt),
		});

		// ⚠ claim_status 0 means "this reward is WAITING TO BE TAKEN", the
		// same polarity as a task row — NOT "never claimed".  Sending 0 for
		// all five rungs told the client every milestone was ready, so one
		// task claim lit up all five chests and the screen announced the
		// Journal complete at 60/1000.
		const auto msFound = stored.find(milestoneKey(milestone.milestone_id));
		const bool msClaimed = msFound != stored.end() && msFound->second.second != 0;
		const bool msEarned = earned >= static_cast<int32_t>(milestone.points);

		resp.user_milestones.push_back(::SummonerJournalUserMilestoneInfo{
			.user_id = identity.userId,
			.milestone_id = milestone.milestone_id,
			.claim_status = (msEarned && !msClaimed) ? 0 : 1,
		});

		if (!msClaimed)
		{
			allMilestonesClaimed = false;
		}
	}

	// Sent here as well as on UserInfo/Initialize.  Those two are what make the
	// Rewards TILE exist at all (loadMenuList reads the flag before this screen
	// can be opened); this copy is what the screen itself reads for its points
	// header.
	resp.user_info.user_id = identity.userId;
	resp.user_info.points = earned;
	// Per the wiki the Journal is new-Summoner-only and "will disappear" once
	// every mission is done and every prize claimed, replaced by a red
	// treasure chest on Home.  The flag is what retires it — and it also gates
	// the Rewards TILE, so this is what makes the feature go away.
	resp.user_info.journal_flag = (allTasksClaimed && allMilestonesClaimed) ? 0 : 1;

	co_return resp;
}

} // namespace gme
