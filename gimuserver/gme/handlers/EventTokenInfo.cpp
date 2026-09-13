#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/EventTokens.hpp>

// EventTokenInfo (f49als4D) — the player's event-token balances.  Sent when the
// client enters the Event Bazaar area inside the Town scene; the request body
// is the identity envelope alone (EventTokenInfoRequest::createBody @0x1C491EC
// is 44 bytes and adds nothing).
//
// The reply is a list of EventTokenInfo under `l234vdKs`, field-mapped from
// EventTokenInfoResponse::readParam @0x1C49274 — see net/event.kdl.  Token 8
// "Rift Token" is the Frontier Gate's currency; the box pays it as present type
// 8004 (gme::grantEventToken) and this is where the balance comes back.
//
// Still unbuilt above this: the exchange itself.  The client has
// EventTokenExchangeInfoRequest and the shop's item names live in its own
// sgtext (MST_ET_EXCHANGE_*, 61 rows), but no F_EVENT_TOKEN_EXCHANGE MST was
// dumped, so there is nothing to spend tokens ON yet.
HANDLEF(EventTokenInfo)
{
	(void)session;

	EventTokenInfoReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "EventTokenInfo: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	EventTokenInfoResp resp{};
	resp.event_token_info = co_await gme::loadEventTokens(theDb(), identity);

	LOG_INFO << "EventTokenInfo: " << resp.event_token_info.size()
		<< " token balance(s) for " << identity.userId;

	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
