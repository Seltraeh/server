#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>

// Hunter Orbs — the attempt currency Frontier Gate and Frontier Hunter share.
//
// They are NOT `team_info.fight_point`.  That is the ARENA Orbs
// (ShopHelFightScene's title string is "Arena Orbs"), and an earlier session
// spent a while restoring it and watching the Frontier Gate prompt keep
// appearing.  Hunter Orbs are internally "Aube" and live in the client's
// `ChallengeHeaderInfo` singleton, whose ONLY writer is the `kN2i7qds`
// ChallengeUserTeam response — a response this server had never sent.  So the
// count sat at the default 0 forever and every Frontier Gate entry took
// FrontierGateConditionScene::startCheck's no-orbs branch (@0x15DE978: it calls
// entryCheck only when getAube() > 0).
//
// Which of that response's three unnamed fields was the count had been left to
// a probe, and the probe came back inconclusive because every value it tried
// sat above the cap.  Disassembling ChallengeUserTeamResponse::readParam
// @0x13D3F94 settles it outright — see the citations in net/handlers.kdl.
//
// THE CLIENT RUNS THE CLOCK ITSELF.  `decFightRestTimer` @0x1258270 ticks the
// rest timer down one second at a time and, each time the remainder divides
// evenly by DefineMst recover_time_frohun, adds an orb and reloads the timer,
// stopping at max_frohun_p.  So the server's job is only to hand over an
// honest starting state — how many orbs are held, and how many seconds are left
// on the one currently refilling.  Both sides then agree without either
// polling the other.
//
// SPENDING is split, and the split is in the binary rather than a convention:
// MissionStartScene::initConnect @0x1823358 debits an orb client-side, but only
// when `FrontierGateUtils::nowFrontierGate` is FALSE.  A Frontier Gate entry is
// deliberately not debited there, so that charge belongs to the server.

