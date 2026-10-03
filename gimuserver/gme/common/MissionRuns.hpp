#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <chrono>
#include <cstdint>
#include <optional>

namespace gme
{

/*!
* Battle serials.
*
* MissionNumInfo.serial_id (Kz7qfSs5.k9cxD7Ba) is the client's per-BATTLE token,
* not a mission id.  MissionNumResponse::readParam @0x13E39C8 stores it with
* MissionInfo::setMissionSerialID and every reader treats it as opaque:
* MissionEndRequest / MissionContinueRequest and the raid requests echo it,
* MissionScene::initBattle @0x18040AC seeds BattleState's random number
* generator from it (StrToLong -> setRandomSeed), and the start scenes tag the
* saved battle files and helper record with it (saveBattleMstFiles,
* encodeMissionReinforcement) so a resume can check it has the right battle.
* The mission itself travels separately (UserState::getLastMissionID, which
* the start scenes pass to requestMissionFiles).
*
* The server used to serve the mission id there, so every run of a mission had
* the same serial -- and the same battle seed -- and a repeated or late
* MissionEnd could not be told from the one that belonged to the open battle
* (#19).  Each MissionStart now issues its own serial from user_mission_runs,
* numbered above every mission id (mission_mst tops out at 100016023).  A serial
* below the floor is one the old server issued -- the mission id itself -- so
* a battle in flight across the upgrade still maps to its mission.
*/
inline constexpr uint32_t kMissionRunSerialFloor = 1'000'000'000u;

/*!
* The mission a MissionEnd / MissionContinue serial stands for.
*
* @return The mission id; the serial itself when it is below the floor; nullopt
*         for a run serial this user was never issued.
*/
inline drogon::Task<std::optional<uint32_t>> missionForSerial(
	const db::Database database,
	const UserIdentity identity,
	const uint32_t serial)
{
	if (serial < kMissionRunSerialFloor)
		co_return serial;
	const auto rows = co_await database->execSqlCoro(
		"SELECT mission_id FROM user_mission_runs WHERE serial = $1 AND user_id = $2;",
		static_cast<int64_t>(serial), identity.userId);
	if (rows.empty())
		co_return std::nullopt;
	co_return rows[0]["mission_id"].as<uint32_t>();
}

/*!
* Issues the serial for the battle a MissionStart is about to open.  Recording
* it as the OPEN battle is the caller's job, once the start has succeeded; a
* start that fails after this leaves an unused row that can never settle.
*/
inline drogon::Task<uint32_t> issueMissionSerial(
	const db::Database database,
	const UserIdentity identity,
	const uint32_t missionId)
{
	const auto now = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	const auto rows = co_await database->execSqlCoro(
		"INSERT INTO user_mission_runs (user_id, mission_id, started_at) VALUES ($1, $2, $3)"
		" RETURNING serial;",
		identity.userId, static_cast<int64_t>(missionId), static_cast<int64_t>(now));
	co_return rows[0]["serial"].as<uint32_t>();
}

} // namespace gme
