#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/MissionBreak.hpp>
#include <gimuserver/gme/common/MissionRuns.hpp>
#include <gimuserver/gme/common/Transactions.hpp>

#include <drogon/utils/Utilities.h>

#include <chrono>
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

// WHICH REVIVAL.  The request carries no nonce.  BaseRequest::create runs
// createBody once, when the scene builds the request, and BaseRequest::
// getSendData @0x139FDB8 only serialises those groups and AES-ECB encrypts
// them -- deterministically.  Both retry paths re-send the SAME object:
// HttpConnector::retry -> WrapAsyncHttpConnector::retry @0x1005ED8 calls
// getSendData on it again, and NetworkManager::OnErrorRetryPressed @0xF5DCCC
// re-sends the very CCHttpRequest.  So a retry repeats serial, status and
// suspend blob byte for byte, while a genuine later revival carries a new
// blob: MissionScene::createSuspendData writes the turn counters, every
// unit's HP, buffs, gauges and the battle, skill and item logs.
//
// What is NOT used: 6FrKacq7.Kn51uR4Y is the server's own signal key echoed
// back (SignalKeyResponse::readParam is its only writer, createSignalKeyTag
// its only reader) and this server echoes a constant; the envelope's
// F4q6i9xe.aV6cLn3v is IntToString(CCObject::m_uID) of the request object --
// a per-process creation counter that restarts with the client, never
// reaches a handler, and has not been checked on the Windows build.
//
// The limit that leaves: two GENUINE revivals of one run whose status and
// blob are byte-identical would be taken for one, the second free.  The
// blob's turn counters and logs make that unreachable in a real battle, and
// a free revival is the cheap direction.
std::string revivalFingerprint(const int32_t status, const std::string& blob)
{
	return drogon::utils::getSha256(std::to_string(status) + ':' + blob);
}

