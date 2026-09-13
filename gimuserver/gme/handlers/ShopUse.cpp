#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/HunterOrbs.hpp>

// ShopUse (xe8tiSf4 / qthMXTQSkz3KfH9R) — the client's generic "spend a
// currency on a shop action" endpoint.  Frontier Gate reaches it through the
// "You have no Hunter Orbs left / use 1 Gem to fully restore" prompt.
//
// Request: group 32ibWjFG, one node.  CAPTURED 2026-08-07 from a live Recover
// tap: {"60IsqxDt":"5","03UGMHxF":"1","5gXxT7LZ":"0","rA9jDCP5":"0"}
//   60IsqxDt = ShopUseType, the action discriminator
//   03UGMHxF = gem cost
//
// Response: no ShopUseResponse class exists in the binary, so this returns the
// refreshed team_info under fEi17cnx the way UnitSell/UnitMix do.  That reply
// IS the purchase as far as the client is concerned: ShopUseConnectScene only
// checks for an error and changes scene, and the energy, orb and friend setters
// have no caller outside UserTeamInfoResponse::readParam and the mission and
// result scenes.  A "{}" reply leaves every counter where it was.
//
// Every ShopUseType the client sets (setShopUseType call sites):
//   1   Restore Energy — the shop, every "not enough Energy" prompt
//   2/3 expand the unit / item box
//   4   restore Arena Orbs (ShopHelFightScene, ArenaTopScene)
//   5   restore Hunter Orbs (Frontier Gate / Frontier Hunter prompts)
//   7   expand friend capacity (ShopFriendExtScene)
//   10  Quest Repeat's Auto Energy Recovery
//   6 ShopBuyStampScene, 8 ColosseumTopScene, 9 UnitDetailVirtuallyInfoScene,
//   11 ShopSummonerItemBuyDetailScene, 12 SummonerUnitMakeScene and
//   103 VortexArenaTopScene are not handled — see the default branch.

