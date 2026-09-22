#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <string>

// The login advent calendar -- the grid of numbered cells on Home that pays a
// prize for each distinct day the player logs in.
//
// WHY IT WAS DEAD.  Three separate things were missing, and each one alone was
// enough to produce "day 1 is the only claimable cell and it grants nothing":
//
//  1. `InitializeResp.campaignInfo` (`3da6bd0a`) was never populated, so
//     `UserLoginCampaignInfo::shared()` kept its constructed defaults and the
//     calendar believed the player was on day 0.
//  2. `UserLoginCampaignInfo` (5fc8bf2c), the 12-hour re-check, replied `{}`
//     with a comment saying no campaign runs offline.
//  3. Nothing ever granted a prize.  It could not have: the client's
//     `LoginCampaignRewardObject::claimObject` @0x1CB14CC is PURE ANIMATION --
//     CCMoveBy, CCScaleTo, CCSpawn, CCSequence and nothing else.  It sends no
//     request and reports no claim.
//
// That third point is the load-bearing one.  The calendar does not ask for its
// prize, it CELEBRATES a prize the server has already banked.  So the server
// decides when a day turns over, grants it, and merely tells the client which
// cell to light and whether to play the animation.
//
// `first_for_the_day` (`4tswNoV9`) is what triggers that animation, and
// `LoginCampaignRewardObject::claimObject` reads it through
// `UserLoginCampaignInfo::getFirstForTheDay` @0x1CB16EC.  Sending it true on a
// reply that did NOT advance the day would replay the flourish for a prize
// already collected, so it is true exactly once per new day.
namespace gme
{

/*! One prize, as authored in deploy/archive/login_campaign.json. */
struct LoginCampaignPrizeRow
{
	std::string kind;
	std::string name;
	int32_t id = 0;
	int32_t count = 0;
};

/*! One cell of the calendar. */
struct LoginCampaignDayRow
{
	int32_t day = 0;
	std::vector<LoginCampaignPrizeRow> rewards;
};

/*! Where the player has got to, and whether this call moved them. */
struct LoginCampaignState
{
	int32_t current_day = 0;
	int32_t total_days = 0;
	bool first_for_the_day = false;
};

/*!
* The campaign id the MST declares.
*
* One campaign ships (`login_campaign_mst.json`, id 1).  The client matches the
* id it is told against the MST it loaded; a mismatch draws an empty grid.
*/
inline constexpr int32_t kLoginCampaignId = 1;

void loadLoginCampaignArchive(const std::string& archiveRoot);

/*! The authored calendar, empty until the archive loads. */
const std::vector<LoginCampaignDayRow>& loginCampaignDays();

/*!
* Advance the calendar if this is a new day, granting that day's prizes.
*
* Idempotent within a day: the day key is stored, so the second and later calls
* on the same date return the same cell with first_for_the_day false and grant
* nothing.  That matters because both Initialize and the 12-hour re-check land
* here, and a player who relaunches four times must not collect four prizes.
*
* THE CALENDAR STOPS AT THE LAST CELL rather than wrapping.  Wrapping would
* make the grid an infinite Gem faucet, and the live campaign was a fixed run
* that simply ended.  Once the last day is reached the player keeps the grid,
* fully lit, and stops being paid.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @return The cell to draw and whether the animation should play.
*/
drogon::Task<LoginCampaignState> advanceLoginCampaign(
	const db::Database database,
	const UserIdentity identity);

/*!
* Build the wire block for `3da6bd0a` / the 5fc8bf2c reply.
*
* A SINGLETON on both sides: the field serialises through
* pkg::glaze::single_array, and readParam @0x1C70D74 writes straight into
* UserLoginCampaignInfo::shared() with no row-0 clear.
*/
::UserLoginCampaignInfo loginCampaignInfo(const LoginCampaignState& state);

} // namespace gme
