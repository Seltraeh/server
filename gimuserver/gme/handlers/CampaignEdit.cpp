#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Campaign.hpp>

#include <map>
#include <set>

// The Grand Quest's party, item and suspend requests.  All four were
// unregistered until 2026-09-11, so the client's "Unsupported request" ended
// the session the first time one fired — DeckEdit and ItemEdit are queued
// through ConnectRequestList by the party and item screens, Save by the
// suspend prompt, Restart by the field's restart path.  Request shapes and
// evidence are in net/handlers.kdl (CampaignDeckEditReq and friends).

namespace
{

/// Total count per item id across a loadout's lists.
std::map<uint32_t, int64_t> itemTotals(const std::vector<::CampaignItemEntry>& a,
	const std::vector<::CampaignItemEntry>& b)
{
	std::map<uint32_t, int64_t> totals;
	for (const auto* list : { &a, &b })
	{
		for (const auto& item : *list)
		{
			try { totals[static_cast<uint32_t>(std::stoul(item.item_id))] += item.item_num; }
			catch (const std::exception&) {}
		}
	}
	return totals;
}

} // namespace

// CampaignDeckEdit (D74TYRf1) — stores the Grand Quest parties.  The first
// request of a run that names the mission, so the run opens here too.
HANDLEF(CampaignDeckEdit)
{
	LOG_INFO << "CampaignDeckEdit: " << json;

	::CampaignDeckEditReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "CampaignDeckEdit: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	auto transaction = co_await theDb()->newTransactionCoro();
	try
	{
		if (!req.mission.empty())
			co_await gme::openCampaignRun(transaction, identity, req.mission.front().mission_id);

		// Only units the user owns (or an empty / guest slot, unit 0).
		std::set<std::string> owned;
		for (const auto& row : co_await transaction->execSqlCoro(
			"SELECT user_unit_id FROM user_units WHERE user_id = $1;", identity.userId))
		{
			owned.insert(row["user_unit_id"].as<std::string>());
		}

		co_await transaction->execSqlCoro(
			"DELETE FROM user_campaign_decks WHERE user_id = $1;", identity.userId);

		size_t stored = 0;
		for (const auto& member : req.decks)
		{
			const std::string unitId = member.user_unit_id.empty() ? "0" : member.user_unit_id;
			if (unitId != "0" && !owned.contains(unitId))
			{
				LOG_WARN << "CampaignDeckEdit: unit " << unitId << " is not owned — slot dropped";
				continue;
			}
			int64_t unit = 0;
			try { unit = std::stoll(unitId); }
			catch (const std::exception&) { continue; }
			co_await transaction->execSqlCoro(
				"INSERT INTO user_campaign_decks (user_id, deck_num, member_type, user_unit_id, disporder)"
				" VALUES ($1, $2, $3, $4, $5)"
				" ON CONFLICT(user_id, deck_num, disporder) DO UPDATE SET"
				" member_type = excluded.member_type, user_unit_id = excluded.user_unit_id;",
				identity.userId, member.deck_num, member.member_type, unit, member.disp_order);
			++stored;
		}

		LOG_INFO << "CampaignDeckEdit: stored " << stored << " party member(s)";
	}
	catch (...)
	{
		transaction->rollback();
		throw;
	}

	co_return HandleResult::success("{}");
}

