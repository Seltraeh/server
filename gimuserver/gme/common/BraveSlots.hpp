#pragma once

#include <gimuserver/db/Types.h>
#include <gimuserver/packets/all.hpp>

#include <drogon/drogon.h>

#include <cstdint>
#include <string>
#include <vector>

namespace gme
{
struct UserIdentity;

/*!
* The medal Brave Slots spends.
*
* Named by `b5yeVr61` in deploy/system/brave_slots.json, which reaches the
* client as `SlotgameInfo::setSlotUseMedal` and is looked up through
* `UserBraveMedalInfoList::getPossessionWithMedalID(std::string)` — so it is an
* ID, not a cost, despite the "UseMedal" name.
*/
inline constexpr const char* kBraveSlotMedalId = "1";

/*!
* Medals granted on the player's first visit to the machine.
*
* Offline there is nothing to earn them from: the live sources were Brave
* Points and events, both unbuilt.  A finite stock keeps the machine's cost
* meaningful while leaving room for a real source later.
*/
inline constexpr int32_t kBraveSlotSeedMedals = 100;

/*!
* Medals a single pull costs.
*/
inline constexpr int32_t kBraveSlotPullCost = 1;

/*!
* Loads the slot payout table from archive_root/brave_slots.json.
*
* @param archiveRoot Configured archive_root.
*/
void loadBraveSlotArchive(const std::string& archiveRoot);

/*!
* Reads the caller's medal balances, seeding the starter stock on first sight.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @return Rows ready to send under 6C0kzwM5.
*/
drogon::Task<std::vector<UserBraveMedalInfo>> loadBraveMedals(
	db::Database database,
	const UserIdentity& identity);

/*!
* Plays one pull: spends a medal, picks an outcome, and awards it.
*
* The SERVER decides the outcome and the client animates to it, so the reel
* positions in the reply are not decoration — they are what the machine stops
* on.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param result Filled with the outcome for the s8r5M6wI reply.
* @return false when the player cannot afford the pull, in which case nothing
*         is spent and nothing is awarded.
*/
drogon::Task<bool> playBraveSlot(
	db::Database database,
	const UserIdentity& identity,
	SlotgameResultInfo& result);

} // namespace gme
