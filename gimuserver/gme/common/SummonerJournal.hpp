#pragma once

#include <gimuserver/db/Types.h>
#include <gimuserver/packets/all.hpp>

#include <drogon/drogon.h>

#include <string>

namespace gme
{
struct UserIdentity;

/*!
* Loads the authored Journal from archive_root/summoner_journal.json.
*
* @param archiveRoot Configured archive_root.
*/
void loadSummonerJournalArchive(const std::string& archiveRoot);

/*!
* Archive key of the Brave Slots mission ("Spin the brave medal slots 10
* times").  The FIRST mission wired to a real counter — the pattern the other
* 44 follow.
*/
inline constexpr const char* kJournalTaskBraveSlots = "brave_medal_slots";

/*!
* Archive keys for the missions that need a COUNTER because the action leaves
* no lasting trace to read back.
*
* Everything else the Journal asks about is derived from state instead (deck
* size, favourites, town levels, recipe crafts, quests cleared), which makes
* those retroactive — an account that already satisfies the condition shows
* the mission complete the first time the screen is opened.
*/
inline constexpr const char* kJournalTaskUnitFusion = "unit_fusion";
inline constexpr const char* kJournalTaskSellUnits = "sell_units";
inline constexpr const char* kJournalTaskEvolution = "evolution";
inline constexpr const char* kJournalTaskZelHarvest = "zel_harvest";
inline constexpr const char* kJournalTaskKarmaHarvest = "karma_harvest";
inline constexpr const char* kJournalTaskMetalKeys = "metal_key_collection";
inline constexpr const char* kJournalTaskJewelKeys = "jewel_key_collection";

/*!
* Adds to a mission's progress, creating the row on first sight.
*
* Clamped to the mission's target so a counter cannot run past what the
* Journal will ever ask for, and silently ignored when the archive has no such
* mission — a caller naming a task that was renamed should not throw at the
* point of the gameplay action it is counting.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param taskKey Archive `key` of the mission, e.g. kJournalTaskBraveSlots.
* @param amount How much to add.
*/
drogon::Task<void> addJournalProgress(
	db::Database database,
	const UserIdentity& identity,
	const std::string& taskKey,
	int32_t amount);

/*!
* receipt_type stamped on Journal mission rewards in the present box.
*/
inline constexpr int32_t kJournalRewardReceiptType = 3;

/*!
* Claims one finished mission: marks it claimed and queues its reward.
*
* Marks BEFORE paying, the same order Mystery Chest uses, so a reward that
* throws cannot leave the mission claimable again.  Refuses a mission that is
* not finished or is already claimed rather than paying twice.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param taskId The wire task_id being claimed (23DaiBpe).
* @return true when a reward was queued by THIS call.
*/
drogon::Task<bool> claimJournalTask(
	db::Database database,
	const UserIdentity& identity,
	const std::string& taskId);

/*!
* Claims every milestone the caller's points have reached.
*
* The 3a83iY3r request carries NO milestone id (createBody @0xE379E8 is
* identity + signal key only), so it means "give me everything I have earned"
* rather than naming one rung.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @return How many rungs were paid by THIS call.
*/
drogon::Task<uint32_t> claimJournalMilestones(
	db::Database database,
	const UserIdentity& identity);

/*!
* Builds the whole Summoner's Journal reply.
*
* The captured 32Gwida0 request carries no parameters at all — identity,
* signal key and the MST manifest — so the entire screen has to be assembled
* here from the archive plus this user's state.
*
* Progress is reported as 0 for every mission, because nothing feeds it yet
* (every archive row is `tracked: false`).  The screen renders correctly in
* that state; it simply cannot advance.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @return The populated response.
*/
drogon::Task<::SummonerJournalInfoResp> buildSummonerJournal(
	db::Database database,
	const UserIdentity& identity);

} // namespace gme
