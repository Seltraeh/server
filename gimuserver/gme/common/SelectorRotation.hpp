#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <ctime>
#include <string>
#include <vector>

// The weekly 6-star selector.
//
// Ten 6-star units are claimable each week, and the ten are THE SAME FOR
// EVERYONE.  That is the point: it is a shared event, so two people comparing
// notes on a Wednesday are looking at the same list and can talk about which
// units are up this week.
//
// NO STORED STATE.  The rotation is a pure function of the week number, so
// there is no table to keep in sync, nothing to migrate, no drift between
// players, and no way for a restart to lose anyone's place in the cycle.
//
// NO REPEATS UNTIL THE WHOLE POOL HAS CYCLED.  Every 6-star unit in the archive
// -- 330 of them -- sits in ONE fixed order, and each week takes the next ten,
// wrapping at the end.  A unit is therefore exactly 33 weeks from its own last
// appearance, always, and every week is exactly ten units.
//
// ⚠ THE ORDER IS FIXED, AND IT HAS TO BE.  Re-shuffling per cycle was tried
// first, to keep the order unpredictable; it cannot work.  Two independent
// permutations laid end to end will always put some unit near the end of one
// and near the start of the next -- a sweep found repeats two weeks apart, and
// seam-patching only moves the collision rather than removing it.  A single
// repeated order is the only arrangement where the guarantee holds, and it is
// also what "a predetermined pattern everyone is on" asks for: the community
// can build a calendar from it.
namespace gme
{

/*! How many units the selector offers each week.  AUTHORED. */
inline constexpr int32_t kWeeklySelectorSize = 10;

/*! The rarity the rotation draws from.  7-star and omni get their own tickets later. */
inline constexpr int32_t kWeeklySelectorRarity = 6;

/*!
* The selector this rotation drives.
*
* Its row in unit_selector_gacha_ticket_mst.json carries a placeholder pool;
* what actually ships is rewritten per reply by applyWeeklySelector, because the
* whole point is that it changes without anyone editing data.
*/
inline constexpr int32_t kWeeklySelectorId = 17;

/*! The summon gate that selector opens.  Must match deploy/archive/gacha.json. */
inline constexpr int32_t kWeeklySelectorGate = 19000;

/*! Team level at which a fresh account is given its first selector. */
inline constexpr int32_t kStarterSelectorLevel = 50;

/*! Index the 6-star pool once, after the unit archive is up. */
void loadSelectorRotation();

/*! How many units are in the rotation -- 0 until loadSelectorRotation runs. */
size_t selectorPoolSize();

/*!
* Whole weeks since the rotation epoch.
*
* ⚠ THE EPOCH IS A FRIDAY MIDNIGHT.  1970-01-01 was a THURSDAY, so dividing the
* raw Unix day count by 7 rolls the week over on a Thursday.  Shifting first
* puts the boundary on Friday 00:00 UTC, which is what the rotation is specified
* to do; getting this wrong is a silent off-by-one-day nobody notices until a
* Thursday.
*/
int64_t selectorWeekIndex(std::time_t now = std::time(nullptr));

/*!
* The ten units on offer in a given week, identical for every player.
*
* @param week Week index; defaults to the current one.
*/
std::vector<int32_t> weeklySelectorPool(int64_t week);

/*! This week's ten. */
std::vector<int32_t> weeklySelectorPool();

/*!
* Point the rotating selector at this week's ten before the catalogue ships.
*
* `UnitSelectorGachaMst.unit_pool` is what the picker draws, and the catalogue
* goes out on BOTH UserInfo and the summon screen, so both senders have to apply
* this or whichever refreshed last leaves the placeholder showing.
*
* @param catalogue The cache's selector list, copied for this reply.
*/
void applyWeeklySelector(std::vector<::UnitSelectorGachaMst>& catalogue);

/*!
* Give a fresh account its first selector once it reaches level 50.
*
* Delivered through the PRESENT BOX rather than straight into the ticket
* inventory so the player is told it arrived -- a ticket that appears silently
* in a menu they have never opened is a reward nobody notices.
*
* Guarded by user_info.starter_selector_granted so it pays exactly once, even
* though this runs on every UserInfo.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @return true when the ticket was granted by this call.
*/
drogon::Task<bool> grantStarterSelectorIfDue(
	const db::Database database,
	const UserIdentity identity);

} // namespace gme
