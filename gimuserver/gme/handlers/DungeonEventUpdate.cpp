#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Common.hpp>

// DungeonEventUpdate (BjAt1D6b / k5EiNe9x) — fires when the client enters
// the Grand Gaia world map after the opening cutscene.  The original handler
// returns an empty JSON object; no fields are read by the client from this
// response.  Without this handler the server returns "Unsupported request"
// and the client hard-crashes out of the cutscene.
// It also carries the one-off scene-intro list the client has played, in its
// login envelope under 9yVsu21R ("randall," -> "randall,frontiergate_v2,").
// This is the only request observed to report it, so this is where it gets
// persisted; UserInfo echoes the stored value back via
// LoginInfoResp.user_special_scenario_info so the intros stay marked as seen.
//
// The list is stored verbatim rather than merged token-by-token: the client
// sends the complete set every time and only ever grows it, so a union would
// be extra machinery for no behavioural difference.  A shorter incoming list is
// therefore treated as authoritative — if that ever turns out to drop scenes,
// switch to a union here.
HANDLEF(DungeonEventUpdate)
{
    LOG_INFO << "DungeonEventUpdate: " << json;

    ::DungeonEventUpdateReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "DungeonEventUpdate: parse error: " << glz::format_error(ec, json);
    }

    const auto& reported = req.login_info.special_scenario_info;
    if (!reported.empty())
    {
        try
        {
            const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
            co_await db::DatabaseInterface::update(
                theDb(),
                "user_info",
                {
                    db::Data("special_scenario_info", reported),
                    db::Lookup("id", identity.userId),
                });
            LOG_INFO << "DungeonEventUpdate: stored scene-intro list \"" << reported << "\"";
        }
        catch (const std::exception& ex)
        {
            // Empty OK regardless — this is a best-effort side effect and the
            // client hard-crashes out of the cutscene if the request fails.
            LOG_WARN << "DungeonEventUpdate: could not store scene-intro list: " << ex.what();
        }
    }

    co_return HandleResult::success("{}");
}

// GetScenarioPlayingInfo now lives in Scenario.cpp — it returns the real
// viewed-cutscene set from user_scenarios instead of the old {} stub.

// UpdatePermitPlaceInfo (1MJT6L3W / 3zip5Htw) — sent after entering an
// area to refresh the server-side permit-place allow-list.  Original handler
// returns {}.  Our PermitPlace is injected once in UserInfo so no update
// action is required here.
HANDLEF(UpdatePermitPlaceInfo)
{
    LOG_INFO << "UpdatePermitPlaceInfo: " << json;
    co_return HandleResult::success("{}");
}

// UpdateEventInfo (rCB7ZI8x / L1o4eGbi) — updates the client's view of
// active event data.  Original handler returns {}.
HANDLEF(UpdateEventInfo)
{
    LOG_INFO << "UpdateEventInfo: " << json;
    co_return HandleResult::success("{}");
}

// Chronology (5o8ZlDGX / SNrhAG29) — Chronology timeline feature endpoint.
// Original handler returns {}.
HANDLEF(Chronology)
{
    LOG_INFO << "Chronology: " << json;
    co_return HandleResult::success("{}");
}
