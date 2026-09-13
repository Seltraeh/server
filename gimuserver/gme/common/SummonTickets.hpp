#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <map>
#include <string>
#include <vector>

// Summon tickets.  TWO separate economies, and the client keeps them apart:
//
//  * the PLAIN ticket — one counter, `user_info.summon_tickets`, reported in
//    team_info (`UserTeamInfo::getSummonTicket`) and spent by the "use summon
//    tickets" checkbox on a rare-summon gate.  Present type 8000 pays it
//    (PresentCommon::createPresentName @0x11C3900 names type 8000
//    "SUMMON_TICKET").
//
//  * the V2 ticket — a per-TYPE inventory keyed by SummonTicketV2Mst.id
//    (`b0D2iq2d`), each type redeemable on one gate (`target_gacha`).  Present
//    type 8001 pays it: createPresentName @0x11C3A2C looks the target id up in
//    SummonTicketV2MstList for the display name.  The inventory reaches the
//    client as SummonTicketV2UserInfo under `a3d5d12i`, and
//    SummonTicketV2UserInfoResponse::readParam @0x1C6EBB4 calls
//    removeAllObjects() first, so every send is a FULL REPLACE.
//
// The V2 list is load-bearing for the ticket summon, not decoration:
// SummonsDetailScene::touchBegan @0x16A7FA8 walks SummonTicketV2UserInfoList,
// sums the amounts of the ticket types whose MST row targets the open gate, and
// only then lets the pull through (otherwise it shows the shortage dialog).
// An empty list means "you have no tickets" no matter what the present box paid.

namespace gme
{

/*!
* The V2 ticket inventory as `a3d5d12i` rows.
*
* Every row the user has is sent, INCLUDING zeros: readParam replaces the whole
* list, but only when the key carries at least one row, so a ticket spent down
* to its last copy has to keep reporting itself at 0 or the client would go on
* showing the old amount.  Ticket types the user has never held are left out —
* the catalogue is 89 rows and the client only needs what it must display.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to read.
* @return One row per ticket type the user has a record of, by ticket id.
*/
inline drogon::Task<std::vector<::SummonTicketV2UserInfo>> loadSummonTicketsV2(
	const db::Database database,
	const UserIdentity identity)
{
	const auto rows = co_await database->execSqlCoro(
		"SELECT ticket_id, count FROM user_summon_tickets_v2"
		" WHERE user_id = $1 ORDER BY ticket_id;",
		identity.userId);

	std::vector<::SummonTicketV2UserInfo> tickets;
	for (const auto& row : rows)
	{
		tickets.push_back({
			.id     = row["ticket_id"].as<int32_t>(),
			.amount = std::max(row["count"].as<int32_t>(), 0),
		});
	}
	co_return tickets;
}

/*!
* Adds V2 tickets of one type (present type 8001, or any other grant).
*/
inline drogon::Task<void> grantSummonTicketV2(
	const db::Database database,
	const UserIdentity identity,
	const std::string& ticketId,
	const int32_t count)
{
	co_await database->execSqlCoro(
		"INSERT INTO user_summon_tickets_v2 (user_id, ticket_id, count) VALUES ($1, $2, $3)"
		" ON CONFLICT(user_id, ticket_id) DO UPDATE SET count = count + $3;",
		identity.userId, ticketId, std::max(count, 1));
}

/*!
* How many tickets one pull on `gachaId` costs, per ticket type that can pay for
* it.  `required_tickets` (JRqU2bS6) is absent on every shipped row, which the
* client reads as one — the same default the selector tickets use.
*
* @return ticket id -> tickets needed for a single pull, empty when the gate
*         takes no ticket.
*/
inline std::map<std::string, int32_t> summonTicketTypesForGate(const uint32_t gachaId)
{
	std::map<std::string, int32_t> types;
	if (gachaId == 0)
		return types;
	const auto wanted = std::to_string(gachaId);
	for (const auto& mst : theServer()->cache().userInfoResp().summon_ticket_v2)
	{
		if (!mst.id || !mst.target_gacha || *mst.target_gacha != wanted)
			continue;
		types[std::to_string(*mst.id)] = std::max(mst.required_tickets.value_or(1), 1);
	}
	return types;
}

/*!
* Spends tickets for `pulls` pulls on `gachaId`, cheapest-to-hold first.
*
* Tickets of DIFFERENT types that target the same gate are interchangeable, so
* the spend walks the types the player actually holds and takes what it needs.
*
* @return true when the whole cost was paid (and debited), false when the player
*         cannot cover it — in which case nothing is debited.
*/
inline drogon::Task<bool> spendSummonTicketsV2(
	const db::Database database,
	const UserIdentity identity,
	const uint32_t gachaId,
	const uint32_t pulls)
{
	const auto types = summonTicketTypesForGate(gachaId);
	if (types.empty())
		co_return false;

	const auto rows = co_await database->execSqlCoro(
		"SELECT ticket_id, count FROM user_summon_tickets_v2"
		" WHERE user_id = $1 AND count > 0 ORDER BY ticket_id;",
		identity.userId);

	// Each type has its own price, so cost is counted in "pulls this type can
	// still pay for" rather than in tickets.
	std::vector<std::pair<std::string, int32_t>> held;
	uint32_t affordable = 0;
	for (const auto& row : rows)
	{
		const auto ticketId = row["ticket_id"].as<std::string>();
		const auto it = types.find(ticketId);
		if (it == types.end())
			continue;
		const auto count = row["count"].as<int32_t>();
		held.emplace_back(ticketId, count);
		affordable += static_cast<uint32_t>(count / it->second);
	}
	if (affordable < pulls)
		co_return false;

	auto remaining = pulls;
	for (const auto& [ticketId, count] : held)
	{
		if (remaining == 0)
			break;
		const auto price = types.at(ticketId);
		const auto canPay = std::min<uint32_t>(remaining, static_cast<uint32_t>(count / price));
		if (canPay == 0)
			continue;
		co_await database->execSqlCoro(
			"UPDATE user_summon_tickets_v2 SET count = count - $1"
			" WHERE user_id = $2 AND ticket_id = $3;",
			static_cast<int32_t>(canPay * price), identity.userId, ticketId);
		remaining -= canPay;
	}
	co_return remaining == 0;
}

} // namespace gme
