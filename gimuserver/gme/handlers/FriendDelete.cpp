#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/Friends.hpp>

// FriendDelete (SfMN9w4p) — unfriend.
//
// Body, from FriendDeleteRequest::createBody @0x13A3EC0: one node `BPz1e5tU`
// carrying `h7eY3sAK`, the user id to drop.  FriendApply and FriendAgree send
// the identical shape, which is worth knowing before those are written.
//
// WHY THIS MATTERS BEYOND TIDINESS: a friend whose evolution chain tops out
// below the player stops appearing in the helper picker (see Friends.hpp).  The
// roster still holds them, on purpose, so the player can see who has fallen
// behind and decide.  Without a working unfriend that list would only ever grow
// and the stale entries could never be cleared -- the mechanic needs this
// handler to be the way out.
//
// The reply is `{}`: the client refreshes by calling FriendGet again rather
// than reading anything back from here.
HANDLEF(FriendDelete)
{
	(void)session;
	LOG_INFO << "FriendDelete: " << json;

	FriendDeleteReq req = {};
	{
		glz::context ctx{};
		if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "FriendDelete: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	int32_t removed = 0;
	try
	{
		for (const auto& node : req.nodes)
		{
			if (node.user_id.empty())
				continue;
			if (co_await gme::removeFriend(theDb(), identity, node.user_id))
				++removed;
			else
			{
				// Not an error: the client can resend, and a friend already
				// gone is the state the player asked for.
				LOG_INFO << "FriendDelete: " << node.user_id
					<< " was not on " << identity.userId << "'s roster";
			}
		}
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		LOG_ERROR << "FriendDelete: DB error: " << ex.base().what();
		// SUCCESS regardless: an error reply is GmeErrorCommand::Close, and
		// dropping the session is worse for the player than a failed unfriend.
		co_return HandleResult::success("{}");
	}

	LOG_INFO << "FriendDelete: removed " << removed << " friend(s) for " << identity.userId;
	co_return HandleResult::success("{}");
}
