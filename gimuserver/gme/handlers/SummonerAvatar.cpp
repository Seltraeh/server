#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

// THE SUMMONER AVATAR ARC — the three requests its screens make.
//
// Pairs recovered 2026-09-20 from getRequestID/getEncodeKey on each request
// class, none of which was registered:
//
//   UserSummonerInfoEdit  ZJYkXcHo / lyR0us9b   appearance (gender/hair/element)
//   SummonerMix           nv4d3O7F / vM65bAB4   fuse Frogs into the Summoner
//   SummonerSkillGet      qWkYyw5i / mrXjLLJB   raise a Parameter with SP
//
// ⚠ ALL THREE CLOSED THE SESSION.  The arc is reachable — 33
// layout_summoner_*.csv ship in _dlcbundle — and UserSummonerInfoEdit is the
// FIRST request it makes, because the wiki's Summoner Avatar article says the
// appearance prompt is what greets a player entering the arc.  An unregistered
// GroupId and a handler error look identical to the client and neither writes
// an http_log line.
//
// ⚠ THE ECONOMICS ARE THE MST'S, NOT INVENTED.  SummonerAbilityLevelMst.need_sp
// prices a Parameter level, and it matches the wiki's SP cost tables row for row
// (Max HP lv2 10, lv3 40, lv4 80, lv5 130 ...) — which is what proves the table
// is the authority rather than the article.  The fusion table is the wiki's, and
// it is short enough to state exactly: Frogs only.
//
// ⚠ `summoner_ability_info` PACKS THE PARAMETER LEVELS as `id:lv/id:lv`.
// UserSummonerInfoResponse::readParam hands the whole string to
// UserSummonerAbilityInfoList::setParams @0x127CF0C, which splits it with two
// CommonUtils::parseList calls whose separators are the inlined SSO constants
// 0x2f02 and 0x3a02 — '/' then ':'.  Read off the immediates, not guessed.