// How one MissionContinue was judged; see the handler.
enum class Revival
{
	Accepted,      // charged, recorded, marked continued
	Repeated,      // a copy of a revival already accepted in this run
	Superseded,    // a copy of a refused revival the run has since moved past
	NoGems,        // refused: the balance does not cover the price
	NotOpenBattle, // refused: not the open battle, or a body naming two
};
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

	// WHICH BATTLE.  MissionContinueRequest::createBody @0x13A7528 writes
	// MissionInfo::getMissionSerialID into BOTH groups -- Kz7qfSs5.k9cxD7Ba and
	// 5PR2VmH1.k9cxD7Ba -- next to MissionScene::createSuspendData's blob, so a
	// body whose two serials disagree was not built by the client.  The serial
	// in it is the one MissionStart issued (gme/common/MissionRuns.hpp), or the
	// one the login's 5PR2VmH1 handed back to MissionRestartScene::initialize
	// @0x17FC7A8 for a resumed battle, which is the same value.
	const uint32_t wireSerial = req.mission_num.serial_id;
	const auto serialText = std::to_string(wireSerial);
	const auto& breakSerial = req.mission_break.mission_serial_id;
	const auto& blob = req.mission_break.mission_break_info;

	// A status of 0 would disarm the resume record rather than arm it (see
	// gme::recordMissionBreak); the builder only ever sends 4, or 6 under
	// auto-continue, so a 0 is treated like a body that names two battles.
	const auto state = req.mission_num.mission_status.value_or(4);
	const auto fingerprint = revivalFingerprint(state, blob);
	const auto cost = continueGemCost();
	const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	::MissionContinueResp resp{};
	resp.signal_key = req.signal_key;
	std::string body;
	auto verdict = Revival::NotOpenBattle;
	int64_t gemsBefore = 0;

	// ONE TRANSACTION: the ownership check, the receipt, the charge, the
	// "continued" marker and the resume record commit together or not at all,
	// so a failure part way cannot keep the gem and lose the record (or the
	// reverse), cannot leave a receipt that would turn the player's next try
	// into a "repeat", and a MissionEnd for the same battle is ordered strictly
	// before or after it.  Success is reported only once the commit has.
	auto transaction = co_await theDb()->newTransactionCoro();
	try
	{
		const auto rows = co_await transaction->execSqlCoro(
			"SELECT gems, open_mission_id, open_mission_serial FROM user_info WHERE id = $1;",
			identity.userId);
		if (rows.empty())
			throw std::runtime_error("MissionContinue: no user_info row");
		const auto& user = rows[0];
		gemsBefore = user["gems"].as<int64_t>();

		// ONLY THE OPEN BATTLE CAN BE CONTINUED.  Same ownership rule as
		// MissionEnd's settlement: an issued serial must be the open run's; a
		// mission-id serial is honoured only while the open battle predates
		// serials (open_mission_serial NULL -- see MissionEnd).  Anything else
		// -- a superseded or settled run, a serial never issued to this player
		// (unknown or someone else's), or a body whose two serials disagree --
		// revives nothing the server can stand behind: no charge, no receipt,
		// no "continued" mark on whatever mission is open now, and the resume
		// record keeps pointing at the open battle.
		const bool serialEra = !user["open_mission_serial"].isNull();
		const auto openSerial = serialEra ? user["open_mission_serial"].as<int64_t>() : int64_t{ 0 };
		const bool openUnknown = user["open_mission_id"].isNull();
		const auto openMission = openUnknown ? int64_t{ -1 } : user["open_mission_id"].as<int64_t>();
		const bool conflicting = (!breakSerial.empty() && breakSerial != serialText) || state == 0;
		const bool owned = wireSerial >= gme::kMissionRunSerialFloor
			? serialEra && openSerial == static_cast<int64_t>(wireSerial)
			: !serialEra && wireSerial != 0
				&& (openUnknown || openMission == static_cast<int64_t>(wireSerial));

		if (!owned || conflicting)
		{
			LOG_WARN << "MissionContinue: " << identity.userId << " continued serial " << wireSerial
				<< " (break block \"" << breakSerial << "\", status " << state << ") but the open battle is "
				<< openMission << " (serial " << (serialEra ? std::to_string(openSerial) : std::string{ "pre-serial" })
				<< "); refused, nothing charged or recorded";
		}
		else
		{
			// EVERY REVIVAL THIS RUN HAS SEEN, not just the latest: a delayed
			// copy of revival A that arrives after a genuine revival B matches
			// A's receipt, so it neither pays again nor rewinds B's resume data.
			// The run is the serial (issued serials are unique; a pre-serial
			// battle is the save's one open battle of that mission, and no new
			// one can be opened without issuing a serial).
			const auto seen = co_await transaction->execSqlCoro(
				"SELECT seq, accepted FROM user_mission_continue_receipts"
				" WHERE user_id = $1 AND run_serial = $2 AND fingerprint = $3;",
				identity.userId, serialText, fingerprint);

			bool movedOn = false;
			if (!seen.empty() && seen[0]["accepted"].as<int32_t>() == 0)
			{
				// A refusal is not a verdict on later tries: once the player
				// has the gems, the same revival is accepted -- unless the run
				// has seen a newer revival since, which means this copy is a
				// late duplicate of a try the client moved on from.
				const auto newer = co_await transaction->execSqlCoro(
					"SELECT 1 FROM user_mission_continue_receipts"
					" WHERE user_id = $1 AND run_serial = $2 AND seq > $3 LIMIT 1;",
					identity.userId, serialText, seen[0]["seq"].as<int64_t>());
				movedOn = !newer.empty();
			}

			if (!seen.empty() && seen[0]["accepted"].as<int32_t>() != 0)
				verdict = Revival::Repeated;
			else if (movedOn)
				verdict = Revival::Superseded;
			else if (gemsBefore < cost)
				verdict = Revival::NoGems;
			else
				verdict = Revival::Accepted;

			if (seen.empty())
			{
				co_await transaction->execSqlCoro(
					"INSERT INTO user_mission_continue_receipts"
					" (user_id, run_serial, fingerprint, seq, accepted, gems_charged, first_seen, last_seen)"
					" SELECT $1, $2, $3, COALESCE(MAX(seq), 0) + 1, $4, $5, $6, $7"
					" FROM user_mission_continue_receipts WHERE user_id = $8 AND run_serial = $9;",
					identity.userId, serialText, fingerprint,
					verdict == Revival::Accepted ? 1 : 0,
					verdict == Revival::Accepted ? static_cast<int64_t>(cost) : int64_t{ 0 },
					now, now, identity.userId, serialText);
			}
			else
			{
				// $N in strict first-appearance order (the sqlite named-param
				// gotcha -- see CampaignReceipt).  A repeat only counts the
				// delivery; an acceptance upgrades the refused row it re-tried.
				const auto updated = co_await transaction->execSqlCoro(
					"UPDATE user_mission_continue_receipts SET deliveries = deliveries + 1, last_seen = $1,"
					" accepted = CASE WHEN $2 THEN 1 ELSE accepted END,"
					" gems_charged = CASE WHEN $3 THEN $4 ELSE gems_charged END"
					" WHERE user_id = $5 AND run_serial = $6 AND fingerprint = $7;",
					now, verdict == Revival::Accepted ? 1 : 0, verdict == Revival::Accepted ? 1 : 0,
					static_cast<int64_t>(cost), identity.userId, serialText, fingerprint);
				if (updated.affectedRows() != 1)
					throw std::runtime_error("MissionContinue: receipt changed under the request");
			}

			if (verdict == Revival::Accepted)
			{
				// CHARGE.  Guarded on the balance just read, so nothing can
				// spend the same gem twice.
				const auto charged = co_await transaction->execSqlCoro(
					"UPDATE user_info SET gems = gems - $1 WHERE id = $2 AND gems >= $3;",
					static_cast<int64_t>(cost), identity.userId, static_cast<int64_t>(cost));
				if (charged.affectedRows() != 1)
					throw std::runtime_error("MissionContinue: balance changed under the charge");

				// MARK THE RUN AS CONTINUED, for the "Cleared (No Continues)"
				// achievements.  Recorded against the MISSION the open battle
				// is: MissionStart/MissionEnd bound the run either side.  (An
				// issued run always has open_mission_id beside it; only a save
				// older than that column leaves it NULL, and there the
				// mission-id serial IS the mission.)
				const auto missionId = openUnknown ? static_cast<int64_t>(wireSerial) : openMission;
				co_await transaction->execSqlCoro(
					"INSERT INTO user_mission_continues (user_id, mission_id) VALUES ($1, $2)"
					" ON CONFLICT(user_id, mission_id) DO NOTHING;",
					identity.userId, std::to_string(missionId));

				// REMEMBER THE BATTLE.  The blob and serial are what the next
				// login hands back as 5PR2VmH1, and a non-zero state is the
				// only thing that gets the client to the restart screen.
				// Written here rather than through gme::recordMissionBreak,
				// which logs and swallows a failure -- inside this transaction
				// a failure has to undo the charge with it.
				co_await transaction->execSqlCoro(
					"UPDATE user_info SET mission_break_serial = $1, mission_break_info = $2,"
					" mission_break_state = $3 WHERE id = $4;",
					serialText, blob, state, identity.userId);
			}
		}

		resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
		if (const auto error = glz::write_json(resp, body); error)
			throw std::runtime_error(glz::format_error(error, body));
	}
	catch (const std::exception& ex)
	{
		transaction->rollback();
		LOG_ERROR << "MissionContinue: " << identity.userId << " serial " << wireSerial
			<< " failed, nothing kept: " << ex.what();
		co_return HandleResult::refuseToRetry("The Continue could not be saved. Please try again.", ex.what());
	}
	catch (...)
	{
		// Never let an unexpected exception leave with the transaction still
		// open: Drogon would commit whatever was written so far.
		transaction->rollback();
		LOG_ERROR << "MissionContinue: " << identity.userId << " serial " << wireSerial
			<< " failed with a non-standard exception, nothing kept";
		co_return HandleResult::refuseToRetry("The Continue could not be saved. Please try again.",
			"unknown exception");
	}

	if (!(co_await gme::CommitTransaction(std::move(transaction))))
	{
		LOG_ERROR << "MissionContinue: " << identity.userId << " serial " << wireSerial << " did not commit";
		co_return HandleResult::refuseToRetry("The Continue could not be saved. Please try again.",
			"commit failed");
	}

	// THE REPLY DECIDES WHAT THE CLIENT DOES.  Every request completion runs the
	// scene's checkConnectResult first (GameScene::update @0x1601470, slot
	// +0x310); MissionGameOverScene inherits GameScene's, which shows an error
	// through checkResponseMessage.  An ordinary reply leaves no message, so
	// loopContinue @0x17FB474 calls MissionScene::requestContinue -- the
	// revival.  A Retry (cmd 2) error shows its notice, and noticeOK(-4000)
	// @0x17FB62C returns the scene to initContinueConfirm: no revival, the
	// player can try again or give up.  (Close would exit the app and
	// ReturnToGame abandon the battle at Home.)  So a revival the server did
	// not accept gets a Retry, never an ordinary reply: an ordinary reply IS
	// the revival.  Accepted and repeated revivals -- both paid -- get the
	// ordinary reply with the post-charge header.
	switch (verdict)
	{
	case Revival::Accepted:
		LOG_INFO << "MissionContinue: " << identity.userId << " serial " << wireSerial << " revived for "
			<< cost << " gem(s); " << resp.team_info.brave_coin << " left";
		co_return HandleResult::success(body);
	case Revival::Repeated:
		LOG_INFO << "MissionContinue: " << identity.userId << " serial " << wireSerial
			<< " repeated a revival already paid for; not charged again, resume record untouched";
		co_return HandleResult::success(body);
	case Revival::Superseded:
		LOG_WARN << "MissionContinue: " << identity.userId << " serial " << wireSerial
			<< " re-sent a refused revival after a newer one; ignored";
		co_return HandleResult::refuseToRetry("This Continue is no longer valid.", "superseded revival");
	case Revival::NoGems:
		LOG_WARN << "MissionContinue: " << identity.userId << " has " << gemsBefore << " gem(s), the revival costs "
			<< cost << "; refused, nothing charged or recorded";
		co_return HandleResult::refuseToRetry("You do not have enough Gems to continue.", "insufficient gems");
	case Revival::NotOpenBattle:
	default:
		co_return HandleResult::refuseToRetry("This battle can no longer be continued.", "not the open battle");
	}
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
