#pragma once

#include <gimuserver/archive/UnitArchiver.hpp>
#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <string>
#include <vector>

// Dual Brave Burst — two of a player's units paired so they share a skill.
//
// The whole feature was dark here: DbbMst and DbbBondRecipeMst have been in
// the cache since the July port and had ZERO readers, and the two per-player
// blocks (`sxorQ3Mb`, `tR4katob`) were declared on UserInfo as TODO stubs with
// a placeholder key.  With DbbMstList empty the client cannot know a unit has
// a DBB at all, so the Bond button never appeared on any unit page.
//
// What the client does with each piece:
//
//   fTV3A0By  DbbMst            141 pairings.  UnitDetailUserUnitScene offers
//                               the Bond button off getDbbMstListByUnitId,
//                               DbbSelectBondScene fills its partner list from
//                               the same call, and BattleUnit::
//                               initializeBaseStatus resolves the shared skill
//                               with getDbbMstByUnitIdsCombo.
//   keTrog0p  DbbBondRecipeMst  105 rows pricing bond ranks 2..6, keyed by
//                               (element pair, rank) — 21 unordered element
//                               pairs x 5 ranks.  UnitBondBoost spends them.
//   sxorQ3Mb  UserUnitDbbInfo   this player's bonds, ONE ROW PER BOND — the
//                               client builds the reverse lookup itself
//                               (addObject @0x1CC3374).
//   tR4katob  UserUnitDbbLevelInfo  the level of each, keyed by DBB id.
//
// All four are REPLACE lists, and an empty array never reaches readParam at
// all — so "no bonds" cannot be said by sending nothing.  fillDbb takes the
// units an unbond just released and reports each with an EMPTY partner, which
// is the value the client's own UserUnitDbbInfo constructor leaves there.

