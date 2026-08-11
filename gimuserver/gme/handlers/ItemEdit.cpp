#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

// ItemEdit (ruoB7bD8) — fired during mission prep when the player sets the
// battle-item loadout (potions carried into the fight), alongside the
// party/reinforcement select.
//
// This used to be a bare ack, which was half of a two-sided bug:
//   * the player's chosen loadout was parsed by nobody and thrown away, so it
//     never survived leaving the prep screen; and
//   * UserInfo, having nothing persisted to report, synthesised the equip list
//     by walking the whole warehouse and emitting EVERY battle consumable the
//     player owned — so the slots came back filled with whatever was in
//     inventory order rather than what the player picked.
// Persisting here fixes the first half; UserInfo now reads this table for the
// second.
//
// The client still owns the in-battle consumption (MissionEnd reports what was
// used), so the response body stays empty — matching the legacy
// ItemEditRequestHandler.
//
// GroupId = "ruoB7bD8", AES key = "DHEfRexCu0q5TAQm" (from the legacy
// ItemEditRequestHandler GetGroupId/GetAesKey).
//
// ItemEditReq is generated from the KDL (packet-generator/assets/net/items.kdl).
HANDLEF(ItemEdit)
{
    (void)session;
    LOG_INFO << "ItemEdit: " << json;

    ItemEditReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
        {
            // Parse failure means we cannot tell an empty loadout from an
            // unreadable one, and clearing the table on a bad read would wipe a
            // good loadout.  Ack without touching storage.
            LOG_WARN << "ItemEdit: parse error, loadout not persisted: "
                     << glz::format_error(ec, json);
            co_return HandleResult::success("{}");
        }
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const std::string kUserId = identity.userId;

    // Full-replace: the client always sends the complete loadout, and an empty
    // group is how it expresses "every slot cleared".  Deleting first is what
    // makes un-equipping work at all.
    try
    {
        co_await theDb()->execSqlCoro(
            "DELETE FROM user_equip_items WHERE user_id=$1;", kUserId);
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "ItemEdit: loadout DELETE failed: " << ex.base().what();
        co_return HandleResult::success("{}");
    }

    size_t stored = 0;
    for (const auto& e : req.equip_items)
    {
        if (e.item_id == 0)
            continue;

        try
        {
            co_await theDb()->execSqlCoro(
                "INSERT INTO user_equip_items (user_id, disp_order, item_id, item_num)"
                " VALUES ($1, $2, $3, $4)"
                " ON CONFLICT(user_id, disp_order) DO UPDATE SET"
                " item_id=$3, item_num=$4;",
                kUserId,
                static_cast<int32_t>(e.disp_order),
                static_cast<int32_t>(e.item_id),
                static_cast<int32_t>(e.item_num));
            ++stored;
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "ItemEdit: slot " << e.disp_order << " INSERT failed: "
                     << ex.base().what();
        }
    }

    LOG_INFO << "ItemEdit: stored " << stored << " equipped item(s) for " << kUserId;

    co_return HandleResult::success("{}");
}
