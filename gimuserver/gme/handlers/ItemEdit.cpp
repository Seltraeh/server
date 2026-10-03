#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/BattleItems.hpp>
#include <gimuserver/gme/common/Common.hpp>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// ItemEdit (ruoB7bD8) — the battle-item loadout the player sets on the item
// screen and the quest-prep screen, and the one the client re-sends after
// GameUtils::eqpItemFull tops every slot up before a quest.
//
// THE LOADOUT IS NOT A VIEW OF THE WAREHOUSE — IT IS A SEPARATE STORE.
// The client moves counts between the two itself (see
// gme/common/BattleItems.hpp) and only reports the resulting bars, so the
// server has to derive the warehouse movement from the difference: whatever
// the bars gained came out of storage, whatever they lost went back.  This
// handler used to replace the bars and leave user_items alone, so every
// equipped item was owned twice and came back doubled at the next login.
//
// All or nothing: the new bars are validated against ItemMst and the stored
// stock BEFORE anything is written, and every write shares one transaction.
// A request the server cannot honour (an unknown or uncarryable item, a slot
// over ItemMst::getMaxEquipNum, a repeated slot, or more than the warehouse
// holds) changes nothing.  It is still acknowledged with {}: every handler
// error closes the client's session, and the next login's UserInfo resends
// the server's true bars and warehouse.
//
// The response stays empty, matching the legacy ItemEditRequestHandler; the
// client already shows the bars it built.
//
// GroupId = "ruoB7bD8", AES key = "DHEfRexCu0q5TAQm".
namespace
{

struct Bars
{
	std::vector<gme::LoadoutSlot> equip;   // 71U5wzhI: main run 0..n, second run at +100
	std::vector<gme::LoadoutSlot> bonus;   // nAligJSQ: boost items (Experience Token, ...)
};

/// Validates one bar; nullopt (with the reason logged) if any slot is unusable.
std::optional<std::vector<gme::LoadoutSlot>> validateBar(
	const std::vector<gme::LoadoutSlot>& slots, const char* bar)
{
	const auto& itemMst = theServer()->cache().itemMst();
	std::set<int32_t> orders;
	std::vector<gme::LoadoutSlot> out;
	for (const auto& slot : slots)
	{
		if (slot.itemId == 0)
			continue;                           // an empty slot the client still lists
		if (!orders.insert(slot.dispOrder).second)
		{
			LOG_WARN << "ItemEdit: " << bar << " repeats slot " << slot.dispOrder;
			return std::nullopt;
		}
		const auto mst = std::find_if(itemMst.begin(), itemMst.end(),
			[&slot](const auto& row) { return row.id == slot.itemId; });
		if (mst == itemMst.end())
		{
			LOG_WARN << "ItemEdit: " << bar << " names unknown item " << slot.itemId;
			return std::nullopt;
		}
		// ItemMst::getMaxEquipNum (t1i2vIbT) is the per-slot cap eqpItemFull
		// fills to; 0 marks an item that cannot be carried at all (materials).
		if (mst->max_equipped <= 0 || slot.count < 0 || slot.count > mst->max_equipped)
		{
			LOG_WARN << "ItemEdit: " << bar << " slot " << slot.dispOrder << " holds "
				<< slot.count << " of item " << slot.itemId << " (cap " << mst->max_equipped << ")";
			return std::nullopt;
		}
		out.push_back(slot);
	}
	return out;
}

void addTotals(std::map<int32_t, int64_t>& totals, const std::vector<gme::LoadoutSlot>& slots)
{
	for (const auto& slot : slots)
		totals[slot.itemId] += slot.count;
}

} // namespace

