#include "DailySpin.hpp"

#include "Common.hpp"

#include <gimuserver/archive/archive.hpp>
#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/utils/JsonFile.hpp>
#include <gimuserver/utils/Random.hpp>

#include <algorithm>
#include <chrono>
#include <string>
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
* The authored reward table, loaded once at startup.
*/
std::vector<DailySpinDay> g_days;

// Display caps, matching the values CampaignReceipt already clamps to.  See
// handbook §10 for why exceeding them silently renders as zero.
constexpr int64_t kMaxZelKarma = 99'999'999LL;
constexpr int64_t kMaxGems     = 9'999LL;
constexpr int64_t kMaxTickets  = 99LL;

/*!
* The row for a GROUP number, with no day clamping.  Null when absent.
*/
const DailySpinDay* rowForGroup(const int32_t group)
{
	for (const auto& d : g_days)
	{
		if (static_cast<int32_t>(d.day) == group)
		{
			return &d;
		}
	}
	return nullptr;
}

/*!
* Resolves a spin day to its authored row, clamping at the terminal day.
*/
const DailySpinDay* rowForDay(const int32_t day)
{
	if (g_days.empty())
	{
		return nullptr;
	}
	const auto* row = rowForGroup(dailySpinTableDay(day));
	return row != nullptr ? row : &g_days.back();
}

} // namespace

int32_t dailySpinTerminalDay()
{
	// ⚠ NOT g_days.size().  The table holds 32 groups but only 29 DAYS: the
	// wiki's row reads "Day 29+", groups 29, 30 and 31 are byte-identical
	// (so which of them a late day resolves to cannot be observed), and group
	// 32 is the all-Gem wheel rather than a day at all.  Clamping on the row
	// count would march a player through two duplicate days and then park them
	// on a wheel that pays a Gem every single morning.
	constexpr int32_t kLastDay = 29;
	const auto rows = static_cast<int32_t>(g_days.size());
	return rows < kLastDay ? rows : kLastDay;
}

int32_t dailySpinTableDay(const int32_t day)
{
	const auto last = dailySpinTerminalDay();
	if (last <= 0 || day <= 0)
	{
		return day;
	}
	return day < last ? day : last;
}

DailySpinWheel dailySpinWheel(const DailySpinState& state)
{
	// A guaranteed-Gem day shows the all-Gem group, so the wheel the player
	// watches is the one the spin is about to pay out of.
	const auto* today = rowForDay(state.spinDay);
	const bool guaranteed = today != nullptr
		&& today->guaranteed_gem_on_first_spin
		&& state.spinsUsed == 0
		&& rowForGroup(kDailySpinGemGroup) != nullptr;

	DailySpinWheel wheel{};
	wheel.nextRewardId = guaranteed
		? dailySpinAnchorForGroup(kDailySpinGemGroup)
		: dailySpinAnchor(state.spinDay);

	const auto rows = static_cast<int32_t>(g_days.size()) * kDailySpinSpaces;
	wheel.id = (state.lastRewardId >= 1 && state.lastRewardId <= rows)
		? state.lastRewardId
		: wheel.nextRewardId;
	return wheel;
}

DailySpinGemLabel dailySpinGemLabel(const int32_t day)
{
	const auto last = dailySpinTerminalDay();
	if (last <= 0 || day <= 0)
	{
		return {};
	}

	// Past the terminal row the player never advances again, so the only
	// guaranteed Gem still reachable would be on the terminal row itself.
	for (auto ahead = std::max(day, 1); ahead <= last; ++ahead)
	{
		const auto* row = rowForDay(ahead);
		if (row != nullptr && row->guaranteed_gem_on_first_spin)
		{
			return { std::to_string(ahead - day), " day(s) more to guaranteed Gem!" };
		}
	}
	return {};
}

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

	// The last row is TERMINAL, not the end of a lap: the wiki table this came
	// from reads "Day 28" then "Day 29+", so a player climbs to the last
	// authored day and stays there.  Nothing wraps, so the table may be any
	// length — it does not have to be whole weeks, which it did while the day
	// wrapped modulo the cycle and the client's `day % 7` Gem counter had to
	// stay in step with it.
	LOG_INFO << "DailySpin: loaded " << g_days.size()
		<< " reward group(s) from " << archiveRoot << "/daily_login.json"
		<< "; day " << dailySpinTerminalDay() << " is terminal and repeats from then on";

	// Every group's six rates must sum to 100, or the weighted draw silently
	// favours whatever the shortfall lands on.
	for (const auto& day : g_days)
	{
		uint32_t total = 0;
		for (const auto& space : day.spaces)
			total += space.rate_x100;
		if (total != 10000)
		{
			LOG_WARN << "DailySpin: day " << day.day << "'s rates sum to "
				<< (total / 100.0) << "%, not 100 — the draw will be skewed";
		}
	}

	// Every row up to the terminal one must be present and in order, or a day
	// silently plays `g_days.back()` via rowForDay's fallback.
	for (size_t i = 0; i < g_days.size(); ++i)
	{
		if (static_cast<int32_t>(g_days[i].day) != static_cast<int32_t>(i) + 1)
		{
			LOG_WARN << "DailySpin: row " << i << " is day " << g_days[i].day
				<< ", expected " << (i + 1)
				<< " — the table must be a dense 1..N run or days resolve wrongly";
			break;
		}
	}
}

