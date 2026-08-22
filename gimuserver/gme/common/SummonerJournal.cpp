#include "SummonerJournal.hpp"

#include "Common.hpp"

#include <gimuserver/archive/archive.hpp>
#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/utils/JsonFile.hpp>

#include <string>

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

drogon::Task<::SummonerJournalInfoResp> buildSummonerJournal(
	const db::Database database,
	const UserIdentity& identity)
{
	::SummonerJournalInfoResp resp{};

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
			// The MST side of pG2n1A28 is the TARGET to reach; the user's
			// actual progress travels on SummonerJournalUserTaskInfo.
			.progress = static_cast<int32_t>(task.value),
			.value = static_cast<int32_t>(task.value),
			// GO-button destination.  Scene ids are unverified, so the button
			// is left inert rather than sent somewhere wrong.
			.target_screen = 0,
		});

		resp.rewards.push_back(::SummonerJournalRewardsMst{
			.task_id = task.task_id,
			.present_id = task.task_id,
			.target_param = task.target_id == 0 ? std::string{} : std::to_string(task.target_id),
			.message = task.reward_note,
			.present_type = static_cast<int32_t>(task.present_type),
			.target_cnt = static_cast<int32_t>(task.target_cnt),
		});

		// Every mission is offered and none is complete, because no counter
		// feeds progress yet.
		resp.user_tasks.push_back(::SummonerJournalUserTaskInfo{
			.user_id = identity.userId,
			.task_id = task.task_id,
			.progress = 0,
			.claim_status = 0,
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

		resp.user_milestones.push_back(::SummonerJournalUserMilestoneInfo{
			.user_id = identity.userId,
			.milestone_id = milestone.milestone_id,
			.claim_status = 0,
		});
	}

	// Sent here as well as on UserInfo/Initialize.  Those two are what make the
	// Rewards TILE exist at all (loadMenuList reads the flag before this screen
	// can be opened); this copy is what the screen itself reads for its points
	// header.
	resp.user_info.user_id = identity.userId;
	resp.user_info.points = 0;
	resp.user_info.journal_flag = 1;

	co_return resp;
}

} // namespace gme
