#include "DailySpin.hpp"

#include "Common.hpp"

#include <gimuserver/db/DatabaseInterface.h>

#include <chrono>

namespace gme
{

namespace
{

/*!
* Days since the Unix epoch.
*/
int64_t utcDayNow()
{
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::chrono::duration_cast<std::chrono::seconds>(now).count() / 86400;
}

} // namespace

drogon::Task<DailySpinState> loadDailySpin(const db::Database database, const UserIdentity& identity)
{
	DailySpinState state{};

	const auto rows = (co_await db::DatabaseInterface::read(
		database,
		"user_daily_spin",
		{
			db::Data("spin_day"),
			db::Data("spins_used"),
			db::Data("last_spin_utc_day"),
			db::Data("last_reward_id"),
			db::Lookup("user_id", identity.userId),
		})).data;

	if (rows.empty())
	{
		// First sight of this user.  spins_used stays 0, which is correct and
		// is what makes Home offer the wheel on a fresh account.
		co_await db::DatabaseInterface::insert(
			database,
			"user_daily_spin",
			{
				db::Data("user_id", identity.userId),
				db::Data("spin_day", "1"),
				db::Data("spins_used", "0"),
				db::Data("last_spin_utc_day", std::to_string(utcDayNow())),
				db::Data("last_reward_id", "0"),
			});
		state.lastSpinUtcDay = utcDayNow();
		co_return state;
	}

	const auto& row = rows.front();
	state.spinDay = row["spin_day"].as<int32_t>();
	state.spinsUsed = row["spins_used"].as<int32_t>();
	state.lastSpinUtcDay = row["last_spin_utc_day"].as<int64_t>();
	state.lastRewardId = row["last_reward_id"].as<int32_t>();

	// Roll over without writing.  The stored row still describes yesterday; the
	// write happens when a spin is actually taken, so a player who never opens
	// the game does not accumulate rows of state nobody asked for.
	if (state.lastSpinUtcDay != utcDayNow())
	{
		state.spinsUsed = 0;
	}

	co_return state;
}

drogon::Task<bool> consumeDailySpin(
	const db::Database database,
	const UserIdentity& identity,
	DailySpinState& state)
{
	if (state.spinsUsed >= kDailySpinLimit)
	{
		co_return false;
	}

	state.spinsUsed += 1;
	state.lastSpinUtcDay = utcDayNow();

	// The reward cycle advances only once the day's spins are gone, so a day
	// with several spins draws them all from the same day's group.
	if (state.spinsUsed >= kDailySpinLimit)
	{
		state.spinDay += 1;
	}

	co_await db::DatabaseInterface::update(
		database,
		"user_daily_spin",
		{
			db::Data("spin_day", std::to_string(state.spinDay)),
			db::Data("spins_used", std::to_string(state.spinsUsed)),
			db::Data("last_spin_utc_day", std::to_string(state.lastSpinUtcDay)),
			db::Data("last_reward_id", std::to_string(state.lastRewardId)),
			db::Lookup("user_id", identity.userId),
		});

	co_return true;
}

} // namespace gme
