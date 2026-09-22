#pragma once

#include <set>
#include <utility>

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
* The element of an Elemental Golem, or 0 if the species is not one.
*
* THE UNLOCK MATERIAL.  Global wiki, Bonding: "In order to unlock the ability
* for a unit to Bond with its partner, players must first fuse it with an
* Elemental Golem of the same element type.  Doing this will now permanently
* unlock the unit's DBB Slot."
*
* The eighteen golems are numbered `87<element>026<tier>` -- 87102601 Fire
* Golem, 87102602 Greater Fire Golem, 87102603 Grand Fire Golem, and the same
* three for each of the six elements.  Checked against the archive: all
* eighteen exist and every one's element matches the digit.  Reading the id
* beats a hardcoded list because the tier is wanted too.
*
* @param speciesId The fodder's unit_id (species).
* @param tier Set to 1 Golem, 2 Greater, 3 Grand, when the species is a golem.
* @return Element id 1-6, or 0 when the species is not an Elemental Golem.
*/
inline int32_t dbbGolemElement(const std::string& speciesId, int32_t* tier = nullptr)
{
	int64_t id = 0;
	try
	{
		id = std::stoll(speciesId.substr(0, speciesId.find('_')));
	}
	catch (const std::exception&)
	{
		return 0;
	}
	if (id < 87102601 || id > 87602603)
		return 0;
	const auto element = static_cast<int32_t>((id / 100000) % 10);
	const auto band = static_cast<int32_t>((id / 100) % 1000);
	const auto step = static_cast<int32_t>(id % 100);
	if (element < 1 || element > 6 || band != 26 || step < 1 || step > 3)
		return 0;
	if (tier)
		*tier = step;
	return element;
}

