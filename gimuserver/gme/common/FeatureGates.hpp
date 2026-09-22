#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <vector>

// Feature gating -- the padlocks on the Home screen.
//
// `2375D38i` (FeatureGatingInfoResponse) rides UserInfo and is the ONLY source
// of the client's LevelGatingInfo list.  The predicate is
// FeatureGatingHandler::shouldGateLocked @0x1C87BDC:
//
//     for (g : levelGatingList)
//         if (g->getFeatureID() == featureId)
//             return UserTeamInfo::getLv() < g->getRequiredLevel();
//     return false;                      // NO ROW => UNLOCKED
//
// The fallthrough is what makes this safe to use sparingly: a feature with no
// row is open, so sending four rows locks four things and leaves the other
// eight gateable features -- and everything ungated -- exactly as they are.
// That is the whole policy: everything open from level 1 except the parts that
// are not built yet.
//
// WHICH ID IS WHICH, read off the call sites rather than guessed:
//
//   5, 15  ARENA.  HomeScene2::initialize +0x514 draws `home_locked_arena.png`
//          on the locked branch, and FeatureGatingHandler::setNewForArena
//          @0x1C89C84 marks BOTH ids, which is what pairs them.
//   14     RAID.  HomeScene2::initialize +0xe20; the locked branch swaps in
//          `home_win_raid_close.png`.
//   17     GUILD.  setNewForGuild @0x1C89CE0 names it, and the Home icon has a
//          `guild_home_icon_locked.png` sibling.
//
// The client already ships a locked sprite for each, so this draws the padlock
// the art was made for rather than blanking a button.
//
// COLOSSEUM has no id of its own and no Home button: it is entered through the
// arena battle path (ArenaBattleScene::setColosseumResultParam), so locking the
// Arena closes it.  IAP likewise has no gate and no Home sprite; `2375D38i` has
// nothing to say about it and `UserPurchaseInfo` is never sent, so there is no
// route to a purchase screen to begin with.
//
// TWO PROPERTIES OF THE LOCK worth keeping in mind before extending this:
//
//   * It compares PLAYER LEVEL and nothing else.  There is no field for "clear
//     mission X".  Because the server rebuilds this list per player on every
//     UserInfo, any condition can still be expressed -- emit an unreachable
//     level while it is unmet and drop the row once it is met -- but that is a
//     server-side trick, not something the packet models.
//   * The lock is SPRITE-ONLY.  Neither drawing path calls getRequiredLevel or
//     renders a label, so the number never reaches the player.  That is what
//     makes an unreachable level an honest way to say "not yet", rather than a
//     lie about a level that could be ground out.

namespace gme
{

/*!
* A level no player can reach, so a gate written with it reads as "not yet".
*
* Derived from the progression table rather than hardcoded: `user_level_mst`
* has one row per attainable level (999 here), so one past its end cannot be
* reached however the cap moves.  Falls back to a large constant if the table
* is missing, because a gate that silently became reachable would quietly
* re-open a feature that is not finished.
*/
inline uint32_t unreachableLevel()
{
	const auto& progression = theServer()->cache().initializeResp().progression;
	const auto cap = static_cast<uint32_t>(progression.size());
	return cap > 0 ? cap + 1 : 100000;
}

/*!
* The gate catalogue for `2375D38i`, sent with every UserInfo.
*
* One row per feature that is not finished.  Everything else is deliberately
* absent, which is how it stays unlocked from level 1 -- see the fallthrough in
* shouldGateLocked quoted above.
*
* `req_id` MUST be 1.  FeatureGatingHandler::addObj @0x1C8738C releases any row
* whose ReqID is not 1 before it ever becomes a LevelGatingInfo, so a wrong
* value here does not fail loudly: the rows go out, are dropped on arrival, and
* the feature stays open.
*/
inline std::vector<::FeatureGatingInfo> featureGates()
{
	const auto locked = unreachableLevel();

	// feature id, and a name for the log and for anyone reading a capture --
	// setFeatureName has no drawing path, so this is documentation on the wire.
	const struct { uint32_t id; const char* name; } kUnfinished[] = {
		{  5, "Arena" },
		{ 15, "Arena (second id; setNewForArena marks both)" },
		{ 14, "Raid" },
		// 17 GUILD is NO LONGER LISTED: founding a guild and inviting friends
		// into it works (net/guild.kdl, gme/handlers/Guild.cpp), so the padlock
		// would now be hiding a feature rather than an empty room.  The rest of
		// the guild set answers an empty body rather than closing the session --
		// see the GuildUnimplemented registrations.
	};

	std::vector<::FeatureGatingInfo> gates;
	gates.reserve(std::size(kUnfinished));
	for (const auto& entry : kUnfinished)
	{
		::FeatureGatingInfo row{};
		row.feature_id = entry.id;
		row.feature_name = entry.name;
		row.req_id = 1;              // level requirement; anything else is dropped
		row.req_value = locked;
		gates.push_back(std::move(row));
	}
	return gates;
}

} // namespace gme
