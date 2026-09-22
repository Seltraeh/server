#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/Gifts.hpp>

// GetGiftInfo (ifYoPJ46) - fired by GiftRecieveConnectScene::initConnect just
// to OPEN the gift screen.
//
// It answered `{}` when the only goal was to stop an unregistered GroupId from
// closing the session.  That is no longer enough: the screen renders straight
// from UserGiftInfoList, and setGiftList @0x166D330 null-derefs on an EMPTY
// list - a deterministic crash, identical in two minidumps (+0x84F533,
// read 0x14). So the inbox has to actually arrive.
//
// GIFTS ARE NOT THE PRESENT BOX. `UserGiftInfo` carries user_id_from and
// user_id_to: it is one player sending another a gift. Until a real friend
// system exists the sender is the simulated friend, which is legitimate in a
// way a fake purchase would not be -- a gift is NOT deducted from the sender,
// so nothing is being invented on anyone's behalf.
//
// The daily drop, the weighting toward the player's chosen gift, and the claim
// are all in gme::loadGiftInbox / gme::claimGifts.
HANDLEF(GetGiftInfo)
{
	(void)session;
	LOG_INFO << "GetGiftInfo: " << json;

	GetGiftInfoReq req = {};
	{
		glz::context ctx{};
		if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GetGiftInfo: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	GetGiftInfoResp resp{};
	resp.gift_info = co_await gme::loadGiftInbox(theDb(), identity);

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		const auto& glze = glz::format_error(ec, buffer);
		LOG_ERROR << "GetGiftInfo: serialization error: " << glze;
		// An error reply closes the session; an empty object merely leaves the
		// screen bare.
		co_return HandleResult::success("{}");
	}

	LOG_INFO << "GetGiftInfo: " << resp.gift_info.size() << " gift(s) for " << identity.userId;
	co_return HandleResult::success(buffer);
}
