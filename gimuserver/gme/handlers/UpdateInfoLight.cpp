#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/HunterOrbs.hpp>
#include <gimuserver/gme/common/LoginCampaign.hpp>

// UpdateInfoLight (ynB7X5P9 / 7kH9NXwC) — the client's "anything new for me?"
// poll.  Handbook §8.41.
//
// The client fires this on roughly every return to Home — ~143 times in a
// single logged session — and its createBody @0x13AF9F4 emits only the userInfo
// and signalKey tags, so the request carries no fields at all.  This server and
// the legacy fork both answered `{}` every time, i.e. "nothing has changed".
//
// Team/clear state and a partial Hunter Orb snapshot are refreshed here.
// Orb reader @0x13D3F94 has no singleton reset, so sending count/timer only
// preserves rank and Frontier Gate score. The orb query is read-only; regeneration
// is derived at request time. This poll never replaces the unit roster.
HANDLEF(UpdateInfoLight)
{
    ::UpdateInfoLightReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
            co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
    }

    const auto db = theDb();
    const auto identity = (co_await gme::getUserIdentity(db, req.login_info)).nonEmpty();

    ::UpdateInfoLightResp resp{};

    resp.team_info = std::move((co_await gme::getTeamInfo(db, identity)).nonEmpty());

    // Deliberately NO unit list — see the KDL.  4ceMWH6k's readParam calls
    // removeAllObjects() on a list the Home screen is holding party pointers
    // into, and this poll fires ~143 times a session.  It also did not fix the
    // staleness it was added for.
    resp.clear_mission_info = co_await gme::getClearedMissions(db, identity);
    resp.hunter_orbs = co_await gme::loadHunterOrbRefresh(db, identity);

    LOG_INFO << "UpdateInfoLight: refreshed team info + "
             << resp.clear_mission_info.size() << " cleared mission(s)";

    std::string body;
    if (const auto error = glz::write_json(resp, body); error)
        co_return HandleResult::error("Serialization error", glz::format_error(error, body));
    co_return HandleResult::success(body);
}

// UpdateInfo (RUV94Dqz) — Home's 30-minute refresh (see the KDL for the timer).
// Same payload as UpdateInfoLight, plus the server time that restarts the
// client's timer.
HANDLEF(UpdateInfo)
{
    ::UpdateInfoReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
            co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
    }

    const auto db = theDb();
    const auto identity = (co_await gme::getUserIdentity(db, req.login_info)).nonEmpty();

    ::UpdateInfoResp resp{};
    resp.team_info = std::move((co_await gme::getTeamInfo(db, identity)).nonEmpty());
    resp.clear_mission_info = co_await gme::getClearedMissions(db, identity);
    resp.hunter_orbs = co_await gme::loadHunterOrbRefresh(db, identity);
    resp.update_info.server_time = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    LOG_INFO << "UpdateInfo: 30-minute refresh — team info + "
             << resp.clear_mission_info.size() << " cleared mission(s)";

    std::string body;
    if (const auto error = glz::write_json(resp, body); error)
        co_return HandleResult::error("Serialization error", glz::format_error(error, body));
    co_return HandleResult::success(body);
}

// NoticeUpdate (68pTQAJv) — the notice list.  There are no notices offline;
// the empty lists are what tells the client the list finished loading.
HANDLEF(NoticeUpdate)
{
    ::NoticeUpdateReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
            LOG_WARN << "NoticeUpdate: parse error: " << glz::format_error(ec, json);
    }

    (void)(co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    LOG_INFO << "NoticeUpdate: kind "
             << (req.notice.empty() ? -1 : req.notice.front().notice_kind)
             << " — no notices";

    co_return HandleResult::success(glz::write_json(::NoticeUpdateResp{}).value_or("{}"));
}

// UserLoginCampaignInfo (5fc8bf2c) — Home's 12-hour login-campaign check, and
// the thing that rolls the advent calendar over for a client left running past
// midnight.  Initialize does the same work at launch; both go through
// gme::advanceLoginCampaign, which is idempotent within a day, so whichever
// arrives first pays and the other simply reports the same cell.
HANDLEF(UserLoginCampaignInfo)
{
    ::UserLoginCampaignInfoReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
            LOG_WARN << "UserLoginCampaignInfo: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    ::UserLoginCampaignInfoResp resp{};
    try
    {
        const auto state = co_await gme::advanceLoginCampaign(theDb(), identity);
        resp.campaign_info = gme::loginCampaignInfo(state);
        LOG_INFO << "UserLoginCampaignInfo: day " << state.current_day << "/" << state.total_days
                 << (state.first_for_the_day ? " (new day, granted)" : "");
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_ERROR << "UserLoginCampaignInfo: DB error: " << ex.base().what();
        // SUCCESS regardless: an error reply is GmeErrorCommand::Close.
        co_return HandleResult::success("{}");
    }

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
