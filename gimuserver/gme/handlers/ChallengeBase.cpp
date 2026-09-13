#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/HunterOrbs.hpp>

// ChallengeBase (nUAW2B0a / uE5Tsv6P) — fires when the player opens the Survey
// Office, which is the hub the Frontier Gate entrance sits behind.  Without it
// registered the dispatcher answers "Unsupported request" and the client errors
// out at the Survey Office, so FrontierGateInfo (M17pPotk) is never reached —
// confirmed against deploy/log: no M17pPotk request appears in any client
// session, only in wire-harness traffic.
//
// Request: identity only.  ChallengeBaseRequest::createBody emits the three
// BaseRequest tags (userInfo/signalKey/version) and no addParam/addGroup.
//
// Response: cvg8hzp9 (ChallengeInfoResponse) carrying the running Frontier
// Hunter event id.  The binary defines no ChallengeBaseResponse class, so the
// key set is inferred; this is the minimum the hub plausibly needs, and it is
// deliberately just ONE key so a client test says something specific.
//
// Why an event id at all: "Challenge" is this binary's internal name for
// Frontier Hunter (c5yZnpB4 = setFrohunID), every row of challenge_mst.json is
// a real 2014-2022 window, and all of them have expired.  ServerCache::Setup()
// keeps the newest one open; this returns its id.
//
// The working theory being tested is that Hunter Orbs — Frontier Hunter's
// attempt currency, which Frontier Gate shares per the in-game intro at
// content/event/randall_FG.txt — read zero because no event is live, which
// would explain why the client ignores a perfectly good team_info fight_point.
//
// RESOLVED 2026-09-13.  s7A3bGLe is ChallengeUserInfo's own reply, not the
// hub's — it has its own GroupId (jF3AS4cp), which was unregistered and was
// closing the session; see ChallengeUserInfo.cpp.  kN2i7qds does belong here,
// and its three unnamed fields are now decoded from the binary rather than by
// probe (see gme/common/HunterOrbs.hpp).  The HR ladder still does NOT belong
// here — challenge_hr_mst.json already ships under h09mEvDR via Initialize,
// the same key ChallengeHrResponse uses.

HANDLEF(ChallengeBase)
{
    LOG_INFO << "ChallengeBase: " << json;

    const auto activeId = theServer()->cache().activeChallengeId();
    if (activeId == 0)
    {
        // No catalog to point at — empty OK rather than advertising event 0,
        // which would be a made-up id (handbook §3.4).
        LOG_WARN << "ChallengeBase: no Frontier Hunter event available";
        co_return HandleResult::success("{}");
    }

    ::ChallengeBaseResp resp{};
    resp.challenge_info.frohun_id = activeId;

    // Hunter Orbs live here, not in team_info.  Binary xref 2026-08-07: orbs
    // are internally "Aube" and the counter is ChallengeHeaderInfo::getAube,
    // which this response feeds — UserTeamInfo::fight_point, which the server
    // had been setting, is a different currency entirely (the Arena Orbs).
    //
    // The three fields that used to carry probe placeholders are decoded now —
    // ChallengeUserTeamResponse::readParam @0x13D3F94 names all five setters in
    // the open, so 38sHatGk is the count, 6mh0jiKJ the unread AubeTimer and
    // RHKA30s5 the seconds to the next orb.  gme::loadChallengeHeader derives
    // the live state from the save; see HunterOrbs.hpp for why the client and
    // the server can both keep time without polling each other.
    //
    // Hunter Rank is still a flat 1: challenge_hr_mst.json ships the ladder,
    // but nothing here scores Frontier Hunter yet, so a higher rank would be
    // invented.  FrontierGateUtils::entryCheckHr reads it, so it has to be a
    // real ladder value rather than 0.
    ::FriendGetReq idOnly{};   // identity-only shape: IKqx1Cn9 and nothing else
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(idOnly, json, ctx); ec)
            LOG_WARN << "ChallengeBase: parse error: " << glz::format_error(ec, json);
    }
    const auto identity = (co_await gme::getUserIdentity(theDb(), idOnly.login_info)).nonEmpty();
    constexpr int32_t kBaseHunterRank = 1;
    resp.user_team = co_await gme::loadChallengeHeader(theDb(), identity, kBaseHunterRank, 0);

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
