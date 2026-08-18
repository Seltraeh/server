#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <string>

// TownFacilityUpdate (8v43tz7g) — fired when the player confirms a batch of
// facility/location upgrades.  The client sends the COMPLETE desired new state
// for ALL facilities + locations plus the total karma injected in the batch.
// Server persists whatever the client says and deducts karma.
//
// Karma here is an EXP BAR, not a price.  MyTownFacilityExtScene2::injectionFacility
// (libgame.so 0x18F66D4) adds the injected karma to the row's running total and
// only bumps the level once that total reaches TownFacilityLvMst.karma, keeping
// the remainder.  So the per-row HTVh8a65 is progress toward the NEXT level and
// has to be persisted — this handler used to drop it, which reset every
// half-filled bar to zero on the next login.
//
// Request struct is generated from KDL (TownFacilityUpdateReq in all.hpp):
//   EuY6L7AX[0].HTVh8a65  — total karma injected this batch
//   YRgx49WG[].y9ET7Aub   — facility_id
//   YRgx49WG[].D9wXQI2V   — lv
//   YRgx49WG[].HTVh8a65   — karma banked toward this facility's next level
//   yj46Q2xw[].un80kW9Y   — location_id
//   yj46Q2xw[].D9wXQI2V   — lv
//   yj46Q2xw[].HTVh8a65   — karma banked toward this location's next level

HANDLEF(TownFacilityUpdate)
{
    LOG_INFO << "TownFacilityUpdate: " << json;

    TownFacilityUpdateReq req{};
    glz::context ctx{};
    if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
    {
        LOG_WARN << "TownFacilityUpdate: parse error: " << glz::format_error(ec, json);
        co_return HandleResult::success("{}");
    }

    // Resolve the current user from the request's login info.
    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const std::string userId = identity.userId;
    const int64_t karmaCost     = req.karma_payment.karma;

    // Persist each facility's new level AND its banked karma, batched into one
    // statement.  The client sends every facility every time, so an await per
    // row was six sequential round trips on the single-connection SQLite pool
    // (handbook §6.14).  UPSERT because the first update for a facility can
    // arrive before provisionTown ever ran for this account.
    //
    // Values are ints straight off a parsed struct plus a server-minted user id,
    // so inlining them carries no injection surface.
    if (!req.facilities.empty())
    {
        std::string sql =
            "INSERT INTO user_town_facilities (user_id, facility_id, lv, karma) VALUES ";
        bool first = true;
        for (const auto& f : req.facilities)
        {
            if (!first)
                sql += ',';
            first = false;
            sql += "('" + userId + "'," + std::to_string(f.facility_id) + ","
                + std::to_string(f.lv) + "," + std::to_string(std::max(f.karma, 0)) + ")";
        }
        sql += " ON CONFLICT(user_id, facility_id) DO UPDATE SET"
               " lv=excluded.lv, karma=excluded.karma;";

        try
        {
            co_await theDb()->execSqlCoro(sql);
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "TownFacilityUpdate: facility UPSERT failed: " << ex.base().what();
        }
    }

    // Same for the resource tiles.  Their level drives the drop pool and tap
    // allowance the next harvest period rolls from, so a lost level here is a
    // visibly weaker tile.
    if (!req.locations.empty())
    {
        std::string sql =
            "INSERT INTO user_town_locations (user_id, location_id, lv, karma) VALUES ";
        bool first = true;
        for (const auto& l : req.locations)
        {
            if (!first)
                sql += ',';
            first = false;
            sql += "('" + userId + "'," + std::to_string(l.location_id) + ","
                + std::to_string(l.lv) + "," + std::to_string(std::max(l.karma, 0)) + ")";
        }
        sql += " ON CONFLICT(user_id, location_id) DO UPDATE SET"
               " lv=excluded.lv, karma=excluded.karma;";

        try
        {
            co_await theDb()->execSqlCoro(sql);
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "TownFacilityUpdate: location UPSERT failed: " << ex.base().what();
        }
    }

    // Deduct karma from the user's balance.
    if (karmaCost > 0)
    {
        try
        {
            co_await theDb()->execSqlCoro(
                "UPDATE user_info SET karma=MAX(0, karma-$1) WHERE id=$2;",
                karmaCost, userId);
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "TownFacilityUpdate: karma deduct failed: " << ex.base().what();
        }
    }

    co_return HandleResult::success("{}");
}
