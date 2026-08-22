#include "BraveSlots.hpp"

#include "Common.hpp"

#include <gimuserver/archive/archive.hpp>
#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/utils/JsonFile.hpp>
#include <gimuserver/utils/Random.hpp>

#include <algorithm>
#include <chrono>
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
* Splits a comma-separated id list.
*/
std::vector<uint32_t> splitIds(const std::string& csv)
{
	std::vector<uint32_t> out;
	std::string cur;
	for (size_t i = 0; i <= csv.size(); ++i)
	{
		if (i == csv.size() || csv[i] == ',')
		{
			if (!cur.empty())
			{
				try { out.push_back(static_cast<uint32_t>(std::stoul(cur))); }
				catch (const std::exception&) {}
			}
			cur.clear();
		}
		else
		{
			cur += csv[i];
		}
	}
	return out;
}

/*!
* Joins the reel stops the way h6smq0WE and D20kuSLy carry them.
*
* These are PICTURE IDS.  The client derives the strip position itself:
* RandallSlotActionScene::stopReelAction @0x1A71E60 walks the reel strip
* comparing each entry against the target string and uses the position it
* finds, and getReelPictureFile @0x1A72838 runs StrToInt and then
* SlotgamePictureInfoList::getObject(id) to pick the sprite.
*
* THIS USED TO SEND STRIP INDICES, AND THAT ONE MISTAKE CAUSED BOTH OF THE
* SLOTS BUGS.  An index is a small integer that sometimes collides with a real
* picture id and sometimes does not, so it failed two different ways at once:
*   - symbols 1,7,9 sit at indices 6,7,8, which ARE valid picture ids, so the
*     reels stopped on the wrong pictures and the popup disagreed with them;
*   - symbols 81,13,82 sit at indices 0,4,5, which are NOT picture ids, so
*     getObject @0x1305118 returned xzr and getSlotPictureName dereferenced it.
* Nothing checks that null, so an undrawable symbol is a hard client crash -
* which is why one is substituted here rather than sent.
*/
std::string joinSymbols(const std::vector<uint32_t>& reels)
{
	const auto& stored = theServer()->cache().braveSlotsResp();

	// Reel 1's strip stands for all of them; they are authored identical.
	const auto strip = stored.reels.empty()
		? std::vector<uint32_t>{}
		: splitIds(stored.reels.front().reel_data);

	// `id` is i32::str - int32_t in C++, quoted only on the wire (handbook 3.4).
	std::vector<uint32_t> drawable;
	for (const auto& picture : stored.pictures)
	{
		drawable.push_back(static_cast<uint32_t>(picture.id));
	}

	const auto canDraw = [&drawable](const uint32_t symbol) {
		return std::find(drawable.begin(), drawable.end(), symbol) != drawable.end();
	};

	std::string out;
	for (auto symbol : reels)
	{
		if (!canDraw(symbol))
		{
			// The first strip symbol the machine can actually draw.  Loud,
			// because the reels will not read the way they were authored.
			const auto fallback = std::find_if(strip.begin(), strip.end(), canDraw);
			const uint32_t safe = fallback == strip.end() ? 0 : *fallback;
			LOG_ERROR << "BraveSlots: reel symbol " << symbol
				<< " is not in the picture list - the client would dereference a"
				   " null SlotgamePictureInfo; substituting " << safe;
			symbol = safe;
		}
		else if (std::find(strip.begin(), strip.end(), symbol) == strip.end())
		{
			// Not fatal: stopReelAction falls back to the reel's stored stop
			// when its search misses.  The reel just lands somewhere else.
			LOG_WARN << "BraveSlots: reel symbol " << symbol
				<< " is drawable but absent from the reel strip - the reel will"
				   " stop somewhere other than where it was authored";
		}

		if (!out.empty())
		{
			out += ',';
		}
		out += std::to_string(symbol);
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
	case 6:                 return "1";   // unit   -> UnitMstList
	case 4: case 5: case 7: return "2";   // items  -> ItemMstList
	case 12:                return "5";   // medal  -> MedalMstList
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

drogon::Task<bool> grantDailyBraveMedals(
	const db::Database database,
	const UserIdentity& identity)
{
	// Midnight UTC, matching how the daily spin rolls over — one clock for
	// everything that resets daily, so they cannot disagree at the boundary.
	const auto now = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
	const int64_t startOfDay = (now / 86400) * 86400;

	// Keyed off the present box itself rather than a table of its own: the
	// gift IS the record, so there is no second piece of state to drift.
	const auto existing = co_await database->execSqlCoro(
		"SELECT 1 FROM user_presents"
		" WHERE user_id = $1 AND receipt_type = $2 AND present_date >= $3 LIMIT 1;",
		identity.userId, kBraveSlotDailyGiftReceiptType, startOfDay);

	if (!existing.empty())
	{
		co_return false;
	}

	// present_type 12 is the MEDAL type, with target_id naming which medal —
	// PresentCommon::createThumbnail @0x11C4900 routes 12 to
	// GameUtils::getMedalThumbnail(target_id).
	co_await gme::addUserPresent(
		database, identity,
		12, kBraveSlotMedalId, kBraveSlotDailyGift,
		kBraveSlotDailyGiftReceiptType, "Daily Raid Medals");

	LOG_INFO << "BraveSlots: queued " << kBraveSlotDailyGift
		<< " daily medal(s) for " << identity.userId;
	co_return true;
}

drogon::Task<std::vector<SlotgameResultInfo>> playBraveSlot(
	const db::Database database,
	const UserIdentity& identity,
	const int32_t drawCount)
{
	// Seeds on first sight, so a player who goes straight to the machine can
	// still pull.
	const auto medals = co_await loadBraveMedals(database, identity);

	const auto held = std::find_if(medals.begin(), medals.end(),
		[](const UserBraveMedalInfo& m) { return m.medal_id == std::stoi(kBraveSlotMedalId); });
	const int32_t balance = held == medals.end() ? 0 : held->possession;

	std::vector<SlotgameResultInfo> results;

	// The count is clamped rather than trusted.  calcSeqCount @0x1A70998
	// already caps it client-side, but d04gRmkE arrives over the wire and a
	// bad one would spend medals the player does not have and roll a hundred
	// prizes.  Affordability decides the rest, matching the client's own
	// walk-down.
	const int32_t affordable = kBraveSlotPullCost > 0 ? balance / kBraveSlotPullCost : 0;
	const int32_t pulls = std::min({std::max(drawCount, 1), kBraveSlotMaxPulls, affordable});

	if (pulls <= 0)
	{
		// The client checks this too (RANDALL_SLOTGAME_MEDAL_ERROR), so this is
		// the backstop rather than the primary gate.
		LOG_WARN << "BraveSlots: " << identity.userId << " has " << balance
			<< " medal(s), needs " << kBraveSlotPullCost;
		co_return results;
	}

	if (pulls < drawCount)
	{
		LOG_WARN << "BraveSlots: " << identity.userId << " asked for " << drawCount
			<< " pull(s), playing " << pulls
			<< " (balance " << balance << ", max " << kBraveSlotMaxPulls << ")";
	}

	// Charged up front, in one write, so a throwing award cannot leave the
	// player having rolled for free.
	const int32_t remaining = balance - (pulls * kBraveSlotPullCost);
	co_await db::DatabaseInterface::update(
		database,
		"user_brave_medals",
		{
			db::Data("possession", std::to_string(remaining)),
			db::Lookup("user_id", identity.userId),
			db::Lookup("medal_id", kBraveSlotMedalId),
		});

	int64_t zel = 0, gems = 0;
	int32_t medalsWon = 0;
	for (int32_t pull = 0; pull < pulls; ++pull)
	{
		const auto* prize = rollPrize();
		if (prize == nullptr)
		{
			LOG_WARN << "BraveSlots: payout table is empty, nothing to roll";
			break;
		}

		// Pay out through the shared vocabulary; present_type 0 means this
		// outcome pays nothing, which is a legitimate result rather than an
		// error.
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
		case 12:
			// The medal symbol paying medals back — the wiki's "1 Raid Medal"
			// row.  ACCUMULATED rather than written here: the medals were
			// charged in one write before the loop, and `remaining` is what
			// every result reports as the balance, so crediting mid-loop would
			// leave each medal_num understating what the player actually holds.
			medalsWon += static_cast<int32_t>(prize->target_cnt);
			break;
		default:
			LOG_WARN << "BraveSlots: present_type " << prize->present_type
				<< " not supported — skipped";
			break;
		}

		// Derived rather than stored, so the shown prize cannot drift from the
		// awarded one.  loadBraveSlotArchive has already dropped any row this
		// would map to 0, so the value here is always renderable.
		results.push_back(SlotgameResultInfo{
			.prize_type = popupPrizeType(prize->present_type),
			// ⚠ TWO fields, '@'-separated: "<targetId>@<count>".  NOT the
			// description, and NOT the bare id either.
			//
			// createPrizeDrawInfo @0x1A75674 runs CommonUtils::split over this
			// using RandallSlotScene::SLOT_PRIZE_DATA_DELIMITER (a global that
			// resolves to "@"), looks element [0] up with
			// UnitMstList/ItemMstList::getObject(STRING), and then builds the
			// popup label as getItemName() + "x" + element [1] — the COUNT.
			//
			// Element [1] is read at [vector + 0x18] with NO bounds check
			// (@0x1A75CE0), so a single-field value makes the client construct
			// a std::string from whatever heap follows the vector and memcpy
			// from a wild pointer.  That is a CORRUPTION bug, not a clean
			// crash: it depends on what happens to sit past the allocation, so
			// the SAME payload rendered fine once and took the client down
			// twice.
			//
			// Same shape as b5yeVr61 ("<medalId>@<cost>") — when a slot field
			// looks like a lone id, check whether it is really an @-pair.
			.prize_data = std::to_string(prize->target_id) + "@"
				+ std::to_string(prize->target_cnt),
			.prize_rank = prize->prize_rank,
			.picture_pattern = joinSymbols(prize->reels),
			.effect_sam = "",
			.effect_type = "0",
			.effect_param = "0",
			.reel_stop_num = joinSymbols(prize->reels),
			// Every entry carries the balance AFTER the whole action, because
			// the client reads the counter off the result it is showing and a
			// per-pull figure would count back down as the list is paged.
			.medal_num = std::to_string(remaining),
		});

		LOG_INFO << "BraveSlots: " << identity.userId << " pulled '" << prize->key
			<< "' (" << prize->description << "), reels "
			<< results.back().reel_stop_num;
	}

	if (medalsWon > 0)
	{
		co_await database->execSqlCoro(
			"UPDATE user_brave_medals SET possession = MIN(possession + $1, $2)"
			" WHERE user_id = $3 AND medal_id = $4;",
			medalsWon, kBraveSlotMedalCap, identity.userId, kBraveSlotMedalId);

		// Every entry reports the same post-action balance, so they all have to
		// be corrected together once the winnings are known.
		const auto finalBalance = std::min(remaining + medalsWon, kBraveSlotMedalCap);
		for (auto& entry : results)
		{
			entry.medal_num = std::to_string(finalBalance);
		}
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

	LOG_INFO << "BraveSlots: " << identity.userId << " played " << results.size()
		<< " pull(s), " << remaining << " medal(s) left";
	co_return results;
}

} // namespace gme