HANDLEF(ItemEdit)
{
	(void)session;
	LOG_INFO << "ItemEdit: " << json;

	ItemEditReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
		{
			// Parse failure means we cannot tell an empty loadout from an
			// unreadable one; clearing on a bad read would wipe a good loadout.
			LOG_WARN << "ItemEdit: parse error, loadout not persisted: "
			         << glz::format_error(ec, json);
			co_return HandleResult::success("{}");
		}
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	Bars requested;
	for (const auto& e : req.equip_items)
		requested.equip.push_back({ static_cast<int32_t>(e.disp_order),
			static_cast<int32_t>(e.item_id), static_cast<int32_t>(e.item_num) });
	for (const auto& e : req.bonus_items)
		requested.bonus.push_back({ e.disp_order, e.item_id, e.item_num });

	const auto equip = validateBar(requested.equip, "item bar");
	const auto bonus = validateBar(requested.bonus, "bonus bar");
	if (!equip || !bonus)
	{
		LOG_WARN << "ItemEdit: loadout for " << identity.userId << " refused, nothing changed";
		co_return HandleResult::success("{}");
	}

	auto transaction = co_await theDb()->newTransactionCoro();
	try
	{
		// What the bars hold now, per item, across both bars.
		std::map<int32_t, int64_t> before;
		for (const auto* table : { "user_equip_items", "user_equip_bonus_items" })
		{
			const auto rows = co_await transaction->execSqlCoro(
				std::string("SELECT item_id, item_num FROM ") + table + " WHERE user_id = $1;",
				identity.userId);
			for (const auto& row : rows)
				before[row["item_id"].as<int32_t>()] += row["item_num"].as<int64_t>();
		}

		std::map<int32_t, int64_t> after;
		addTotals(after, *equip);
		addTotals(after, *bonus);

		// A bar that grows takes from storage; refuse the whole edit if
		// storage cannot cover every increase.
		std::map<int32_t, int64_t> delta;
		for (const auto& [itemId, count] : after)
			delta[itemId] += count;
		for (const auto& [itemId, count] : before)
			delta[itemId] -= count;

		for (const auto& [itemId, change] : delta)
		{
			if (change <= 0)
				continue;
			const auto stock = co_await transaction->execSqlCoro(
				"SELECT item_num FROM user_items WHERE user_id = $1 AND item_id = $2;",
				identity.userId, itemId);
			const auto held = stock.empty() ? int64_t{ 0 } : stock[0]["item_num"].as<int64_t>();
			if (held < change)
			{
				LOG_WARN << "ItemEdit: " << identity.userId << " asked to carry " << change
					<< " more of item " << itemId << " but storage holds " << held
					<< "; loadout refused, nothing changed";
				transaction->rollback();
				co_return HandleResult::success("{}");
			}
		}

		for (const auto& [itemId, change] : delta)
		{
			if (change > 0)
			{
				// Keep the row at zero: the client still references its
				// instance id (see the warehouse rule in Common.hpp).
				co_await transaction->execSqlCoro(
					"UPDATE user_items SET item_num = item_num - $1"
					" WHERE user_id = $2 AND item_id = $3;",
					change, identity.userId, itemId);
			}
			else if (change < 0)
			{
				co_await gme::addUserItem(transaction, identity,
					static_cast<uint32_t>(itemId), static_cast<uint32_t>(-change));
			}
		}

		// Full replace of both bars: the client always sends the complete
		// loadout, and an empty group is how it says "every slot cleared".
		co_await transaction->execSqlCoro(
			"DELETE FROM user_equip_items WHERE user_id = $1;", identity.userId);
		co_await transaction->execSqlCoro(
			"DELETE FROM user_equip_bonus_items WHERE user_id = $1;", identity.userId);
		for (const auto& slot : *equip)
		{
			co_await transaction->execSqlCoro(
				"INSERT INTO user_equip_items (user_id, disp_order, item_id, item_num)"
				" VALUES ($1, $2, $3, $4);",
				identity.userId, slot.dispOrder, slot.itemId, slot.count);
		}
		for (const auto& slot : *bonus)
		{
			co_await transaction->execSqlCoro(
				"INSERT INTO user_equip_bonus_items (user_id, disp_order, item_id, item_num)"
				" VALUES ($1, $2, $3, $4);",
				identity.userId, slot.dispOrder, slot.itemId, slot.count);
		}

		LOG_INFO << "ItemEdit: stored " << equip->size() << " item slot(s) and "
		         << bonus->size() << " bonus slot(s) for " << identity.userId;
	}
	catch (...)
	{
		transaction->rollback();
		throw;
	}

	co_return HandleResult::success("{}");
}
