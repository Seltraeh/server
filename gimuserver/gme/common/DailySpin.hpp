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
* The live game gave five, but only the first was free — spins 2..5 each
* required watching a video ad.  There is no ad SDK offline (and the video-ad
* feature flags are deliberately off, see feature_check.kdl), so the free spin
* is the whole allowance and a larger number here would simply be unreachable.
*/
inline constexpr int32_t kDailySpinLimit = 1;

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
