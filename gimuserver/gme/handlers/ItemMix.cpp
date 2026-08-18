#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/Town.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

// ItemMix (4P5GELTF) — synthesis at the town's Synthesis facility (and its
// sphere counterpart, which posts the same request).
//
// The request body is a single param, JTf2jY5o[0].4HqhTf3a, built by
// ItemMixRequest::itemCsvData (libgame.so 0x13A67AC): the client's
// ItemMixResultInfo list deduped by recipe id, counted, and joined as
// "<recipeId>:<count>,...".  The repeats come from
// MyTownItemMixScene::continueMix — the "craft again" button.
//
// 4HqhTf3a is the same key PermitRecipeResponse::readParam hands to
// setRecipeID, so these are ReceipeMst ids, not item ids.  The handler consumes
// ReceipeMst.materials and karma and credits the result.
//
// Full model: tools/TOWN_STATE_MODEL.md.
//
// GroupId = "4P5GELTF", AES key = "AFqKIJ8Z4mHPB9xg" (legacy ItemMixRequestHandler).

namespace
{

/// One recipe to craft, and how many times.
struct MixOrder
{
	int32_t recipeId = 0;
	int32_t count = 0;
};

std::vector<std::string> splitOn(const std::string& in, const char sep)
{
	std::vector<std::string> out;
	size_t start = 0;
	while (true)
	{
		const auto at = in.find(sep, start);
		if (at == std::string::npos)
		{
			out.emplace_back(in.substr(start));
			return out;
		}
		out.emplace_back(in.substr(start, at - start));
		start = at + 1;
	}
}

int32_t toInt(const std::string& in, const int32_t fallback = 0)
{
	try
	{
		return std::stoi(in);
	}
	catch (...)
	{
		return fallback;
	}
}

/// Parses itemCsvData's "<recipeId>:<count>,..." payload.
std::vector<MixOrder> parseOrders(const std::string& csv)
{
	std::vector<MixOrder> orders;
	for (const auto& pair : splitOn(csv, ','))
	{
		if (pair.empty())
		{
			continue;
		}

		const auto parts = splitOn(pair, ':');
		if (parts.empty())
		{
			continue;
		}

		MixOrder order{};
		order.recipeId = toInt(parts[0]);
		// A bare id with no ':' has never been captured, but treating it as one
		// craft is the reading that cannot silently drop a player's request.
		order.count = parts.size() > 1 ? toInt(parts[1], 1) : 1;
		if (order.recipeId > 0 && order.count > 0)
		{
			orders.push_back(order);
		}
	}

	return orders;
}

} // namespace

