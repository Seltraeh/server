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
* The most pulls one SlotAction may play.
*
* `RandallSlotScene::SLOT_SEQUENCE_MAX` in .rodata @0x23652B0, read by
* `RandallSlotActionScene::calcSeqCount` @0x1A70998 — which takes
* min(SLOT_SEQUENCE_MAX, requested) and then walks it DOWN until the balance
* covers cost*N.  The client therefore never legitimately asks for more than
* this, and the value is mirrored rather than invented.
*/
inline constexpr int32_t kBraveSlotMaxPulls = 10;

/*!
* Plays `drawCount` pulls: spends a medal each, picks an outcome, awards it.
*
* The SERVER decides the outcome and the client animates to it, so the reel
* positions in the reply are not decoration — they are what the machine stops
* on.
*
* Every pull gets its own entry, because the client renders a multi-pull as a
* LIST: `RandallSlotResultListScene::setPrizeList` @0x1A72B44 loops over
* `SlotgameResultInfoList::getCount()`.  Returning one entry for a ten-medal
* pull would show one prize and silently pocket the rest.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param drawCount Pulls requested (d04gRmkE); clamped to what is affordable
*        and to kBraveSlotMaxPulls.
* @return One result per pull actually played, empty when none were
*         affordable — in which case nothing is spent and nothing is awarded.
*/
drogon::Task<std::vector<SlotgameResultInfo>> playBraveSlot(
	db::Database database,
	const UserIdentity& identity,
	int32_t drawCount);

} // namespace gme
