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
* Loads the Mystery Chest definitions from archive_root/mystery_chest.json.
*
* Call once during server setup.  Unlike the Daily Spin, whose catalogue the
* client already holds, chest contents are entirely ours: the client renders
* exactly what MysteryBoxResponse carries, so these definitions ARE the chests.
*
* @param archiveRoot Configured archive_root.
*/
void loadMysteryChestArchive(const std::string& archiveRoot);

/*!
* Grants a user the starter chests, once.
*
* The live game handed chests out through operator giveaways and events, which
* an offline server has no equivalent of.  Rather than invent an event system,
* every definition in the archive is granted on the player's first visit to the
* screen, dated from that moment.
*
* Idempotent: a chest is only granted when the user has no row for it at all,
* so an opened chest is never handed back.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
*/
drogon::Task<void> provisionMysteryChests(db::Database database, const UserIdentity& identity);

/*!
* Reads the caller's claimable chests, newest first.
*
* Excludes claimed chests and any whose expiry has passed — the client would
* draw an expired chest as a zero countdown, so filtering here keeps the two
* views in agreement.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @return Chests ready to send under d0ajLeRi.
*/
drogon::Task<std::vector<MysteryBoxInfo>> listMysteryChests(
	db::Database database,
	const UserIdentity& identity);

/*!
* Opens one chest: awards its contents and marks it claimed.
*
* Rewards go out through the shared present_type vocabulary, so this reuses the
* same primitives as CampaignReceipt rather than introducing another switch.
*
* @param database Database client or transaction to use.
* @param identity Resolved caller.
* @param boxId Chest to open, as sent in rEFRefr8.
* @param rewards Filled with what was awarded, for the CoAp2aph reply.
* @return true when a claimable chest matched and was opened.
*/
drogon::Task<bool> claimMysteryChest(
	db::Database database,
	const UserIdentity& identity,
	const std::string& boxId,
	std::vector<MysteryBoxRewardInfo>& rewards);

} // namespace gme
