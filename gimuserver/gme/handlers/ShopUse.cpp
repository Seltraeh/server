#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Common.hpp>

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
// refreshed team_info under fEi17cnx the way UnitSell/UnitMix do — that is how
// the client's counters update without a full UserInfo round trip.

namespace
{
// ShopUseType 5 — "fully restore your Hunter Orbs".  The only value captured so
// far.  Every other action this endpoint can perform (energy refill, warehouse
// expansion, inventory slots) arrives here under a type we have NOT decoded, so
// anything else is logged and ignored rather than guessed at.
constexpr int32_t kShopUseRestoreHunterOrbs = 5;

// Hunter Orbs are the Frontier Hunter attempt currency, which Frontier Gate
// shares — confirmed by the in-game intro script
// (deploy/game_content/content/event/randall_FG.txt): "similar to when you join
// the land surveys in Frontier Hunter... you'll need Hunter Orbs to run
// Frontier Gate too."
//
// // UNVERIFIED which wire field the client actually reads for the orb count.
// UserTeamInfo::fight_point / max_fight_point are the best candidates by name
// and are what this restores, BUT the client still reported "no Hunter Orbs"
// while the wire carried fight_point=3/max_fight_point=3, so they may not be
// the counter the Frontier Gate screen checks.  defines_mst has TWO recovery
// cadences — recover_time_fight (3600s) and recover_time_frohun (10800s) —
// which suggests fight points and Hunter Orbs are separate currencies and the
// orb counter is a frohun-side value not yet located on the wire.
//
// Restoring fight_point here is therefore both the honest best guess and a
// direct experiment: if the prompt stops appearing after a Recover, fight_point
// was the right field; if it does not, the counter is elsewhere and this
// handler needs repointing rather than rewriting.
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

    if (useType != kShopUseRestoreHunterOrbs)
    {
        // Unknown action: acknowledge without touching state.  Guessing which
        // currency to deduct for an undecoded type is the §3.4 failure mode.
        LOG_WARN << "ShopUse: unhandled ShopUseType " << useType
                 << " (cost " << cost << ") — acknowledged without effect; "
                    "decode the type before implementing it";
        co_return HandleResult::success("{}");
    }

    try
    {
        const auto current = co_await db::DatabaseInterface::read(
            theDb(),
            "user_info",
            {
                db::Data("gems"),
                db::Data("fight_point"),
                db::Data("max_fight_point"),
                db::Lookup("id", identity.userId),
            });

        const auto gems = current.front<int64_t>("gems");
        const auto maxFightPoint = current.front<int32_t>("max_fight_point");

        if (gems < cost)
        {
            LOG_WARN << "ShopUse: user " << identity.userId << " has " << gems
                     << " gems, needs " << cost << " — refusing the restore";
            co_return HandleResult::success("{}");
        }

        co_await db::DatabaseInterface::update(
            theDb(),
            "user_info",
            {
                db::Data("gems", static_cast<int64_t>(gems - cost)),
                db::Data("fight_point", maxFightPoint),
                db::Lookup("id", identity.userId),
            });

        LOG_INFO << "ShopUse: restored Hunter Orbs to " << maxFightPoint
                 << " for " << cost << " gem(s); " << (gems - cost) << " remaining";
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        // Empty OK rather than a closed session (handbook §5).
        LOG_ERROR << "ShopUse: restore failed: " << ex.base().what();
        co_return HandleResult::success("{}");
    }

    ::ShopUseResp resp{};
    resp.team_info = std::move((co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
