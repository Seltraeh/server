#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/DailyTask.hpp>
#include <gimuserver/gme/common/Town.hpp>

#include <algorithm>
#include <charconv>
#include <limits>
#include <optional>
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

int32_t toInt(const std::string& in)
{
    int32_t value = 0;
    const auto [end, error] = std::from_chars(in.data(), in.data() + in.size(), value);
    return error == std::errc{} && end == in.data() + in.size() && value > 0 ? value : 0;
}

/// Reject a malformed batch as a whole; never silently reinterpret it as one craft.
std::optional<std::vector<MixOrder>> parseOrders(const std::string& csv)
{
    if (csv.empty() || csv.size() > 65536) return std::nullopt;
    std::vector<MixOrder> orders;
    int64_t total = 0;
    for (const auto& pair : splitOn(csv, ','))
    {
        const auto parts = splitOn(pair, ':');
        if (parts.size() != 2) return std::nullopt;
        const MixOrder order{toInt(parts[0]), toInt(parts[1])};
        total += order.count;
        if (order.recipeId == 0 || order.count == 0 || total > std::numeric_limits<int32_t>::max())
            return std::nullopt;
        orders.push_back(order);
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
        co_return HandleResult::error("Invalid synthesis request");
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const auto orders = parseOrders(req.mix.recipe_csv);
    if (!orders)
    {
        co_return HandleResult::error("Invalid synthesis recipe/count list");
    }

    // Reserve the connection before reading stock. All effects and counters
    // commit together; callers cannot both spend the same pre-transaction stock.
    auto transaction = co_await theDb()->newTransactionCoro();
    try
    {

        // Only recipes the player's facility levels have actually unlocked can be
        // crafted.  The client can only offer what we sent it in PermitRecipe, so a
        // recipe outside that set is a desync or a tampered body, not a normal path.
        std::set<int32_t> permitted;
        {
            std::set<int32_t> cleared;
            const auto missions = co_await gme::getClearedMissions(transaction, identity);
            for (const auto& done : missions)
            {
                cleared.insert(done.mission_id);
            }

            for (const auto& entry :
                co_await gme::Town::permittedRecipes(transaction, identity, cleared))
            {
                permitted.insert(entry.recipe_id);
            }
        }

        // Current stock of everything any ordered recipe might consume, read once.
        std::map<int32_t, int64_t> held;
        {
            const auto rows = co_await transaction->execSqlCoro(
                "SELECT item_id, item_num FROM user_items WHERE user_id = $1;",
                identity.userId);
            for (const auto& row : rows)
            {
                held[row["item_id"].as<int32_t>()] = row["item_num"].as<int64_t>();
            }
        }

        int64_t karmaHeld = 0;
        {
            const auto rows = co_await transaction->execSqlCoro(
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

        for (const auto& order : *orders)
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
            std::map<int32_t, int64_t> cost;
            for (const auto& pair : splitOn(recipe->materials, ','))
            {
                if (pair.empty())
                {
                    continue;
                }
                const auto parts = splitOn(pair, ':');
                if (parts.size() != 2) throw std::runtime_error("Malformed synthesis material");
                const auto itemId = toInt(parts[0]);
                const auto qty = static_cast<int64_t>(toInt(parts[1]));
                if (itemId > 0 && qty > 0)
                {
                    cost[itemId] += qty;
                }
                else throw std::runtime_error("Invalid synthesis material");
            }

            // Preserve affordable-prefix behavior without a request-count-sized loop.
            if (recipe->karma < 0 || recipe->item_count <= 0)
                throw std::runtime_error("Invalid synthesis recipe cost or output");
            int64_t count = order.count;
            if (recipe->karma > 0) count = std::min(count, (karmaHeld - karmaSpent) / recipe->karma);
            for (const auto& [itemId, qty] : cost)
                count = std::min(count, (held[itemId] - consumed[itemId]) / qty);
            if (count <= 0) continue;
            const int64_t output = count * recipe->item_count;
            if (held[recipe->item_id] + produced[recipe->item_id] + output > std::numeric_limits<int32_t>::max())
                throw std::runtime_error("Synthesis output stack overflow");
            for (const auto& [itemId, qty] : cost) consumed[itemId] += qty * count;
            karmaSpent += static_cast<int64_t>(recipe->karma) * count;
            produced[recipe->item_id] += output;
            crafted[order.recipeId] += static_cast<int32_t>(count);

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

            co_await transaction->execSqlCoro(
                "UPDATE user_items SET item_num = MAX(0, item_num - (CASE item_id" + cases
                + " ELSE 0 END)) WHERE user_id='" + identity.userId
                + "' AND item_id IN (" + ids + ");");

            // A spent-out stack is removed rather than left at zero, so it stops
            // occupying a warehouse slot the client counts against the cap.
            co_await transaction->execSqlCoro(
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
            co_await transaction->execSqlCoro(sql);
        }

        if (karmaSpent > 0)
        {
            co_await transaction->execSqlCoro(
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
            co_await transaction->execSqlCoro(sql);
        }

        for (const auto& [recipeId, count] : crafted)
        {
            LOG_INFO << "ItemMix: crafted recipe " << recipeId << " x" << count;
        }

        // DAILY TASK `CM` ("Craft N Items/Spheres").  Counted by ITEMS MADE, not by
        // recipes used, because the task text counts items -- a continueMix run
        // that applies one recipe five times has crafted five.  The client reads
        // this code in StepScene but never reports it, so the tally is ours.
        {
            int32_t made = 0;
            for (const auto& [recipeId, count] : crafted)
            {
                (void)recipeId;
                made += count;
            }
            co_await gme::advanceDailyTask(transaction, identity, "CM", made);
        }

        // Records counters: 100250 syntheses performed, 100260 materials consumed,
        // 100270 spheres created.  A "sphere" is an ItemMst row with a non-zero
        // sphere_type (its sphere category), so the Sphere Synthesis screen and
        // the Item Synthesis screen both feed
        // 100250/100260 and only the former also feeds 100270.
        {
            int64_t crafts = 0, materials = 0, spheres = 0;
            for (const auto& [recipeId, count] : crafted)
            {
                (void)recipeId;
                crafts += count;
            }
            for (const auto& [itemId, qty] : consumed)
            {
                (void)itemId;
                materials += qty;
            }
            const auto& itemMst = theServer()->cache().itemMst();
            for (const auto& [itemId, qty] : produced)
            {
                for (const auto& m : itemMst)
                {
                    if (m.id == static_cast<int32_t>(itemId))
                    {
                        if (m.sphere_type != 0)
                            spheres += qty;
                        break;
                    }
                }
            }
            co_await gme::bumpArchiveCounters(transaction, identity, {
                { "item_mix_cnt",      crafts    },
                { "item_mix_elem_cnt", materials },
                { "sphere_mix_cnt",    spheres   },
            }, true);
        }

        co_return HandleResult::success("{}");
    }
    catch (...)
    {
        transaction->rollback();
        throw;
    }
}
