#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

// Event tokens — the per-event currencies.  Token 8 is the Frontier Gate's
// "Rift Token", and F_FROGATE_REWARD_MST pays it on 505 rows as present type
// 8004; the other 95 named tokens belong to seasonal events (Halloween Token,
// Matsuri Token, 7th Anniversary Token, …).
//
// Wire shape: EventTokenInfo under `l234vdKs`, decoded from
// EventTokenInfoResponse::readParam @0x1C49274 — see net/event.kdl for the
// field map and the evidence.  readParam clears EventTokenInfoList first, so
// every send is a FULL REPLACE.
//
// Names come from deploy/mst/event_token_mst.json, which is derived from the
// client's own sgtext_Exchange_Token text (EVENT_TOKEN_%04d_NAME) — the same
// place PresentCommon::createPresentName @0x11C3AF8 resolves a token present's
// label, so the present box is already correctly named without this list.

namespace gme
{

namespace detail
{

/*!
* Token id -> the dungeons that pay it, as the comma list `0Dk4fc81` wants.
*
* This is the field that makes a token VISIBLE.  EventTokenInfoList::
* addObjectIntoMap @0x1C7E81C splits this list and keys the token by each
* dungeon in it, and the three screens that draw a token panel all look it up
* that way -- MissionSelectScene2::setLayoutControl, MissionCheckScene::
* setEventTokenInfo @0x187B8E4 and MissionResultScene::scoreDraw @0x18D0180
* each ask EventTokenInfoList::getObjectWithDungeonId for the dungeon they are
* showing and skip the panel entirely when nothing answers.  With the list
* empty (which is how this shipped) a token could be earned and banked and
* still never appear anywhere, which is what "I still don't see the points
* incremented, it sits at 0" was on 2026-09-12.
*
* Derived, not authored: F_FROGATE_REWARD_MST says which gate pays which token
* (present type 8004, target_id = the token), and FrontierGateMst says which
* dungeon backs that gate.  Built once -- both tables are boot-time caches.
*/
inline const std::map<int32_t, std::string>& tokenDungeons()
{
	static const auto map = []
	{
		std::map<int32_t, std::set<int32_t>> byToken;
		std::map<int32_t, int32_t> gateDungeon;
		for (const auto& gate : theServer()->cache().frontierGateMst())
			gateDungeon[gate.id] = gate.dungeon_id;

		for (const auto& reward : theServer()->cache().frontierGateRewardMst())
		{
			if (reward.present_type != 8004)
				continue;
			const auto dungeon = gateDungeon.find(reward.frogate_id);
			if (dungeon == gateDungeon.end() || dungeon->second == 0)
				continue;
			try { byToken[std::stoi(reward.target_id)].insert(dungeon->second); }
			catch (const std::exception&) { continue; }
		}

		std::map<int32_t, std::string> joined;
		for (const auto& [token, dungeons] : byToken)
		{
			std::string list;
			for (const auto dungeon : dungeons)
			{
				if (!list.empty())
					list += ',';
				list += std::to_string(dungeon);
			}
			joined[token] = std::move(list);
		}
		return joined;
	}();
	return map;
}

} // namespace detail

/*!
* The player's event-token balances as `l234vdKs` rows.
*
* Rows are kept once created, zeros included: the list is a full replace but an
* empty array never runs readParam, so a token spent to nothing has to keep
* reporting itself or the client would show the old balance.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return One row per token the user has a record of, by token id.
*/
inline drogon::Task<std::vector<::EventTokenInfo>> loadEventTokens(
	const db::Database database,
	const UserIdentity identity)
{
	const auto rows = co_await database->execSqlCoro(
		"SELECT token_id, count FROM user_event_tokens WHERE user_id = $1 ORDER BY token_id;",
		identity.userId);

	std::map<std::string, std::string> names;
	for (const auto& token : theServer()->cache().eventTokenMst())
		names[std::to_string(token.token_id)] = token.name;

	std::vector<::EventTokenInfo> tokens;
	for (const auto& row : rows)
	{
		const auto tokenId = row["token_id"].as<std::string>();
		const auto named = names.find(tokenId);
		::EventTokenInfo info{};
		try { info.token_id = std::stoi(tokenId); }
		catch (const std::exception&) { continue; }
		info.name = named == names.end() ? std::string{} : named->second;
		info.amount = std::max(row["count"].as<int32_t>(), 0);
		// No expiry: nothing in this fork drives a token's clock, and sending a
		// stale timeleft would count down to an expiry that never happens.
		info.timeleft = 0;
		// The dungeons that pay it -- see detail::tokenDungeons, and note this
		// is what decides whether the token is ever drawn.  unit_ids stays empty
		// until there is an exchange to offer units from.
		if (const auto it = detail::tokenDungeons().find(info.token_id);
			it != detail::tokenDungeons().end())
			info.dungeon_ids = it->second;
		tokens.push_back(std::move(info));
	}
	co_return tokens;
}

/*!
* Adds tokens of one type (present type 8004, Frontier Gate payouts, …).
*/
inline drogon::Task<void> grantEventToken(
	const db::Database database,
	const UserIdentity identity,
	const std::string& tokenId,
	const int32_t count)
{
	co_await database->execSqlCoro(
		"INSERT INTO user_event_tokens (user_id, token_id, count) VALUES ($1, $2, $3)"
		" ON CONFLICT(user_id, token_id) DO UPDATE SET count = count + $3;",
		identity.userId, tokenId, std::max(count, 1));
}

/*!
* Whether a token id is one the client can name.
*
* Used to refuse an unknown token rather than bank a currency that would render
* as a blank tile — the same rule the selector and V2 tickets follow.
*/
inline bool isKnownEventToken(const std::string& tokenId)
{
	int32_t id = 0;
	try { id = std::stoi(tokenId); }
	catch (const std::exception&) { return false; }

	const auto& mst = theServer()->cache().eventTokenMst();
	return std::any_of(mst.begin(), mst.end(),
		[id](const ::EventTokenInfo& token) { return token.token_id == id; });
}

} // namespace gme
