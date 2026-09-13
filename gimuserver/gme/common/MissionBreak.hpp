#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <string>
#include <vector>

// The interrupted battle a player can be offered at their next login.
//
// WHAT THE SERVER ACTUALLY OWNS HERE IS THE TRIGGER, not the battle.
// MissionRestartScene::initialize @0x17FC7A8 reloads the suspend blob from the
// client's OWN SaveData (existMissionSuspendData / decodeMissionSuspendData)
// and feeds that to setMissionBreakInfo, so the battle state is
// client-authoritative in the same way Grand Quest's campaignEncodeSuspendData
// is.  But LoginScene::changeNextScene @0x174C544 never reaches that scene
// unless the server's `5PR2VmH1` block carries a non-zero state — so without
// this record a revival the player PAID A GEM FOR is forgotten the moment the
// client closes.
//
// The blob is stored and echoed anyway.  It costs one TEXT column, it is what
// the client sent, and MissionBreakInfo::setMissionBreakInfo is a real setter
// on the response — sending the field empty while claiming a battle is open
// would be the less honest of the two options.
//
// Three things can close a record, and all of them are cheap:
//   * MissionEnd — the battle finished, win or lose.
//   * MissionStart — a new battle is beginning, so whatever was open is gone.
//   * a login where the client turns out to have no local suspend data, which
//     the CLIENT handles on its own (setMissionRestartFlg(false)) and which
//     costs a wasted screen rather than a wedged login.
// Erring towards keeping a record slightly too long is therefore the safe
// direction, because the client has the last word on whether it can resume.

namespace gme
{

/*!
* The `5PR2VmH1` block for a user: the open battle, or "nothing open".
*
* A SINGLETON on the client (MissionBreakInfo::shared(), no list operations in
* readParam @0x13E20B4), and modelled as `::size(1)` rather than a list for the
* same reason — so this returns the one row, with state 0 when there is nothing
* to resume, which is what LoginScene reads as "go to Home".
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity.
* @return The MissionBreakInfo row.
*/
inline drogon::Task<::MissionBreakInfo> loadMissionBreak(
	const db::Database database,
	const UserIdentity identity)
{
	::MissionBreakInfo info = {};
	info.state = 0;

	try
	{
		const auto rows = co_await database->execSqlCoro(
			"SELECT mission_break_serial, mission_break_info, mission_break_state"
			" FROM user_info WHERE id = $1;",
			identity.userId);
		if (!rows.empty())
		{
			info.mission_serial_id  = rows[0]["mission_break_serial"].as<std::string>();
			info.mission_break_info = rows[0]["mission_break_info"].as<std::string>();
			info.state              = rows[0]["mission_break_state"].as<int32_t>();
		}
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		// A login must never fail because of a resume offer.  An unreadable
		// record reads as "nothing open", which is the safe direction.
		LOG_WARN << "loadMissionBreak: " << ex.base().what();
		info = {};
		info.state = 0;
	}

	// A record that names no battle cannot be resumed, and telling the client
	// otherwise sends it to the restart screen with nothing to restart.
	if (info.mission_serial_id.empty() || info.mission_serial_id == "0")
	{
		info = {};
		info.state = 0;
	}

	co_return info;
}

/*!
* Records an interrupted battle.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity.
* @param serial   The battle's serial, as the client reported it.
* @param blob     The client's suspend data, stored verbatim and never parsed.
* @param state    The MissionStatus the interruption carried (4 continue, 6
*                 auto-continue, 5 restart).  Any non-zero value arms the
*                 resume; 0 would silently disarm it, so it is refused.
*/
inline drogon::Task<void> recordMissionBreak(
	const db::Database database,
	const UserIdentity identity,
	const std::string serial,
	const std::string blob,
	const int32_t state)
{
	if (serial.empty() || serial == "0" || state == 0)
	{
		LOG_WARN << "recordMissionBreak: refusing an unusable record for "
			<< identity.userId << " (serial \"" << serial << "\", state " << state << ")";
		co_return;
	}

	try
	{
		co_await database->execSqlCoro(
			"UPDATE user_info SET mission_break_serial = $1, mission_break_info = $2,"
			" mission_break_state = $3 WHERE id = $4;",
			serial, blob, state, identity.userId);
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		LOG_WARN << "recordMissionBreak: " << ex.base().what();
	}
	co_return;
}

/*!
* Closes the open battle record, if there is one.
*
* Called wherever a battle stops being resumable: the end of one, and the start
* of the next.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity.
*/
inline drogon::Task<void> clearMissionBreak(
	const db::Database database,
	const UserIdentity identity)
{
	try
	{
		co_await database->execSqlCoro(
			"UPDATE user_info SET mission_break_serial = '', mission_break_info = '',"
			" mission_break_state = 0 WHERE id = $1 AND mission_break_state != 0;",
			identity.userId);
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		LOG_WARN << "clearMissionBreak: " << ex.base().what();
	}
	co_return;
}

} // namespace gme
