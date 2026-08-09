#include "App.hpp"
#include "Handlers.hpp"

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
// // UNVERIFIED: whether the hub also wants s7A3bGLe (ChallengeUserInfo, 8
// fields) or kN2i7qds (ChallengeUserTeam, which carries a FrogateScore).  The
// HR ladder does NOT belong here — challenge_hr_mst.json already ships under
// h09mEvDR via Initialize, the same key ChallengeHrResponse uses.
//
// If this changes nothing, stop guessing at the hub and xref Hunter Orbs
// directly in the binary instead.

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
    // had been setting, is a different currency entirely.
    resp.user_team.hr_id = 1;
    resp.user_team.frogate_score = 0;

    // THE ORB COUNT — and the one field group still carrying placeholders.
    //
    // ChallengeUserTeamResponse has five fields.  readparam_analysis names only
    // vvXp7Uek (FrogateScore); Sv80kL5r is HRID by cross-class reuse.  The
    // other three are inlined, and the candidate setters on ChallengeHeaderInfo
    // are Aube, AubeTimer, AubeRestTimer, Order and Search.  One of the three
    // IS the Hunter Orb count: before this response was sent at all the client
    // reported zero orbs, and populating these made orbs work.
    //
    // // UNVERIFIED which is which.  A probe sending 4/5/6 was INCONCLUSIVE
    // because those values sit above the cap — ChallengeHeaderInfo::getAubeMax
    // returns DefineMst::getMaxfrohunP, observed as 3, so the client clamped
    // all of them to 3 and the reading was identical whichever field it read.
    // The next probe must use values BELOW the cap (1/2/3); the displayed count
    // then names the field outright.
    //
    // Until then all three carry the observed cap so the orb bar reads full and
    // no field carries a number the client could interpret as a stale timer.
    // This is a deliberate placeholder, not a decoded value — do not build
    // orb spending on it (FrontierGateRetry deliberately does not debit).
    constexpr int32_t kObservedOrbCap = 3;
    resp.user_team.unk_first = kObservedOrbCap;
    resp.user_team.unk_second = kObservedOrbCap;
    resp.user_team.unk_third = kObservedOrbCap;

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