// CampaignItemEdit (W2VU91I7) — stores the Grand Quest item loadout.  The
// client has already moved the items between its warehouse and the loadout,
// so the server mirrors the difference: whatever a list gained leaves
// user_items, whatever it lost goes back.  No warehouse block in the reply —
// the request is queued, so the reply lands in whatever scene is up, and a
// replacing 9wjrh74P there would free stacks that scene may be holding.
HANDLEF(CampaignItemEdit)
{
	LOG_INFO << "CampaignItemEdit: " << json;

	::CampaignItemEditReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "CampaignItemEdit: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	const auto eqp = gme::toCampaignItems(req.eqp_items);
	const auto rsv = gme::toCampaignItems(req.rsv_items);

	auto transaction = co_await theDb()->newTransactionCoro();
	try
	{
		co_await gme::ensureCampaignState(transaction, identity);
		const auto state = co_await transaction->execSqlCoro(
			"SELECT eqp_items, rsv_items FROM user_campaign_state WHERE user_id = $1;",
			identity.userId);

		const auto before = itemTotals(gme::parseCampaignItems(state[0]["eqp_items"].as<std::string>()),
			gme::parseCampaignItems(state[0]["rsv_items"].as<std::string>()));
		const auto after = itemTotals(eqp, rsv);

		std::set<uint32_t> ids;
		for (const auto& [id, count] : before) ids.insert(id);
		for (const auto& [id, count] : after) ids.insert(id);

		for (const auto id : ids)
		{
			const auto delta = (after.contains(id) ? after.at(id) : 0) - (before.contains(id) ? before.at(id) : 0);
			if (delta > 0)
			{
				// Stacks stay at 0 rather than being deleted, so instance ids stay
				// stable for the client's warehouse model (as ItemSell does).
				const auto held = co_await transaction->execSqlCoro(
					"SELECT item_num FROM user_items WHERE user_id = $1 AND item_id = $2;",
					identity.userId, static_cast<int64_t>(id));
				const auto have = held.empty() ? 0 : held[0]["item_num"].as<int64_t>();
				if (have < delta)
				{
					LOG_WARN << "CampaignItemEdit: loadout takes " << delta << "x item " << id
						<< " but the warehouse holds " << have << " — taking what is there";
				}
				co_await transaction->execSqlCoro(
					"UPDATE user_items SET item_num = MAX(0, item_num - $1) WHERE user_id = $2 AND item_id = $3;",
					delta, identity.userId, static_cast<int64_t>(id));
			}
			else if (delta < 0)
			{
				co_await gme::addUserItem(transaction, identity, id, static_cast<uint32_t>(-delta));
			}
		}

		co_await transaction->execSqlCoro(
			"UPDATE user_campaign_state SET eqp_items = $1, rsv_items = $2 WHERE user_id = $3;",
			glz::write_json(eqp).value_or("[]"), glz::write_json(rsv).value_or("[]"), identity.userId);

		LOG_INFO << "CampaignItemEdit: loadout " << eqp.size() << " equipped + "
			<< rsv.size() << " reserve slot(s)";
	}
	catch (...)
	{
		transaction->rollback();
		throw;
	}

	co_return HandleResult::success("{}");
}

// CampaignSave (Utzc3oj5) — the suspend prompt.  The suspend data stays on the
// client; the server keeps the mission and the loadout as it now stands
// (items used in battle are gone from it).
HANDLEF(CampaignSave)
{
	LOG_INFO << "CampaignSave: " << json;

	::CampaignSaveReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "CampaignSave: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	if (!req.mission.empty())
		co_await gme::openCampaignRun(theDb(), identity, req.mission.front().mission_id);
	co_await gme::ensureCampaignState(theDb(), identity);
	co_await theDb()->execSqlCoro(
		"UPDATE user_campaign_state SET eqp_items = $1 WHERE user_id = $2;",
		glz::write_json(gme::toCampaignItems(req.eqp_items)).value_or("[]"), identity.userId);

	// Where the party stands, so the resumed run starts on the same spot
	// rather than at point 0 (gme::storeCampaignDeckPos).
	if (req.deck_pos)
		co_await gme::storeCampaignDeckPos(theDb(), identity, *req.deck_pos);

	LOG_INFO << "CampaignSave: run suspended at "
		<< (!req.deck_pos || req.deck_pos->empty()
			? std::string{"no reported spot"}
			: "spot " + std::to_string(req.deck_pos->front().now_point_num));
	co_return HandleResult::success("{}");
}

// CampaignRestart (Ht2jeWV8) — the field's restart path.  The client rebuilds
// the run from its own suspend data (CampaignSceneBase::missionDeckSuspend-
// Construction), so there is nothing to send back.
HANDLEF(CampaignRestart)
{
	LOG_INFO << "CampaignRestart: " << json;

	::CampaignRestartReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "CampaignRestart: parse error: " << glz::format_error(ec, json);
	}

	(void)(co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
	co_return HandleResult::success("{}");
}