HANDLEF(ItemMix)
{
	(void)session;
	LOG_INFO << "ItemMix: " << json;

	ItemMixReq req{};
	if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		LOG_WARN << "ItemMix: parse error: " << glz::format_error(ec, json);
		co_return HandleResult::success("{}");
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
	const auto orders = parseOrders(req.mix.recipe_csv);
	if (orders.empty())
	{
		co_return HandleResult::success("{}");
	}

	// Only recipes the player's facility levels have actually unlocked can be
	// crafted.  The client can only offer what we sent it in PermitRecipe, so a
	// recipe outside that set is a desync or a tampered body, not a normal path.
	std::set<int32_t> permitted;
	{
		std::set<int32_t> cleared;
		const auto missions = co_await gme::getClearedMissions(theDb(), identity);
		for (const auto& done : missions)
		{
			cleared.insert(done.mission_id);
		}

		for (const auto& entry :
			co_await gme::Town::permittedRecipes(theDb(), identity, cleared))
		{
			permitted.insert(entry.recipe_id);
		}
	}

	// Current stock of everything any ordered recipe might consume, read once.
	std::map<int32_t, int64_t> held;
	{
		const auto rows = co_await theDb()->execSqlCoro(
			"SELECT item_id, item_num FROM user_items WHERE user_id = $1;",
			identity.userId);
		for (const auto& row : rows)
		{
			held[row["item_id"].as<int32_t>()] = row["item_num"].as<int64_t>();
		}
	}

	int64_t karmaHeld = 0;
	{
		const auto rows = co_await theDb()->execSqlCoro(
			"SELECT karma FROM user_info WHERE id = $1;", identity.userId);
		if (rows.size() == 0)
		{
			LOG_WARN << "ItemMix: no user_info row for " << identity.userId;
			co_return HandleResult::success("{}");
		}
		karmaHeld = rows[0]["karma"].as<int64_t>();
	}

	const auto& recipes = theServer()->cache().initializeResp().receipe;

	std::map<int32_t, int64_t> consumed;   // item_id -> total consumed
	std::map<int32_t, int64_t> produced;   // item_id -> total produced
	std::map<int32_t, int32_t> crafted;    // recipe_id -> crafts applied
	int64_t karmaSpent = 0;

	for (const auto& order : orders)
	{
		const auto recipe = std::find_if(recipes.begin(), recipes.end(),
			[&order](const auto& r) { return r.id == order.recipeId; });
		if (recipe == recipes.end())
		{
			LOG_WARN << "ItemMix: unknown recipe " << order.recipeId;
			continue;
		}

		if (!permitted.count(order.recipeId))
		{
			LOG_WARN << "ItemMix: recipe " << order.recipeId
				<< " is not unlocked for this user; skipping";
			continue;
		}

		// materials is "<itemId>:<count>,..." per craft.
		std::vector<std::pair<int32_t, int64_t>> cost;
		for (const auto& pair : splitOn(recipe->materials, ','))
		{
			if (pair.empty())
			{
				continue;
			}
			const auto parts = splitOn(pair, ':');
			if (parts.size() < 2)
			{
				continue;
			}
			const auto itemId = toInt(parts[0]);
			const auto qty = static_cast<int64_t>(toInt(parts[1]));
			if (itemId > 0 && qty > 0)
			{
				cost.emplace_back(itemId, qty);
			}
		}

		// Craft one at a time so a batch that runs out part-way still applies
		// the crafts the player could afford, matching what they were shown.
		for (int32_t i = 0; i < order.count; ++i)
		{
			const bool affordable = karmaHeld - karmaSpent >= recipe->karma
				&& std::all_of(cost.begin(), cost.end(), [&](const auto& need) {
					const auto stock = held.count(need.first) ? held.at(need.first) : 0;
					const auto used = consumed.count(need.first) ? consumed.at(need.first) : 0;
					return stock - used >= need.second;
				});

			if (!affordable)
			{
				LOG_WARN << "ItemMix: recipe " << order.recipeId << " craft "
					<< (i + 1) << "/" << order.count << " not affordable; stopping";
				break;
			}

			for (const auto& [itemId, qty] : cost)
			{
				consumed[itemId] += qty;
			}
			karmaSpent += recipe->karma;
			produced[recipe->item_id] += std::max(recipe->item_count, 1);
			++crafted[order.recipeId];
		}
	}

	if (crafted.empty())
	{
		co_return HandleResult::success("{}");
	}

	// One batched statement per effect rather than one await per craft — the
	// SQLite pool is single-connection and a continueMix run can be long
	// (handbook §6.14).  Drogon's SQLite client rejects semicolon-separated
	// statements, so the per-item decrements fold into a single CASE.  Every
	// inlined value is an int from MST data or a server-minted user id.
	if (!consumed.empty())
	{
		std::string cases;
		std::string ids;
		for (const auto& [itemId, qty] : consumed)
		{
			cases += " WHEN " + std::to_string(itemId) + " THEN " + std::to_string(qty);
			if (!ids.empty())
			{
				ids += ',';
			}
			ids += std::to_string(itemId);
		}

		co_await theDb()->execSqlCoro(
			"UPDATE user_items SET item_num = MAX(0, item_num - (CASE item_id" + cases
			+ " ELSE 0 END)) WHERE user_id='" + identity.userId
			+ "' AND item_id IN (" + ids + ");");

		// A spent-out stack is removed rather than left at zero, so it stops
		// occupying a warehouse slot the client counts against the cap.
		co_await theDb()->execSqlCoro(
			"DELETE FROM user_items WHERE user_id = $1 AND item_num <= 0;",
			identity.userId);
	}

	{
		std::string sql = "INSERT INTO user_items (user_id, item_id, item_num) VALUES ";
		bool first = true;
		for (const auto& [itemId, qty] : produced)
		{
			if (!first)
			{
				sql += ',';
			}
			first = false;
			sql += "('" + identity.userId + "'," + std::to_string(itemId) + ","
				+ std::to_string(qty) + ")";
		}
		sql += " ON CONFLICT(user_id, item_id) DO UPDATE SET"
			" item_num = item_num + excluded.item_num;";
		co_await theDb()->execSqlCoro(sql);
	}

	if (karmaSpent > 0)
	{
		co_await theDb()->execSqlCoro(
			"UPDATE user_info SET karma = MAX(0, karma - $1) WHERE id = $2;",
			karmaSpent, identity.userId);
	}

	// PermitRecipe.craft_count has to match the client's own running tally,
	// which GameUtils::updatePermitRecipe bumps locally after each craft.
	{
		std::string sql =
			"INSERT INTO user_recipe_crafts (user_id, recipe_id, craft_count) VALUES ";
		bool first = true;
		for (const auto& [recipeId, count] : crafted)
		{
			if (!first)
			{
				sql += ',';
			}
			first = false;
			sql += "('" + identity.userId + "'," + std::to_string(recipeId) + ","
				+ std::to_string(count) + ")";
		}
		sql += " ON CONFLICT(user_id, recipe_id) DO UPDATE SET"
			" craft_count = craft_count + excluded.craft_count;";
		co_await theDb()->execSqlCoro(sql);
	}

	for (const auto& [recipeId, count] : crafted)
	{
		LOG_INFO << "ItemMix: crafted recipe " << recipeId << " x" << count;
	}

	co_return HandleResult::success("{}");
}