namespace
{
// ShopUseType 1 / 10 — "Fully restore Energy" (MST_INFO_SHOP_HEL_ACTION_
// DESCRIPTION).  Type 1 is every energy prompt the client has: the shop's
// Restore Energy (ShopHelActionScene::touchBegan), the quest-select shortage
// prompt (MissionSelectShortageStaminaScene::touchBegan),
// GameScene::confirmAnswerYesEnergy and the campaign, trial and raid party
// screens.  Type 10 is Quest Repeat's Auto Energy Recovery
// (MissionResultRepeatCheckScene::recoverStamina).  All of them price the
// refill at DefineMst action_point_heal_count and send ext_cnt 0, and the shop
// refuses the tap outright while energy is full (SHOP_HEL_ACTION_MAX_ACTION).
constexpr int32_t kShopUseRestoreEnergy = 1;
constexpr int32_t kShopUseRepeatRestoreEnergy = 10;

// ShopUseType 2 / 3 — expand the unit box / item box: the shop's Unit / Item
// Capacity screens, and the Expand button on the "Over Unit Capacity" warning.
// ShopUnitBoxExtScene::confirmAnswerYes @0x1AD9984 and
// ShopItemBoxExtScene::confirmAnswerYes @0x1AD0528 call setShopUseType(2) and
// (3); ext_cnt (5gXxT7LZ) carries the slots — captured 5, 95 and 100.
constexpr int32_t kShopUseExpandUnitBox = 2;
constexpr int32_t kShopUseExpandItemBox = 3;

// Slots bought per step.  Both scenes' ctors load 5.0f as the divisor
// SetNeedDiaNum uses (need_dia_num = slots / 5 — unit box @0x1AD6ACC, item box
// @0x1ACD870), and DefineMst unit/item_box_ext_count is the gem price of ONE
// step (1 today).  That is how 95 slots arrived priced at 19 gems.
constexpr int32_t kBoxSlotsPerStep = 5;

// ShopUseType 4 — "fully restore your Arena Orbs" (SHOP_HEL_FIGHT_DESC_AREA).
// ShopHelFightScene::touchBegan compares getFightPoint with getMaxFightPoint
// (team_info YS2JG9no / 9m5FWR8q — the Arena Orbs, not the Hunter Orbs) and
// prices the restore at DefineMst fight_point_heal_count.
constexpr int32_t kShopUseRestoreArenaOrbs = 4;

// ShopUseType 5 — "fully restore your Hunter Orbs".
constexpr int32_t kShopUseRestoreHunterOrbs = 5;

// ShopUseType 7 — "You can use 1 Gem to increase your Friend capacity by 5
// Friends" (SHOP_FRIEND_EXT_MESSAGE).  ShopFriendExtScene::touchBegan sends
// ext_cnt 5 at DefineMst friend_ext_count gems, and stops selling once
// team_info's friend sum reaches the level's UserLevelMst friend_count +
// add_friend_count (SHOP_FRIEND_EXT_MAX_CNT).  So the level MST's
// add_friend_count is how many slots can be bought at that level, and
// user_info.max_friend_count holds how many were (see gme::getTeamInfo).
constexpr int32_t kShopUseExpandFriends = 7;
constexpr int32_t kFriendSlotsPerStep = 5;

// Buys `slots` more unit or item capacity.  The client's price is a claim
// (handbook §5), so the server charges its own, refuses anything that is not a
// whole number of steps or would pass DefineMst's cap (4000 units, 3200 items —
// the getMaxUnitCnt / getMaxWarehouseCnt the scenes clamp their slider with),
// and changes nothing on a refusal.  The caller's team_info reply
// resynchronises the client either way.
drogon::Task<void> expandBox(
    const gme::UserIdentity identity,
    const bool units,
    const int32_t slots,
    const int32_t claimedCost)
{
    const auto& defines = theServer()->cache().initializeResp().defines;
    const int32_t pricePerStep = units ? defines.unit_box_ext_count : defines.item_box_ext_count;
    const int32_t cap = units ? defines.max_unit_count : defines.max_warehouse_count;
    // The unit box's starting slots ride add_unit_count (gme::getTeamInfo), so
    // its column holds only what was bought; the item box's column holds it all.
    const std::string column = units ? "max_unit_count" : "max_warehouse_count";
    const int32_t base = units ? gme::kBaseUnitBoxSlots : 0;
    const char* box = units ? "unit" : "item";

    if (slots <= 0 || slots % kBoxSlotsPerStep != 0 || pricePerStep <= 0)
    {
        LOG_WARN << "ShopUse: refusing a " << box << "-box expansion of " << slots
                 << " slot(s) — not a whole number of " << kBoxSlotsPerStep << "-slot steps";
        co_return;
    }

    const int64_t price = static_cast<int64_t>(slots / kBoxSlotsPerStep) * pricePerStep;
    if (price != claimedCost)
    {
        LOG_WARN << "ShopUse: client priced " << slots << " " << box << " slot(s) at "
                 << claimedCost << " gem(s); charging the server's " << price;
    }

    const auto rows = co_await theDb()->execSqlCoro(
        "SELECT gems, " + column + " AS bought FROM user_info WHERE id = $1;",
        identity.userId);
    if (rows.empty())
        co_return;

    const auto gems = rows[0]["gems"].as<int64_t>();
    const auto capacity = base + rows[0]["bought"].as<int32_t>();
    if (capacity + slots > cap)
    {
        LOG_WARN << "ShopUse: " << box << " box at " << capacity << " cannot take "
                 << slots << " more (cap " << cap << ")";
        co_return;
    }
    if (gems < price)
    {
        LOG_WARN << "ShopUse: " << gems << " gem(s) cannot buy " << slots << " "
                 << box << " slot(s) at " << price;
        co_return;
    }

    // One statement that re-checks the balance, so two quick taps cannot spend
    // the same gems twice.  $N in strict first-appearance order (the sqlite
    // named-param gotcha — see CampaignReceipt), hence price bound twice.
    co_await theDb()->execSqlCoro(
        "UPDATE user_info SET gems = gems - $1, " + column + " = " + column + " + $2"
        " WHERE id = $3 AND gems >= $4;",
        price, slots, identity.userId, price);

    LOG_INFO << "ShopUse: " << box << " box " << capacity << " -> " << (capacity + slots)
             << " for " << price << " gem(s)";
}

// Fills energy to the level's maximum.  Energy is stored as a value plus the
// time it will be full, so the refill works on the DERIVED current value and
// clears the timer, as a level-up refill does (gme::UserEnergy::refresh).  It
// is refused while energy is already full or overcapped — the shop refuses
// that tap itself — and when the gems are short.
drogon::Task<void> restoreEnergy(const gme::UserIdentity identity, const int32_t claimedCost)
{
    const int32_t price = theServer()->cache().initializeResp().defines.action_point_heal_count;
    const auto rows = co_await theDb()->execSqlCoro(
        "SELECT level, energy, energy_full_ts, gems FROM user_info WHERE id = $1;",
        identity.userId);
    if (rows.empty() || price <= 0)
        co_return;

    const auto level = rows[0]["level"].as<uint32_t>();
    const auto storedEnergy = rows[0]["energy"].as<int64_t>();
    const auto energyFullTs = rows[0]["energy_full_ts"].as<int64_t>();
    const auto gems = rows[0]["gems"].as<int64_t>();
    const auto mst = gme::getLevelMst(level);
    if (!mst)
        co_return;

    auto energy = static_cast<uint32_t>(storedEnergy);
    gme::UserEnergy::derive(level, static_cast<uint64_t>(energyFullTs), energy);
    if (energy >= mst->energy)
    {
        LOG_WARN << "ShopUse: energy " << energy << "/" << mst->energy
                 << " is already full — nothing to restore";
        co_return;
    }
    if (price != claimedCost)
    {
        LOG_WARN << "ShopUse: client priced the energy refill at " << claimedCost
                 << " gem(s); charging the server's " << price;
    }
    if (gems < price)
    {
        LOG_WARN << "ShopUse: " << gems << " gem(s) cannot buy an energy refill at " << price;
        co_return;
    }

    // Guarded on the row just read, so a second tap or a mission start in
    // between cannot spend twice.  $N in first-appearance order, as above.
    const auto result = co_await theDb()->execSqlCoro(
        "UPDATE user_info SET gems = gems - $1, energy = $2, energy_full_ts = 0"
        " WHERE id = $3 AND gems >= $4 AND energy = $5 AND energy_full_ts = $6;",
        static_cast<int64_t>(price), static_cast<int64_t>(mst->energy), identity.userId,
        static_cast<int64_t>(price), storedEnergy, energyFullTs);
    if (result.affectedRows() == 0)
    {
        LOG_WARN << "ShopUse: energy row changed under the refill — nothing charged";
        co_return;
    }

    LOG_INFO << "ShopUse: energy " << energy << " -> " << mst->energy
             << " for " << price << " gem(s)";
}

// Fills the Arena Orbs to their cap.  Nothing on this server spends an orb
// yet, so the save is normally full and this charges nothing; the team_info
// reply still hands the client the server's count.
drogon::Task<void> restoreArenaOrbs(const gme::UserIdentity identity, const int32_t claimedCost)
{
    const int32_t price = theServer()->cache().initializeResp().defines.fight_point_heal_count;
    if (price <= 0)
        co_return;
    if (price != claimedCost)
    {
        LOG_WARN << "ShopUse: client priced the Arena Orb restore at " << claimedCost
                 << " gem(s); charging the server's " << price;
    }

    const auto result = co_await theDb()->execSqlCoro(
        "UPDATE user_info SET gems = gems - $1, fight_point = max_fight_point"
        " WHERE id = $2 AND gems >= $3 AND fight_point < max_fight_point;",
        static_cast<int64_t>(price), identity.userId, static_cast<int64_t>(price));
    if (result.affectedRows() == 0)
    {
        LOG_WARN << "ShopUse: Arena Orbs already full or " << price
                 << " gem(s) short — nothing charged";
        co_return;
    }

    LOG_INFO << "ShopUse: Arena Orbs restored for " << price << " gem(s)";
}

// Buys `slots` more friend capacity, up to what the player's level allows.
drogon::Task<void> expandFriends(
    const gme::UserIdentity identity,
    const int32_t slots,
    const int32_t claimedCost)
{
    const int32_t pricePerStep = theServer()->cache().initializeResp().defines.friend_ext_count;
    if (slots <= 0 || slots % kFriendSlotsPerStep != 0 || pricePerStep <= 0)
    {
        LOG_WARN << "ShopUse: refusing a friend expansion of " << slots
                 << " slot(s) — not a whole number of " << kFriendSlotsPerStep << "-slot steps";
        co_return;
    }

    const int64_t price = static_cast<int64_t>(slots / kFriendSlotsPerStep) * pricePerStep;
    if (price != claimedCost)
    {
        LOG_WARN << "ShopUse: client priced " << slots << " friend slot(s) at "
                 << claimedCost << " gem(s); charging the server's " << price;
    }

    const auto rows = co_await theDb()->execSqlCoro(
        "SELECT level, gems, max_friend_count AS bought FROM user_info WHERE id = $1;",
        identity.userId);
    if (rows.empty())
        co_return;

    const auto mst = gme::getLevelMst(rows[0]["level"].as<uint32_t>());
    if (!mst)
        co_return;

    const auto gems = rows[0]["gems"].as<int64_t>();
    const auto bought = rows[0]["bought"].as<int32_t>();
    if (bought + slots > mst->add_friend_count)
    {
        LOG_WARN << "ShopUse: " << bought << " friend slot(s) bought; level "
                 << mst->level << " allows " << mst->add_friend_count << " in all";
        co_return;
    }
    if (gems < price)
    {
        LOG_WARN << "ShopUse: " << gems << " gem(s) cannot buy " << slots
                 << " friend slot(s) at " << price;
        co_return;
    }

    const auto result = co_await theDb()->execSqlCoro(
        "UPDATE user_info SET gems = gems - $1, max_friend_count = max_friend_count + $2"
        " WHERE id = $3 AND gems >= $4 AND max_friend_count = $5;",
        price, slots, identity.userId, price, bought);
    if (result.affectedRows() == 0)
    {
        LOG_WARN << "ShopUse: friend row changed under the purchase — nothing charged";
        co_return;
    }

    LOG_INFO << "ShopUse: friend capacity " << (mst->friend_count + bought) << " -> "
             << (mst->friend_count + bought + slots) << " for " << price << " gem(s)";
}

// Hunter Orbs are the Frontier Hunter attempt currency, which Frontier Gate
// shares — confirmed by the in-game intro script
// (deploy/game_content/content/event/randall_FG.txt): "similar to when you join
// the land surveys in Frontier Hunter... you'll need Hunter Orbs to run
// Frontier Gate too."
//
// REPOINTED 2026-09-13.  This used to restore fight_point, which is the ARENA
// Orb count (ShopHelFightScene's title is "Arena Orbs") — the two recovery
// cadences in defines_mst, recover_time_fight 3600s and recover_time_frohun
// 10800s, were the hint that they are separate currencies, and the guess was
// left in as a live experiment.  The experiment is over: Hunter Orbs are
// ChallengeHeaderInfo::Aube, written only by the kN2i7qds response, and
// ChallengeUserTeamResponse::readParam @0x13D3F94 names the exact field.  So
// this now refills the real counter and leaves the Arena Orbs alone.
drogon::Task<void> restoreHunterOrbs(const gme::UserIdentity identity, const int32_t cost)
{
    const auto current = co_await db::DatabaseInterface::read(
        theDb(),
        "user_info",
        {
            db::Data("gems"),
            db::Lookup("id", identity.userId),
        });

    const auto gems = current.front<int64_t>("gems");

    if (gems < cost)
    {
        LOG_WARN << "ShopUse: user " << identity.userId << " has " << gems
                 << " gems, needs " << cost << " — refusing the restore";
        co_return;
    }

    co_await db::DatabaseInterface::update(
        theDb(),
        "user_info",
        {
            db::Data("gems", static_cast<int64_t>(gems - cost)),
            db::Lookup("id", identity.userId),
        });
    co_await gme::restoreHunterOrbsToCap(theDb(), identity);

    // The refreshed count reaches the client on the NEXT kN2i7qds — the Survey
    // Office hub or the Frontier Hunter lobby — because no ShopUse response
    // class exists and team_info has no field for this currency.  The prompt
    // that sent the player here re-enters through one of those, so the orb is
    // spendable immediately.
    LOG_INFO << "ShopUse: restored Hunter Orbs to " << gme::hunterOrbCap()
             << " for " << cost << " gem(s); " << (gems - cost) << " gem(s) remaining";
}
}

