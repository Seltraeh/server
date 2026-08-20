#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/archive/UnitArchiver.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/utils/Random.hpp>

#include <algorithm>
#include <optional>
#include <string>

// UnitSelectorGachaTicket (GroupId k57TdKDj, AES 1IJ8SaNk) — "use a Unit
// Selector Gacha Ticket".  The player taps a selector banner (e.g. "Brave Burst
// Heroes Selector Ticket"), picks one unit from that selector's pool, and the
// server grants it and consumes a matching V2 ticket.
//
// Request (bfdata/createbody/UnitSelectorGachaTicketRequest.txt):
//   "CGHaOZda":[{ "7Ffmi96v":gacha_id, "XIvaD6Jp":selector_id,
//                 "pn16CNah":picked_unit_id, "H6k1LIxC":?, "C5QbG2DM":? }]
//   (H6k1LIxC / C5QbG2DM are UNVERIFIED — likely count / cost; ignored here.)
//
// Selector pools come from F_UNIT_SELECTOR_GACHA_TICKET_MST
// (ServerCache::unitSelectorGacha, wrapper JukkSeNA — ported via port_mst.py).
//
// RESPONSE FORMAT IS A WORKING HYPOTHESIS.  A selector grants a unit exactly
// like a summon, so we mirror GachaActionResp (unit_info qC2tJs4E + dictionary
// GV81ctzR + ope_user_unit Km35HAXv + team_info).  There is no captured
// UnitSelectorGachaTicket response and no readParam audit of its response class
// yet, so confirm the shape against a live capture / an IDA audit
// (tools/ida/readparam_audit.py) before relying on it.

namespace
{

// Local shim for the sole-offline-user lookup.  The shared gme::getSoleUserId
// bridge was retired upstream (decompfrontier/server PR #28 review [14]); this
// debug-only selector is a local-only handler, so it keeps its own copy rather
// than reintroducing the bridge into the shared header.
drogon::Task<std::string> selectorSoleUserId(db::Database database)
{
	const auto rows = co_await database->execSqlCoro("SELECT id FROM user_info LIMIT 1;");
	co_return rows.size() ? rows[0]["id"].as<std::string>() : std::string{};
}

// Weighted summon-animation effect for a rarity (mirrors Gacha.cpp's local
// helper — kept local here to avoid coupling the two handlers).
std::optional<uint32_t> selectorGachaEffect(uint32_t rarity)
{
	const auto& mst = theServer()->cache().initializeResp().gacha_effects;
	uint32_t totalWeight = 0;
	std::vector<const GachaEffectMst*> effects;
	for (auto it = mst.begin(); it != mst.end() && it->rarity <= rarity; ++it)
	{
		if (it->rarity == rarity && it->weight > 0)
		{
			effects.push_back(&*it);
			totalWeight += it->weight;
		}
	}
	if (effects.empty())
	{
		return std::nullopt;
	}
	auto roll = RandomUInt(1, totalWeight);
	for (const auto& effect : effects)
	{
		if (roll <= effect->weight)
		{
			return effect->id;
		}
		roll -= effect->weight;
	}
	return std::nullopt;
}

struct SelectorReqItem
{
	std::string gacha_id;     // 7Ffmi96v
	std::string selector_id;  // XIvaD6Jp
	std::string unit_id;      // pn16CNah
};

struct SelectorReq
{
	std::vector<SelectorReqItem> entries;  // CGHaOZda
};

} // namespace

template <> struct glz::meta<SelectorReqItem>
{
	using T = SelectorReqItem;
	static constexpr auto value = glz::object(
		"7Ffmi96v", &T::gacha_id,
		"XIvaD6Jp", &T::selector_id,
		"pn16CNah", &T::unit_id);
};

template <> struct glz::meta<SelectorReq>
{
	using T = SelectorReq;
	static constexpr auto value = glz::object("CGHaOZda", &T::entries);
};

