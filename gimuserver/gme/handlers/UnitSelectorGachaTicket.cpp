#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/archive/UnitArchiver.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/SelectorRotation.hpp>
#include <gimuserver/utils/Random.hpp>

#include <algorithm>
#include <optional>
#include <string>

// UnitSelectorGachaTicket (GroupId k57TdKDj, AES 1IJ8SaNk) — "use a Unit
// Selector Gacha Ticket".  The player taps a selector banner (e.g. "Brave Burst
// Heroes Selector Ticket"), picks one unit from that selector's pool, and the
// server spends one of that selector's tickets and grants the unit.
//
// Request (bfdata/createbody/UnitSelectorGachaTicketRequest.txt):
//   "CGHaOZda":[{ "7Ffmi96v":gacha_id, "XIvaD6Jp":ticket_id,
//                 "pn16CNah":picked_unit_id, "H6k1LIxC":"1", "C5QbG2DM":type }]
//   Decoded 2026-09-11 from UnitSelectorGachaTicketRequest::createBody
//   @0x1CCB8B8: H6k1LIxC is the constant "1" (one ticket per pick), and
//   C5QbG2DM is UnitSelectorGachaUnitObjManager::getSelectedUnitType (+0x88) —
//   the TYPE the player picked on the confirm screen.
//   UnitSelectorGachaUnitConfirmScene::touchEnded @0x1D95860 stores the chosen
//   button's index + 1, and initialize labels button i with
//   UnitTypeMstList::getObject(i + 1) @0x1D943CC, so it is the UnitTypeMst id
//   (1-6, Lord..Rex) the granted unit must carry.
//
// The selector ticket INVENTORY is not the V2 ticket list: no V2 ticket
// targets a selector gacha.  Selector tickets live in user_selector_tickets,
// keyed by the selector MST's ticket id (XIvaD6Jp — UnitSelectorGachaMst
// setTicketId), arrive as type-8005 presents (PresentReceipt), and reach the
// client as UnitSelectorGachaUserInfo (response key CGHaOZda, the same hash as
// this request's group).  That list is a full replace and nothing on the
// client decrements it, so this reply sends the whole inventory after the
// spend — the spent ticket included, at 0 if it was the last one.
//
// One ticket buys one pick: the request's H6k1LIxC is the constant "1", and
// the MST's required-tickets field (JRqU2bS6) is absent from the data, which
// the client reads as 1.  A pick with no ticket left is refused rather than
// granted free.
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
	std::string ticket_id;    // XIvaD6Jp — the selector MST's ticket id
	std::string unit_id;      // pn16CNah
	std::string unit_type;    // C5QbG2DM — the UnitTypeMst id the player picked
};

struct SelectorReq
{
	LoginInfoReq login_info;               // IKqx1Cn9
	std::vector<SelectorReqItem> entries;  // CGHaOZda
};

} // namespace

template <> struct glz::meta<SelectorReqItem>
{
	using T = SelectorReqItem;
	static constexpr auto value = glz::object(
		"7Ffmi96v", &T::gacha_id,
		"XIvaD6Jp", &T::ticket_id,
		"pn16CNah", &T::unit_id,
		"C5QbG2DM", &T::unit_type);
};

