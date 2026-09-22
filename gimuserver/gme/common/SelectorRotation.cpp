#include "App.hpp"

#include <gimuserver/gme/common/SelectorRotation.hpp>
#include <gimuserver/archive/UnitArchiver.hpp>

#include <algorithm>
#include <random>

namespace gme
{

namespace
{

/*! Every unit the rotation can offer, sorted so the shuffle is reproducible. */
std::vector<int32_t> g_pool;

/*!
* The one fixed order the rotation walks, forever.
*
* ⚠ ONE SHUFFLE, NOT ONE PER CYCLE.  The first cut re-shuffled per cycle so the
* order would not be predictable.  That CANNOT deliver the guarantee: two
* independent permutations laid end to end put some unit near the end of one and
* near the start of the next, and no amount of seam-patching fixes it -- a debug
* CLI sweep found repeats two weeks apart.  The only ordering where every unit is
* exactly 33 weeks from its last appearance is a single order repeated, which is
* also precisely what "a predetermined pattern everyone is on" means: the
* community can build a calendar from it, which is the point.
*
* Shuffled once from a fixed seed rather than left in id order so the weeks are
* a mix of elements and eras instead of ten fire units in a row.
*/
const std::vector<int32_t>& rotationOrder()
{
	static const std::vector<int32_t> order = [] {
		auto shuffled = g_pool;
		// Arbitrary but FIXED.  Changing it reshuffles everyone's calendar, so it
		// is a constant, not a knob.
		std::mt19937 rng(0x6BF5EC7u);
		std::shuffle(shuffled.begin(), shuffled.end(), rng);
		return shuffled;
	}();
	return order;
}

} // namespace

void loadSelectorRotation()
{
	g_pool.clear();
	for (const auto& [id, unit] : UnitArchiver::instance().all())
	{
		if (static_cast<int32_t>(unit.rarity) == kWeeklySelectorRarity)
			g_pool.push_back(static_cast<int32_t>(id));
	}
	// Sorted because an unordered_map's iteration order is not reproducible and
	// the shuffle indexes into this -- an unsorted base would give two servers
	// (or two boots) different rotations from the same seed.
	std::sort(g_pool.begin(), g_pool.end());

	const auto weeks = g_pool.empty()
		? 0 : (g_pool.size() + kWeeklySelectorSize - 1) / kWeeklySelectorSize;
	LOG_INFO << "SelectorRotation: " << g_pool.size() << " " << kWeeklySelectorRarity
		<< "-star unit(s) in the pool, " << weeks << " week(s) to cycle";
}

size_t selectorPoolSize()
{
	return g_pool.size();
}

int64_t selectorWeekIndex(std::time_t now)
{
	const auto days = static_cast<int64_t>(now / 86400);
	// +6 rather than -1 so the arithmetic never goes negative near the epoch.
	return (days + 6) / 7;
}

std::vector<int32_t> weeklySelectorPool(int64_t week)
{
	std::vector<int32_t> out;
	if (g_pool.empty())
		return out;

	const auto& order = rotationOrder();
	const auto n = static_cast<int64_t>(order.size());

	// Walk the fixed order ten at a time, wrapping.  Every unit is therefore
	// exactly ceil(n/10) weeks from its own last appearance -- the no-repeat
	// guarantee holds by construction rather than by patching collisions.
	const auto start = (week * kWeeklySelectorSize) % n;
	for (int32_t i = 0; i < kWeeklySelectorSize; ++i)
		out.push_back(order[static_cast<size_t>((start + i) % n)]);
	return out;
}

std::vector<int32_t> weeklySelectorPool()
{
	return weeklySelectorPool(selectorWeekIndex());
}

void applyWeeklySelector(std::vector<::UnitSelectorGachaMst>& catalogue)
{
	const auto pool = weeklySelectorPool();
	if (pool.empty())
		return;

	for (auto& row : catalogue)
	{
		if (row.selector_id != kWeeklySelectorId)
			continue;
		row.unit_pool.assign(pool.begin(), pool.end());
		return;
	}
}

drogon::Task<bool> grantStarterSelectorIfDue(
	const db::Database database,
	const UserIdentity identity)
{
	const auto rows = co_await database->execSqlCoro(
		"SELECT level, starter_selector_granted FROM user_info WHERE id = $1;",
		identity.userId);
	if (rows.empty())
		co_return false;

	if (rows[0]["starter_selector_granted"].as<int32_t>() != 0)
		co_return false;
	if (rows[0]["level"].as<int32_t>() < kStarterSelectorLevel)
		co_return false;

	// Mark first: a throw in the grant costs one ticket, whereas marking after
	// would hand out a new one on every request until it succeeded.
	co_await database->execSqlCoro(
		"UPDATE user_info SET starter_selector_granted = 1 WHERE id = $1;",
		identity.userId);

	co_await addUserPresent(database, identity, 8005,
		std::to_string(kWeeklySelectorId), 1, 0,
		"This ticket can be used to select from a weekly refreshing"
		" (Friday at 0:00 Server Time) list of 10 heroes");

	LOG_INFO << "SelectorRotation: granted the first selector to " << identity.userId
		<< " at level " << kStarterSelectorLevel;
	co_return true;
}

} // namespace gme