namespace gme
{

namespace detail
{

/*! One stored bond, joined to the DbbMst row the two species make up. */
struct DbbBond
{
	int32_t mainUserUnitId = 0;
	int32_t subUserUnitId = 0;
	std::string dbbId;
	std::string mainUnitId;   // species, for UserUnitDbbLevelInfo
	int32_t bondLevel = 1;
};

/*!
* Reads this player's bonds.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return One entry per bond, in insertion order.
*/
inline drogon::Task<std::vector<DbbBond>> dbbBonds(
	const db::Database database,
	const UserIdentity identity)
{
	std::vector<DbbBond> bonds;
	for (const auto& row : co_await database->execSqlCoro(
		"SELECT d.user_unit_id, d.bonded_user_unit_id, d.dbb_id, d.bond_level,"
		"       u.unit_id AS main_unit_id"
		" FROM user_unit_dbb d"
		" JOIN user_units u ON u.user_unit_id = d.user_unit_id"
		" WHERE d.user_id = $1 ORDER BY d.user_unit_id;",
		identity.userId))
	{
		DbbBond bond{};
		bond.mainUserUnitId = row["user_unit_id"].as<int32_t>();
		bond.subUserUnitId = row["bonded_user_unit_id"].as<int32_t>();
		bond.dbbId = row["dbb_id"].as<std::string>();
		bond.mainUnitId = row["main_unit_id"].as<std::string>();
		bond.bondLevel = row["bond_level"].as<int32_t>();
		bonds.push_back(std::move(bond));
	}
	co_return bonds;
}

} // namespace detail

/*!
* Which DbbMst row pairs these two species, if any.
*
* Order does not matter: the catalogue lists each pairing once and the client
* matches it both ways (getDbbMstByUnitIdsCombo).
*
* @param speciesA One unit's unit_id.
* @param speciesB The other unit's unit_id.
* @return The DbbMst id, or an empty string when the two cannot be bonded.
*/
inline std::string dbbIdForPair(const std::string& speciesA, const std::string& speciesB)
{
	for (const auto& row : theServer()->cache().dbbMst())
	{
		const auto one = std::to_string(row.unit_id_1);
		const auto two = std::to_string(row.unit_id_2);
		if ((one == speciesA && two == speciesB) || (one == speciesB && two == speciesA))
			return std::to_string(row.id);
	}
	return {};
}

/*! The highest bond rank DbbBondRecipeMst prices. */
inline constexpr int32_t kDbbMaxBondLevel = 6;

/*!
* One unit's element id, from its SPECIES rather than its row.
*
* `user_units.element` is not dependable: 150 of the 208 units on the live save
* carry an empty string there, because only the paths that go through
* gme::addUserUnit fill it in and the rest of the roster predates that.  The
* archive is keyed by species and is always right, so it is asked first and the
* stored string is only a fallback.
*
* @param speciesId The unit_id (species), as stored on the row.
* @param stored The row's own `element` column, used when the archive misses.
* @return Element id 1-6, or 0 when neither source can say.
*/
inline int32_t dbbElementOf(const std::string& speciesId, const std::string& stored)
{
	try
	{
		const auto record = UnitArchiver::instance().lookup(
			static_cast<uint32_t>(std::stoul(speciesId)));
		if (record && record->element >= 1 && record->element <= 6)
			return static_cast<int32_t>(record->element);
	}
	catch (const std::exception&) { /* fall through to the stored name */ }

	if (stored == "fire")    return 1;
	if (stored == "water")   return 2;
	if (stored == "earth")   return 3;
	if (stored == "thunder") return 4;
	if (stored == "light")   return 5;
	if (stored == "dark")    return 6;
	return 0;
}

/*!
* The DbbBondRecipeMst element-pair key for two bonded units.
*
* `iNy0ZU5M` is a two-digit number whose digits are element ids 1..6, LOWER
* FIRST: the table carries 21 pairs, which is exactly the unordered
* combinations of six elements, so 61 is never a row and 16 is.
*
* @param a One unit's element id.
* @param b The other unit's.
* @return The pair key, or 0 when either id is out of range.
*/
inline int32_t dbbElementPair(const int32_t a, const int32_t b)
{
	if (a < 1 || a > 6 || b < 1 || b > 6)
		return 0;
	return std::min(a, b) * 10 + std::max(a, b);
}

/*!
* Fills every Dual Brave Burst block on a reply that declares them.
*
* Templated because two unrelated generated structs carry the same four
* fields: UserInfo (login, where the client first learns any of it) and
* DbbBond (the change itself).  The MSTs are only sent where the struct has
* them — DbbBond does not, because the catalogue cannot change under a bond.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @param resp Reply to fill; it must declare dbb_info and dbb_level_info.
* @param released Units whose bond was just dropped, reported with an empty
* partner so the client is actually told -- see the note in the body.
*/
template <typename Resp>
inline drogon::Task<void> fillDbb(
	const db::Database database,
	const UserIdentity identity,
	Resp& resp,
	const std::vector<int32_t> released = {})
{
	const auto bonds = co_await detail::dbbBonds(database, identity);

	std::vector<::UserUnitDbbInfo> info;
	std::vector<::UserUnitDbbLevelInfo> levels;
	for (const auto& bond : bonds)
	{
		::UserUnitDbbInfo row{};
		row.user_unit_id = bond.mainUserUnitId;
		row.bonded_user_unit_id = std::to_string(bond.subUserUnitId);
		info.push_back(std::move(row));

		::UserUnitDbbLevelInfo level{};
		level.user_id = identity.userId;
		level.dbb_id = bond.dbbId;
		level.main_unit_id = bond.mainUnitId;
		level.bond_level = bond.bondLevel;
		levels.push_back(std::move(level));
	}

	// An unbond cannot be said by omission: an empty array never reaches
	// readParam, so removeAllObjects never runs and the client keeps the bond
	// it already drew.  Naming the released unit with an empty partner is what
	// says it -- and an empty partner is the value UserUnitDbbInfo's own
	// constructor leaves there.
	for (const auto unitId : released)
	{
		::UserUnitDbbInfo row{};
		row.user_unit_id = unitId;
		info.push_back(std::move(row));
	}

	resp.dbb_info = std::move(info);
	resp.dbb_level_info = std::move(levels);
	co_return;
}

} // namespace gme