namespace
{

/*! SP cap, from the wiki: "A maximum of 99,999 SP can be held at a time." */
constexpr int32_t kSummonerSpCap = 99'999;

/*!
* What a fused unit is worth, and what it costs.
*
* The Summoner takes FROGS ONLY — the wiki's Fusion table is the whole rule, and
* the Zel price is per unit.  Anything else offered is refused rather than
* quietly eaten, because a fusion DESTROYS what it consumes.
*/
struct FrogValue { int32_t unitId; int64_t zel; int32_t sp; };
constexpr FrogValue kFrogs[] = {
	{ 10312,  50'000,  1 },   // Burst Frog
	{ 10313, 250'000,  5 },   // Burst Emperor
	{ 20302, 500'000, 10 },   // Sphere Frog
};

const FrogValue* frogFor(const int32_t unitId)
{
	for (const auto& frog : kFrogs)
	{
		if (frog.unitId == unitId)
			return &frog;
	}
	return nullptr;
}

/*! Parameter levels, unpacked from `id:lv/id:lv`. */
std::map<std::string, int32_t> unpackAbilities(const std::string& packed)
{
	std::map<std::string, int32_t> out;
	size_t start = 0;
	while (start <= packed.size())
	{
		const auto slash = packed.find('/', start);
		const auto entry = packed.substr(
			start, slash == std::string::npos ? std::string::npos : slash - start);
		if (const auto colon = entry.find(':'); colon != std::string::npos)
		{
			try { out[entry.substr(0, colon)] = std::stoi(entry.substr(colon + 1)); }
			catch (const std::exception&) {}
		}
		if (slash == std::string::npos)
			break;
		start = slash + 1;
	}
	return out;
}

std::string packAbilities(const std::map<std::string, int32_t>& levels)
{
	std::string out;
	for (const auto& [id, lv] : levels)
	{
		if (!out.empty())
			out += '/';
		out += id + ':' + std::to_string(lv);
	}
	return out;
}

/*! The reply every Summoner screen wants: the refreshed block. */
drogon::Task<std::string> summonerReply(
	const db::Database database,
	const gme::UserIdentity identity,
	const SignalKey signal)
{
	UserSummonerInfoEditResp resp{};
	resp.signal_key = signal;
	resp.summoner_info = std::vector<::UserSummonerInfo>{
		co_await gme::loadSummonerInfo(database, identity) };

	std::string buffer{};
	if (const auto ec = glz::write_json(resp, buffer); ec)
		throw std::runtime_error(glz::format_error(ec, buffer));
	co_return buffer;
}

} // namespace

HANDLEF(UserSummonerInfoEdit)
{
	(void)session;

	UserSummonerInfoEditReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
		{
			const auto error = glz::format_error(ec, json);
			LOG_ERROR << "UserSummonerInfoEdit deserialization failed: " << error;
			co_return HandleResult::error("Deserialization error", error);
		}
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// ⚠ THE ELEMENT IS NOT COSMETIC.  The wiki: "the Quest you complete will
	// merit you Summoning Arts based on the element you have picked" — so this
	// decides which of the six element levels a quest feeds, and those are what
	// Randall bands 56000-61000 measure.
	//
	// Upsert rather than update: a save that has never entered the arc has no
	// row, and the column defaults are init()'s own values, so a partial write
	// still describes a valid Summoner.
	co_await theDb()->execSqlCoro(
		"INSERT INTO user_summoner (user_id, sex, element, hair_id) VALUES ($1, $2, $3, $4)"
		" ON CONFLICT(user_id) DO UPDATE SET"
		" sex = excluded.sex, element = excluded.element, hair_id = excluded.hair_id;",
		identity.userId, req.edit.sex, req.edit.element, req.edit.hair_id);

	LOG_INFO << "UserSummonerInfoEdit: " << identity.userId
		<< " set sex " << req.edit.sex << ", element " << req.edit.element
		<< ", hair " << req.edit.hair_id;

	co_return HandleResult::success(
		co_await summonerReply(theDb(), identity, req.signal_key));
}

HANDLEF(SummonerMix)
{
	(void)session;

	SummonerMixReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
		{
			const auto error = glz::format_error(ec, json);
			LOG_ERROR << "SummonerMix deserialization failed: " << error;
			co_return HandleResult::error("Deserialization error", error);
		}
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
	if (!req.materials || req.materials->empty())
		co_return HandleResult::error("Invalid fusion", "no units offered");

	std::string body;
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// Resolve the offered units to species server-side.  A favourited
			// unit, one that is not this player's, or one that is not a Frog is
			// simply not spent — the fusion destroys what it eats, so the guard
			// matters more here than the price does.
			std::string offered;
			for (const auto& material : *req.materials)
				offered += (offered.empty() ? "" : ",") + std::to_string(material.user_unit_id);

			int64_t zelCost = 0;
			int32_t spGain = 0;
			int32_t eaten = 0;
			std::string spend;
			for (const auto& row : co_await transaction->execSqlCoro(
				"SELECT user_unit_id, unit_id FROM user_units"
				" WHERE user_id = $1 AND favorite_flg = 0 AND user_unit_id IN (" + offered + ");",
				identity.userId))
			{
				const auto* frog = frogFor(row["unit_id"].as<int32_t>());
				if (frog == nullptr)
					continue;
				zelCost += frog->zel;
				spGain += frog->sp;
				++eaten;
				spend += (spend.empty() ? "" : ",")
					+ std::to_string(row["user_unit_id"].as<int32_t>());
			}

			if (spend.empty())
			{
				transaction->rollback();
				co_return HandleResult::error("Invalid fusion",
					"the Summoner only takes Burst Frogs, Burst Emperors and Sphere Frogs");
			}

			// Charged in the statement that tests affordability, so nothing can
			// slip between the test and the debit.
			if ((co_await transaction->execSqlCoro(
					"UPDATE user_info SET zel = zel - $1"
					" WHERE id = $2 AND zel >= $1 RETURNING zel;",
					zelCost, identity.userId)).empty())
			{
				transaction->rollback();
				co_return HandleResult::error("Invalid fusion", "not enough Zel");
			}

			// Spheres first: deleting the rows without returning them would
			// destroy owned items, same as UnitSell and UnitMix.
			co_await gme::returnEquippedSpheres(transaction, identity, spend);
			co_await transaction->execSqlCoro(
				"DELETE FROM user_units WHERE user_id = $1 AND user_unit_id IN (" + spend + ");",
				identity.userId);

			// ⚠ CAPPED IN THE STATEMENT THAT ADDS.  The client's counter is five
			// digits, so an uncapped total would both display wrongly and be
			// unspendable back down.
			co_await transaction->execSqlCoro(
				"INSERT INTO user_summoner (user_id, sp) VALUES ($1, MIN($2, $3))"
				" ON CONFLICT(user_id) DO UPDATE SET sp = MIN(user_summoner.sp + $2, $3);",
				identity.userId, spGain, kSummonerSpCap);

			LOG_INFO << "SummonerMix: " << identity.userId << " fused " << eaten
				<< " frog(s) for " << spGain << " SP at " << zelCost << " zel";

			body = co_await summonerReply(transaction, identity, req.signal_key);
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}
	co_return HandleResult::success(body);
}

HANDLEF(SummonerSkillGet)
{
	(void)session;

	SummonerSkillGetReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
		{
			const auto error = glz::format_error(ec, json);
			LOG_ERROR << "SummonerSkillGet deserialization failed: " << error;
			co_return HandleResult::error("Deserialization error", error);
		}
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// The request may carry "<id>" or "<id>:<level>"; take the id either way.
	auto abilityId = req.ability_id.value_or(std::string{});
	if (const auto colon = abilityId.find(':'); colon != std::string::npos)
		abilityId = abilityId.substr(0, colon);
	if (abilityId.empty())
		co_return HandleResult::error("Invalid parameter", "no Parameter named");

	std::string body;
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			const auto held = co_await transaction->execSqlCoro(
				"SELECT sp, ability_info FROM user_summoner WHERE user_id = $1;",
				identity.userId);
			const auto sp = held.empty() ? 0 : held[0]["sp"].as<int32_t>();
			auto levels = unpackAbilities(
				held.empty() || held[0]["ability_info"].isNull()
					? std::string{} : held[0]["ability_info"].as<std::string>());

			// A Parameter starts at level 1, so the first purchase buys level 2
			// — which is why SummonerAbilityLevelMst's level-1 row costs 0 and
			// the wiki prints "–" against it.
			const auto current = levels.contains(abilityId) ? levels[abilityId] : 1;
			const auto want = current + 1;

			const auto& table = theServer()->cache().summonerAbilityLevelMst();
			const auto next = std::find_if(table.begin(), table.end(),
				[&abilityId, want](const ::SummonerAbilityLevelMst& row) {
					return std::to_string(row.ability_id) == abilityId && row.lv == want;
				});
			if (next == table.end())
			{
				transaction->rollback();
				co_return HandleResult::error("Invalid parameter",
					"that Parameter is already at its maximum");
			}

			// Spent and recorded together: a debit without the level, or a level
			// without the debit, are both worse than refusing.
			if ((co_await transaction->execSqlCoro(
					"UPDATE user_summoner SET sp = sp - $1"
					" WHERE user_id = $2 AND sp >= $1 RETURNING sp;",
					next->need_sp, identity.userId)).empty())
			{
				transaction->rollback();
				co_return HandleResult::error("Invalid parameter", "not enough SP");
			}

			levels[abilityId] = want;
			co_await transaction->execSqlCoro(
				"UPDATE user_summoner SET ability_info = $1 WHERE user_id = $2;",
				packAbilities(levels), identity.userId);

			LOG_INFO << "SummonerSkillGet: " << identity.userId << " raised Parameter "
				<< abilityId << " to level " << want << " for " << next->need_sp << " SP";

			body = co_await summonerReply(transaction, identity, req.signal_key);
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}
	co_return HandleResult::success(body);
}
