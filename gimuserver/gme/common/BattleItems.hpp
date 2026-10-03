#pragma once

#include "Common.hpp"

#include <charconv>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// BATTLE ITEMS: THE LOADOUT AND THE WAREHOUSE ARE DISJOINT.
//
// The client keeps two separate stores and moves counts between them itself
// (libgame.so arm64):
//
//   * ItemEditSelectCntScene::setEqpItem @0x17198C8 returns the slot's old
//     count to the warehouse (GameUtils::incWarehouseItem) and takes the new
//     count out of it (GameUtils::decWarehouseItem); ItemEditListScene::removeEqp
//     returns a slot; ItemBonusListScene::addEqpList does the same for the bonus
//     bar.  GameUtils::eqpItemFull(bool,bool) @0x1188D34 -- run by
//     MissionCheckScene::setParams before every quest -- tops each slot up to
//     ItemMst::getMaxEquipNum (t1i2vIbT) out of the warehouse and queues an
//     ItemEditRequest.  The server only ever sees the resulting loadout.
//   * In battle, MissionBattleManager::useItem @0x1078B0C spends from the
//     LOADOUT (UserEquipItemInfo::useItem @0x128ED50 decrements its count) and
//     records the use in BattleUseItemLogList, which MissionEnd reports as
//     47cJBUxz "itemId:count,...".
//
// So the server has to mirror both moves.  ItemEdit used to replace the loadout
// without touching user_items (every equipped item existed twice -- in the bar
// and still in storage), and MissionEnd ignored the use log (the bar came back
// full at the next login).  Those were the "duplicated unused items" and "kept
// the items I used".
//
// Items a battle SUPPLIES never pass through here: the Battle Simulator's 99s
// (SandbagEquipItemInfoList), a challenge's selected item set
// (BattleState::setChallengeItemSetID), raid and Grand Quest bars all live in
// separate client lists (GameUtils::getBattleEquipItemInfo @0x11A1008), and a
// use log can only ever take items OUT of the owned loadout, never put any in.
namespace gme
{

/// One occupied slot of either bar, as the request / table carries it.
struct LoadoutSlot
{
	int32_t dispOrder = 0;
	int32_t itemId = 0;
	int32_t count = 0;
};

/*!
* Parses MissionEnd's use-item log (47cJBUxz, BattleUseItemLogList::getCsv
* @0x1055330): comma-separated `itemId:count`, both positive decimals.  Empty
* means nothing was used.  Repeated ids are summed.
*
* @throws std::runtime_error on anything else, so a malformed log rejects the
*         whole result instead of being half-applied.
*/
inline std::map<int32_t, int64_t> parseUseItemLog(const std::string& log)
{
	std::map<int32_t, int64_t> used;
	if (log.empty())
		return used;

	const auto number = [](std::string_view text) {
		int32_t value = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (error != std::errc{} || end != text.data() + text.size() || value <= 0)
			throw std::runtime_error("Invalid use-item log number: " + std::string(text));
		return value;
	};

	size_t at = 0;
	while (true)
	{
		const auto comma = log.find(',', at);
		const std::string_view entry(log.data() + at,
			(comma == std::string::npos ? log.size() : comma) - at);
		const auto colon = entry.find(':');
		if (colon == std::string_view::npos || entry.find(':', colon + 1) != std::string_view::npos)
			throw std::runtime_error("Invalid use-item log entry: " + std::string(entry));
		used[number(entry.substr(0, colon))] += number(entry.substr(colon + 1));
		if (comma == std::string::npos)
			break;
		at = comma + 1;
		if (at == log.size())
			throw std::runtime_error("Trailing use-item log delimiter");
	}
	return used;
}

/*!
* Whether a mission's battle spends the player's OWN loadout.
*
* Dungeon types whose battles run on a supplied item list report uses of THAT
* list, which must not be taken out of the player's bar:
*   1 / 3  normal / SP challenge -- the selected ChallengeItemMst set
*          (GameUtils::isNormalChallengeMission / isSPChallengeMission)
*   6      the Battle Simulator -- "items are unlimited", 99 each
*          (GameUtils::isSandbag @0x1EC18D8)
*/
inline bool missionUsesOwnedLoadout(const int32_t missionId)
{
	const auto& cache = theServer()->cache();
	for (const auto& [dungeonId, missions] : cache.missionsByDungeon())
	{
		if (std::find(missions.begin(), missions.end(), missionId) == missions.end())
			continue;
		for (const auto& dungeon : cache.dungeonMst())
		{
			if (dungeon.dungeon_id != dungeonId)
				continue;
			const auto type = dungeon.dungeon_type;
			return type != 1 && type != 3 && type != 6;
		}
		break;
	}
	return true;
}

/*!
* Spends a battle's reported item uses from the owned loadout.
*
* Each used item comes out of that item's slots in slot order and never takes a
* slot below zero.  Uses beyond what the bar holds are NOT taken from the
* warehouse: the warehouse was never in the battle, and the excess can only be
* a supplied item or a log we cannot trust -- so it is logged and ignored.
* Slots are kept at zero rather than deleted: the client keeps the empty slot
* on its bar, and the next eqpItemFull refills it by id.
*
* @return What was actually taken, item id -> count.
*/
inline drogon::Task<std::map<int32_t, int64_t>> consumeLoadoutItems(
	const db::Database database,
	const UserIdentity identity,
	const std::map<int32_t, int64_t>& used)
{
	std::map<int32_t, int64_t> taken;
	for (const auto& [itemId, count] : used)
	{
		int64_t remaining = count;
		const auto slots = co_await database->execSqlCoro(
			"SELECT disp_order, item_num FROM user_equip_items"
			" WHERE user_id = $1 AND item_id = $2 ORDER BY disp_order;",
			identity.userId, itemId);
		for (const auto& slot : slots)
		{
			if (remaining <= 0)
				break;
			const auto held = slot["item_num"].as<int64_t>();
			const auto take = std::min(held, remaining);
			if (take <= 0)
				continue;
			co_await database->execSqlCoro(
				"UPDATE user_equip_items SET item_num = item_num - $1"
				" WHERE user_id = $2 AND disp_order = $3;",
				take, identity.userId, slot["disp_order"].as<int32_t>());
			remaining -= take;
			taken[itemId] += take;
		}
		if (remaining > 0)
		{
			LOG_WARN << "Battle items: " << identity.userId << " reported using " << count
				<< " of item " << itemId << " but the owned bar held only "
				<< (count - remaining) << "; the rest is not charged to the warehouse";
		}
	}
	co_return taken;
}

} // namespace gme
