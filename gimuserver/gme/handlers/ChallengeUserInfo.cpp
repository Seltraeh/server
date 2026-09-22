#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/HunterOrbs.hpp>

// ChallengeUserInfo (jF3AS4cp / Nst6MK5m) — the per-event state the Frontier
// Hunter lobby runs on, and until now an UNREGISTERED GroupId.
//
// That is not a theoretical cost.  An unregistered GroupId is answered
// "Unsupported request" with a Close command and the client ends the session,
// which on screen is indistinguishable from a crash (handbook §7).  The log has
// it happening for real: 2026-09-13 07:28:53, immediately after a
// DungeonEventUpdate whose 9yVsu21R marker list ends in "challenge," — the
// Frontier Hunter intro had just played — followed by a relaunch thirty
// seconds later.
//
// Request (ChallengeUserInfoRequest::createBody @0x13A25D4): group a7HQ9sEB
// carrying the event id (c5yZnpB4) and the mission being inspected (j28VNcUW).
//
// Response (ChallengeUserInfoResponse::readParam @0x13D3BEC): eight setters
// split across TWO client singletons, with no list operations on either, so
// this is one full row rather than a list.  Six go to ChallengeInfo and two to
// ChallengeHeaderInfo; the field docs in net/handlers.kdl carry the per-key
// citations.
//
// The one field that decides anything is frohun_stat.  ChallengeLobbyScene::
// startCheck @0x155CDB8 and ChallengeConditionScene::startCheck proceed only
// when it is exactly 1; every other value draws CHALLENG_CHECK_TIME and the
// lobby refuses to start.  btnSetSt @0x155B904 adds one more known value: 3
// draws the blue "collect your reward" button, so 3 is the ended-and-payable
// phase and the rest fall through to the red entry button.
//
// THIS SERVER REPORTS 1 — RUNNING — SINCE 2026-09-14.
//
// It reported 0 for a day on the theory that a lit Enter button would fire
// ChallengeStart (sQfU18kH) and, that being unregistered, move the session kill
// one tap further in.  THAT WAS WRONG, and the binary says so plainly:
// ChallengeStartRequest has ZERO callers.  Nothing constructs it.
//
// What Enter actually does is ChallengeLobbyScene::missionScene @0x155C45C ->
// MissionSelectScene2 with ChallengeBase::getDunMst() — the ordinary
// quest-select screen.  Frontier Hunter therefore reuses the whole existing
// mission pipeline and needs no battle machinery of its own; all it needed was
// its topology in the permit list, which ServerCache now collects for the
// active event only.
//
// The event is a DECIDED one: every row of challenge_mst.json is a real
// 2014-2022 window and all 97 have expired, so a date-derived status would shut
// the mode forever.  ServerCache already makes that call in the other direction
// by keeping the newest event open, and this agrees with it rather than
// contradicting it.
//
// ⚠ The four missions of event 97 (1009700-1009703) have no authored battle
// content, so they take MissionStart's documented mission-10 template fallback.
// They are enterable and winnable; the waves are not the real ones.
//
// None of this touches Frontier Gate.  FG sits behind the same Survey Office
// but its entry check reads Hunter Orbs out of ChallengeHeaderInfo and never
// looks at frohun_stat (FrontierGateConditionScene::startCheck @0x15DE978), so
// the orb half of this work stands on its own.
//
// kN2i7qds rides along because the lobby reads the two together: startCheck
// pulls ChallengeHeaderInfo::getAube and ChallengeInfo::getFrohunStat one after
// the other, so a stale orb count here would refuse an entry the orb state
// actually allows.

namespace
{
// The lobby gate.  ChallengeLobbyScene::startCheck @0x155CDB8 proceeds only on
// exactly 1; btnSetSt @0x155B904 treats 3 as ended-with-rewards and everything
// else draws the red entry button.  See the note above.
constexpr int32_t kFrohunStatRunning = 1;

// Hunter Rank.  Flat 1 for the same reason ChallengeBase sends 1: the ladder
// ships in challenge_hr_mst.json but nothing scores Frontier Hunter yet, and
// FrontierGateUtils::entryCheckHr reads this, so it has to be a real ladder
// value rather than 0.
constexpr int32_t kBaseHunterRank = 1;
}

HANDLEF(ChallengeUserInfo)
{
	(void)session;
	LOG_INFO << "ChallengeUserInfo: " << json;

	::ChallengeUserInfoReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "ChallengeUserInfo: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// Answer about the event the client asked for when it named one, and fall
	// back to the event this server considers live.  Echoing an id the client
	// did not ask about would point the lobby's later requests — the ranking,
	// result and reward screens all read getFrohunID back — at the wrong event.
	const auto activeId = theServer()->cache().activeChallengeId();
	std::string eventId = req.challenge.frohun_id;
	if (eventId.empty() || eventId == "0")
	{
		if (activeId == 0)
		{
			// Nothing to describe.  An empty success still answers the request,
			// which is the whole point of registering it: the session survives.
			LOG_WARN << "ChallengeUserInfo: no Frontier Hunter event available";
			co_return HandleResult::success("{}");
		}
		eventId = std::to_string(activeId);
	}

	::ChallengeUserInfoResp resp{};
	resp.signal_key = req.signal_key;

	resp.challenge_user.frohun_id = eventId;
	resp.challenge_user.frohun_stat = kFrohunStatRunning;
	// Score and ladder position: this server does not run Frontier Hunter
	// scoring, so both are 0 rather than an invented standing.  They are
	// display-only (the header rank and the survey score), not gates.
	resp.challenge_user.search = 0;
	resp.challenge_user.order = 0;
	// frohun_flg and frohun_try have no readers in this build — left at 0
	// rather than carrying a guess.
	resp.challenge_user.frohun_flg = 0;
	resp.challenge_user.frohun_try = 0;
	// The period line and the navigator message are drawn verbatim; empty
	// leaves the lobby's own defaults rather than asserting a window that the
	// expired MST row would contradict.
	resp.challenge_user.frohun_term = "";
	resp.challenge_user.comment = "";

	resp.user_team = co_await gme::loadChallengeHeader(theDb(), identity, kBaseHunterRank, 0);

	LOG_INFO << "ChallengeUserInfo: event " << eventId << " reported RUNNING for "
		<< identity.userId << "; " << resp.user_team.aube << " Hunter Orb(s), "
		<< resp.user_team.aube_rest_timer << "s to the next";

	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
