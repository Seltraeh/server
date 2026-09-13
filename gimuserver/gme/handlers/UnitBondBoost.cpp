#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/Dbb.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

// UnitBondBoost (tr5rOwro / fus9A2ut) — raise a Dual Brave Burst bond one rank.
//
// It is named Unit*, not Dbb*, which is why the first pass over this subsystem
// concluded there was no boost request and left bonds pinned at rank 1.  There
// is one, and the boost button sits on the bond screen — so it became reachable
// the moment DbbMst first went out, and an unregistered GroupId closes the
// session.
//
// ⚠ THE PRICE COMES FROM THE RECIPE, NOT FROM THE REQUEST.  DbbBondRecipeMst is
// keyed by (element pair, target rank): the two bonded units give the pair, the
// stored bond gives the current rank, and the row that joins them says what it
// costs in zel, karma, units, items and element shards.  The material list the
// client sends only chooses WHICH of the player's interchangeable objects to
// spend — it is checked against the recipe and can never make one cheaper.

namespace
{

/*! One line of a recipe: "<type>:<id>:<count>" read across three colon lists. */
struct Material
{
	std::string type;
	std::string id;
	int32_t count = 0;
};

/*!
* The recipe's bill, with its three parallel colon lists zipped back up.
*
* They arrive already split: the KDL types them `[str]::sep(colon)`, which the
* generator turns into pkg::string_list (a deque).  Zipping is all that is
* left, and the shortest list bounds it -- a row whose three lists disagree is
* data drift, not something to guess at.
*/
std::vector<Material> billOf(const ::DbbBondRecipeMst& recipe)
{
	std::vector<Material> bill;
	const auto lines = std::min({ recipe.material_type.size(),
		recipe.material_id.size(), recipe.material_count.size() });
	for (size_t i = 0; i < lines; ++i)
	{
		if (recipe.material_type[i].empty())
			continue;
		Material material{};
		material.type = recipe.material_type[i];
		material.id = std::to_string(recipe.material_id[i]);
		material.count = static_cast<int32_t>(recipe.material_count[i]);
		bill.push_back(std::move(material));
	}
	return bill;
}

} // namespace

