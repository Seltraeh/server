#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/FeSkills.hpp>
#include <gimuserver/gme/common/HunterOrbs.hpp>
#include <gimuserver/gme/common/Transactions.hpp>

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
//   9   reset a unit's SP enhancements (UnitDetailVirtuallyInfoScene)
//   10  Quest Repeat's Auto Energy Recovery
//   6 ShopBuyStampScene, 8 ColosseumTopScene, 11 ShopSummonerItemBuyDetailScene,
//   12 SummonerUnitMakeScene and 103 VortexArenaTopScene are not handled —
//   see the default branch.

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

// ShopUseType 9 — reset a unit's SP enhancements (see resetFeSkills).
constexpr int32_t kShopUseResetFeSkills = 9;

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
    const auto capacity = static_cast<int64_t>(base) + rows[0]["bought"].as<int32_t>();
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
    const auto result = co_await theDb()->execSqlCoro(
        "UPDATE user_info SET gems = gems - $1, " + column + " = " + column + " + $2"
        " WHERE id = $3 AND gems >= $4 AND " + column + " <= $5;",
        price, slots, identity.userId, price, static_cast<int64_t>(cap) - base - slots);

    if (result.affectedRows() == 0) co_return;

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
    if (static_cast<int64_t>(bought) + slots > mst->add_friend_count)
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
// Authored offline price, matching the captured type-5 prompt: one gem.
// The client-provided cost is not an authority and cannot mint gems when negative.
drogon::Task<HandleResult> restoreHunterOrbs(const gme::UserIdentity identity)
{
    constexpr int64_t price = 1;
    std::string body;
    auto transaction = co_await theDb()->newTransactionCoro();
    try
    {
        const auto before = co_await gme::loadHunterOrbRefresh(transaction, identity);
        if (before.aube < gme::hunterOrbCap())
        {
            co_await transaction->execSqlCoro(
                "UPDATE user_info SET gems = gems - $1, hunter_orbs = $2, hunter_orb_rest_ts = 0"
                " WHERE id = $3 AND gems >= $4;", price, gme::hunterOrbCap(), identity.userId, price);
        }
        ::ShopUseResp resp{};
        resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
        resp.user_team = co_await gme::loadHunterOrbRefresh(transaction, identity);
        if (const auto error = glz::write_json(resp, body); error)
            throw std::runtime_error(glz::format_error(error, body));
    }
    catch (const std::exception& ex)
    {
        transaction->rollback();
        co_return HandleResult::error("Hunter Orb refill failed", ex.what());
    }
    co_return HandleResult::success(body);
}

