#include "App.hpp"
#include "Handlers.hpp"

// ControlCenterEnter (uYF93Mhc / d0k6LGUu).
//
// Fired by LoginScene::updateEvent and RandallSummonScene — NOT by any Slots
// scene — but it is what configures the Brave Slots machine, because
// getResponseObject is a global key→class registry (§7.12.4): any response
// carrying `C7J9hbmr` populates `SlotgameStandInfo` regardless of which handler
// sent it.  The machine is set up at login, long before the tile is opened.
//
// ⚠ THIS USED TO SEND THE INNER STRUCT AND THE CLIENT DROPPED THE WHOLE REPLY.
// `brave_slots.json` is stored as `SlotGameInfoR` — root keys `C38FmiUn` and
// `rY6j0Jvs` — and that was serialized verbatim.  But `getResponseObject`
// dispatches on **`C7J9hbmr`**, and `C38FmiUn` appears nowhere in that table:
// it is a FIELD inside the response, not the response.  So the reply could not
// be dispatched at all, `SlotgameStandInfo` stayed empty, and the Slots scene
// ran on default data — `RandallSlotActionScene::downloadFiles` asked for no
// sprites at all, because `getSlotgameInfo()->getSlotImage()` had nothing in
// it.  Classic §6.11: the handler "succeeded" every time.
//
// The second trap is in the same place: **`C38FmiUn` is a JSON STRING, not a
// nested object.**  `SlotgameInfoResponse::readParam` @0x1447E5C runs `strlen`
// then `picojson::parse` over the value and looks the inner fields up in the
// resulting map, so the whole `SlotGameInfo` has to be serialized and embedded
// as one string.  Sending it as an object leaves every inner field unset.
HANDLEF(ControlCenterEnter)
{
	const auto& stored = theServer()->cache().braveSlotsResp();

	// The inner machine config travels as its own JSON document, embedded as a
	// string — see the note above.
	std::string gameInfo{};
	if (const auto& ec = glz::write_json(stored.info, gameInfo); ec)
	{
		const auto& glze = glz::format_error(ec, gameInfo);
		LOG_DEBUG << "Gme ControlCenterEnter: cannot serialize slot game info: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	::ControlCenterEnterResp resp{};
	resp.slotgame.game_info = std::move(gameInfo);
	resp.slotgame.pictures = stored.pictures;
	// iW62Scdg is the third field SlotgameInfoResponse handles.  We have no reel
	// definitions to send, so it goes out empty rather than fabricated.

	std::string buffer{};
	const auto& ec = glz::write_json(resp, buffer);
	if (ec)
	{
		const auto& glze = glz::format_error(ec, buffer);
		LOG_DEBUG << "Gme ControlCenterEnter Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}
