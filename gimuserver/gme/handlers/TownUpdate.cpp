#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/Town.hpp>

// TownUpdate (CuQ5oB8U) — the resource-tile tap report.
//
// Despite the name this is not in-town navigation.  MyTownTopScene::collectItem
// (libgame.so 0x18F244C) queues one of these after every tap on a sparkling
// tile, carrying MyTownCollectLogList as "<locationId>:<tapCnt>,..." under
// EuY6L7AX[0].0mRaAo39.
//
// The client applies the rewards locally before sending, straight off the
// pre-rolled drop list the server already handed it in
// UserTownLocationDetail.drop_item_info — so this handler is not deciding
// anything, it is committing what the player already saw.  It resolves the same
// entries by index (getCollectItemInfo reads element `count - tap_cnt`) and
// credits them, which is why there is nothing to desync.
//
// Full model: tools/TOWN_STATE_MODEL.md.

HANDLEF(TownUpdate)
{
	(void)session;
	LOG_INFO << "TownUpdate: " << json;

	TownUpdateReq req{};
	if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		LOG_WARN << "TownUpdate: parse error: " << glz::format_error(ec, json);
		co_return HandleResult::success("{}");
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	co_await gme::Town::applyTaps(theDb(), identity, req.collect.collect_log);

	// Trophy 100280 村採取タッチ数 -- the number of TAPS, counted off the same
	// "<locationId>:<tapCnt>,..." log applyTaps just consumed, so the counter
	// and the harvest cannot disagree about how many touches happened.
	{
		int64_t taps = 0;
		const auto& log = req.collect.collect_log;
		for (size_t at = 0; at < log.size();)
		{
			const auto comma = log.find(',', at);
			const auto entry = log.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
			const auto colon = entry.find(':');
			if (colon != std::string::npos)
			{
				try { taps += std::stoll(entry.substr(colon + 1)); }
				catch (const std::exception&) { /* a malformed entry counts as zero */ }
			}
			if (comma == std::string::npos) break;
			at = comma + 1;
		}

		co_await gme::bumpArchiveCounters(theDb(), identity, {
			{ "town_harvest_cnt", taps },
		});
	}

	// LzKDI2i7 (the owned sound-room track list) rides along on the same
	// request.  It is not persisted: nothing server-side reads it back, and the
	// sound room is a client-local purchase list.  Logged above with the rest of
	// the body so the first real purchase produces the capture.

	co_return HandleResult::success("{}");
}
