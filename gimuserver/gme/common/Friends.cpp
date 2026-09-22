#include "App.hpp"

#include <gimuserver/gme/common/Friends.hpp>
#include <gimuserver/archive/UnitArchiver.hpp>
#include <gimuserver/utils/JsonFile.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <random>

namespace gme
{

/*!
* deploy/archive/friends.json, as Glaze sees it.
*
* ⚠ AT NAMESPACE SCOPE DELIBERATELY.  Glaze reflects an aggregate declared
* here; one inside an anonymous namespace fails to compile outright
* (`variable with internal linkage declared but not defined` out of
* get_name.hpp), and a function-local one silently fails to parse.  The same
* trap cost a debugging round on MstDownloadManifest.
*
* AUTHORED POLICY, not recovered game data.
*
* ⚠ THE FILE IS DATA ONLY.  LoadJson rejects unknown keys, so a "_doc" note
* inside it does not annotate it, it stops it loading -- and the failure is
* quiet: g_devs stays empty, every recruit returns false, and FriendApply
* happily replies success to a request that did nothing.  None of the other
* archives carry one.  That is why this comment is here rather than in there.
*
* A developer friend fields a fixed evolution CHAIN, named by its base (lowest
* rarity) form; base_unit_id is the BOTTOM of it, verified by the id below it
* being absent.  All five chains reach rarity 8 (omni), so a developer can never
* go stale and drop out of the picker the way an ordinary friend can.
*
* Ordinary friends are not listed here at all -- they are generated from a
* random chain base in unit.json and take that form's name.
*
* PENDING: Arves100 has not chosen a unit.  An unknown developer stays OUT of
* the file rather than fielding a guess.
*/
struct FriendArchive
{
	std::vector<DevFriend> devs;
};

namespace
{

std::vector<DevFriend> g_devs;

/*! Chain bases, computed once: a unit with no rarity-1 predecessor at id-1. */
std::vector<int32_t> g_chainBases;

/*! One equippable sphere, indexed once at startup. */
struct SphereRow
{
	int32_t id = 0;
	int32_t sphere_type = 0;
	int32_t rarity = 0;
};

std::vector<SphereRow> g_spheres;

/*!
* Index every sphere in ItemMst once.
*
* A sphere is an item with a non-zero `sphere_type`; that field is the CATEGORY
* (1..14), and the game does not let two of the same category be worn together.
* ItemSphereEqp resolves it with a linear scan per equip, which is fine for one
* call but not for rolling a loadout on every picker open, so it is cached here.
*/
// A string carrying CJK: the recovered MST data is the Japanese build and
// `tools/translate_mst.py` cannot reach every row, so some item names are still
// untranslated.  Cheap byte scan over UTF-8 lead bytes rather than a real
// decode -- everything here is either ASCII or Japanese.
bool hasJapanese(const std::string& text)
{
	for (size_t i = 0; i + 2 < text.size(); ++i)
	{
		const auto a = static_cast<unsigned char>(text[i]);
		const auto b = static_cast<unsigned char>(text[i + 1]);
		// U+3000..U+9FFF covers kana and the CJK ideographs.
		if (a == 0xE3 || a == 0xE4 || a == 0xE5 || a == 0xE6 || a == 0xE7
			|| a == 0xE8 || a == 0xE9)
		{
			if (b >= 0x80 && b <= 0xBF)
				return true;
		}
	}
	return false;
}

void buildSphereIndex()
{
	g_spheres.clear();
	size_t untranslated = 0;
	for (const auto& m : theServer()->cache().itemMst())
	{
		if (m.sphere_type <= 0)
			continue;
		// A HELPER MUST NOT ADVERTISE A SPHERE WE CANNOT NAME.  Evan saw
		// "ダーイン" on a borrowed unit (item 65040, which has no English string
		// in any client bundle and none on the wiki either).  The kit is
		// authored, so it can simply pick from gear that reads in English.
		if (hasJapanese(m.name))
		{
			++untranslated;
			continue;
		}
		g_spheres.push_back(SphereRow{ m.id, m.sphere_type, m.rarity });
	}
	std::sort(g_spheres.begin(), g_spheres.end(),
		[](const SphereRow& a, const SphereRow& b) { return a.id < b.id; });
	LOG_INFO << "Friends: " << g_spheres.size() << " equippable sphere(s) indexed"
		<< " (" << untranslated << " skipped for having no English name)";
}

/*! The archived unit, or nullopt.  UnitArchiver keys on a numeric id. */
std::optional<UnitRecord> archivedUnit(int32_t id)
{
	if (id <= 0)
		return std::nullopt;
	// Evolution-chain probes legitimately miss sparse IDs. Keep required-record
	// errors in lookup(), but do not log each absent predecessor as a failure.
	const auto& units = UnitArchiver::instance().all();
	const auto it = units.find(static_cast<UnitArchiver::UnitId>(id));
	if (it == units.end())
		return std::nullopt;
	return it->second;
}

/*!
* Find every evolution-chain base once, at startup.
*
* ⚠ ENUMERATE, DO NOT SWEEP.  The first cut of this walked ids 1..90,000 on the
* assumption that the archive lived below that.  It does not: ids are sparse and
* reach 87,602,603, so that cap silently dropped 550 of 2,053 units -- including
* Ciara (810104) and Zeruiah (830115), two of the developer identities -- while
* raising it far enough would mean tens of millions of lookups per boot.
* UnitArchiver::all() exists for exactly this.
*/
void buildChainBases()
{
	g_chainBases.clear();
	for (const auto& [id, unit] : UnitArchiver::instance().all())
	{
		if (unit.rarity == 0)
			continue;

		// A base is a form whose id-1 is NOT the rarity directly below it.
		const auto prev = archivedUnit(static_cast<int32_t>(id) - 1);
		if (prev && prev->rarity == unit.rarity - 1)
			continue;
		g_chainBases.push_back(static_cast<int32_t>(id));
	}
	// Sorted so seeding is reproducible: an unordered_map's iteration order is
	// not, and the seed RNG indexes into this.
	std::sort(g_chainBases.begin(), g_chainBases.end());
	LOG_INFO << "Friends: " << g_chainBases.size() << " evolution-chain bases";
}

} // namespace

void loadFriendArchive(const std::string& archiveRoot)
{
	if (archiveRoot.empty())
	{
		LOG_WARN << "Friends: archive_root is empty, no developer identities loaded";
		return;
	}

	try
	{
		g_devs = LoadJson<FriendArchive>(archiveRoot, "friends.json").devs;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "Friends: failed to load friends.json: " << e.what();
	}

	buildChainBases();
	buildSphereIndex();
	LOG_INFO << "Friends: " << g_devs.size() << " developer identit(ies) loaded";
}

const std::vector<DevFriend>& devFriends()
{
	return g_devs;
}

int32_t chainTopRarity(int32_t baseUnitId)
{
	const auto unit = archivedUnit(baseUnitId);
	if (!unit)
		return 0;

	int32_t id = baseUnitId;
	auto rarity = static_cast<int32_t>(unit->rarity);
	for (;;)
	{
		const auto next = archivedUnit(id + 1);
		if (!next || static_cast<int32_t>(next->rarity) != rarity + 1)
			return rarity;
		++id;
		rarity = static_cast<int32_t>(next->rarity);
	}
}

int32_t chainFormAtRarity(int32_t baseUnitId, int32_t rarity)
{
	const auto unit = archivedUnit(baseUnitId);
	if (!unit)
		return 0;

	int32_t id = baseUnitId;
	auto at = static_cast<int32_t>(unit->rarity);
	while (at < rarity)
	{
		const auto next = archivedUnit(id + 1);
		if (!next || static_cast<int32_t>(next->rarity) != at + 1)
			break;          // chain tops out below what was asked for
		++id;
		at = static_cast<int32_t>(next->rarity);
	}
	return id;
}

namespace
{

/*!
* Seed a fresh roster.
*
* Ordinary friends take their NAME from the base form of the unit they field,
* so a friend is named after the unit the player will actually see -- the
* player's own request.  Seeding is deterministic per user so a reinstall does
* not reshuffle somebody's friends list.
*/
drogon::Task<void> seedRoster(
	const db::Database database,
	const UserIdentity identity)
{
	if (g_chainBases.empty())
	{
		LOG_WARN << "Friends: no chain bases; roster cannot be seeded";
		co_return;
	}

	std::seed_seq seed{ static_cast<uint32_t>(std::hash<std::string>{}(identity.userId)) };
	std::mt19937 rng(seed);
	std::uniform_int_distribution<size_t> pick(0, g_chainBases.size() - 1);

	std::vector<int32_t> chosen;
	for (int32_t i = 0; i < kSeedFriendCount * 4 && static_cast<int32_t>(chosen.size()) < kSeedFriendCount; ++i)
	{
		const auto base = g_chainBases[pick(rng)];
		if (std::find(chosen.begin(), chosen.end(), base) != chosen.end())
			continue;
		// Seed only with chains that can still grow a little, so a brand-new
		// roster is not half-stale on the day it is created.
		if (chainTopRarity(base) < 5)
			continue;
		chosen.push_back(base);
	}

	for (size_t i = 0; i < chosen.size(); ++i)
	{
		const auto unit = archivedUnit(chosen[i]);
		if (!unit)
			continue;
		// Reserved synthetic ids, distinct from kSyntheticHelperUserId and from
		// each other -- a collision would make two friends the same person to
		// every consumer that keys on user id.
		const auto friendId = "DCF" + std::string(5 - std::min<size_t>(5, std::to_string(i + 1).size()), '0')
			+ std::to_string(i + 1);
		co_await database->execSqlCoro(
			"INSERT OR IGNORE INTO user_friends"
			" (user_id, friend_id, handle_name, base_unit_id, is_dev, favorite, added_day)"
			" VALUES ($1, $2, $3, $4, 0, 0, date('now'));",
			identity.userId, friendId, unit->name, chosen[i]);
	}

	LOG_INFO << "Friends: seeded " << chosen.size() << " friend(s) for " << identity.userId;
}

} // namespace

drogon::Task<PlayerPeak> playerPeak(
	const db::Database database,
	const UserIdentity identity)
{
	PlayerPeak peak{};

	// Rarity first, then level within it: a level 120 four-star is not a
	// stronger yardstick than a level 1 omni for what a friend should field.
	for (const auto& row : co_await database->execSqlCoro(
		"SELECT unit_id, unit_lvl FROM user_units WHERE user_id = $1;",
		identity.userId))
	{
		const auto raw = row["unit_id"].as<std::string>();
		const auto sep = raw.find('_');
		int32_t id = 0;
		try { id = std::stoi(sep != std::string::npos ? raw.substr(0, sep) : raw); }
		catch (const std::exception&) { continue; }

		const auto unit = archivedUnit(id);
		if (!unit)
			continue;
		const auto rarity = static_cast<int32_t>(unit->rarity);
		const auto level = row["unit_lvl"].as<int32_t>();
		if (rarity > peak.rarity || (rarity == peak.rarity && level > peak.level))
		{
			peak.rarity = rarity;
			peak.level = std::max(level, 1);
		}
	}

	co_return peak;
}

FriendUnit friendUnitFor(const FriendRow& row, const PlayerPeak& peak)
{
	FriendUnit out{};

	out.unit_id = chainFormAtRarity(row.base_unit_id, peak.rarity);
	if (out.unit_id == 0)
		return out;

	const auto& catalogue = theServer()->cache().unitMst();
	const auto mst = std::find_if(catalogue.begin(), catalogue.end(),
		[&out](const ::UnitMst& u) { return u.id == out.unit_id; });
	if (mst == catalogue.end())
	{
		// No MST row: send the id and a level, but never invent a statline.
		out.level = std::max(peak.level, 1);
		return out;
	}

	out.rarity = mst->rarity;
	out.element = mst->element;

	// A level near the player's, decided once per (friend, player level) so the
	// same friend does not shuffle a few levels each time the picker opens.
	std::seed_seq seed{ static_cast<uint32_t>(std::hash<std::string>{}(row.friend_id)),
						static_cast<uint32_t>(peak.level),
						static_cast<uint32_t>(peak.rarity) };
	std::mt19937 rng(seed);
	std::uniform_int_distribution<int32_t> jitter(-kFriendLevelSpread, kFriendLevelSpread);

	const auto maxLv = std::max(mst->max_lv, 1);
	out.level = std::clamp(peak.level + jitter(rng), 1, maxLv);

	// The same min..max interpolation scaleUnitBaseStats uses.
	const auto at = [&](int32_t minV, int32_t maxV) -> int32_t {
		if (maxLv <= 1 || out.level <= 1) return minV;
		if (out.level >= maxLv)           return maxV;
		return minV + static_cast<int32_t>(
			static_cast<double>(maxV - minV) * (out.level - 1) / (maxLv - 1));
	};
	out.base_hp  = at(mst->min_hp,  mst->max_hp);
	out.base_atk = at(mst->min_atk, mst->max_atk);
	out.base_def = at(mst->min_def, mst->max_def);
	out.base_rec = at(mst->min_rec, mst->max_rec);

	// ⚠ UnitMst.skill_id IS THE BRAVE BURST.  The handbook records this trap
	// explicitly -- user_units.skill_id is a different, unused column, and
	// reading it by name is what left borrowed helpers with no BB at all.
	out.bb_id = mst->skill_id;
	out.sbb_id = mst->extra_skill_id;
	out.bb_lvl = 10;
	out.sbb_lvl = out.sbb_id != 0 ? 10 : 0;

	return out;
}

drogon::Task<std::vector<FriendRow>> loadFriendRoster(
	const db::Database database,
	const UserIdentity identity,
	const bool forPicker)
{
	std::vector<FriendRow> roster;

	const auto count = co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n FROM user_friends WHERE user_id = $1;", identity.userId);
	if (!count.empty() && count[0]["n"].as<int64_t>() == 0)
		co_await seedRoster(database, identity);

	// The player's own ceiling -- the rarity every friend is measured against.
	int32_t bestRarity = 1;
	const auto best = co_await database->execSqlCoro(
		"SELECT unit_id FROM user_units WHERE user_id = $1;", identity.userId);
	for (const auto& row : best)
	{
		const auto raw = row["unit_id"].as<std::string>();
		const auto sep = raw.find('_');
		int32_t id = 0;
		try { id = std::stoi(sep != std::string::npos ? raw.substr(0, sep) : raw); }
		catch (const std::exception&) { continue; }
		const auto unit = archivedUnit(id);
		if (unit && static_cast<int32_t>(unit->rarity) > bestRarity)
			bestRarity = static_cast<int32_t>(unit->rarity);
	}

	for (const auto& row : co_await database->execSqlCoro(
		"SELECT friend_id, handle_name, base_unit_id, is_dev, favorite, sphere_1, sphere_2"
		" FROM user_friends WHERE user_id = $1 ORDER BY favorite DESC, friend_id;",
		identity.userId))
	{
		FriendRow entry{};
		entry.friend_id = row["friend_id"].as<std::string>();
		entry.handle_name = row["handle_name"].as<std::string>();
		entry.base_unit_id = row["base_unit_id"].as<int32_t>();
		entry.is_dev = row["is_dev"].as<int32_t>();
		entry.favorite = row["favorite"].as<int32_t>();
		entry.sphere_1 = row["sphere_1"].as<int32_t>();
		entry.sphere_2 = row["sphere_2"].as<int32_t>();

		// BACKFILL.  Rows that predate the sphere columns -- and every row the
		// seeder writes -- carry 0/0.  Roll once, here, and persist: the whole
		// point is that a kit is fixed from the moment the friend exists, so it
		// cannot be left to be re-derived on each read.
		if (entry.sphere_1 == 0)
		{
			const auto rolled = rollFriendSpheres(
				entry.friend_id, chainTopRarity(entry.base_unit_id));
			if (rolled.first != 0)
			{
				co_await database->execSqlCoro(
					"UPDATE user_friends SET sphere_1 = $1, sphere_2 = $2"
					" WHERE user_id = $3 AND friend_id = $4;",
					rolled.first, rolled.second, identity.userId, entry.friend_id);
				entry.sphere_1 = rolled.first;
				entry.sphere_2 = rolled.second;
			}
		}

		// STALE: this friend's species cannot reach where the player already
		// is, so they cannot field a comparable helper.  Hidden from the
		// picker, still listed socially so the player can choose to drop them.
		if (forPicker && chainTopRarity(entry.base_unit_id) < bestRarity)
		{
			LOG_DEBUG << "Friends: " << entry.handle_name << " tops out at r"
				<< chainTopRarity(entry.base_unit_id) << " below the player's r"
				<< bestRarity << "; hidden from the picker";
			continue;
		}

		roster.push_back(std::move(entry));
	}

	co_return roster;
}

std::string devFriendId(const DevFriend& dev)
{
	// Derived from the NAME, not the roster position, so the id a player is
	// offered today is the same one FriendApply names back and the same one
	// stored on recruit.  "DEV" keeps it clear of the seeded DCF##### block.
	std::string id = "DEV";
	for (const char c : dev.name)
	{
		if (std::isalnum(static_cast<unsigned char>(c)))
			id += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	return id.substr(0, 16);
}

std::optional<DevFriend> devEncounterFor(
	const std::vector<FriendRow>& alreadyFriends,
	const std::string& userId,
	const std::string& day)
{
	if (g_devs.empty())
		return std::nullopt;

	// Whoever is already on the roster cannot be met again.
	std::vector<const DevFriend*> available;
	for (const auto& dev : g_devs)
	{
		const auto id = devFriendId(dev);
		const bool have = std::any_of(alreadyFriends.begin(), alreadyFriends.end(),
			[&id](const FriendRow& row) { return row.friend_id == id; });
		if (!have)
			available.push_back(&dev);
	}
	if (available.empty())
		return std::nullopt;

	// Deterministic per (user, day): no storage, and reopening the picker
	// cannot re-roll a miss into a hit.
	std::seed_seq seed{ static_cast<uint32_t>(std::hash<std::string>{}(userId)),
						static_cast<uint32_t>(std::hash<std::string>{}(day)) };
	std::mt19937 rng(seed);

	std::uniform_int_distribution<int32_t> gate(1, kDevEncounterOneIn);
	if (gate(rng) != 1)
		return std::nullopt;        // most days, nobody is there

	std::uniform_int_distribution<size_t> pick(0, available.size() - 1);
	return *available[pick(rng)];
}

std::string strangerFriendId(int32_t baseUnitId)
{
	// 8 digits: chain bases run to 87,602,603, so anything narrower would
	// truncate the top of the archive back onto a different chain.
	auto digits = std::to_string(baseUnitId);
	if (digits.size() < 8)
		digits.insert(0, 8 - digits.size(), '0');
	return "NEW" + digits;
}

namespace
{

/*! The chain behind a stranger id, or 0 when this is not one. */
int32_t strangerChainBase(const std::string& friendId)
{
	if (friendId.size() != 11 || friendId.compare(0, 3, "NEW") != 0)
		return 0;
	if (!std::all_of(friendId.begin() + 3, friendId.end(),
			[](unsigned char c) { return std::isdigit(c) != 0; }))
		return 0;
	try { return std::stoi(friendId.substr(3)); }
	catch (...) { return 0; }
}

/*!
* Chains the player may no longer be offered.
*
* Ordinary friends CLAIM their chain; developers do not.  Evan fields Krantz as
* an easter egg, not as Krantz's representative, so recruiting him must leave an
* ordinary Krantz friend perfectly available.
*/
std::vector<int32_t> claimedChains(const std::vector<FriendRow>& roster)
{
	std::vector<int32_t> claimed;
	for (const auto& row : roster)
	{
		if (row.is_dev == 0 && row.base_unit_id > 0)
			claimed.push_back(row.base_unit_id);
	}
	return claimed;
}

} // namespace

std::vector<FriendRow> strangerSuggestions(
	const std::vector<FriendRow>& roster,
	const std::string& userId,
	const std::string& day,
	const PlayerPeak& peak)
{
	std::vector<FriendRow> out;
	if (g_chainBases.empty())
		return out;

	const auto claimed = claimedChains(roster);

	std::seed_seq seed{ static_cast<uint32_t>(std::hash<std::string>{}(userId)),
						static_cast<uint32_t>(std::hash<std::string>{}(day)) };
	std::mt19937 rng(seed);
	std::uniform_int_distribution<size_t> pick(0, g_chainBases.size() - 1);

	// Bounded tries rather than a shuffle of all 847: most draws are accepted,
	// and a player deep enough to have exhausted the pool should get a short
	// list rather than a scan of the whole archive on every picker open.
	for (int32_t tries = 0;
		 tries < kSuggestionCount * 20 && static_cast<int32_t>(out.size()) < kSuggestionCount;
		 ++tries)
	{
		const auto base = g_chainBases[pick(rng)];

		if (std::find(claimed.begin(), claimed.end(), base) != claimed.end())
			continue;
		if (std::any_of(out.begin(), out.end(),
				[base](const FriendRow& r) { return r.base_unit_id == base; }))
			continue;
		// Never offer someone who is stale the moment they are added.
		if (chainTopRarity(base) < peak.rarity)
			continue;

		const auto unit = archivedUnit(base);
		if (!unit)
			continue;

		out.push_back(FriendRow{ strangerFriendId(base), unit->name, base, 0, 0 });
	}

	return out;
}

drogon::Task<bool> recruitFriend(
	const db::Database database,
	const UserIdentity identity,
	const std::string& friendId)
{
	// What the client is showing as "5 / 20": the level's own allowance plus
	// whatever was bought through ShopUse type 7, exactly as gme::getTeamInfo
	// builds it.  Enforced here so accepting cannot push the roster past the
	// number the player can see.
	const auto info = co_await database->execSqlCoro(
		"SELECT level, max_friend_count FROM user_info WHERE id = $1;", identity.userId);
	if (!info.empty())
	{
		if (const auto levelMst = getLevelMst(info[0]["level"].as<uint32_t>()))
		{
			const auto capacity = levelMst->friend_count
				+ std::min(info[0]["max_friend_count"].as<int32_t>(), levelMst->add_friend_count);
			const auto held = (co_await database->execSqlCoro(
				"SELECT COUNT(*) AS n FROM user_friends WHERE user_id = $1;",
				identity.userId))[0]["n"].as<int32_t>();
			if (held >= capacity)
			{
				LOG_INFO << "Friends: " << identity.userId << " is at " << held << "/" << capacity
					<< " friends; " << friendId << " not added";
				co_return false;
			}
		}
	}

	std::string handleName;
	int32_t baseUnitId = 0;
	int32_t isDev = 0;
	std::string flavour;

	const auto dev = std::find_if(g_devs.begin(), g_devs.end(),
		[&friendId](const DevFriend& d) { return devFriendId(d) == friendId; });
	if (dev != g_devs.end())
	{
		handleName = dev->name;
		baseUnitId = dev->base_unit_id;
		isDev = 1;
		flavour = dev->unit;
	}
	else if (const auto base = strangerChainBase(friendId); base != 0)
	{
		// Only a real chain base, and only one the roster has not already
		// claimed -- a stale offer the player sat on must not slip past the
		// one-friend-per-chain rule after they befriended that chain elsewhere.
		if (!std::binary_search(g_chainBases.begin(), g_chainBases.end(), base))
		{
			LOG_WARN << "Friends: " << friendId << " names " << base
				<< ", which is not a chain base; ignored";
			co_return false;
		}
		const auto held = co_await database->execSqlCoro(
			"SELECT COUNT(*) AS n FROM user_friends"
			" WHERE user_id = $1 AND is_dev = 0 AND base_unit_id = $2;",
			identity.userId, base);
		if (held[0]["n"].as<int32_t>() > 0)
		{
			LOG_INFO << "Friends: chain " << base << " is already on "
				<< identity.userId << "'s roster; " << friendId << " not added";
			co_return false;
		}

		const auto unit = archivedUnit(base);
		if (!unit)
			co_return false;
		handleName = unit->name;
		baseUnitId = base;
		flavour = unit->name;
	}
	else
	{
		co_return false;
	}

	// THE KIT IS LOCKED IN RIGHT HERE.  Rolled once, at the moment of adding,
	// and never touched again -- the player's reward for shopping the picker is
	// that what they saw is what they keep, and the only way to reroll it is to
	// unfriend and meet them again.
	const auto spheres = rollFriendSpheres(friendId, chainTopRarity(baseUnitId));

	// INSERT OR IGNORE: the client can resend an apply, and somebody already on
	// the roster must not be duplicated onto it.
	const auto result = co_await database->execSqlCoro(
		"INSERT OR IGNORE INTO user_friends"
		" (user_id, friend_id, handle_name, base_unit_id, is_dev, favorite, added_day,"
		"  sphere_1, sphere_2)"
		" VALUES ($1, $2, $3, $4, $5, 0, date('now'), $6, $7);",
		identity.userId, friendId, handleName, baseUnitId, isDev,
		spheres.first, spheres.second);

	if (result.affectedRows() > 0)
		LOG_INFO << "Friends: " << identity.userId << " recruited " << handleName
			<< " (" << flavour << ")" << (isDev != 0 ? " [dev]" : "");
	co_return result.affectedRows() > 0;
}

FriendSpheres rollFriendSpheres(const std::string& seedKey, int32_t maxRarity)
{
	FriendSpheres out{};
	if (g_spheres.empty())
		return out;

	// Only gear the friend could plausibly have earned by the top of their own
	// chain -- judged against what they will become, because the roll is
	// permanent and gating it on today's form would leave an early friend
	// stuck with starter gear forever.
	std::vector<const SphereRow*> eligible;
	for (const auto& sphere : g_spheres)
	{
		if (sphere.rarity <= maxRarity)
			eligible.push_back(&sphere);
	}
	if (eligible.empty())
	{
		for (const auto& sphere : g_spheres)
		{
			if (sphere.rarity <= 1)
				eligible.push_back(&sphere);
		}
	}
	if (eligible.empty())
		return out;

	// Rarity is deliberately NOT in the seed: a kit must not change when its
	// owner evolves.  Only being unfriended and met again rerolls it.
	std::seed_seq seed{ static_cast<uint32_t>(std::hash<std::string>{}(seedKey)) };
	std::mt19937 rng(seed);
	std::uniform_int_distribution<size_t> pick(0, eligible.size() - 1);

	const auto* first = eligible[pick(rng)];
	out.first = first->id;

	// DIFFERENT CATEGORY.  Two spheres sharing a sphere_type is a loadout the
	// game would never let a player build, and the client draws each slot's
	// frame from that same field, so it would also render two identical frames.
	for (int32_t tries = 0; tries < 64; ++tries)
	{
		const auto* second = eligible[pick(rng)];
		if (second->sphere_type != first->sphere_type)
		{
			out.second = second->id;
			break;
		}
	}

	return out;
}

std::vector<::FriendInfo> socialList(
	const std::vector<FriendRow>& roster,
	const PlayerPeak& peak)
{
	std::vector<::FriendInfo> out;
	const auto loginTimestamp = static_cast<int32_t>(std::time(nullptr));

	for (const auto& mate : roster)
	{
		const auto unit = friendUnitFor(mate, peak);
		if (unit.unit_id == 0)
		{
			LOG_WARN << "Friends: " << mate.handle_name << " has unknown chain base "
				<< mate.base_unit_id << "; left off the Social list rather than sent half-built";
			continue;
		}

		::FriendInfo fi{};
		fi.user_id         = mate.friend_id;
		fi.handle_name     = mate.handle_name;
		fi.team_lv         = 999;
		// MUST be 1: existTypeOK @0x12607CC accepts nothing else, and it is what
		// pays the friend Honor rate instead of the stranger rate.
		fi.friend_type     = 1;
		fi.last_login_date = loginTimestamp;
		fi.unit_id         = unit.unit_id;
		fi.unit_lv         = unit.level;
		fi.base_hp         = unit.base_hp;
		fi.base_atk        = unit.base_atk;
		fi.base_def        = unit.base_def;
		fi.base_heal       = unit.base_rec;
		fi.skill_id        = std::to_string(unit.bb_id);
		fi.skill_lv        = unit.bb_lvl;
		fi.extra_skill_id  = std::to_string(unit.sbb_id);
		fi.extra_skill_lv  = unit.sbb_lvl;
		fi.unit_type_id    = unit.unit_type_id;
		fi.element         = unit.element;
		// Ge8Yo32T / mZA7fH2v are the helper's SPHERES (setEquipItemID, not
		// setMissionID as they were once documented).  STORED on the roster row
		// and locked from the moment they were added -- see friendSpheresFor.
		fi.equipitem_id    = std::to_string(mate.sphere_1);
		// The second slot exists from 7-star up.  The sphere itself was rolled
		// and stored when they were added; this only decides whether the slot
		// is open yet.
		fi.equipitem_id2   = std::to_string(
			unit.rarity >= kSecondSphereRarity ? mate.sphere_2 : 0);
		fi.friend_id       = mate.friend_id;
		fi.friend_message  = mate.is_dev != 0 ? "decompfrontier" : "GG WP";
		fi.favorite        = mate.favorite;
		fi.priority        = 1;
		fi.deck_no         = 0;
		fi.guild_id        = 0;
		out.emplace_back(std::move(fi));
	}

	return out;
}

drogon::Task<bool> isFriend(
	const db::Database database,
	const UserIdentity identity,
	const std::string& friendId)
{
	if (friendId.empty())
		co_return false;
	const auto row = co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n FROM user_friends WHERE user_id = $1 AND friend_id = $2;",
		identity.userId, friendId);
	co_return row[0]["n"].as<int32_t>() > 0;
}

drogon::Task<bool> removeFriend(
	const db::Database database,
	const UserIdentity identity,
	const std::string& friendId)
{
	if (friendId.empty())
		co_return false;

	const auto result = co_await database->execSqlCoro(
		"DELETE FROM user_friends WHERE user_id = $1 AND friend_id = $2;",
		identity.userId, friendId);
	co_return result.affectedRows() > 0;
}

} // namespace gme
