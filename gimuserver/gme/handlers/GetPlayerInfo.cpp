#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

// GetPlayerInfo (vUQrAV65 / 7pW4xF9H) — the Records / Archive screens' fetch.
//
// Resolved 2026-08-30 from the .rodata GroupId block: the AES key sits at
// GroupId+9 and the mangled class name ends the block —
//
//   "vUQrAV65\0" "7pW4xF9H\0" "/actionSymbol/DheJ07aI.php\0" "20GetPlayerInfoRequest"
//
// Until this existed the client got a handler error on the "Record" button,
// which is what surfaced it.
//
// WHO SENDS IT (GetPlayerInfoRequest ctor xrefs):
//   * PlayerInfoBattleResultScene::initConnect   — the Records screen
//   * ArenaArchiveScene::initConnect
//   * ColosseumArchiveScene::initConnect
//   * ColosseumBattleEndScene::updateEvent
//
// Those are exactly the three scenes that call
// PlayerInfoBattleResultScene::getActual (libgame.so 0x1791EEC), the 32KB
// trophy-id chain that answers one trophy per UserTeamArchive counter.  So this
// request is how the trophy screens get their progress: the archive blocks ride
// in UserInfo for the Home path, and here for the Records path.
//
// REQUEST: GetPlayerInfoRequest::createBody @0x13A4E64 is 44 bytes and adds
// NOTHING beyond the standard user-info / signal-key / version tags.  There are
// no parameters to read; the whole contract is the response.
//
// RESPONSE: the two archive blocks, keyed the same way UserInfo keys them.
// getResponseObject is a global key -> class registry (handbook §7.12.4), so
// zI2tJB7R and PQ56vbkI dispatch to UserTeamArchiveResponse and
// UserTeamArenaArchiveResponse regardless of which handler carried them — the
// same argument TownFacilityUpdate uses for emitting 51yQrDBR.
//
// Both are singleton readParams with no list ops, so re-sending them on every
// Records visit is a clean overwrite, not an append.
//
// PQ56vbkI (the arena half) is still all zeros: nothing instruments arena
// battles yet.  It is sent anyway because the class is a singleton — omitting it
// leaves whatever the last response put there, and a stale arena record is worse
// than an honest zero.

HANDLEF(GetPlayerInfo)
{
	(void)session;
	LOG_INFO << "GetPlayerInfo: " << json;

	GetPlayerInfoReq req = {};
	{
		glz::context ctx{};
		if (const auto& ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
		{
			LOG_WARN << "GetPlayerInfo: bad request JSON: " << glz::format_error(ec, json);
			co_return HandleResult::error("Deserialization error");
		}
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	GetPlayerInfoResp resp = {};
	resp.archive = co_await gme::loadTeamArchive(theDb(), identity);
	resp.arena_archive = gme::zeroedArenaArchive(identity);
	resp.team_info = std::move((co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		LOG_ERROR << "GetPlayerInfo: serialization error: " << glz::format_error(ec, buffer);
		co_return HandleResult::error("Serialization error");
	}

	co_return HandleResult::success(buffer);
}
