#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <string>
#include <vector>

// Honor — the client's name for Friend Points (`FRIEND_POINT^Honor` in
// sgtext_en_338), the currency the Honor Summon spends.
//
// The sink was already built and reachable: gacha_category_mst row 2
// (`small_banner_03Honor.png`) points at gate 1000, gacha_mst gives gates 1000
// and 1001 a `J3stQ7jd` of 200, Gacha.cpp charges it against
// `user_info.friend_points`, and the archive gives gate 1000 a 33-unit pool.
// The SOURCE was missing entirely — nothing in this server ever added Honor, so
// the banner could only ever answer NOT_FREE_SUMMONS, "Insufficient Honor
// Points."
//
// Two things had to be true for a helper card to offer any:
//
//  1. THE ROW GATES.  `ReinforcementInfo::getFriendPoint` @0x126DDA8 returns 0
//     outright unless that row's own friend_point is >= 1.  The client stores
//     it obfuscated — setFriendPoint @0x126DEE4 draws a random byte r into
//     +0x30c and writes value^r to +0x308, and the getter re-XORs them — so a
//     zero really is zero rather than an uninitialised read.  FriendGet sent 0.
//
//  2. THE SINGLETON GIVES THE AMOUNT.  Past the gate the getter ignores the row
//     and asks `FriendInfoList::existTypeOK(row.user_id)` whether the helper is
//     a friend, then returns `FriendPointInfo::getFriendPointNum()` or
//     `getNormalPointNum()` — the `6e4b7sQt` block, which this server never
//     sent.  Its ctor @0x1260DD8 assigns both strings the literal "0", so every
//     card read "Honor +0" (REINFORCEMENT_SELECT_FRIEND_POINT, "Honor
//     +<param=num>", drawn by FriendListUtils::setFriendPoint @0x1172914).
//
// Both values are COMMA-SEPARATED LISTS on the wire, not numbers:
// getNormalPointNum(int i) @0x1260EF8 splits on "," and takes element i,
// falling back to element 0 when the list is shorter.  Everything that draws a
// helper card uses the no-arg overload, which is index 0; only
// SummonerReinforcementInfo::getFriendPoint passes a real index, and the
// Summoner cluster has no content folder in this drop.  One element is
// therefore the honest send — a longer list would be inventing an index scheme.

namespace gme
{

/*!
* Honor earned for borrowing a helper who IS on the player's friend list.
*
* AUTHORED POLICY.  Nothing in the data sets a rate: DefineMst has no Honor
* field (deYOowYJ, its only two-element CSV, is setInitSummonerArmID), and the
* help text says only that a friend is worth more —
* MST_HELP_SUBTOPIC_300_1_DESCRIPTION, "You can get more Honor Pts when you
* hire a friend as a Summoner Helper."  Chosen against the one number that IS
* recovered, the 200 the Honor Summon costs: four friend-helped quests to a
* pull, twenty with a stranger.
*/
inline constexpr int32_t kHonorPerFriendHelper = 50;

/*! Honor for a helper who is not a friend.  Authored — see above. */
inline constexpr int32_t kHonorPerNormalHelper = 10;

/*!
* The `6e4b7sQt` singleton: what one borrowed helper is worth.
*
* Returned as a one-element vector because the field is modelled as a list, but
* it is a SINGLETON on the client — FriendPointInfoResponse::readParam
* @0x13DA7CC writes straight into FriendPointInfo::shared() with no row-0
* clear.  Send this row whole; an empty array never reaches readParam at all
* and leaves the ctor's "0" standing.
*/
inline std::vector<::FriendPointInfo> friendPointInfo()
{
	::FriendPointInfo info = {};
	info.friend_point_num = std::to_string(kHonorPerFriendHelper);
	info.normal_point_num = std::to_string(kHonorPerNormalHelper);
	return std::vector<::FriendPointInfo>{ std::move(info) };
}

/*!
* Whether a MissionStart's `h7eY3sAK` names a helper that was actually borrowed.
*
* Captured starts carry exactly two shapes in `9Q1Lq5FS`: `"0"` for a solo run
* and the helper's user id otherwise (107 captures, 65 of them with the
* synthetic DecompFriend id).  Older logs also show the key absent and the
* empty string, so all three non-answers are treated alike.
*/
inline bool borrowedHelper(const std::string& reinforceUserId)
{
	return !reinforceUserId.empty() && reinforceUserId != "0";
}

/*!
* The original synthetic Summoner, `DecompFriend`, reserved user-unit id 999999.
*
* NO LONGER A HELPER.  FriendGet offers the real roster now, so this id is never
* borrowed and never reported by MissionStart.  It survives as the account the
* daily gift is sent FROM (gme::kGiftSenderId), and the roster's DCF / DEV / NEW
* id bands are all chosen to stay clear of it.
*/
inline constexpr const char* kSyntheticHelperUserId = "n9ZMPC0t";

/*!
* Honor owed for one completed mission.
*
* ⚠ THE CALLER MUST ASK THE ROSTER.  This used to decide "is a friend" by
* comparing the id to kSyntheticHelperUserId, back when FriendGet emitted
* exactly one synthetic helper.  It now emits the player's whole roster plus
* strangers, so that test answered NO for every real friend and quietly paid 10
* Honor for a card the client had drawn "Honor +50" on.
*
* Which rate the card promised is decided client-side by
* `FriendInfoList::existTypeOK` @0x12607CC -- it walks FriendInfoList for the
* user id and returns true only when that row's `friend_type` is exactly 1.
* FriendGet sends friend_type 1 for roster members and 0 for strangers, so
* `gme::isFriend()` against user_friends is the server-side question that
* matches it.  Anything looser is a silent mismatch in the player's favour or
* against it.
*
* @param reinforceUserId The helper recorded at MissionStart, or "" / "0".
* @param helperIsFriend  gme::isFriend() for that id, on this player's roster.
* @return The Honor to credit, or 0 when no helper was borrowed.
*/
inline int32_t honorForMission(const std::string& reinforceUserId, bool helperIsFriend)
{
	if (!borrowedHelper(reinforceUserId))
	{
		return 0;
	}
	return helperIsFriend ? kHonorPerFriendHelper : kHonorPerNormalHelper;
}

} // namespace gme
