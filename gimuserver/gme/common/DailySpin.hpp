#pragma once

#include <gimuserver/db/Types.h>

#include <drogon/drogon.h>

#include <cstdint>
#include <string>

namespace gme
{
struct UserIdentity;

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
* @return Human-readable description of what was awarded, for logging.
*/
drogon::Task<std::string> awardDailySpin(
	db::Database database,
	const UserIdentity& identity,
	DailySpinState& state);

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