template <> struct glz::meta<SelectorReq>
{
	using T = SelectorReq;
	static constexpr auto value = glz::object(
		"IKqx1Cn9", pkg::glaze::single_array<&T::login_info>(),
		"CGHaOZda", &T::entries);
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
	uint32_t ticketId = 0, gachaId = 0, pickedUnit = 0;
	try
	{
		ticketId   = static_cast<uint32_t>(std::stoul(item.ticket_id));
		gachaId    = static_cast<uint32_t>(std::stoul(item.gacha_id));
		pickedUnit = static_cast<uint32_t>(std::stoul(item.unit_id));
	}
	catch (const std::exception&)
	{
		co_return HandleResult::error("UnitSelectorGachaTicket: non-numeric ids");
	}

	// The type the player picked (C5QbG2DM), if it names a real UnitTypeMst
	// row.  The confirm screen only lets the tap through once a type is chosen,
	// so a missing or out-of-range value is a malformed body; it falls back to a
	// random normal type rather than refusing the pick.
	uint32_t unitType = 0;
	try
	{
		unitType = static_cast<uint32_t>(std::stoul(item.unit_type));
	}
	catch (const std::exception&)
	{
	}
	if (unitType < 1 || unitType > 6)
	{
		LOG_WARN << "UnitSelectorGachaTicket: no valid chosen type ('" << item.unit_type
			<< "'); rolling a random one";
		unitType = UnitArchiver::getRandomType();
	}

	// Resolve the selector and validate the picked unit is in its pool.
	//
	// ⚠ THE ROTATION HAS TO BE APPLIED HERE TOO.  The weekly selector's pool is
	// NOT in the MST file -- the file carries a one-unit stub and
	// applyWeeklySelector rewrites it per reply, which is what the emitters in
	// Gacha.cpp and UserInfo.cpp do.  Validating against the RAW cache compared
	// the pick against that stub, so every pick but unit 10016 was refused with
	// "picked unit not in pool" no matter what the player was shown.  Take a
	// copy and run the same transform the client was sent.
	auto selectors = theServer()->cache().unitSelectorGacha();
	gme::applyWeeklySelector(selectors);
	const auto sel = std::find_if(selectors.begin(), selectors.end(),
		[ticketId](const UnitSelectorGachaMst& s)
		{ return static_cast<uint32_t>(s.selector_id) == ticketId; });
	if (sel == selectors.end())
	{
		co_return HandleResult::error("UnitSelectorGachaTicket: unknown selector ticket",
			std::to_string(ticketId));
	}
	if (std::find(sel->unit_pool.begin(), sel->unit_pool.end(),
			static_cast<int32_t>(pickedUnit)) == sel->unit_pool.end())
	{
		co_return HandleResult::error("UnitSelectorGachaTicket: picked unit not in pool",
			std::to_string(pickedUnit));
	}
	if (static_cast<uint32_t>(sel->gacha_id) != gachaId)
	{
		LOG_WARN << "UnitSelectorGachaTicket: ticket " << ticketId << " opens gate " << sel->gacha_id
			<< " but the request names gate " << gachaId << " — going by the ticket";
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	GachaActionResp resp{};
	int32_t ticketsLeft = 0;
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// Spend one of THIS selector's tickets.  Guarded on count >= 1, so a
			// pick without a ticket changes nothing and is refused.
			const auto spent = co_await transaction->execSqlCoro(
				"UPDATE user_selector_tickets SET count = count - 1"
				" WHERE user_id = $1 AND ticket_id = $2 AND count >= 1 RETURNING count;",
				identity.userId, std::to_string(ticketId));
			if (spent.empty())
			{
				transaction->rollback();
				LOG_WARN << "UnitSelectorGachaTicket: user " << identity.userId
					<< " holds no '" << sel->name << "' ticket (" << ticketId << ") — pick refused";
				co_return HandleResult::error("UnitSelectorGachaTicket: no selector ticket",
					std::to_string(ticketId));
			}
			ticketsLeft = spent[0]["count"].as<int32_t>();

			// Grant the picked unit (a selector pick, like a summon), with the
			// type the player picked for it.
			auto unit = gme::fromArchivedUnit(pickedUnit, unitType);
			if (!unit)
			{
				throw std::runtime_error(
					"Unable to create selector unit from archive: " + std::to_string(pickedUnit));
			}

			auto added = std::move(
				(co_await gme::addUserUnit(transaction, identity, *unit)).nonEmpty());

			// The COMPLETE dictionary, not just the picked species: this
			// response rebuilds the reference list the summon lineup reads
			// (UserUnitDictionary in net/user.kdl).
			resp.unit_dictionary = co_await gme::loadUnitDictionary(transaction, identity);

			const auto unitRecord = UnitArchiver::instance().lookup(pickedUnit);
			const auto effect = unitRecord ? selectorGachaEffect(unitRecord->rarity) : std::nullopt;
			resp.ope_user_unit.push_back({
				.user_unit_id = added.user_unit_id,
				.gacha_effect_id = effect.value_or(0),
			});
			resp.unit_info.push_back(std::move(added));

			resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());

			// The inventory after the spend.  The client never decrements it, so
			// without this the banner would keep offering the ticket just used.
			resp.selector_ticket_info = co_await gme::loadSelectorTickets(transaction, identity);
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

	LOG_INFO << "UnitSelectorGachaTicket: granted unit " << pickedUnit << " (type "
		<< unitType << ") for a '" << sel->name << "' ticket (" << ticketId << "); "
		<< ticketsLeft << " left";
	co_return HandleResult::success(buffer);
}
