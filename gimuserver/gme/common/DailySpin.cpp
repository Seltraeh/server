#include "DailySpin.hpp"

#include "Common.hpp"

#include <gimuserver/archive/archive.hpp>
#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/utils/JsonFile.hpp>
#include <gimuserver/utils/Random.hpp>

#include <chrono>
#include <vector>

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

/*!
* The authored reward cycle, loaded once at startup.
*/
std::vector<DailySpinDay> g_days;

// Display caps, matching the values CampaignReceipt already clamps to.  See
// handbook §10 for why exceeding them silently renders as zero.
constexpr int64_t kMaxZelKarma = 99'999'999LL;
constexpr int64_t kMaxGems     = 9'999LL;
constexpr int64_t kMaxTickets  = 99LL;

/*!
* Resolves a cycle day to its authored row.
*
* The live cycle repeats its final row for every day past the end of the table,
* so anything beyond it clamps rather than wrapping back to day 1.
*/
const DailySpinDay* rowForDay(const int32_t day)
{
	if (g_days.empty())
	{
		return nullptr;
	}

	for (const auto& d : g_days)
	{
		if (static_cast<int32_t>(d.day) == day)
		{
			return &d;
		}
	}

	return &g_days.back();
}

} // namespace

void loadDailySpinArchive(const std::string& archiveRoot)
{
	if (archiveRoot.empty())
	{
		LOG_WARN << "DailySpin: archive_root is empty, reward table not loaded";
		return;
	}

	try
	{
		g_days = LoadJson<std::vector<DailySpinDay>>(archiveRoot, "daily_login.json");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "DailySpin: failed to load daily_login.json: " << e.what();
		return;
	}

	LOG_INFO << "DailySpin: loaded " << g_days.size()
		<< " reward days from " << archiveRoot << "/daily_login.json";
}

drogon::Task<std::string> awardDailySpin(
	const db::Database database,
	const UserIdentity& identity,
	DailySpinState& state)
{
	const auto* row = rowForDay(state.spinDay);
	if (row == nullptr)
	{
		LOG_WARN << "DailySpin: no reward table loaded, nothing awarded";
		co_return "nothing (reward table not loaded)";
	}

	// Days 7/14/21/28 guarantee a Gem on the first spin of the day; every other
	// spin picks a space at random, which is what the live game did.
	const bool guaranteed = row->guaranteed_gem_on_first_spin && state.spinsUsed == 0;
	const uint32_t space = guaranteed ? 0 : RandomUInt(0, kDailySpinSpaces - 1);

	// XIvaD6Jp is the prize SELECTOR, and its id space is the client's —
	// F_SG_DAILYLOGIN_REWARDS_MST is loaded through DataMstManager and is in
	// none of our decoded MSTs (handbook §7.14), so this was recovered by
	// observation rather than derived.
	//
	// CONFIRMED 2026-08-21 from two independent client readings:
	//   id  1 on day 1 -> the client drew the BLUE space (200,000 Karma)
	//   id 37 on day 7 -> the client drew the BLUE space (Almighty Imp Arton)
	// Blue is index 1 in the wiki's column order, and both satisfy
	//   id = (day - 1) * 6 + space
	// with space 0-based.  An earlier revision added a further +1 here, which
	// shifted every award one space anticlockwise of what the wheel showed:
	// day 7 displayed the blue Imp while the server paid out red's Gem.
	//
	// Note this makes day 1's red space id 0, so the client's cycle is
	// 0-based.  Do NOT "correct" that to 1.
	state.lastRewardId = dailySpinAnchor(state.spinDay) + static_cast<int32_t>(space);

	if (space >= row->spaces.size())
	{
		LOG_WARN << "DailySpin: day " << state.spinDay << " has only "
			<< row->spaces.size() << " spaces, nothing awarded";
		co_return "nothing (malformed reward row)";
	}

	const auto& prize = row->spaces[space];
	const std::string label =
		prize.name + " x" + std::to_string(prize.count)
		+ " (day " + std::to_string(state.spinDay)
		+ ", space " + std::to_string(space) + ")";

	if (!prize.available)
	{
		LOG_WARN << "DailySpin: " << label
			<< " has no id in our MST tables — displayed but NOT awarded";
		co_return label + " — not awarded, absent from our MSTs";
	}

	if (prize.kind == "item")
	{
		co_await gme::addUserItem(database, identity, prize.id, prize.count);
	}
	else if (prize.kind == "unit")
	{
		const auto& unitMst = theServer()->cache().unitMst();
		const auto unit = std::find_if(unitMst.begin(), unitMst.end(),
			[&prize](const auto& u) { return u.id == prize.id; });
		if (unit == unitMst.end())
		{
			LOG_WARN << "DailySpin: unit " << prize.id << " not in unit_mst — skipped";
			co_return label + " — not awarded, unit missing from unit_mst";
		}
		for (uint32_t i = 0; i < prize.count; ++i)
		{
			co_await gme::addUserUnit(database, identity, *unit);
		}
	}
	else
	{
		// Currency kinds are userinfo columns.  Each is clamped to its own
		// display width: past the cap the client silently renders zero, so a
		// generous award would read as a loss (handbook §10).
		std::string column;
		int64_t cap = 0;
		if (prize.kind == "gem")         { column = "gems";           cap = kMaxGems; }
		else if (prize.kind == "karma")  { column = "karma";          cap = kMaxZelKarma; }
		else if (prize.kind == "zel")    { column = "zel";            cap = kMaxZelKarma; }
		else if (prize.kind == "ticket") { column = "summon_tickets"; cap = kMaxTickets; }
		else
		{
			LOG_WARN << "DailySpin: unknown reward kind '" << prize.kind << "' — skipped";
			co_return label + " — not awarded, unknown kind";
		}

		co_await database->execSqlCoro(
			"UPDATE user_info SET " + column + " = MIN(" + column + " + $1, $2) WHERE id = $3;",
			static_cast<int64_t>(prize.count), cap, identity.userId);
	}

	co_return label;
}

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
