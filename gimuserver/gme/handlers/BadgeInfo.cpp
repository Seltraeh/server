#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

// BadgeInfo (nJ3A7qFp) — the badge singleton the client asks for at login,
// answered under h23iRjGN (BadgeInfoResponse::readParam @0x13CDCB8).  Of its
// six counters only the key-ready badge has a reader in this build; see
// BadgeInfo in net/badge_info.kdl.
HANDLEF(BadgeInfo)
{
	::BadgeInfoReq req{};
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		LOG_WARN << "BadgeInfo: parse error: " << glz::format_error(ec, json);
	}

	// Use BadgeInfoResp (the wrapper): it carries the "h23iRjGN" dispatch key the
	// client requires (see decompfrontier/server PR #23).
	::BadgeInfoResp resp{};

	// A badge must never fail the login sequence it rides in, so an unresolved
	// caller simply gets the zero badge this handler always sent before.
	try
	{
		const auto identity = co_await gme::getUserIdentity(theDb(), req.login_info);
		if (!identity.data.userId.empty())
		{
			resp.badge_info.dungeon_key_num =
				gme::claimableKeyCount(co_await gme::dungeonKeyState(theDb(), identity.data));
		}
	}
	catch (const std::exception& ex)
	{
		LOG_WARN << "BadgeInfo: sending the zero badge — " << ex.what();
	}

	std::string buffer{};
	const auto& ec = glz::write_json(resp, buffer);
	if (ec)
	{
		const auto& glze = glz::format_error(ec, buffer);
		LOG_DEBUG << "Gme BadgeInfo Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}
