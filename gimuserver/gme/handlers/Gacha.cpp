#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/archive/GachaArchiver.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/SelectorBanners.hpp>
#include <gimuserver/gme/common/SummonTickets.hpp>
#include <gimuserver/utils/Random.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{

// Gate 2000 — the client labels type 20000 "Tutorial Gacha" and it is the only
// row with that type across all 1305 gacha_mst rows.  Tutorial-only: shown in
// the banner rail while the tutorial runs, hidden afterwards.
constexpr int32_t kTutorialGachaId = 2000;

/*!
* Picks a summon animation effect id for a summoned unit rarity.
*
* @param rarity Unit rarity used to choose the animation effect.
* @return Gacha effect id to send, or std::nullopt if none is available.
*/
inline std::optional<uint32_t> getGachaEffect(uint32_t rarity)
{
	const auto& mst = theServer()->cache().initializeResp().gacha_effects;

	uint32_t totalWeight = 0;
	std::vector<const GachaEffectMst*> effects;
	// The MST is already sorted by rarity.
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
		LOG_ERROR << "Unable to find gacha effect for rarity " << rarity;
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

} // namespace

HANDLEF(GachaAction)
{
	GachaActionReq req{};
	const auto& ec = glz::read_json(req, json);
	if (ec)
	{
		const auto& fmte = glz::format_error(ec, json);
		LOG_DEBUG << "Gme GachaAction Error during JSON read: " << fmte;
		co_return HandleResult::error("Deserialization error", fmte);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// Summon tickets.  `324b023k` is set by SummonsDetailScene::touchBegan
	// @0x16A80BC after it has already satisfied itself that the player holds a
	// ticket for this gate — either a V2 ticket (it sums the amounts of the
	// types whose SummonTicketV2Mst row targets the gate, @0x16A7FA8) or the
	// plain counter it reads off team_info (UserTeamInfo::getSummonTicket,
	// @0x16A7F1C).  The server decides which one actually pays, prefers the
	// specific ticket over the generic one, and skips the currency cost when
	// a ticket covers the pull.
	const bool usingTicket = req.gacha_action_info.using_gacha_ticket;

	// A count of 0 means one pull, not a bad request.
	//
	// `a329kbl8` is written straight from UserState::noOfGachaRequests, and
	// that value has exactly one writer on the summon path:
	// SummonsDetailScene::touchBegan, which does setNumberOfGachaRequest(1)
	// when the player presses the single-summon button (the other three
	// callers are GachaActionScene::checkConnectResult and GachaSummaryScene,
	// which reset it to 0).  The tutorial never goes through that press —
	// tuto15.txt drives `change_gacha_action_scene` from the script — so the
	// count arrives as the 0 it was last reset to:
	//
	//     "1IR86sAv":[{"7Ffmi96v":"2000","a329kbl8":"0","324b023k":"0"}]
	//
	// The gate id is correct in that payload, so this is not a parse failure
	// and not a client bug to route around: no response field influences
	// noOfGachaRequests, it is pure client state.  Rejecting 0 makes the
	// free-summon tutorial unfinishable, so clamp it to a single pull.  The
	// currency check below still applies, so this cannot be used to summon
	// for free.
	const auto count = std::max<uint32_t>(req.gacha_action_info.request_count, 1);

	// Find the summon gate and determine the cost.
	const auto gachaRecord = GachaArchiver::instance().lookup(req.gacha_action_info.gacha_id);
	if (!gachaRecord)
	{
		co_return HandleResult::error("Invalid gacha", "Unknown summon gate");
	}

	GachaInfoMst mst{};
	if (!GachaArchiver::instance().populatePacket(*gachaRecord, mst))
	{
		co_return HandleResult::error("Invalid gacha", "Missing in MST");
	}

	// A SELECTOR gate is never paid for.  Its SUMMON button is supposed to open
	// the unit picker and come back as UnitSelectorGachaTicket, and the gate's
	// own gem cost is the shipped 999 "not for sale" marker — so if a pull ever
	// arrives here for one (a client that missed JukkSeNA, a replayed body), the
	// only safe answer is to refuse it rather than take 999 gems per pull.
	{
		const auto& selectors = theServer()->cache().unitSelectorGacha();
		const bool isSelectorGate = std::any_of(selectors.begin(), selectors.end(),
			[&](const UnitSelectorGachaMst& sel)
			{ return static_cast<uint32_t>(sel.gacha_id) == req.gacha_action_info.gacha_id; });
		if (isSelectorGate)
		{
			LOG_WARN << "GachaAction: gate " << req.gacha_action_info.gacha_id
				<< " is a selector gate and cannot be pulled for currency; refused";
			co_return HandleResult::error("Invalid gacha request",
				"This gate is redeemed with its selector ticket");
		}
	}

	const auto gemCost = mst.gems * count;
	const auto friendPointCost = mst.friend_points * count;

	const auto summonedUnits = GachaArchiver::instance().summonFrom(*gachaRecord, count);
	if (summonedUnits.size() != count)
	{
		co_return HandleResult::error("Invalid gacha", "Unable to summon from gate");
	}

	// Ensure the user has enough currency to perform the summon.
	auto currency = co_await db::DatabaseInterface::read(
		theDb(),
		"user_info",
		{
			db::Data("gems"),
			db::Data("friend_points"),
			db::Data("summon_tickets"),
			db::Lookup("gumi_user_id", identity.gumiUserId),
			db::Lookup("id", identity.userId),
		});
	const auto currentGems = currency.front<int32_t>("gems");
	const auto currentFriendPoints = currency.front<int32_t>("friend_points");
	const auto currentTickets = currency.front<int32_t>("summon_tickets");

	// Which ticket, if any, is paying.  The V2 types that name this gate go
	// first — they exist only to be spent here — and the plain counter is the
	// fallback, because it works on any ticket-enabled gate.
	const bool ticketTypesExist = !gme::summonTicketTypesForGate(req.gacha_action_info.gacha_id).empty();
	const bool plainTicketsPay = currentTickets >= static_cast<int32_t>(count);
	if (usingTicket && !ticketTypesExist && !plainTicketsPay)
	{
		co_return HandleResult::error("Invalid gacha request", "Not enough summon tickets");
	}
	if (!usingTicket && (gemCost > currentGems || friendPointCost > currentFriendPoints))
	{
		co_return HandleResult::error("Invalid gacha request", "Not enough currency");
	}

	// We need to wrap these operations in a transaction to avoid an invalid
	// intermediate state.
	GachaActionResp resp{};
	resp.signal_key = req.signal_key;
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// Deduct what is paying for the pull: a ticket, or the currency.
			// A gate can have V2 types the player happens to hold none of, so a
			// failed V2 spend falls back to the plain counter before refusing.
			bool v2Paid = false;
			if (usingTicket && ticketTypesExist)
			{
				v2Paid = co_await gme::spendSummonTicketsV2(
					transaction, identity, req.gacha_action_info.gacha_id, count);
			}
			if (v2Paid)
			{
				resp.summon_ticket_v2_user = co_await gme::loadSummonTicketsV2(transaction, identity);
			}
			else if (usingTicket && !plainTicketsPay)
			{
				transaction->rollback();
				co_return HandleResult::error(
					"Invalid gacha request", "Not enough summon tickets");
			}
			else if (usingTicket)
			{
				(co_await db::DatabaseInterface::update(
					transaction,
					"user_info",
					{
						db::Data("summon_tickets", currentTickets - static_cast<int32_t>(count)),
						db::Lookup("gumi_user_id", identity.gumiUserId),
						db::Lookup("id", identity.userId),
					})).nonEmpty();
			}
			else
			{
				(co_await db::DatabaseInterface::update(
					transaction,
					"user_info",
					{
						db::Data("gems", currentGems - gemCost),
						db::Data("friend_points", currentFriendPoints - friendPointCost),
						db::Lookup("gumi_user_id", identity.gumiUserId),
						db::Lookup("id", identity.userId),
					})).nonEmpty();
			}

			// Commit the summoned units and populate the response.
			for (const auto& unitId : summonedUnits)
			{
				// Build and inject the unit.
				const auto unitRecord = UnitArchiver::instance().lookup(unitId);
				if (!unitRecord)
				{
					throw std::runtime_error("Unable to find summoned unit archive record");
				}

				auto unit = gme::fromArchivedUnit(
					unitId,
					UnitArchiver::getRandomType());
				if (!unit)
				{
					throw std::runtime_error("Unable to create summoned unit from archive");
				}

				auto added = std::move(
					(co_await gme::addUserUnit(transaction, identity, *unit)).nonEmpty());
				const auto gachaEffect = getGachaEffect(unitRecord->rarity);
				if (!gachaEffect)
				{
					throw std::runtime_error(
						"Unable to find gacha effect for summoned unit rarity");
				}

				resp.ope_user_unit.push_back({
					.user_unit_id = added.user_unit_id,
					.gacha_effect_id = *gachaEffect,
				});
				resp.unit_info.push_back(std::move(added));
			}

			// TROPHY 100100 (Honor spent).  Counted here because this is the
			// only place Honor leaves the balance; its counterpart 100090 is
			// bumped at MissionEnd, where it is earned.  Zero for a gem or
			// ticket pull, and bumpArchiveCounters drops zero deltas.
			co_await gme::bumpArchiveCounters(transaction, identity, {
				{ "friend_p_use", usingTicket ? 0 : static_cast<int64_t>(friendPointCost) },
			});

			// The COMPLETE dictionary, not just the summoned species: this
			// response rebuilds the reference list the summon lineup reads
			// (UserUnitDictionary in net/user.kdl).
			resp.unit_dictionary = co_await gme::loadUnitDictionary(transaction, identity);

			resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	std::string buffer;
	const auto& ec2 = glz::write_json(resp, buffer);
	if (ec2)
	{
		const auto& glze = glz::format_error(ec2, buffer);
		LOG_DEBUG << "Gme GachaAction Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}

HANDLEF(GachaList)
{
	GachaListReq req = {};
	const auto& ec = glz::read_json(req, json);
	if (ec)
	{
		const auto& fmte = glz::format_error(ec, json);
		LOG_DEBUG << "Gme GachaList Error during JSON read: " << fmte;
		co_return HandleResult::error("Deserialization error", fmte);
	}

	// The Tutorial Gacha (gate 2000, type 20000 — the client's own label, see
	// GachaActionScene::initConnect) is a TUTORIAL-ONLY door.  tuto15 drives the
	// player into it by tapping a fixed screen position, so it has to be in the
	// banner rail while the tutorial runs — but leaving it there afterwards is
	// wrong twice over: it displaces the large event banner from the top of the
	// rail, and opening it outside the tutorial crashes the client.
	//
	// So it is filtered out once the player reports the tutorial finished.  The
	// rail then returns to its original order with the event tile first.
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
	bool tutorialDone = true;
	{
		const auto rows = co_await theDb()->execSqlCoro(
			"SELECT tutorial_end_flag FROM user_info WHERE id = $1;", identity.userId);
		if (!rows.empty())
			tutorialDone = rows[0]["tutorial_end_flag"].as<int32_t>() != 0;
	}

	GachaListResp resp = theServer()->cache().gachaListRsp();
	resp.gacha_info = GachaArchiver::instance().populateAllPackets();
	resp.signal_key = req.signal_key;

	// The selector catalogue, which is what tells the detail scene that a gate
	// is ticket-only — see the JukkSeNA field doc in net/handlers.kdl.
	resp.unit_selector_gacha = theServer()->cache().unitSelectorGacha();

	// Selector banners are per player: a selector gate is redeemable only with
	// its own ticket, so its rail tile is added here and only while one is held
	// (gme/common/SelectorBanners.hpp).  gacha_category_mst.json holds the
	// permanent tiles.
	{
		auto selectors = co_await gme::selectorGachaCategories(theDb(), identity);
		resp.gacha_categories.insert(resp.gacha_categories.end(),
			std::make_move_iterator(selectors.begin()),
			std::make_move_iterator(selectors.end()));
	}

	if (tutorialDone)
	{
		std::erase_if(resp.gacha_categories, [](const GachaCategory& c) {
			return c.gacha_id_list == std::to_string(kTutorialGachaId);
		});
		std::erase_if(resp.gacha_info, [](const GachaInfoMst& g) {
			return g.id == kTutorialGachaId;
		});
	}

	std::string buffer;
	const auto& ec2 = glz::write_json(resp, buffer);
	if (ec2)
	{
		const auto& glze = glz::format_error(ec2, buffer);
		LOG_DEBUG << "Gme GachaList Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}