// ShopUseType 9 — Reset on a unit's Enhancements screen.
//
// UnitDetailVirtuallyInfoScene::resetConnect @0x1BDEBDC sends type 9, the
// price DefineMst::getResetFeSkillDiaCnt (reset_fe_skill_dia_count, 5csFoG1G:
// 1 in this data) and the unit in edy7fq3L.  The client gates both ends:
// resetBtnSet @0x1BE2C88 disables the button while getFeUsedBP() is 0, and
// resetDialog @0x1BDF278 offers the reset only when UserTeamInfo's gems cover
// that price (the shop shortage dialog otherwise).  It changes nothing itself:
// after the reply, updateEvent's state 2 calls GameScene::updateHeader (gems
// from team_info) and drawUP, which re-resolves the unit by id and redraws
// getFeBP / getFeUsedBP / the acquired skills from the roster.  So the reply is
// team_info plus the COMPLETE roster (4ceMWH6k, full replace), FeSkillGet's
// shape.
//
// The reset: every spent SP back to available, no acquired skills, used 0,
// the cap untouched — available + used is preserved, nothing is granted —
// and the price charged once, all in one transaction that must commit before
// the reply goes out.
//
// A reset that has nothing to undo (used 0 and no skills: the client's own
// "disabled" state) charges nothing and just resynchronises, and so does one
// the balance cannot cover (the client's next tap then sees the shortage).
// That is what makes a retry safe: the body names the unit and the price and
// nothing else — no nonce, nothing about the unit's state — so a copy of a
// reset that went through finds the unit empty.  What the protocol cannot tell
// apart is a copy of reset #1 delayed until after the player bought skills
// again from a genuine reset #2: that copy resets again and charges again
// (the SP is refunded, never lost).  docs/CLAUDE_SESSION_6_HANDOFF.md.
drogon::Task<HandleResult> resetFeSkills(const gme::UserIdentity identity, const ::ShopUseEntry& use)
{
    const auto ownedId = gme::FeSkillSet::id(use.user_unit_id);
    if (!ownedId || *ownedId <= 0)
        co_return HandleResult::refuseToHome("Invalid enhancement reset.",
                                             "user unit id \"" + use.user_unit_id + "\"");

    const int32_t price = theServer()->cache().initializeResp().defines.reset_fe_skill_dia_count;
    if (price <= 0)
        co_return HandleResult::refuseToHome("Enhancement reset is unavailable.",
                                             "reset_fe_skill_dia_count " + std::to_string(price));
    if (price != use.cost)
    {
        LOG_WARN << "ShopUse: client priced the enhancement reset at " << use.cost
                 << " gem(s); charging the server's " << price;
    }

    std::string body;
    std::string cleared;
    int64_t refunded = 0;
    bool reset = false;
    auto tx = co_await theDb()->newTransactionCoro();
    try
    {
        const auto rows = co_await tx->execSqlCoro(
            "SELECT unit_id, fe_sp, fe_used_sp, fe_max_sp, fe_skill_info FROM user_units"
            " WHERE user_id = $1 AND user_unit_id = $2;", identity.userId, *ownedId);
        if (rows.empty())
        {
            tx->rollback();
            co_return HandleResult::refuseToHome("The selected unit is no longer available.",
                                                 "user unit " + std::to_string(*ownedId) + " not owned");
        }
        const auto& row = rows[0];

        // Only an Omni unit has an Enhancements screen (FeSkillGet's rule).
        const auto species = gme::FeSkillSet::id(row["unit_id"].as<std::string>());
        const UnitMst* unit = nullptr;
        for (const auto& candidate : theServer()->cache().unitMst())
            if (species && candidate.id == *species) { unit = &candidate; break; }
        if (!unit || unit->rarity < 8)
        {
            tx->rollback();
            co_return HandleResult::refuseToHome("This unit has no enhancements to reset.",
                                                 "species " + row["unit_id"].as<std::string>());
        }

        const auto available = row["fe_sp"].as<int64_t>();
        const auto spent = row["fe_used_sp"].as<int64_t>();
        const auto limit = row["fe_max_sp"].as<int64_t>();
        if (available < 0 || spent < 0 || limit < 0)
        {
            tx->rollback();
            co_return HandleResult::refuseToHome("This unit's enhancement data is invalid.",
                                                 "fe_sp/fe_used_sp/fe_max_sp " + std::to_string(available) + "/"
                                                 + std::to_string(spent) + "/" + std::to_string(limit));
        }
        cleared = row["fe_skill_info"].as<std::string>();
        const bool empty = spent == 0 && gme::FeSkillSet::parse(cleared).size() == 0;

        const auto balance = co_await tx->execSqlCoro(
            "SELECT gems FROM user_info WHERE id = $1;", identity.userId);
        const auto gems = balance.empty() ? int64_t{ 0 } : balance[0]["gems"].as<int64_t>();

        if (empty)
        {
            LOG_INFO << "ShopUse: user unit " << *ownedId << " has no enhancements to reset; nothing charged";
        }
        else if (gems < price)
        {
            LOG_WARN << "ShopUse: " << gems << " gem(s) cannot pay the " << price
                     << "-gem enhancement reset of user unit " << *ownedId << "; nothing changed";
        }
        else
        {
            // Guarded on the values just read, so a second copy racing this
            // one cannot refund or charge twice.  $N in first-appearance order.
            const auto paid = co_await tx->execSqlCoro(
                "UPDATE user_info SET gems = gems - $1 WHERE id = $2 AND gems >= $3;",
                static_cast<int64_t>(price), identity.userId, static_cast<int64_t>(price));
            const auto refundedRow = co_await tx->execSqlCoro(
                "UPDATE user_units SET fe_sp = $1, fe_used_sp = 0, fe_skill_info = ''"
                " WHERE user_id = $2 AND user_unit_id = $3 AND fe_sp = $4 AND fe_used_sp = $5;",
                available + spent, identity.userId, *ownedId, available, spent);
            if (paid.affectedRows() != 1 || refundedRow.affectedRows() != 1)
                throw std::runtime_error("ShopUse: unit or balance changed under the enhancement reset");
            refunded = spent;
            reset = true;
        }

        ::ShopUseResp resp{};
        resp.team_info = std::move((co_await gme::getTeamInfo(tx, identity)).nonEmpty());
        resp.unit_refresh = std::move((co_await db::PacketInterfaceFor<::UserUnitInfo>::read(
            tx, "user_units", { db::Lookup("user_id", identity.userId) })).data);
        if (const auto error = glz::write_json(resp, body); error)
            throw std::runtime_error(glz::format_error(error, body));
    }
    catch (const std::exception& ex)
    {
        tx->rollback();
        co_return HandleResult::refuseToHome("Enhancement reset failed. Please try again.", ex.what());
    }
    catch (...)
    {
        tx->rollback();
        co_return HandleResult::refuseToHome("Enhancement reset failed. Please try again.", "unknown exception");
    }

    if (!(co_await gme::CommitTransaction(std::move(tx))))
        co_return HandleResult::refuseToHome("Enhancement reset could not be saved. Please reconnect.",
                                             "commit failed");

    if (reset)
    {
        LOG_INFO << "ShopUse: reset user unit " << *ownedId << " (\"" << cleared << "\"), " << refunded
                 << " SP back to available, for " << price << " gem(s)";
    }
    co_return HandleResult::success(body);
}

}

HANDLEF(ShopUse)
{
    LOG_INFO << "ShopUse: " << json;

    ::ShopUseReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
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
            co_return co_await restoreHunterOrbs(identity);
        case kShopUseExpandFriends:
            co_await expandFriends(identity, req.shop_use.ext_cnt, cost);
            break;
        case kShopUseResetFeSkills:
            co_return co_await resetFeSkills(identity, req.shop_use);
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