drogon::Task<std::string> awardDailySpin(
	const db::Database database,
	const UserIdentity& identity,
	DailySpinState& state,
	GrantedRewards& granted)
{
	const auto* row = rowForDay(state.spinDay);
	if (row == nullptr)
	{
		LOG_WARN << "DailySpin: no reward table loaded, nothing awarded";
		co_return "nothing (reward table not loaded)";
	}

	// Days 7/14/21/28 guarantee a Gem on the first spin of the day.  The prize
	// comes from the ALL-GEM GROUP rather than from this day's row, so the
	// wheel the client is pointed at (kDailySpinGemGroup, every space a Gem)
	// and the prize it is paid describe the same thing.  Awarding this day's
	// Gem space instead would have the client draw six ordinary prizes and then
	// hand over a Gem that was not on the wheel.
	const bool guaranteed = row->guaranteed_gem_on_first_spin && state.spinsUsed == 0;
	const auto* drawFrom = row;
	bool fromGemGroup = false;
	if (guaranteed)
	{
		if (const auto* gems = rowForGroup(kDailySpinGemGroup))
		{
			drawFrom = gems;
			fromGemGroup = true;
		}
		else
		{
			LOG_WARN << "DailySpin: group " << kDailySpinGemGroup
				<< " is missing, paying day " << state.spinDay << "'s own row";
		}
	}

	// ⚠ THE DRAW IS WEIGHTED.  `rate` is a percentage and a group's six sum to
	// 100; a flat RandomUInt over the six handed out the 1-percent Gem as often
	// as the 32-percent Karma, which is a sixfold inflation of the one prize
	// that gates summoning.  Falls back to uniform only if a group's rates are
	// all zero, so a malformed row cannot make the spin award nothing.
	uint32_t space = 0;
	{
		uint32_t total = 0;
		for (const auto& candidate : drawFrom->spaces)
			total += candidate.rate_x100;
		if (total == 0)
		{
			space = RandomUInt(0, static_cast<uint32_t>(drawFrom->spaces.size()) - 1);
		}
		else
		{
			// Integer end to end: the rates are hundredths of a percent and a
			// group's six sum to 10000, so this is exact.
			auto roll = RandomUInt(0, total - 1);
			for (size_t i = 0; i < drawFrom->spaces.size(); ++i)
			{
				space = static_cast<uint32_t>(i);
				if (roll < drawFrom->spaces[i].rate_x100)
					break;
				roll -= drawFrom->spaces[i].rate_x100;
			}
		}
	}
	row = drawFrom;

	// XIvaD6Jp is the prize SELECTOR: the client resolves it to a row, draws
	// that row's GROUP as the wheel, and reports that row as the prize.
	//
	// ⚠ NO LONGER INFERRED.  The id space was guessed from two client readings
	// until 2026-09-20, when the client's own F_SG_DAILYLOGIN_REWARDS_MST was
	// decrypted out of its LocalState cache (tools/gen_daily_spin_archive.py).
	// It is 192 rows in 32 groups of six, ids 1..192, dense and 1-BASED:
	//
	//     id = (group - 1) * 6 + space + 1
	//
	// The guess was 0-based, so every id named the space before the one the
	// wheel drew, and at space 0 it named the previous GROUP: on 2026-09-20 the
	// client drew group 28's Omni Frogs while the server paid group 29's Imps.
	// The old note claiming "an earlier revision added a further +1, which
	// shifted every award one space anticlockwise" had it backwards — the +1 is
	// correct and removing it is what caused the shift.
	//
	// dailySpinAnchor resolves the day and applies the +1; a guaranteed-Gem day
	// takes its id from the all-Gem group so the wheel and the prize agree.
	state.lastRewardId =
		(fromGemGroup ? dailySpinAnchorForGroup(kDailySpinGemGroup)
		              : dailySpinAnchor(state.spinDay))
		+ static_cast<int32_t>(space);

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
		granted.items = true;
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
			granted.userUnitIds.push_back(co_await gme::addUserUnit(database, identity, *unit));
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
