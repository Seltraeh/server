#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/Gifts.hpp>

// FixGiftInfo (gLRIn74v) — the gift screen's ONE write, and the Receive button.
//
// It had been a `{}` acknowledgement since the old tree, which is why claiming a
// gift did nothing.  FixGiftInfoRequest::createBody @0x13A37E4 shows it carries
// three things at once:
//
//   s2WnRw9N  the gift the player is ASKING friends for (UserTeamInfo::
//             getWantGift) — the preference that biases tomorrow's drop
//   23Xi0jom  comma-separated gift_identify_ids being CLAIMED, joined from
//             UserState::getUserGiftInfoListRecieve
//   qD74LPXb  gifts being SENT, as `user_id_to:gift_id` pairs
//
// Sending is parsed and deliberately ignored: there is nobody to send to until
// a real friend system exists, and silently accepting it is better than
// refusing — an error reply is GmeErrorCommand::Close, which drops the session.
//
// The request also fires after MissionEnd, where every list is empty. That path
// must stay cheap and must not mint or claim anything, which it does not: an
// empty `23Xi0jom` claims nothing and an unchanged want_gift writes nothing.
HANDLEF(FixGiftInfo)
{
	(void)session;
	LOG_INFO << "FixGiftInfo: " << json;

	FixGiftInfoReq req = {};
	{
		glz::context ctx{};
		if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "FixGiftInfo: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	std::string wantGift;
	std::string receiveIds;
	std::string sendPairs;
	for (const auto& node : req.nodes)
	{
		wantGift = node.want_gift;
		receiveIds = node.receive_ids;
		sendPairs = node.send_pairs;
	}

	int32_t paid = 0;
	try
	{
		auto transaction = co_await theDb()->newTransactionCoro();

		// The preference first, so that even a claim that grants nothing still
		// records what the player asked for.
		if (!wantGift.empty())
		{
			co_await transaction->execSqlCoro(
				"UPDATE user_info SET want_gift = $1 WHERE id = $2;",
				wantGift, identity.userId);
		}

		paid = co_await gme::claimGifts(transaction, identity, receiveIds);
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		LOG_ERROR << "FixGiftInfo: DB error: " << ex.base().what();
		// Still a SUCCESS: an error reply closes the session, and losing the
		// session is worse for the player than a claim that did not land.
		co_return HandleResult::success("{}");
	}

	if (!sendPairs.empty())
	{
		LOG_INFO << "FixGiftInfo: ignoring " << sendPairs
			<< " — sending needs a real friend to send to";
	}

	LOG_INFO << "FixGiftInfo: " << identity.userId << " claimed " << paid
		<< " gift(s)" << (wantGift.empty() ? "" : ", wants " + wantGift);

	co_return HandleResult::success("{}");
}