HANDLEF(UnitBondBoost)
{
	(void)session;

	UnitBondBoostReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "UnitBondBoost: parse error: " << glz::format_error(ec, json);
	}
	if (req.target.empty())
		co_return HandleResult::error("Invalid boost request", "no pairing named");

	const auto dbbId = req.target.front().dbb_id;
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	UnitBondBoostResp resp{};
	resp.signal_key.key = "5EdKHavF";
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// The bond, and the two units that make it — their elements are
			// what pick the recipe.
			const auto held = co_await transaction->execSqlCoro(
				"SELECT d.user_unit_id, d.bonded_user_unit_id, d.bond_level,"
				"       a.unit_id AS species_a, b.unit_id AS species_b,"
				"       a.element AS element_a, b.element AS element_b"
				" FROM user_unit_dbb d"
				" JOIN user_units a ON a.user_unit_id = d.user_unit_id"
				" JOIN user_units b ON b.user_unit_id = d.bonded_user_unit_id"
				" WHERE d.user_id = $1 AND d.dbb_id = $2;",
				identity.userId, dbbId);
			if (held.empty())
			{
				transaction->rollback();
				co_return HandleResult::error("Invalid boost request", "that pairing is not bonded");
			}

			const auto level = held[0]["bond_level"].as<int32_t>();
			if (level >= gme::kDbbMaxBondLevel)
			{
				transaction->rollback();
				co_return HandleResult::error("Invalid boost request", "that bond is already at its maximum");
			}
			const auto want = level + 1;

			const auto pair = gme::dbbElementPair(
				gme::dbbElementOf(held[0]["species_a"].as<std::string>(),
					held[0]["element_a"].as<std::string>()),
				gme::dbbElementOf(held[0]["species_b"].as<std::string>(),
					held[0]["element_b"].as<std::string>()));
			if (pair == 0)
			{
				transaction->rollback();
				LOG_WARN << "UnitBondBoost: DBB " << dbbId << " joins elements this server cannot name";
				co_return HandleResult::error("Invalid boost request", "unknown element pair");
			}

			const auto& recipes = theServer()->cache().dbbBondRecipeMst();
			const auto recipe = std::find_if(recipes.begin(), recipes.end(),
				[pair, want](const ::DbbBondRecipeMst& r)
				{ return r.element_pair == pair && r.lv == want; });
			if (recipe == recipes.end())
			{
				transaction->rollback();
				LOG_WARN << "UnitBondBoost: no recipe for element pair " << pair << " rank " << want;
				co_return HandleResult::error("Invalid boost request", "no recipe for that rank");
			}

			// What the player offered, split the way the request splits it:
			// kinds 1 and 2 name user_unit_ids, 3 and 4 name item_ids.
			std::vector<int32_t> offeredUnits;
			std::map<std::string, int32_t> offeredItems;
			if (req.materials)
			{
				for (const auto& material : *req.materials)
				{
					const auto count = std::max(material.count, 1);
					if (material.kind == 1 || material.kind == 2)
					{
						try { offeredUnits.push_back(std::stoi(material.target_id)); }
						catch (const std::exception&) {}
					}
					else
					{
						offeredItems[material.target_id] += count;
					}
				}
			}

			// The bill.  Units are consumed by SPECIES, so the offered rows are
			// resolved first and then matched against what the recipe asks for
			// — a unit that is bonded, favourited or not this player's cannot
			// pay for anything.
			std::map<std::string, std::vector<int32_t>> unitsBySpecies;
			if (!offeredUnits.empty())
			{
				std::string list;
				for (const auto id : offeredUnits)
					list += (list.empty() ? "" : ",") + std::to_string(id);
				for (const auto& row : co_await transaction->execSqlCoro(
					"SELECT user_unit_id, unit_id FROM user_units"
					" WHERE user_id = $1 AND favorite_flg = 0 AND user_unit_id IN (" + list + ")"
					" AND user_unit_id NOT IN (SELECT user_unit_id FROM user_unit_dbb WHERE user_id = $1)"
					" AND user_unit_id NOT IN (SELECT bonded_user_unit_id FROM user_unit_dbb WHERE user_id = $1);",
					identity.userId))
				{
					unitsBySpecies[row["unit_id"].as<std::string>()].push_back(
						row["user_unit_id"].as<int32_t>());
				}
			}

			std::vector<int32_t> spendUnits;
			for (const auto& material : billOf(*recipe))
			{
				if (material.type == "UNIT")
				{
					auto& pool = unitsBySpecies[material.id];
					if (static_cast<int32_t>(pool.size()) < material.count)
					{
						transaction->rollback();
						LOG_WARN << "UnitBondBoost: " << identity.userId << " offered "
							<< pool.size() << " of unit " << material.id << ", recipe wants "
							<< material.count;
						co_return HandleResult::error("Invalid boost request", "not enough materials");
					}
					spendUnits.insert(spendUnits.end(), pool.begin(), pool.begin() + material.count);
					pool.erase(pool.begin(), pool.begin() + material.count);
				}
				else
				{
					// ITEM and ELE_SHARD are both item_mst ids; the shards have
					// their own request kind purely so the picker can group
					// them, which is a client concern.
					const auto spent = co_await transaction->execSqlCoro(
						"UPDATE user_items SET item_num = item_num - $1"
						" WHERE user_id = $2 AND item_id = $3 AND item_num >= $1"
						" RETURNING item_num;",
						material.count, identity.userId, material.id);
					if (spent.empty())
					{
						transaction->rollback();
						LOG_WARN << "UnitBondBoost: " << identity.userId << " lacks "
							<< material.count << " of item " << material.id;
						co_return HandleResult::error("Invalid boost request", "not enough materials");
					}
					if (offeredItems.find(material.id) == offeredItems.end())
					{
						LOG_INFO << "UnitBondBoost: the client did not list item " << material.id
							<< ", but the recipe requires it; charged from the recipe";
					}
				}
			}

			// Zel and karma, guarded in the same statement that spends them.
			const auto paid = co_await transaction->execSqlCoro(
				"UPDATE user_info SET zel = zel - $1, karma = karma - $2"
				" WHERE id = $3 AND zel >= $1 AND karma >= $2 RETURNING zel;",
				recipe->zel, recipe->karma, identity.userId);
			if (paid.empty())
			{
				transaction->rollback();
				LOG_WARN << "UnitBondBoost: " << identity.userId << " cannot afford rank " << want
					<< " (" << recipe->zel << " zel, " << recipe->karma << " karma)";
				co_return HandleResult::error("Invalid boost request", "not enough zel or karma");
			}

			if (!spendUnits.empty())
			{
				std::string list;
				for (const auto id : spendUnits)
					list += (list.empty() ? "" : ",") + std::to_string(id);
				// Spheres first: deleting the row without returning them would
				// destroy owned items, same as UnitSell and UnitMix.
				co_await gme::returnEquippedSpheres(transaction, identity, list);
				co_await transaction->execSqlCoro(
					"DELETE FROM user_units WHERE user_id = $1 AND user_unit_id IN (" + list + ");",
					identity.userId);
			}

			co_await transaction->execSqlCoro(
				"UPDATE user_unit_dbb SET bond_level = $1 WHERE user_id = $2 AND dbb_id = $3;",
				want, identity.userId, dbbId);

			// Everything the payment emptied.  The roster is a full REPLACE, so
			// it is what removes the eaten units from the client's list.
			co_await gme::fillDbb(transaction, identity, resp);
			resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
			resp.unit_info = std::move((co_await db::PacketInterfaceFor<UserUnitInfo>::read(
				transaction,
				"user_units",
				{ db::Lookup("user_id", identity.userId) })).data);
			auto snapshot = co_await gme::loadWarehouseSnapshot(transaction, identity);
			resp.warehouse_info = std::move(snapshot.warehouse);
			resp.item_dictionary_info = std::move(snapshot.dictionary);

			LOG_INFO << "UnitBondBoost: " << identity.userId << " raised DBB " << dbbId
				<< " to rank " << want << " for " << recipe->zel << " zel, " << recipe->karma
				<< " karma and " << spendUnits.size() << " unit(s)";
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
