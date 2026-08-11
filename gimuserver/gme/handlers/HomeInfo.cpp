#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>

// HomeInfo (NiYWKdzs / f6uOewOD) — the client's request as it ENTERS Home.
// Handbook §8.41.
//
// This is where the unit cache gets rebuilt, and it is the answer to "unit
// changes do not show until relaunch".
//
// Why it has to happen here rather than on the mutation itself: the client has
// no in-session path to correct a cached unit.  Both unit-list keys funnel into
// UserUnitInfoResponse::readParam, which ENDS with
//
//     v187 = UserUnitInfoList::exist(list, userUnitId);
//     if ( (v187 & 1) == 0 ) { ...; UserUnitInfoList::addObject(...); return 1; }
//     return 1LL;                        // already owned -> entry DISCARDED
//
// so qC2tJs4E is INSERT-IF-ABSENT.  Sending an owned unit under it does not
// append, does not merge — the freshly parsed object is dropped on the floor.
// It exists for units the client does not have yet (drops, gacha), not updates.
// getResponseObject registers exactly two keys onto that class, 4ceMWH6k(v3,1)
// and qC2tJs4E(v3,0), so there is no third "merge" key; and shared() falls
// through to sharedOriginal() outside Raid/Campaign/FrontierGate/FGPlus, so
// there is no second list to land in either.
//
// The only key that genuinely rebuilds the cache is 4ceMWH6k, whose readParam
// calls UserUnitInfoList::removeAllObjects().  That RELEASES every CCObject in
// the list, so it is safe only where nothing holds unit pointers — a scene
// boundary.  Login proves the pattern (UserInfo lands, then scenes build) and
// HomeInfo has the same ordering.  The same key sent mid-fusion soft-locked the
// ritual screen, which is what established the rule.
//
// Consequence worth knowing: the fusion screen ITSELF still shows the old level
// immediately after the ritual, because it is holding a pointer from
// initialize().  Nor does the client ask for anything after a fusion — the
// request log goes quiet between the UnitMix response and the player walking
// back to Home, and entering the fusion menu fetches no list either.  The
// ritual screen's own numbers come from UnitOpeResult (1ZbHB6Im), which is
// scene-local and needs no cache write; the cache itself waits for here.
HANDLEF(HomeInfo)
{
    ::HomeInfoReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
            LOG_WARN << "HomeInfo: parse error: " << glz::format_error(ec, json);
    }

    const auto db = theDb();
    const auto identity = (co_await gme::getUserIdentity(db, req.login_info)).nonEmpty();

    ::HomeInfoResp resp{};

    resp.team_info = std::move((co_await gme::getTeamInfo(db, identity)).nonEmpty());

    // Same read UserInfo makes at login — the shape the client provably
    // accepts.  Do not hand-assemble entries here: `received_order` is not a
    // column (the schema maps it onto user_unit_id) and silently went out as 0
    // when UnitMix built its entry by hand.
    resp.unit_info = std::move((co_await db::PacketInterfaceFor<::UserUnitInfo>::read(
        db,
        "user_units",
        { db::Lookup("user_id", identity.userId) })).data);

    LOG_INFO << "HomeInfo: rebuilt unit cache — " << resp.unit_info.size() << " unit(s)";

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
