#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

// Three requests that had no handler, and whose only cost was that an
// unregistered GroupId CLOSES the session — which on the screen it happens on
// looks exactly like a crash (handbook: the "Vortex crash" that was really
// Home's UpdateInfo poll).
//
//   BannerClick      a5k36D28 / a63Ghbi2   a banner on the link screen was tapped
//   NoticeList       5s4aVWfc / miMBpUZ3   the in-game notice board
//   NoticeReadUpdate cuKwx5rF / o2rhxCmg   a notice was opened
//
// None of them carries state this fork has.  BannerClick is telemetry:
// BannerLinkTopScene::sendBannerClickRequest @0x1E87D5C fires it and opens the
// link itself without waiting on the reply.  The notice board has no data —
// F_NOTICE_INFO_MST was never dumped, and the Information list the client
// already gets (5nBa3CAe) is a different table — so both of its lists go out
// EMPTY rather than filled with invented rows.  An empty array never reaches
// readParam, so the client keeps its own empty list; see NoticeListResp in
// net/notice.kdl for the full field map, which is decoded and waiting for
// data.
//
// Registering them is the whole point.  Sweep for the rest with a diff of every
// `*Request::getRequestID` string against GmeControllerHandlers.cpp — 192 are
// unregistered, though only one has ever actually been fired here.

HANDLEF(BannerClick)
{
	(void)session;

	BannerClickReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "BannerClick: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
	for (const auto& banner : req.banners)
		LOG_INFO << "BannerClick: " << identity.userId << " tapped banner " << banner.banner_id;

	BannerClickResp resp{};
	resp.signal_key.key = "5EdKHavF";
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

HANDLEF(NoticeList)
{
	(void)session;

	NoticeListReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "NoticeList: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
	LOG_INFO << "NoticeList: empty board for " << identity.userId
		<< " (no notice data was dumped for this fork)";

	NoticeListResp resp{};
	resp.signal_key.key = "5EdKHavF";
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

HANDLEF(NoticeReadUpdate)
{
	(void)session;

	NoticeReadUpdateReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "NoticeReadUpdate: parse error: " << glz::format_error(ec, json);
	}

	(void)(co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	NoticeReadUpdateResp resp{};
	resp.signal_key.key = "5EdKHavF";
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
