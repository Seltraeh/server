#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

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
        co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
    }

    // Resolve the current user from the request's login info.
    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const std::string userId = identity.userId;
    const int64_t karmaCost     = req.karma_payment.karma;

    std::string buffer;
    auto transaction = co_await theDb()->newTransactionCoro();
    try
    {
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

            co_await transaction->execSqlCoro(sql);
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

            co_await transaction->execSqlCoro(sql);
        }

        // Preserve the existing clamped payment policy, but count the actual debit.
        const auto balance = co_await transaction->execSqlCoro(
            "SELECT karma FROM user_info WHERE id=$1;", userId);
        const auto spent = std::min(std::max<int64_t>(karmaCost, 0),
            std::max<int64_t>(balance[0]["karma"].as<int64_t>(), 0));
        if (spent > 0)
        {
            co_await transaction->execSqlCoro("UPDATE user_info SET karma=karma-$1 WHERE id=$2;", spent, userId);
            co_await transaction->execSqlCoro(
                "INSERT INTO user_team_archive (user_id,karma_use) VALUES ($1,$2)"
                " ON CONFLICT(user_id) DO UPDATE SET karma_use=karma_use+excluded.karma_use;", userId, spent);
        }

        // Report back the two lists the upgrade just invalidated.
        //
        // The client recomputed its own facility/location levels before sending, so
        // those need no echo.  These two it cannot derive:
        //
        //   * the synthesis menu — PermitRecipeInfoList is otherwise filled once
        //     from UserInfo, so a facility that just unlocked new recipes showed
        //     none of them until the next relaunch.  That was the whole "I levelled
        //     Synthesis and no new recipe tiles appeared" report.
        //   * the tile harvest state — Town::locationState re-rolls a levelled-up
        //     tile's remaining taps against its new drop pool, and drop_item_info is
        //     server-authored.
        //
        // Both response classes replace their list wholesale: readParam calls
        // removeAllObjects() at (row 0, field 0) and addObject() on each row's last
        // field, with no per-row operation code to get wrong (contrast the present
        // box, handbook §6.20).  Emitting them from here works because
        // GameResponseParser::getResponseObject is a global key -> class registry
        // (§7.12.4), so the keys dispatch the same regardless of which handler
        // carried them.
        TownFacilityUpdateResp resp{};
        {
            std::set<int32_t> cleared;
            for (const auto& done : co_await gme::getClearedMissions(transaction, identity))
            {
                cleared.insert(done.mission_id);
            }

            resp.permit_receipes = co_await gme::Town::permittedRecipes(transaction, identity, cleared);

            std::vector<::UserTownLocationInfo> unusedInfo;
            co_await gme::Town::locationState(
                transaction, identity, cleared, unusedInfo, resp.town_location_detail);

            // The header, because the karma this batch spent left the balance here
            // and the deduction is clamped server-side (MAX(0, karma-cost)) — the
            // client's own number can disagree, and nothing else on this screen
            // brings a fresh one.
            resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
        }

        if (const auto error = glz::write_json(resp, buffer); error)
            throw std::runtime_error(glz::format_error(error, buffer));
    }
    catch (const std::exception& ex)
    {
        transaction->rollback();
        co_return HandleResult::error("Town upgrade failed", ex.what());
    }
    co_return HandleResult::success(buffer);
}
