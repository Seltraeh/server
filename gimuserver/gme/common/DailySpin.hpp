#pragma once

#include <gimuserver/db/Types.h>

#include <drogon/drogon.h>

#include <cstdint>
#include <string>

namespace gme
{
struct UserIdentity;
struct GrantedRewards;

/*!
* Per-user Daily Spin state (the Rewards menu's task_dailyloginspin tile).
*
* ⚠ READ THIS BEFORE CHANGING WHAT GOES OUT AS 35JXN4Ay.  `spinsUsed` is not a
* display counter — it decides whether the HOME SCREEN force-opens the wheel
* over itself:
*
*     DailyLoginRewardsUserInfo::isDailyLoginAvailable @0x1CC8278
*       return this->[0x1c] < 1;          // user_current_count < 1
*
* and its only callers are HomeScene2::updateEvent and
* AnotherHomeScene::updateEvent.  A value below 1 means "hasn't spun today", so
* Home opens the wheel; the wheel then only closes when
* `spinsUsed >= spinLimit` (DailyLoginScene::updateEvent state 1 @0xE52FDC,
* whose else-branch is state 7 = exit to Home).  Report 0 with a limit above 0
* and there is no way out of the wheel at all.  See handbook §7.14.
*/
struct DailySpinState
{
	/*!
	* Which day of the reward cycle the NEXT spin draws from, 1-based.
	* Advances once the day's spins are used up.
	*/
	int32_t spinDay = 1;

	/*!
	* Spins already taken TODAY.  Goes out as 35JXN4Ay.
	*/
	int32_t spinsUsed = 0;

	/*!
	* Days since the Unix epoch for the day `spinsUsed` refers to.  Stored as an
	* integer rather than a date string so the rollover test is an integer
	* compare and cannot be tripped by formatting or locale.
	*/
	int64_t lastSpinUtcDay = 0;

	/*!
	* The reward row the last spin landed on, echoed back as XIvaD6Jp.
	*/
	int32_t lastRewardId = 0;
};

/*!
* Spins allowed per day.
*
* The live game gave five, but only the FIRST was free — spins 2..5 each
* required watching a video ad.
*
* ⚠ RAISING THIS DOES NOT GRANT MORE USABLE SPINS.  Tried at 2 on
* 2026-08-21: the client renders the second spin as a "Bonus Ad Spin" button
* and refuses it with "There are no ads currently available for your region".
* The ad gate is the CLIENT's, and it does not consult this number — so a
* limit above 1 only advertises a spin the player can never take.  It stays at
* one, which is exactly the free-spin allowance a live player had.
*
* The consequence to accept: days 7 / 14 / 21 / 28 guarantee a Gem on the first
* spin of the day, so on this server those days ALWAYS pay the Gem and their
* other five prizes never come up.  That is faithful — a live player's free
* spin on those days was the guaranteed Gem too, and the rest of the row was
* only reachable by paying with an ad.
*/
inline constexpr int32_t kDailySpinLimit = 1;

/*!
* Number of spaces on the Daily Spin wheel.  Six, and fixed by the artwork.
*/
inline constexpr int32_t kDailySpinSpaces = 6;

/*!
* The group whose six spaces are all Gems, shown on a guaranteed-Gem day.
*
* Recovered from the client's own table (32 groups; see
* tools/gen_daily_spin_archive.py).  Pointing the client here on days
* 7/14/21/28 is what makes the wheel it DRAWS agree with the Gem it is about to
* be PAID — the alternative is a wheel showing six ordinary prizes that cannot
* come up.
*/
inline constexpr int32_t kDailySpinGemGroup = 32;

/*!
* Resolves an ever-increasing spin day to the table row it plays.
*
* The stored `spin_day` counts up forever, but the reward table is finite, so
* every caller that turns a day into EITHER a payout row OR a reward id has to
* go through here first, and both have to agree.  They did not: `rowForDay`
* clamped past the end of the table while `dailySpinAnchor` kept using the raw
* day, so from day 30 on the client drew its own day-N wheel (the id says which
* group) while the server paid day 29's row, and the gap widened daily.
* Observed at day 32 — reward id 191, i.e. day 32 space 5, paid out of day 29.
*
* ⚠ IT CLAMPS, IT DOES NOT WRAP, and that is what the source says.  The Global
* wiki's table — the same one every row of daily_login.json came from — ends
* `Day 28` and then `Day 29+`.  The last row is TERMINAL: a live player climbed
* days 1 to 28 and then sat on the Imp row for good, rather than starting the
* four weeks over.  This wrapped modulo the largest whole number of weeks until
* 2026-09-20, which both restarted a cycle the live game never restarted and
* made the last authored row unreachable.
*
* Clamping also retires the whole-weeks constraint the wrap needed: with no
* wrap there is nothing for the client's `day % 7` Gem counter to fall out of
* step with, so the table may be any length.
*
* @param day Stored spin day, 1-based and unbounded.
* @return Row to play, 1-based, never past the last authored day.
*/
int32_t dailySpinTableDay(int32_t day);

/*!
* The last authored day — the one that repeats from then on.
*
* @return Day number of the terminal row, or 0 when no table is loaded.
*/
int32_t dailySpinTerminalDay();

/*!
* The two id fields that decide which wheel the client DRAWS.
*/
struct DailySpinWheel
{
	/*! `XIvaD6Jp` — the prize last won.  Must resolve; see dailySpinWheel. */
	int32_t id = 0;

