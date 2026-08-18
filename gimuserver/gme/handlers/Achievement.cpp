#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

// GetAchievementInfo (YPBU7MD8 / AKjzyZ81).
//
// This is the request the client fires when the menu hosting achievements
// opens.  It is NOT the present box — that assumption cost a build.  YPBU7MD8
// surfaced in an "Unsupported request" dialog while opening the gift tab, which
// made it look like the present list, but it resolves to
// GetAchievementInfoRequest two independent ways: its xref goes to
// `GetAchievementInfoRequest::getRequestID`, and its .rodata block terminates
// with `25GetAchievementInfoRequest` (see tools/ida/groupid_key_pair_audit.py
// and handbook §4.2).  The present box lives at nhjvB52R / bV5xa0ZW.
//
// The screen evidently loads achievements first, so an unregistered YPBU7MD8
// killed it before anything else on it — including any present request — could
// run.  Registering this handler is what lets the rest of that screen proceed.
//
// The response is deliberately EMPTY.  The four achievement response classes
// are known but undecoded — readparam_analysis.json has their keys and C++
// types with every setter name null, so the field semantics are unknown:
//
//     Bnc4LpM8  UserAchievementInfoResponse           3 x uint32
//     YTRJLG65  UserAchievementSubjectInfoResponse    9 fields (M7SXoc31 = subject id)
//     9j3ALx8I  UserAchievementTradeInfoResponse      4 fields
//     LcFCx1Uz  UserAchievementSPSubjectInfoResponse  1 field  (M7SXoc31)
//
// Handbook §3.4: never ship a response key whose fields you would have to fill
// with 0/"" — the client takes a present-but-wrong value at face value, while a
// missing key falls back to its own default.  So this sends nothing but the
// signal key until a readParam audit names the setters.  The old fork answered
// achievements with an empty OK for the same reason.
//
// NOTE the client CLEARS the list it is about to receive: createBody branches
// on `mode` immediately after sending it — 1 calls
// UserAchievementSubjectInfoList::removeDataObject, 2 calls
// UserAchievementTradeInfoList::removeAllObjects.  So an omitted list reads as
// empty, never as stale, which is what makes the empty response safe.
//
// To populate this later: audit the four classes above with
// tools/ida/readparam_audit.py, then join F_ACHIEVEMENT_SUBJECT_MST (already on
// disk, 5058 rows) on M7SXoc31 / KT71m8Ae / 3rhygS9K — the request's category
// and sub_category use the same keys as that MST's columns.
HANDLEF(GetAchievementInfo)
{
	(void)session;

	GetAchievementInfoReq req = {};
	{
		glz::context ctx{};
		if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GetAchievementInfo: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// Log the selector so the first live capture tells us which lists the
	// screen actually asks for, and in what order.
	for (const auto& node : req.nodes)
	{
		LOG_INFO << "GetAchievementInfo: mode=" << node.mode
			<< " category=" << node.category
			<< " sub_category=" << node.sub_category
			<< " (returning empty — response classes not yet decoded)";
	}
	if (req.nodes.empty())
		LOG_INFO << "GetAchievementInfo: no selector node for " << identity.userId;

	GetAchievementInfoResp resp = {};
	resp.signal_key.key = "5EdKHavF";

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		const auto& glze = glz::format_error(ec, buffer);
		LOG_DEBUG << "Gme GetAchievementInfo Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}
