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
*
* THREE, per the wiki: "You can use the Brave Slots by inserting 3 Raid
* Medals into the machine. Up to 10 spins can be done at once (costing 30
* Raid Medals)."  That 10 matches kBraveSlotMaxPulls read out of .rodata,
* which is a good independent check on both numbers.
*
* ⚠ This must stay in step with the cost half of `b5yeVr61` in
* deploy/system/brave_slots.json ("<medalId>@<cost>"): the CLIENT prices the
* spin from that field (calcSeqCount reads it at reel_info+0x378) while the
* server charges from this one, and if they disagree the machine quotes one
* price and bills another.
*/
inline constexpr int32_t kBraveSlotPullCost = 3;

/*!
* Most medals a player may hold.
*
* "A maximum of 9999 Raid Medals can be carried at a time." (wiki)
*/
inline constexpr int32_t kBraveSlotMedalCap = 9999;

/*!
* Medals gifted per day through the present box.
*
* Live, medals came from Raid Battle missions — a mode this server does not
* implement — so without a substitute source the machine is unplayable once
* the starter stock runs out.  120/day is 40 spins.
*/
inline constexpr int32_t kBraveSlotDailyGift = 120;

/*!
* Loads the slot payout table from archive_root/brave_slots.json.
*
* @param archiveRoot Configured archive_root.
*/
void loadBraveSlotArchive(const std::string& archiveRoot);

/*!
* receipt_type stamped on the daily medal gift, so the once-a-day check can
* find it again without a schema change.
*/
inline constexpr int32_t kBraveSlotDailyGiftReceiptType = 2;

/*!
* Queues the day's medal gift into the present box, once per UTC day.
*
* Live, medals came from Raid Battle — not implemented here — so the machine
* would otherwise be a one-time toy: the 100-medal starter stock is 33 spins
* at 3 apiece and nothing replaces it.
*
* Idempotent by construction: it looks for a gift of its own receipt_type
* stamped today rather than tracking state of its own, so calling it on every
* login (or twice in one) grants at most one.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @return true when a gift was queued by THIS call.
*/
drogon::Task<bool> grantDailyBraveMedals(
	db::Database database,
	const UserIdentity& identity);

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
