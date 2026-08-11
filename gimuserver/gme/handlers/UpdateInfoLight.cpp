#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>

// UpdateInfoLight (ynB7X5P9 / 7kH9NXwC) — the client's "anything new for me?"
// poll.  Handbook §8.41.
//
// The client fires this on roughly every return to Home — ~143 times in a
// single logged session — and its createBody @0x13AF9F4 emits only the userInfo
// and signalKey tags, so the request carries no fields at all.  This server and
// the legacy fork both answered `{}` every time, i.e. "nothing has changed".
//
// That is the leading explanation for the standing complaint that mutations do
// not appear until the client is relaunched: cleared missions still rendering
// "NEW" with no successor, and unit evolutions and level-ups not showing.  The
// data was verified to reach the client correctly in the login snapshot; what
// was missing was any way for it to arrive again afterwards.
//
// Nothing constrains a GME response to a fixed class — GameResponseParser
// dispatches each top-level key independently through getResponseObject (504
// keys; tools/ida/audits/getResponseObject_FULL.txt), so a response is a bag of
// keys and this poll can carry whatever actually changed.
//
// Scope is deliberately narrow: only per-user mutable state.  MST tables,
// PermitPlace, the gacha catalog and friends are login-time data and stay in
// UserInfo — resending them on a poll this frequent would be wasteful and would
// re-run PermitPlace's progression build every few seconds.
//
// // UNVERIFIED: that this is what the real server answered with.  The legacy
// stub is the only hint — it includes SignalKey.hpp and UserUnitInfo.hpp and
// then uses neither.  Everything here is data the client already accepts from
// UserInfo under the same keys, so the worst case is redundant work rather
// than a malformed packet.
HANDLEF(UpdateInfoLight)
{
    ::UpdateInfoLightReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
            LOG_WARN << "UpdateInfoLight: parse error: " << glz::format_error(ec, json);
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

    LOG_INFO << "UpdateInfoLight: refreshed team info + "
             << resp.clear_mission_info.size() << " cleared mission(s)";

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