HANDLEF(ShopUse)
{
    LOG_INFO << "ShopUse: " << json;

    ::ShopUseReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "ShopUse: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    const int32_t useType = req.shop_use.shop_use_type;
    const int32_t cost = req.shop_use.cost;

    try
    {
        switch (useType)
        {
        case kShopUseRestoreEnergy:
        case kShopUseRepeatRestoreEnergy:
            co_await restoreEnergy(identity, cost);
            break;
        case kShopUseExpandUnitBox:
        case kShopUseExpandItemBox:
            co_await expandBox(identity, useType == kShopUseExpandUnitBox, req.shop_use.ext_cnt, cost);
            break;
        case kShopUseRestoreArenaOrbs:
            co_await restoreArenaOrbs(identity, cost);
            break;
        case kShopUseRestoreHunterOrbs:
            co_await restoreHunterOrbs(identity, cost);
            break;
        case kShopUseExpandFriends:
            co_await expandFriends(identity, req.shop_use.ext_cnt, cost);
            break;
        default:
            // Unknown action: acknowledge without touching state.  Guessing
            // which currency to deduct for an undecoded type is the §3.4
            // failure mode.
            LOG_WARN << "ShopUse: unhandled ShopUseType " << useType
                     << " (cost " << cost << ") — acknowledged without effect; "
                        "decode the type before implementing it";
            co_return HandleResult::success("{}");
        }
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        // Empty OK rather than a closed session (handbook §5).
        LOG_ERROR << "ShopUse: type " << useType << " failed: " << ex.base().what();
        co_return HandleResult::success("{}");
    }

    // Sent whether or not anything was bought: a refused purchase still puts
    // the client's counters back on the server's numbers.
    ::ShopUseResp resp{};
    resp.team_info = std::move((co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());
    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