namespace gme
{

/*!
* The orb cap, from DefineMst `max_frohun_p` (`SziK4Xg1`, 3 in this data).
*
* ChallengeHeaderInfo::getAubeMax @0x125819C forwards straight to it, and the
* client clamps to it on every read — which is exactly why the earlier 4/5/6
* probe read back as 3/3/3 and could not tell the three fields apart.
*/
inline int32_t hunterOrbCap()
{
	const auto cap = theServer()->cache().initializeResp().defines.max_frohun_p;
	return cap > 0 ? cap : 3;
}

/*! Seconds to regenerate one orb, from DefineMst `recover_time_frohun` (10800). */
inline int32_t hunterOrbRegenSeconds()
{
	const auto secs = theServer()->cache().initializeResp().defines.recover_time_frohun;
	return secs > 0 ? secs : 10800;
}

/*!
* The orb state as the client should see it right now.
*
* Stored state is the count at the time of the last write plus `restTs`, the
* unix time the orb then regenerating is due.  Everything since is caught up
* here rather than by a background job, the same way UserEnergy derives from
* `energy_full_ts`.
*
* @param orbs   Stored count.  Mutated to the derived current count.
* @param restTs Stored unix time the next orb lands; 0 when nothing is pending.
*               Mutated to the still-pending deadline (0 once at the cap).
* @return Seconds remaining on the orb being refilled, 0 when at the cap.
*/
inline int32_t deriveHunterOrbs(int32_t& orbs, int64_t& restTs)
{
	const auto cap = hunterOrbCap();
	const auto regen = hunterOrbRegenSeconds();

	if (orbs >= cap || restTs <= 0)
	{
		// At the cap, or nothing was ever spent: no clock running.  Clamp
		// rather than trusting an overcapped column — the client clamps too, so
		// a stored 5 would only ever read as 3 and the two would disagree.
		orbs = std::min(orbs, cap);
		restTs = 0;
		return 0;
	}

	const auto now = static_cast<int64_t>(
		std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
	if (now < restTs)
	{
		return static_cast<int32_t>(restTs - now);
	}

	// The pending orb landed, and possibly several more behind it.
	const auto elapsed = now - restTs;
	const auto extra = static_cast<int64_t>(elapsed / regen);
	orbs = static_cast<int32_t>(std::min<int64_t>(orbs + 1 + extra, cap));
	if (orbs >= cap)
	{
		restTs = 0;
		return 0;
	}

	restTs = restTs + (extra + 1) * regen;
	return static_cast<int32_t>(restTs - now);
}

/*!
* Fills the `kN2i7qds` header block for a user.
*
* Reads the stored orb state, catches it up, and writes the caught-up values
* back so the next read starts from them.  HR and the Frontier Gate score ride
* the same block because the client keeps them in the same singleton.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity.
* @param hrId Hunter Rank to report.  Read by FrontierGateUtils::entryCheck and
*             drawn in the header.
* @param frogateScore Frontier Gate score for the header.
* @return The block, ready to assign to a `user_team` field.
*/
inline drogon::Task<::ChallengeUserTeam> loadChallengeHeader(
	const db::Database database,
	const UserIdentity identity,
	const int32_t hrId,
	const int32_t frogateScore)
{
	::ChallengeUserTeam team = {};
	team.hr_id = hrId;
	team.frogate_score = frogateScore;
	team.aube = hunterOrbCap();
	team.aube_timer = 0;
	team.aube_rest_timer = 0;

	try
	{
		const auto rows = co_await database->execSqlCoro(
			"SELECT hunter_orbs, hunter_orb_rest_ts FROM user_info WHERE id = $1;",
			identity.userId);
		if (rows.empty())
		{
			co_return team;
		}

		auto orbs = rows[0]["hunter_orbs"].as<int32_t>();
		auto restTs = rows[0]["hunter_orb_rest_ts"].as<int64_t>();
		const auto storedOrbs = orbs;
		const auto storedTs = restTs;

		team.aube_rest_timer = deriveHunterOrbs(orbs, restTs);
		team.aube = orbs;
		// aube_timer (+0x44) is write-only in this build — getAubeTimer has no
		// callers — so it stays 0 rather than carrying an invented number.

		if (orbs != storedOrbs || restTs != storedTs)
		{
			co_await database->execSqlCoro(
				"UPDATE user_info SET hunter_orbs = $1, hunter_orb_rest_ts = $2 WHERE id = $3;",
				orbs, restTs, identity.userId);
		}
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		// A header must never fail the screen it decorates.
		LOG_WARN << "loadChallengeHeader: " << ex.base().what();
	}

	co_return team;
}

/*!
* Spends one Hunter Orb, starting the refill clock if it was not already running.
*
* For the Frontier Gate entry the client does NOT debit itself
* (MissionStartScene::initConnect skips its decrement while
* FrontierGateUtils::nowFrontierGate is true), so this is the charge.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity.
* @return True when an orb was spent, false when the player had none.
*/
inline drogon::Task<bool> spendHunterOrb(
	const db::Database database,
	const UserIdentity identity)
{
	try
	{
		const auto rows = co_await database->execSqlCoro(
			"SELECT hunter_orbs, hunter_orb_rest_ts FROM user_info WHERE id = $1;",
			identity.userId);
		if (rows.empty())
		{
			co_return false;
		}

		auto orbs = rows[0]["hunter_orbs"].as<int32_t>();
		auto restTs = rows[0]["hunter_orb_rest_ts"].as<int64_t>();
		deriveHunterOrbs(orbs, restTs);

		if (orbs <= 0)
		{
			co_return false;
		}

		const auto wasFull = orbs >= hunterOrbCap();
		--orbs;
		if (wasFull)
		{
			// The clock only starts on the drop BELOW the cap; spending while
			// already refilling must not push the pending orb further away.
			const auto now = static_cast<int64_t>(
				std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
			restTs = now + hunterOrbRegenSeconds();
		}

		co_await database->execSqlCoro(
			"UPDATE user_info SET hunter_orbs = $1, hunter_orb_rest_ts = $2 WHERE id = $3;",
			orbs, restTs, identity.userId);
		co_return true;
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		LOG_WARN << "spendHunterOrb: " << ex.base().what();
		co_return false;
	}
}

/*!
* Refills Hunter Orbs to the cap and stops the clock — the ShopUse type 5 grant.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity.
*/
inline drogon::Task<void> restoreHunterOrbsToCap(
	const db::Database database,
	const UserIdentity identity)
{
	try
	{
		co_await database->execSqlCoro(
			"UPDATE user_info SET hunter_orbs = $1, hunter_orb_rest_ts = 0 WHERE id = $2;",
			hunterOrbCap(), identity.userId);
	}
	catch (const drogon::orm::DrogonDbException& ex)
	{
		LOG_WARN << "restoreHunterOrbsToCap: " << ex.base().what();
	}
	co_return;
}

} // namespace gme
