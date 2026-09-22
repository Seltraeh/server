#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/MissionBreak.hpp>

// The two halves of "your battle was interrupted" — both of them unregistered
// GroupIds until now, which means both of them would have closed the session
// the moment a player used them (handbook §7).
//
//   MissionContinue  p8B2i9rJ / G3FwvQfy5hcxHMen   the squad wiped, pay to revive
//   MissionRestart   IP96ys7T / 0Zy3G9eD           resume it at the next login
//
// Neither GroupId appears in any captured session here, which is not surprising
// — the continue prompt only shows on a defeat and the restart screen is only
// reachable once a break record exists, which it never could.  Registering them
// is what makes the feature reachable at all; see net/mission.kdl for the flow
// and gme/common/MissionBreak.hpp for what the server does and does not own.

namespace
{
// What a revival costs, from DefineMst continu_dia_cnt (QW3HiNv8) — 1 gem in
// this data.  MissionGameOverScene::initContinueConfirm @0x17F8720 reads the
// same value through DefineMst::getContinuDiaCnt to fill the confirm window's
// "use_dia_num", so charging anything else would contradict the price the
// player was shown.
int32_t continueGemCost()
{
	const auto cost = theServer()->cache().initializeResp().defines.continue_dia_count;
	return cost > 0 ? cost : 1;
}
}

HANDLEF(MissionContinue)
{
	(void)session;
	LOG_INFO << "MissionContinue: " << json;

	::MissionContinueReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "MissionContinue: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// MARK THE RUN AS CONTINUED, for the "Cleared (No Continues)" achievements.
	// Recorded against the MISSION, not a battle serial -- this request carries
	// no serial, and MissionStart/MissionEnd bound the run either side, so the
	// mission id is enough to scope it.  Written before the charge because the
	// revival happens whether or not the payment does (see below): a continue
	// the player did not pay for is still a continue.
	co_await theDb()->execSqlCoro(
		"INSERT INTO user_mission_continues (user_id, mission_id) VALUES ($1, $2)"
		" ON CONFLICT(user_id, mission_id) DO NOTHING;",
		identity.userId, std::to_string(req.mission_num.serial_id));

	// CHARGE, BUT NEVER REFUSE.
	//
	// The gate is already on the client: initContinueConfirm shows the price
	// against the player's balance and initShopConfirm is the branch for being
	// short, so reaching here means the client believed it could pay.  If the
	// two disagree the battle is already mid-revival — MissionGameOverScene::
	// loopContinue @0x17FB474 waits only for the request flag to clear and then
	// resumes regardless of what came back — so refusing would desync the fight
	// rather than prevent the spend.  Log the disagreement and let it through
	// without charging; a free revival is the cheaper failure.
	const auto cost = continueGemCost();
	const auto row = co_await db::DatabaseInterface::read(
		theDb(),
		"user_info",
		{
			db::Data("gems"),
			db::Lookup("id", identity.userId),
		});
	const auto gems = row.front<int64_t>("gems");

	if (gems >= cost)
	{
		(co_await db::DatabaseInterface::update(
			theDb(),
			"user_info",
			{
				db::Data("gems", gems - cost),
				db::Lookup("id", identity.userId),
			})).nonEmpty();
	}
	else
	{
		LOG_WARN << "MissionContinue: " << identity.userId << " has " << gems
			<< " gem(s) but the revival costs " << cost
			<< "; the client had already committed, so it is not charged";
	}

	// REMEMBER THE BATTLE.  This is the whole reason the request matters to the
	// server: the blob and serial are what the next login hands back as
	// 5PR2VmH1, and a non-zero state is the only thing that gets the client to
	// the restart screen.
	co_await gme::recordMissionBreak(
		theDb(),
		identity,
		req.mission_break.mission_serial_id.empty()
			? std::to_string(req.mission_num.serial_id)
			: req.mission_break.mission_serial_id,
		req.mission_break.mission_break_info,
		req.mission_num.mission_status.value_or(4));

	::MissionContinueResp resp{};
	resp.signal_key = req.signal_key;
	resp.team_info = std::move((co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());

	LOG_INFO << "MissionContinue: " << identity.userId << " revived; " << resp.team_info.brave_coin
		<< " gem(s) left, break recorded for serial "
		<< req.mission_break.mission_serial_id;

	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

HANDLEF(MissionRestart)
{
	(void)session;
	LOG_INFO << "MissionRestart: " << json;

	::MissionRestartReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "MissionRestart: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// THE RECORD IS NOT CLEARED HERE.  Resuming a battle does not finish it —
	// MissionEnd does, and MissionStart supersedes it.  Clearing on the resume
	// would mean a client that dies twice in the same fight loses the offer the
	// second time even though it still holds the suspend data.
	//
	// There is nothing else to do: the battle is rebuilt from the client's own
	// SaveData, and MissionRestartScene::checkConnectResult @0x17FEAF8 only
	// looks for an error.  Answering at all is the fix.
	::MissionRestartResp resp{};
	resp.signal_key = req.signal_key;
	resp.team_info = std::move((co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());

	LOG_INFO << "MissionRestart: " << identity.userId << " resuming serial "
		<< req.mission_num.serial_id;

	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
