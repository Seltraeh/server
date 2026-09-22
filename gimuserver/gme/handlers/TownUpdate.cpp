#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/SoundRoom.hpp>
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
		co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	auto transaction = co_await theDb()->newTransactionCoro();
	try
	{
		const auto taps = co_await gme::Town::applyTaps(transaction, identity, req.collect.collect_log);
		if (taps > 0)
			co_await transaction->execSqlCoro(
				"INSERT INTO user_team_archive (user_id, town_harvest_cnt) VALUES ($1, $2)"
				" ON CONFLICT(user_id) DO UPDATE SET town_harvest_cnt = town_harvest_cnt + excluded.town_harvest_cnt;",
				identity.userId, taps);
		co_await gme::recordBoughtSounds(transaction, identity, req.collect.bought_sound_ids);
	}
	catch (const std::exception& ex)
	{
		transaction->rollback();
		co_return HandleResult::error("Town update failed", ex.what());
	}

	co_return HandleResult::success("{}");
}
