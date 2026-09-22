#pragma once

#include <gimuserver/db/Types.h>

#include <drogon/drogon.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gme
{
struct UserIdentity;

/*!
* THE SIX TASK CODES THE CLIENT CAN TRACK.
*
* Recovered 2026-09-20 by disassembling every `DailyTaskMst::getTypeKey` xref
* and decoding the two-character comparisons.  ⚠ They are SSO strings, so they
* compile to immediates (`mov w8, #0x5641`) and a literal search of the binary
* finds NOTHING — that absence is not evidence.
*
*     AV  ArenaResultScene2 + ChallengeArenaResultScene   Arena / Hunter wins
*     QE  MissionResultScene                              ordinary quest clears
*     VV  MissionResultScene                              Vortex clears
*     CM  StepScene                                       craft / synthesis
*     UU  UnitDetailCommentScene                          EVOLUTION
*     PU  UnitMixPlayScene                                fusion
*
* ⚠ A CODE OUTSIDE THIS SET CANNOT COMPLETE.  The scenes above are the only
* places the client reads the key, so a task with any other code would display
* and never tick — which is worse than not offering it.
*
* ⚠ THE SCENE IS NOT ALWAYS THE MEANING.  `UU` reads its key in
* UnitDetailCommentScene, which suggested "look at a unit" — a thing the server
* cannot see.  The client's own string says otherwise: DAILYTASK_TYPE_UU_DESC
* is "Evolve any Units 3 Times", and evolution is begun from that screen.  The
* sgtext strings are what name these, not the xref.
*/
inline constexpr const char* kDailyTaskCodes[] = { "AV", "QE", "VV", "CM", "UU", "PU" };

/*!
* Whether the SERVER can observe the event this code counts.
*
* ⚠ A TASK THE SERVER CANNOT COUNT MUST NOT BE OFFERED.  Nothing reports
* progress, so an uncountable code would sit at [0/N] all day and never tick —
* worse than not being on the board.  The rotation is filtered through this.
*
* `AV` is the one that fails today: there is no arena battle-result handler, so
* a win never reaches the server.  Its MST row exists and is correct, so
* offering it becomes a one-line change the moment that handler lands.
*
* The other five are hooked where the event already is:
*   QE / VV  MissionEnd, on a WON clear   CM  ItemMix, per item crafted
*   PU       UnitMix                      UU  UnitEvo
*/
inline bool dailyTaskCountable(const std::string_view code)
{
	return code != "AV";
}

/*! How many tasks are offered at once.  Three tiles is what the screen draws. */
inline constexpr size_t kDailyTasksPerDay = 3;

/*!
* One task as the player currently stands against it.
*/
struct DailyTaskProgress
{
	std::string key;
	int32_t timesCompleted = 0;
	bool rewarded = false;
};

/*!
* Registers progress against whatever task code an event satisfies.
*
* ⚠ THE SERVER HAS TO COUNT THIS ITSELF.  The client ships only two daily-task
* requests — the read and the claim — and neither reports progress, so nothing
* tells the server a task advanced.  Every call site is therefore an existing
* handler that already knows the event happened (MissionEnd, UnitMix, the
* synthesis path, the arena result), and the count reaches the client through
* `DailyTaskMst::times_completed` on the next read.
*
* Does nothing when the code is not among today's offered tasks, so a call site
* can fire unconditionally.  Awards the task's Brave Points exactly once, on
* the transition to complete.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param code One of kDailyTaskCodes.
* @param amount How much the event is worth, normally 1.
*/
drogon::Task<void> advanceDailyTask(
	db::Database database,
	const UserIdentity& identity,
	std::string code,
	int32_t amount = 1);

/*!
* Today's three task codes for this player.
*
* ⚠ THE ROTATION IS OURS TO CHOOSE, and that is not a liberty — the task table
* is not in the decoded MST set and the client never requests it on the
* download channel, so it only ever reached a player over the response channel.
* The live server picked the day's tasks; this picks them the same way, seeded
* on the UTC day so the answer is stable for the whole day without storing it.
*
* @param utcDay Days since the epoch.
* @return kDailyTasksPerDay distinct codes.
*/
std::vector<std::string> dailyTaskRotation(int64_t utcDay);

/*!
* The two areas today's Quest Explorer task is scoped to.
*
* ⚠ `QE` IS NOT "clear any three missions".  The wiki is explicit — "Complete 3
* Missions in (any 2 randomly selected Areas)" — and the client's own string
* ends mid-sentence, "Complete 5 Mission in ", because it expects the areas to
* be named after it.  `task_area_ids` (a3011F8b) is the field that carries
* them, and it was going out empty.
*
* ⚠ AND IT MUST BE SCOPED TO WHAT THIS PLAYER CAN REACH.  Picked from the whole
* map it named Vilanciel on 2026-09-20 for a save that had never set foot in it,
* which is a task that cannot be completed — the exact failure dailyTaskCountable
* exists to prevent, arriving by a different route.  The pool is therefore the
* areas the player has ALREADY CLEARED something in, which is proof of reach
* rather than an inference about it.
*
* Seeded on the day like the rotation, so the pair is stable for the day without
* being stored — but per-player, because the pool is.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param utcDay Days since the epoch.
* @return Up to two area ids the player can reach.
*/
drogon::Task<std::vector<int32_t>> dailyTaskQuestAreas(
	db::Database database,
	const UserIdentity& identity,
	int64_t utcDay);

/*!
* The area a mission belongs to, or 0 when it is not on the quest map.
*
* Resolved mission -> dungeon -> area, because ServerCache drops the MissionMst
* rows (which carry area_id) after boot and keeps only the two indexes.
*
* @param missionId Mission id.
* @return Area id, or 0 for Vortex / Frontier Gate / Grand Quest content.
*/
int32_t missionAreaId(int32_t missionId);

/*!
* Reads this player's progress, rolling it over when the UTC day has advanced.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @return One entry per code in today's rotation, in rotation order.
*/
drogon::Task<std::vector<DailyTaskProgress>> loadDailyTasks(
	db::Database database,
	const UserIdentity& identity);

/*!
* Builds the two catalogue tables with this player's state stamped onto them.
*
* ⚠ THE SCREEN EMPTIES THESE BEFORE EVERY REQUEST, so every reply that touches
* the Brave Points screen has to send both — answering with the prize alone
* blanks the catalogue, exactly as an empty DailyTaskUserInfo reply once did.
*
* Per-user state rides ON the rows: `times_completed` is this task's progress
* and `brave_points` / `brave_points_total` repeat the player's balance on
* every task row, which is where the screen's two counters come from.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param tasks Filled with today's rotation, in order.
* @param prizes Filled with the whole prize catalogue plus claim counts.
*/
drogon::Task<void> fillDailyTaskTables(
	db::Database database,
	const UserIdentity& identity,
	std::vector<::DailyTaskMst>& tasks,
	std::vector<::DailyTaskPrizeMst>& prizes);

/*! Why a claim was refused, or Ok. */
enum class DailyTaskClaimResult
{
	Ok,
	UnknownPrize,
	NotEnoughPoints,
	AlreadyClaimed,
};

/*!
* Claims one prize, delivering it to the present box.
*
* The two halves of the catalogue behave differently and the MST row is what
* says which is which (`milestone_prize`):
*
*   * a MILESTONE is gated on `total_brave_points` reaching its threshold and
*     is claimable once — it spends nothing, because the total is a lifetime
*     counter and spending it would move every later milestone out of reach;
*   * a REGULAR prize spends `brave_points_cost` from `avail_brave_points` and
*     may be claimed up to `max_claim_count` times.
*
* @param database Transaction to use — the debit, the claim tick and the
*        present must land together.
* @param identity Resolved caller.
* @param prizeId DailyTaskPrizeMst.id from the request.
* @return Ok, or why it was refused.
*/
drogon::Task<DailyTaskClaimResult> claimDailyTaskPrize(
	db::Database database,
	const UserIdentity& identity,
	int32_t prizeId);

} // namespace gme