/*!
* Whether a unit may have its DBB slot unlocked by a golem at all.
*
* Mirrors the client's own test, `UserUnitInfo::isDbbEligible` @0x12B5EA4, so
* the two agree about who can do this:
*
*   DbbMstList::getDbbMstListByUnitId(species) is non-empty   (it has a partner)
*   UnitMst::getRare() >= 8                                   (omni)
*   <vtable +0x50>() > 9                                      (Super Brave Burst at 10)
*
* The wiki says the same thing in words -- "Units must be at max level with SP
* Enhancements unlocked before Elemental Golems become able to be fused" -- but
* the binary is what the client will actually enforce when it draws the slot,
* so the binary is what is mirrored.
*
* MAX LEVEL is the fourth condition, and it is the CLIENT that proves it: Evan
* found the golems greyed out in the fusion picker until the unit hit 150, which
* is the wiki's "Units must be at max level with SP Enhancements unlocked before
* Elemental Golems become able to be fused".  isDbbEligible itself does not test
* it -- that filter lives in the fusion screen -- so the server checks it here to
* stay in step rather than let a crafted request open a slot early.
*
* @param speciesId The unit's species id.
* @param rarity The unit's rarity.
* @param sbbLevel The unit's Super Brave Burst level.
* @param level The unit's current level.
* @param maxLevel The species' max_lv.
* @return True when a matching golem would unlock the slot.
*/
inline bool dbbEligible(const std::string& speciesId, const int32_t rarity,
	const int32_t sbbLevel, const int32_t level, const int32_t maxLevel)
{
	if (rarity < 8 || sbbLevel < 10)
		return false;
	if (maxLevel > 0 && level < maxLevel)
		return false;
	// Same pass dbbIdForPair makes: the catalogue names the pair on unit_id_1
	// and unit_id_2, so a unit has a partner if it appears as either.
	for (const auto& row : theServer()->cache().dbbMst())
	{
		if (std::to_string(row.unit_id_1) == speciesId
			|| std::to_string(row.unit_id_2) == speciesId)
		{
			return true;
		}
	}
	return false;
}

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

	// EVERY UNIT WITH AN OPEN SLOT NEEDS A ROW, not just the bonded ones.
	//
	// The client decides a unit has fused its Elemental Golem by looking the
	// unit up in this very list: GameUtils::setDbbFusedIcon @0x1EC4CF8 calls
	// `UserUnitDbbInfoList::shared()->objectForKey(<user_unit_id>)` and draws
	// DBB_Fused_BB.png off the row it finds.  No row, no fused icon and no
	// slot -- which is exactly what Evan saw after a golem fusion that the
	// server had recorded correctly.
	//
	// Safe to send one per unit: addObject @0x1CC3374 keys the primary
	// dictionary on the row's OWN user_unit_id and only adds the reverse
	// bonded->main entry when the partner id is non-empty (the `cbz` at
	// 0x1CC3500).  So an unbonded unit's row cannot claim a partner it has not
	// got, and a bonded pair still contributes exactly one partner mapping.
	std::set<int32_t> emitted;
	std::vector<::UserUnitDbbInfo> info;
	std::vector<::UserUnitDbbLevelInfo> levels;
	for (const auto& bond : bonds)
	{
		// BOTH DIRECTIONS.  A bond row is not "the pair", it is "this unit's
		// partner", and each side needs its own.
		//
		// `isUnitBondedToAnotherUnit` @0x1CC3BF0 answers ONLY out of the reverse
		// map at +0x20, and addObject @0x1CC3374 fills that map from a row's
		// bonded id — so a row whose partner is empty puts nothing in it.  Send
		// the pair once and only ONE of the two units reports as bonded: the
		// other opens the Bond screen still asking for a partner, which is what
		// Evan hit, and re-bonding from that side then sent an unbond.
		//
		// An earlier note in this file warned that a mirror "would make each
		// unit claim the other".  That is exactly what a bond is; the warning
		// was wrong and the single row is what broke it.
		for (const auto [self, partner] :
			{ std::pair{ bond.mainUserUnitId, bond.subUserUnitId },
			  std::pair{ bond.subUserUnitId, bond.mainUserUnitId } })
		{
			::UserUnitDbbInfo row{};
			row.user_unit_id = self;
			row.bonded_user_unit_id = std::to_string(partner);
			info.push_back(std::move(row));
			emitted.insert(self);
		}

		::UserUnitDbbLevelInfo level{};
		level.user_id = identity.userId;
		level.dbb_id = bond.dbbId;
		level.main_unit_id = bond.mainUnitId;
		level.bond_level = bond.bondLevel;
		levels.push_back(std::move(level));
	}

	// Units whose slot is open but which are not bonded -- including the SUB of
	// a pair, whose partner is recorded on the main's row rather than its own.
	// An empty partner is what UserUnitDbbInfo's own constructor leaves there,
	// so it reads as "fused, unbonded" rather than as a bond to unit 0.
	const auto open = co_await database->execSqlCoro(
		"SELECT user_unit_id FROM user_units"
		" WHERE user_id = $1 AND dbb_unlocked = 1;",
		identity.userId);
	for (const auto& row : open)
	{
		const auto unitId = row["user_unit_id"].as<int32_t>();
		if (emitted.insert(unitId).second)
		{
			::UserUnitDbbInfo out{};
			out.user_unit_id = unitId;
			info.push_back(std::move(out));
		}
	}

	// An unbond cannot be said by omission: an empty array never reaches
	// readParam, so removeAllObjects never runs and the client keeps the bond
	// it already drew.  Naming the released unit with an empty partner is what
	// says it.  A unit whose slot is still open is already above; this covers
	// one whose row would otherwise vanish entirely.
	for (const auto unitId : released)
	{
		if (!emitted.insert(unitId).second)
			continue;
		::UserUnitDbbInfo row{};
		row.user_unit_id = unitId;
		info.push_back(std::move(row));
	}

	resp.dbb_info = std::move(info);
	resp.dbb_level_info = std::move(levels);
	co_return;
}

} // namespace gme