	/*! `outas79f` — the anchor of the group to draw now. */
	int32_t nextRewardId = 0;
};

/*!
* Picks the wheel to show a player who has not spun yet this session.
*
* ⚠ THE CLIENT CHOOSES BETWEEN THE TWO IDS BY GROUP, and this is the whole
* reason `id` is not simply today's anchor.  setupSpinWheelRewards @0xE52778:
*
*     row     = getObject(id)
*     nextRow = getObject(next_reward_id)
*     if (nextRow && row->getGroupType() != nextRow->getGroupType())
*         row = nextRow;          // draw the NEXT group
*
* So the pair means "you last won `id`; the wheel in front of you is
* `next_reward_id`'s group".  Sending today's anchor in BOTH made the groups
* match, which pinned the wheel to `id` — correct by accident while both were
* the same day, and wrong the moment they were not.
*
* ⚠ AND `id` MUST RESOLVE.  `row->getGroupType()` is called with no null check,
* so an id the table does not hold is a null dereference in the client — which
* is what a fresh save's `last_reward_id` of 0 would be.  This falls back to the
* wheel's own anchor in that case, which makes the groups match and draws the
* right thing anyway.
*
* @param state The caller's spin state.
* @return The pair to emit, already consistent with each other.
*/
DailySpinWheel dailySpinWheel(const DailySpinState& state);

/*!
* The wheel's guaranteed-Gem label, as the two fields that compose it.
*
* The client renders `counter` immediately followed by `message`, giving
* "7 day(s) more to guaranteed Gem!".  They are returned together because
* blanking one without the other leaves the label reading " day(s) more to
* guaranteed Gem!" with no number in front of it.
*/
struct DailySpinGemLabel
{
	/*! Goes out as `u8iD6ka7`.  Empty draws nothing. */
	std::string counter;

