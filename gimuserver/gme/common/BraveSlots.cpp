#include "BraveSlots.hpp"

#include "Common.hpp"

#include <gimuserver/archive/archive.hpp>
#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/utils/JsonFile.hpp>
#include <gimuserver/utils/Random.hpp>

#include <algorithm>
#include <vector>

namespace gme
{

namespace
{

/*!
* The authored payout table, loaded once at startup.
*/
std::vector<BraveSlotPrize> g_prizes;

/*!
* Joins the reel stops the way h6smq0WE carries them.
*
* Emits the symbol's INDEX in the reel strip rather than its picture id, and
* that choice is deliberate under uncertainty.  Nothing pins which the client
* wants, but the two fail very differently: an index the client reads as a
* picture id merely stops the reel on the wrong symbol, whereas a picture id
* the client reads as an index runs off the end of a 14-entry strip - ids go up
* to 82.  The cosmetic failure is the one to risk.
*
* The index is a position in the REEL STRIP (iW62Scdg), which is a different
* order from the picture list — indexing the wrong one lands the reel on an
* unrelated symbol.  A symbol absent from the strip falls back to 0 rather than
* pushing an out-of-range value onto the wire.
*/
std::string joinReels(const std::vector<uint32_t>& reels)
{
	// Index into the REEL STRIP, not the picture list — the two are different
	// orders and only the strip is what the reel actually cycles through.
	// Reel 1's strip stands for all of them; they are authored identical.
	const auto& stored = theServer()->cache().braveSlotsResp();
	std::vector<uint32_t> strip;
	if (!stored.reels.empty())
	{
		std::string cur;
		const auto& data = stored.reels.front().reel_data;
		for (size_t i = 0; i <= data.size(); ++i)
		{
			if (i == data.size() || data[i] == ',')
			{
				if (!cur.empty())
				{
					try { strip.push_back(static_cast<uint32_t>(std::stoul(cur))); }
					catch (const std::exception&) {}
				}
				cur.clear();
			}
			else
			{
				cur += data[i];
			}
		}
	}

	std::string out;
	for (const auto symbol : reels)
	{
		uint32_t index = 0;
		for (size_t i = 0; i < strip.size(); ++i)
		{
			if (strip[i] == symbol)
			{
				index = static_cast<uint32_t>(i);
				break;
			}
		}

		if (!out.empty())
		{
			out += ',';
		}
		out += std::to_string(index);
	}
	return out;
}

/*!
* The popup's prize code for a reward, or 0 when it has none.
*
* This is the 1..5 vocabulary from RandallSlotScene::createPrizeDrawInfo's jump
* table @0x1A757D8, NOT present_type: 1 unit, 2/3 item, 5 medal.
*/
std::string popupPrizeType(const uint32_t presentType)
{
	switch (presentType)
	{
	case 6:                 return "1";   // unit
	case 4: case 5: case 7: return "2";   // item domains
	default:                return "0";   // nothing the popup can draw
	}
}

/*!
* Picks an outcome, weighted.
*/
const BraveSlotPrize* rollPrize()
{
	uint32_t total = 0;
	for (const auto& p : g_prizes)
	{
		total += p.weight;
	}
	if (total == 0)
	{
		return nullptr;
	}

	auto roll = RandomUInt(1, total);
	for (const auto& p : g_prizes)
	{
		if (roll <= p.weight)
		{
			return &p;
		}
		roll -= p.weight;
	}
	return &g_prizes.back();
}

} // namespace

void loadBraveSlotArchive(const std::string& archiveRoot)
{
	if (archiveRoot.empty())
	{
		LOG_WARN << "BraveSlots: archive_root is empty, payout table not loaded";
		return;
	}

	try
	{
		g_prizes = LoadJson<std::vector<BraveSlotPrize>>(archiveRoot, "brave_slots.json");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "BraveSlots: failed to load brave_slots.json: " << e.what();
		return;
	}

	// ⚠ EVERY row must be renderable, because THE MACHINE CANNOT EXPRESS
	// "NO PRIZE".  RandallSlotActionScene::updateEvent @0x1A7049C pushes the
	// result popup unconditionally once the reels stop, and
	// RandallSlotResultScene::initialize @0x1A74448 runs setPrizeData and then
	// loadLayout REGARDLESS — so a prize the popup cannot draw leaves
	// prize_detail_unit_image unbuilt and setLayoutControl dereferences it.
	// (setPrizeData's popScene() branch, taken when the type is 0 or 4, does
	// NOT prevent that: the layout pass has already been queued.)
	//
	// So an unrenderable row is dropped here rather than allowed to reach a
	// player, and loudly, because the failure it causes is a hard crash with
	// no server-side symptom at all.
	std::erase_if(g_prizes, [](const BraveSlotPrize& p) {
		if (popupPrizeType(p.present_type) != "0")
		{
			return false;
		}
		LOG_ERROR << "BraveSlots: dropping payout row '" << p.key
			<< "' — present_type " << p.present_type
			<< " has no prize-popup code, and sending it CRASHES the client";
		return true;
	});

	LOG_INFO << "BraveSlots: loaded " << g_prizes.size()
		<< " payout rows from " << archiveRoot << "/brave_slots.json";
}

drogon::Task<std::vector<UserBraveMedalInfo>> loadBraveMedals(
	const db::Database database,
	const UserIdentity& identity)
{
	auto rows = (co_await db::DatabaseInterface::read(
		database,
		"user_brave_medals",
		{
			db::Data("medal_id"),
			db::Data("possession"),
			db::Lookup("user_id", identity.userId),
		})).data;

	if (rows.empty())
	{
		// Nothing offline earns medals, so the starter stock is seeded here.
		co_await db::DatabaseInterface::insert(
			database,
			"user_brave_medals",
			{
				db::Data("user_id", identity.userId),
				db::Data("medal_id", kBraveSlotMedalId),
				db::Data("possession", std::to_string(kBraveSlotSeedMedals)),
			});
		LOG_INFO << "BraveSlots: seeded " << kBraveSlotSeedMedals
			<< " medals for " << identity.userId;

		co_return std::vector<UserBraveMedalInfo>{
			UserBraveMedalInfo{
				.user_id = identity.userId,
				.medal_id = std::stoi(kBraveSlotMedalId),
				.possession = kBraveSlotSeedMedals,
			}};
	}

	std::vector<UserBraveMedalInfo> out;
	for (const auto& row : rows)
	{
		out.push_back(UserBraveMedalInfo{
			.user_id = identity.userId,
			.medal_id = std::stoi(row["medal_id"].as<std::string>()),
			.possession = row["possession"].as<int32_t>(),
		});
	}
	co_return out;
}

drogon::Task<bool> playBraveSlot(
	const db::Database database,
	const UserIdentity& identity,
	SlotgameResultInfo& result)
{
	// Seeds on first sight, so a player who goes straight to the machine can
	// still pull.
	const auto medals = co_await loadBraveMedals(database, identity);

	const auto held = std::find_if(medals.begin(), medals.end(),
		[](const UserBraveMedalInfo& m) { return m.medal_id == std::stoi(kBraveSlotMedalId); });
	const int32_t balance = held == medals.end() ? 0 : held->possession;

	if (balance < kBraveSlotPullCost)
	{
		// The client checks this too (RANDALL_SLOTGAME_MEDAL_ERROR), so this is
		// the backstop rather than the primary gate.
		LOG_WARN << "BraveSlots: " << identity.userId << " has " << balance
			<< " medal(s), needs " << kBraveSlotPullCost;
		co_return false;
	}

	const auto* prize = rollPrize();
	if (prize == nullptr)
	{
		LOG_WARN << "BraveSlots: payout table is empty, nothing to roll";
		co_return false;
	}

	const int32_t remaining = balance - kBraveSlotPullCost;
	co_await db::DatabaseInterface::update(
		database,
		"user_brave_medals",
		{
			db::Data("possession", std::to_string(remaining)),
			db::Lookup("user_id", identity.userId),
			db::Lookup("medal_id", kBraveSlotMedalId),
		});

	// Pay out through the shared vocabulary; present_type 0 means this outcome
	// pays nothing, which is a legitimate result rather than an error.
	int64_t zel = 0, gems = 0;
	switch (prize->present_type)
	{
	case 0:
		break;
	case 3:
		zel += prize->target_cnt;
		break;
	case 8:
		gems += prize->target_cnt;
		break;
	case 6:
	{
		const auto& unitMst = theServer()->cache().unitMst();
		const auto unit = std::find_if(unitMst.begin(), unitMst.end(),
			[prize](const auto& u) { return u.id == prize->target_id; });
		if (unit == unitMst.end())
		{
			LOG_WARN << "BraveSlots: prize unit " << prize->target_id
				<< " not in unit_mst — skipped";
			break;
		}
		for (uint32_t i = 0; i < prize->target_cnt; ++i)
		{
			co_await gme::addUserUnit(database, identity, *unit);
		}
		break;
	}
	case 4:
	case 5:
	case 7:
		co_await gme::addUserItem(database, identity, prize->target_id, prize->target_cnt);
		break;
	default:
		LOG_WARN << "BraveSlots: present_type " << prize->present_type
			<< " not supported — skipped";
		break;
	}

	if (zel > 0 || gems > 0)
	{
		// Clamped to the display widths (handbook §10).
		co_await database->execSqlCoro(
			"UPDATE user_info SET"
			" zel  = MIN(zel  + $1, $2),"
			" gems = MIN(gems + $3, $4)"
			" WHERE id = $5;",
			zel, static_cast<int64_t>(99'999'999), gems, static_cast<int64_t>(9'999),
			identity.userId);
	}

	// Derived rather than stored, so the shown prize cannot drift from the
	// awarded one.  loadBraveSlotArchive has already dropped any row this
	// would map to 0, so the value here is always renderable.
	const std::string prizeType = popupPrizeType(prize->present_type);

	result = SlotgameResultInfo{
		.prize_type = prizeType,
		// ⚠ NOT the description.  createPrizeDrawInfo looks this up with
		// UnitMstList/ItemMstList::getObject(STRING), so it is the TARGET ID.
		// A description here returns null, the prize sprite is never built,
		// and the popup crashes positioning prize_detail_unit_image.
		.prize_data = std::to_string(prize->target_id),
		.prize_rank = prize->prize_rank,
		.picture_pattern = joinReels(prize->reels),
		.effect_sam = "",
		.effect_type = "0",
		.effect_param = "0",
		.reel_stop_num = joinReels(prize->reels),
		.medal_num = std::to_string(remaining),
	};

	LOG_INFO << "BraveSlots: " << identity.userId << " pulled '" << prize->key
		<< "' (" << prize->description << "), reels " << result.reel_stop_num
		<< ", " << remaining << " medal(s) left";
	co_return true;
}

} // namespace gme