HANDLEF(UnitSelectorGachaTicket)
{
	(void)session;

	SelectorReq req{};
	// Lenient read — the request also carries the login/MST envelope (§4.4).
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
	}
	if (req.entries.empty())
	{
		co_return HandleResult::error("UnitSelectorGachaTicket: empty request");
	}

	const auto& item = req.entries.front();
	uint32_t selectorId = 0, gachaId = 0, pickedUnit = 0;
	try
	{
		selectorId = static_cast<uint32_t>(std::stoul(item.selector_id));
		gachaId    = static_cast<uint32_t>(std::stoul(item.gacha_id));
		pickedUnit = static_cast<uint32_t>(std::stoul(item.unit_id));
	}
	catch (const std::exception&)
	{
		co_return HandleResult::error("UnitSelectorGachaTicket: non-numeric ids");
	}

	// Resolve the selector and validate the picked unit is in its pool.
	const auto& selectors = theServer()->cache().unitSelectorGacha();
	const auto sel = std::find_if(selectors.begin(), selectors.end(),
		[selectorId](const UnitSelectorGachaMst& s)
		{ return static_cast<uint32_t>(s.selector_id) == selectorId; });
	if (sel == selectors.end())
	{
		co_return HandleResult::error("UnitSelectorGachaTicket: unknown selector",
			std::to_string(selectorId));
	}
	if (std::find(sel->unit_pool.begin(), sel->unit_pool.end(),
			static_cast<int32_t>(pickedUnit)) == sel->unit_pool.end())
	{
		co_return HandleResult::error("UnitSelectorGachaTicket: picked unit not in pool",
			std::to_string(pickedUnit));
	}

	// Resolve the sole offline user (transitional — the request's login_info
	// envelope is ignored by the lenient read above).  Uses the file-local
	// shim since gme::getSoleUserId was retired upstream (this handler is
	// local-only debug tooling).
	gme::UserIdentity identity{};
	identity.userId = co_await selectorSoleUserId(theDb());
	if (identity.userId.empty())
	{
		co_return HandleResult::error("UnitSelectorGachaTicket: no user");
	}
	identity.gumiUserId = (co_await db::DatabaseInterface::read(
		theDb(), "gumi_live_users", { db::Data("id") })).front<std::string>("id");

	GachaActionResp resp{};
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// Consume one V2 ticket.  NOTE this spends whichever ticket the user
			// happens to hold — it does NOT yet join the ticket type to this
			// selector's door (target_gacha == selector.gacha_id), so a player
			// holding an unrelated ticket can pay with it.  Best-effort besides:
			// if the user holds no ticket at all we log and still grant, so
			// beta testing on the tutorial account isn't blocked by seeding.
			const auto ticketRows = co_await transaction->execSqlCoro(
				"UPDATE user_summon_tickets_v2 SET count = count - 1"
				" WHERE user_id = $1 AND count > 0 AND ticket_id IN ("
				"   SELECT ticket_id FROM user_summon_tickets_v2"
				"   WHERE user_id = $1 AND count > 0 LIMIT 1"
				" ) RETURNING ticket_id;",
				identity.userId);
			if (ticketRows.empty())
			{
				LOG_WARN << "UnitSelectorGachaTicket: user " << identity.userId
					<< " has no V2 ticket for gacha " << gachaId
					<< " — granting anyway (beta).";
			}

			// Grant the picked unit (a selector pick, like a summon).
			auto unit = gme::fromArchivedUnit(pickedUnit, UnitArchiver::getRandomType());
			if (!unit)
			{
				throw std::runtime_error(
					"Unable to create selector unit from archive: " + std::to_string(pickedUnit));
			}

			auto added = std::move(
				(co_await gme::addUserUnit(transaction, identity, *unit)).nonEmpty());

			resp.unit_dictionary.push_back(std::move(
				(co_await db::PacketInterfaceFor<UserUnitDictionary>::read(
					transaction,
					"user_unit_dictionary",
					{
						db::Lookup("user_id", identity.userId),
						db::Lookup("unit_id", pickedUnit),
					})).nonEmpty().front()));

			const auto unitRecord = UnitArchiver::instance().lookup(pickedUnit);
			const auto effect = unitRecord ? selectorGachaEffect(unitRecord->rarity) : std::nullopt;
			resp.ope_user_unit.push_back({
				.user_unit_id = added.user_unit_id,
				.gacha_effect_id = effect.value_or(0),
			});
			resp.unit_info.push_back(std::move(added));

			resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	std::string buffer;
	if (const auto& ec2 = glz::write_json(resp, buffer); ec2)
	{
		co_return HandleResult::error("Serialization error", glz::format_error(ec2, buffer));
	}

	LOG_INFO << "UnitSelectorGachaTicket: granted unit " << pickedUnit
		<< " from selector " << selectorId << " (" << sel->name << ")";
	co_return HandleResult::success(buffer);
}
