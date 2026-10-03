#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace gme
{

/*!
* Unix seconds for the energy refill timer.
*
* System time, unless the Debug developer console has frozen it with
* `clock <unix seconds>` -- the only writer.  That exists so a regression
* test can stand exactly on a regeneration-tick boundary (180 s, one or two
* energy per tick below / at level 999) instead of racing the wall clock.
* Release presets build without the console, so nothing can freeze it there.
*/
class GameClock
{
public:
	static uint64_t nowSeconds()
	{
		const auto frozen = frozenAt().load(std::memory_order_relaxed);
		if (frozen > 0)
			return static_cast<uint64_t>(frozen);
		return static_cast<uint64_t>(std::chrono::system_clock::to_time_t(
			std::chrono::system_clock::now()));
	}

	/// Debug console only: pin the clock (0 releases it).
	static void freeze(const int64_t unixSeconds)
	{
		frozenAt().store(unixSeconds > 0 ? unixSeconds : 0, std::memory_order_relaxed);
	}

	static int64_t frozenValue() { return frozenAt().load(std::memory_order_relaxed); }

private:
	static std::atomic<int64_t>& frozenAt()
	{
		static std::atomic<int64_t> value{ 0 };
		return value;
	}
};

} // namespace gme