	/*! Goes out as `ZC0msu2L`.  Empty draws nothing. */
	std::string message;
};

/*!
* Builds the "N day(s) more to guaranteed Gem!" label for a spin day.
*
* ⚠ EMPTY MEANS "DRAW NOTHING", and both readers agree on it: `setMainWindow`
* @0xE52328 and `updateEvent` @0xE533B0 each measure the counter string and
* branch past the append when it is zero-length.  That is the only way to stop
* the label promising a Gem — and past the terminal day none is coming, because
* the repeating row does not guarantee one.  Leaving the old
* `(7 - day % 7) % 7` arithmetic in place would have had the wheel promise a
* Gem every seventh day forever and never pay one.
*
* Read off the table rather than hardcoded to 7/14/21/28: it scans forward for
* the next row flagged `guaranteed_gem_on_first_spin`, so editing which days
* guarantee a Gem moves the counter with them.  Reports 0 on a guaranteed day
* itself, which is what the arithmetic it replaces did.
*
* ⚠ BOTH EMITTERS MUST USE THIS.  Initialize and the DailyLogin handler each
* built this value themselves, and that duplication is exactly how the reward
* id came to be right in one and wrong in the other for a month.
*
* @param day Stored spin day, 1-based and unbounded.
* @return The label's two halves, both empty when no guaranteed Gem remains.
*/
DailySpinGemLabel dailySpinGemLabel(int32_t day);

/*!
* First reward id of the day `day` falls on — the anchor the client groups the
* wheel by.  The day is resolved to its table row here, so an unbounded stored
* `spin_day` is what callers are expected to pass.
*
* ⚠ `XIvaD6Jp` and `outas79f` are REWARD IDS, never day numbers.  The client
* resolves the id to a row, reads that row's group type, and draws the six
* spaces of THAT group (`setupSpinWheelRewards` @0xE52740), so putting a day
* number in either field silently draws the wrong day's prizes: sending 7
* rendered day 2's wheel, complete with a 200,000 Karma space day 7 does not
* have, while the spin scored against day 7.
*
* ⚠ THE IDS ARE 1-BASED.  Group N is ids (N-1)*6+1 .. N*6, read straight out of
* the client's cached table.  The server used 0-based until 2026-09-20, which
* named a space one earlier than the wheel drew and, at space 0, fell into the
* PREVIOUS group entirely: day 29's anchor came out as 168, the client read
* that as group 28 and drew Omni Frogs while the server paid Imps.
*
* ⚠ THE RESOLUTION IS DONE IN HERE, DELIBERATELY, rather than being left to
* callers.  It used to be theirs, the rule was written down twice, and it was
* still missed: Initialize kept passing the raw day for a month after the other
* two call sites were corrected.  A missed lookup does not fail loudly — the
* client just draws a group that belongs to no day.  Resolving here makes an
* out-of-range id unreachable instead of merely discouraged, and it is
* idempotent, so a caller that resolves first is still correct.
*
* @param day Spin day, 1-based; unbounded or already resolved, either works.
* @return Reward id of that day's first space — the one under the pointer.
*/
inline int32_t dailySpinAnchor(const int32_t day)
{
	return (dailySpinTableDay(day) - 1) * kDailySpinSpaces + 1;
}

/*!
* The same arithmetic addressed by GROUP rather than by day, with no clamping.
*
* Needed because `kDailySpinGemGroup` sits past the last DAY: `dailySpinAnchor`
* would resolve 32 to the terminal day 29 and hand back the Imp wheel on the
* one morning a Gem is promised.
*
* @param group Group number, 1-based, as the client's table numbers them.
* @return Reward id of that group's first space.
*/
inline int32_t dailySpinAnchorForGroup(const int32_t group)
{
	return (group - 1) * kDailySpinSpaces + 1;
}

/*!
* Loads the Daily Spin reward table from archive_root/daily_login.json.
*
* Call once during server setup; the table is authored data, not MST, so it is
* read straight from the archive (handbook §6.15 rule 3).
*
* @param archiveRoot Configured archive_root.
*/
void loadDailySpinArchive(const std::string& archiveRoot);

/*!
* Picks the space the wheel lands on, awards it, and records the reward id.
*
* Spaces are chosen at random, which is what the live game did.  On days
* 7 / 14 / 21 / 28 the first spin instead awards the guaranteed Gem.
*
* Prizes whose `available` flag is false are skipped with a warning: nine of
* the wiki's rewards have no id in the MST tables we hold.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param state Spin state; `lastRewardId` is set to the awarded space.
* @param granted Records any unit or item the spin created, so the reply can
*        refresh those client caches.
* @return Human-readable description of what was awarded, for logging.
*/
drogon::Task<std::string> awardDailySpin(
	db::Database database,
	const UserIdentity& identity,
	DailySpinState& state,
	GrantedRewards& granted);

/*!
* Reads the caller's spin state, rolling it over if the UTC day has advanced.
*
* Creates the row on first use.  The returned value is what the client should
* be told right now: after a rollover `spinsUsed` reads 0 even though the
* stored row still refers to yesterday, so callers can emit it directly.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @return The caller's current spin state.
*/
drogon::Task<DailySpinState> loadDailySpin(db::Database database, const UserIdentity& identity);

/*!
* Consumes one spin and persists the result.
*
* Does nothing when the day's spins are already used up — callers should emit
* the state unchanged so the client renders its exhausted state.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param state State to advance, mutated in place.
* @return true when a spin was actually consumed.
*/
drogon::Task<bool> consumeDailySpin(
	db::Database database,
	const UserIdentity& identity,
	DailySpinState& state);

} // namespace gme
