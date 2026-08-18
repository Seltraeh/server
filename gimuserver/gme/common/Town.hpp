#pragma once

#include <gimuserver/db/Types.h>

#include <drogon/drogon.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct UserTownFacilityInfo;
struct UserTownLocationInfo;
struct UserTownLocationDetail;
struct PermitReceipe;

namespace gme
{
struct UserIdentity;

/*!
* The town: resource tiles, facility levels, and the synthesis recipe list.
*
* The wire model this implements is written up in tools/TOWN_STATE_MODEL.md,
* reversed out of libgame.so.  Three facts drive every function here:
*
*  1. The SERVER pre-rolls a harvest period.  UserTownLocationDetail.drop_item_info
*     carries the whole period's loot as "<itemId>:<zel>:<karma>," per tap, and
*     MyTownTopScene::collectItem just replays entry `count - tap_cnt` locally.
*     TownUpdate therefore reports tap COUNTS only, and the server resolves the
*     same entries by index — there is no second roll and nothing to desync.
*  2. Unlocks are enforced CLIENT-side against the cleared-mission list, on
*     TownLocationMst / TownFacilityMst need_mission_id.  The server's job is to
*     ship a row for every tile and facility regardless — setLocationInfo skips
*     any location with no detail row, so withholding one hides the tile
*     permanently rather than locking it.
*  3. The synthesis menu is PermitRecipe (51yQrDBR) and nothing else.  The
*     recipe list scenes walk PermitRecipeInfoList, never the recipe master.
*/
class Town
{
public:
	/*!
	* Seconds in one harvest period.
	*
	* The client has no tile timer at all — UserTownLocationDetail::getStartdate
	* has zero callers — so the refresh clock is entirely ours.  Three hours is
	* the live game's value, documented in the client's own help text under
	* "Village of the Venturer > Gathering".
	*/
	static constexpr int64_t kHarvestPeriodSeconds = 3 * 60 * 60;

	/*!
	* Lowest level present in the level master for a facility.
	*
	* Not always 1: facilities 1 and 2 (sphere and item synthesis) start at lv 1,
	* but 3-6 only ever have a lv 0 row.  Seeding those at 1 makes
	* TownFacilityLvMstList::getObjectWithKey miss and the upgrade screen render a
	* blank detail panel.
	*
	* @param facilityId MST facility id.
	* @return Base level, or 1 when the facility has no level rows at all.
	*/
	static int32_t facilityBaseLevel(int32_t facilityId);

	/*!
	* Lowest level present in the level master for a resource tile.
	*
	* @param locationId MST location id.
	* @return Base level, or 1 when the location has no level rows at all.
	*/
	static int32_t locationBaseLevel(int32_t locationId);

	/*!
	* Whether a need_mission_id gate is satisfied.
	*
	* Mirrors MyTownTopScene::isOpen / setLocationInfo exactly: 0 means always
	* open, anything else must appear in the cleared-mission set.
	*
	* @param needMissionId Gate from TownFacilityMst / TownLocationMst.
	* @param clearedMissions Mission ids the player has cleared.
	* @return True when the facility or tile is unlocked.
	*/
	static bool isUnlocked(int32_t needMissionId, const std::set<int32_t>& clearedMissions);

	/*!
	* Reads back every facility row for UserInfo's YRgx49WG array.
	*
	* @param database Database client or transaction to use.
	* @param identity Resolved user identity.
	* @return One entry per provisioned facility.
	*/
	static drogon::Task<std::vector<::UserTownFacilityInfo>> facilityState(
		db::Database database,
		UserIdentity identity);

	/*!
	* Reads back every tile, rolling a fresh harvest period where one is due.
	*
	* Refreshing is lazy and happens here because UserInfo is the only moment the
	* client learns the tile state.  A tile whose gate is not yet satisfied still
	* gets a row (see the class note) but is never rolled, so nothing accrues
	* behind a lock.
	*
	* @param database Database client or transaction to use.
	* @param identity Resolved user identity.
	* @param clearedMissions Mission ids the player has cleared.
	* @param info Filled with UserInfo's yj46Q2xw array.
	* @param detail Filled with UserInfo's s8TCo2MS array, same cardinality.
	*/
	static drogon::Task<void> locationState(
		db::Database database,
		UserIdentity identity,
		std::set<int32_t> clearedMissions,
		std::vector<::UserTownLocationInfo>& info,
		std::vector<::UserTownLocationDetail>& detail);

	/*!
	* Applies a TownUpdate tap report.
	*
	* Consumes the pre-rolled entries the client already replayed — the ones at
	* [count - tap_cnt, count - tap_cnt + taps) — credits the items, zel and
	* karma they name, and decrements the tile's remaining taps.  A count larger
	* than the tile has left is clamped rather than rejected: the client is
	* replaying its own local decTapCnt, so a mismatch means a dropped packet,
	* not a cheat worth erroring over.
	*
	* @param database Database client or transaction to use.
	* @param identity Resolved user identity.
	* @param collectLog TownUpdate's 0mRaAo39, "<locationId>:<tapCnt>,...".
	*/
	static drogon::Task<void> applyTaps(
		db::Database database,
		UserIdentity identity,
		std::string collectLog);

	/*!
	* The synthesis recipes the player's facility levels have unlocked.
	*
	* Union of TownFacilityLvMst.release_receipe over every facility row at or
	* below its current level, which is how the level master expresses "this
	* upgrade unlocks these recipes".  Locked facilities contribute nothing.
	*
	* @param database Database client or transaction to use.
	* @param identity Resolved user identity.
	* @param clearedMissions Mission ids the player has cleared.
	* @return UserInfo's 51yQrDBR array.
	*/
	static drogon::Task<std::vector<::PermitReceipe>> permittedRecipes(
		db::Database database,
		UserIdentity identity,
		std::set<int32_t> clearedMissions);
};
}
