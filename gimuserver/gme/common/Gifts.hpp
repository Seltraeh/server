#pragma once

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/FriendPoints.hpp>

#include <algorithm>
#include <ctime>
#include <random>
#include <string>
#include <vector>

// The gift inbox -- what a friend has sent you today.
//
// WHAT GIFTS ACTUALLY ARE (checked, not assumed): Honor crafting materials, the
// purple items obtainable ONLY this way.  A player nominates up to three gifts
// they want, friends send them once per day each, and -- the part that makes
// this implementable -- A GIFT IS NOT DEDUCTED FROM THE SENDER.  It costs the
// giver nothing, so a gift arriving from the simulated friend is not taking
// anything from anybody.
//
// That is why this exists at all.  The gift screen renders straight from
// UserGiftInfoList with no request of its own (GiftRecieveListScene2::
// initConnect is a 16-byte stub), and on an EMPTY list setGiftList @0x166D330
// null-derefs -- a deterministic client crash, identical in two minidumps
// (+0x84F533, read 0x14).  There is no server-side gate on the gift button, so
// an empty inbox is not a safe state: the inbox has to have something in it.
//
// THE DESIGN THIS IMPLEMENTS, and where it is going:
// One gift per UTC day from the simulated friend, WEIGHTED so the gift the
// player asked for lands more often than the rest.  That is the hook -- you set
// a preference, and logging in tomorrow is worth something because you might
// get it.  When a real friend system lands, the same table and the same claim
// path serve real senders; only the generator below goes away.
namespace gme
{

/*! Who the gifts come from until real friends exist. */
inline constexpr const char* kGiftSenderId = kSyntheticHelperUserId;
inline constexpr const char* kGiftSenderName = "DecompFriend";

/*! How much likelier the gift the player ASKED for is than any other. */
inline constexpr int32_t kWantedGiftWeight = 4;

/*! Today as the inbox's day key.  UTC, so a timezone cannot mint a second gift. */
inline std::string giftToday()
{
	const auto now = std::time(nullptr);
	std::tm tm{};
#ifdef _WIN32
	gmtime_s(&tm, &now);
#else
	gmtime_r(&now, &tm);
#endif
	char buf[16]{};
	std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
	return buf;
}

namespace detail
{

/*!
* Pick today's gift, biased toward the one the player asked for.
*
* Deterministic per (user, day) on purpose: the daily drop is decided once, and
* re-rolling it on a second login would let a player reload until they liked the
* result.  The seed is the user id and the date, so the same day always yields
* the same gift for that player -- and a different one tomorrow.
*/
inline int32_t rollGift(
	const std::vector<::GiftItemMst>& catalogue,
	const std::string& wantGift,
	const std::string& userId,
	const std::string& day)
{
	if (catalogue.empty())
		return 0;

	std::vector<int32_t> pool;
	pool.reserve(catalogue.size() + kWantedGiftWeight);
	for (const auto& row : catalogue)
	{
		pool.push_back(row.id);
		// The wanted gift goes in several times rather than being forced: the
		// point is a better chance, not a guarantee, or there is no reason to
		// look.
		if (!wantGift.empty() && std::to_string(row.id) == wantGift)
		{
			for (int32_t i = 1; i < kWantedGiftWeight; ++i)
				pool.push_back(row.id);
		}
	}

	std::seed_seq seed{ static_cast<uint32_t>(std::hash<std::string>{}(userId)),
						static_cast<uint32_t>(std::hash<std::string>{}(day)) };
	std::mt19937 rng(seed);
	std::uniform_int_distribution<size_t> pick(0, pool.size() - 1);
	return pool[pick(rng)];
}

} // namespace detail

/*!
* Make sure today's gift exists, then read the unclaimed inbox back.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @return Unclaimed gift rows, newest last, ready for the wire.
*/
inline drogon::Task<std::vector<::UserGiftInfo>> loadGiftInbox(
	const db::Database database,
	const UserIdentity identity)
{
	std::vector<::UserGiftInfo> inbox;

	const auto& catalogue = theServer()->cache().userInfoResp().gift;
	if (catalogue.empty())
	{
		LOG_WARN << "loadGiftInbox: no GiftItemMst rows loaded; the gift screen "
			"would open on an empty inbox and crash, so sending nothing is not "
			"safe -- check deploy/mst/gift_item_mst.json";
		co_return inbox;
	}

	const auto day = giftToday();

	// One per day, and the INSERT is guarded by a read rather than a unique
	// index so the day key can stay human-readable.
	const auto existing = co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n FROM user_gifts WHERE user_id = $1 AND day = $2;",
		identity.userId, day);
	if (!existing.empty() && existing[0]["n"].as<int64_t>() == 0)
	{
		std::string wantGift;
		const auto want = co_await database->execSqlCoro(
			"SELECT want_gift FROM user_info WHERE id = $1;", identity.userId);
		if (!want.empty())
			wantGift = want[0]["want_gift"].as<std::string>();

		const auto giftId = detail::rollGift(catalogue, wantGift, identity.userId, day);
		if (giftId != 0)
		{
			co_await database->execSqlCoro(
				"INSERT INTO user_gifts (user_id, from_user_id, gift_id, day, received)"
				" VALUES ($1, $2, $3, $4, 0);",
				identity.userId, std::string(kGiftSenderId), giftId, day);
			LOG_INFO << "loadGiftInbox: " << kGiftSenderName << " sent gift "
				<< giftId << " to " << identity.userId << " for " << day
				<< (wantGift.empty() ? "" : " (wanted: " + wantGift + ")");
		}
	}

	for (const auto& row : co_await database->execSqlCoro(
		"SELECT id, from_user_id, gift_id, day FROM user_gifts"
		" WHERE user_id = $1 AND received = 0 ORDER BY id;",
		identity.userId))
	{
		const auto giftId = row["gift_id"].as<int32_t>();
		const auto it = std::find_if(catalogue.begin(), catalogue.end(),
			[giftId](const ::GiftItemMst& g) { return g.id == giftId; });
		if (it == catalogue.end())
			continue;   // catalogue changed under a stored row; skip rather than send a half-row

		::UserGiftInfo gift{};
		gift.gift_identify_id = row["id"].as<int32_t>();
		gift.user_id_from = row["from_user_id"].as<std::string>();
		gift.user_id_to = identity.userId;
		gift.gift_id = giftId;
		gift.gift_type = it->type;
		gift.item_id = it->item_id;
		gift.possession = it->possession;
		gift.gift_date = row["day"].as<std::string>() + " 00:00:00";  // chrono_time takes the string form
		gift.gift_ymd = 0;
		gift.recieve_flg = 0;
		gift.handle_name = std::string(kGiftSenderName);
		inbox.push_back(std::move(gift));
	}

	co_return inbox;
}

/*!
* Claim the named gifts, paying each into the right place.
*
* The gift TYPE decides where it lands, from GiftItemMst's own rows:
*   1 Zel, 2 Honor Points, 3 an item (the Honor crafting materials), 4 Karma.
*
* Idempotent by design: the client sends its whole selection and will resend it
* if the screen is reopened, so the UPDATE carries `received = 0` and a row that
* has already paid out simply matches nothing.
*
* @param database Transaction to run in -- the caller owns the commit.
* @param identity Resolved user.
* @param ids      Comma-separated gift_identify_ids from `23Xi0jom`.
* @return How many gifts actually paid out.
*/
inline drogon::Task<int32_t> claimGifts(
	const db::Database database,
	const UserIdentity identity,
	const std::string& ids)
{
	int32_t paid = 0;
	if (ids.empty())
		co_return paid;

	const auto& catalogue = theServer()->cache().userInfoResp().gift;

	// Same split idiom AchievementDeliver uses on its id lists; blanks dropped.
	std::vector<std::string> tokens;
	for (size_t at = 0; at <= ids.size(); )
	{
		const auto comma = ids.find(',', at);
		auto piece = ids.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
		while (!piece.empty() && std::isspace(static_cast<unsigned char>(piece.front())))
			piece.erase(piece.begin());
		while (!piece.empty() && std::isspace(static_cast<unsigned char>(piece.back())))
			piece.pop_back();
		if (!piece.empty())
			tokens.push_back(std::move(piece));
		if (comma == std::string::npos)
			break;
		at = comma + 1;
	}

	for (const auto& token : tokens)
	{
		int32_t identifyId = 0;
		try { identifyId = std::stoi(token); }
		catch (const std::exception&) { continue; }
		if (identifyId <= 0)
			continue;

		// Claim the row FIRST.  If the grant below throws, the transaction
		// rolls the whole thing back together -- but within one pass this stops
		// a duplicated id in the list from paying twice.
		const auto claimed = co_await database->execSqlCoro(
			"UPDATE user_gifts SET received = 1"
			" WHERE id = $1 AND user_id = $2 AND received = 0;",
			identifyId, identity.userId);
		if (claimed.affectedRows() == 0)
			continue;

		const auto row = co_await database->execSqlCoro(
			"SELECT gift_id FROM user_gifts WHERE id = $1;", identifyId);
		if (row.empty())
			continue;
		const auto giftId = row[0]["gift_id"].as<int32_t>();
		const auto it = std::find_if(catalogue.begin(), catalogue.end(),
			[giftId](const ::GiftItemMst& g) { return g.id == giftId; });
		if (it == catalogue.end())
			continue;

		switch (it->type)
		{
		case 1:
			co_await database->execSqlCoro(
				"UPDATE user_info SET zel = zel + $1 WHERE id = $2;",
				it->possession, identity.userId);
			break;
		case 2:
			co_await database->execSqlCoro(
				"UPDATE user_info SET friend_points = friend_points + $1 WHERE id = $2;",
				it->possession, identity.userId);
			break;
		case 3:
			// The Honor crafting materials.  user_items stacks by species, so
			// add to the stack when one exists and open a new one when it does
			// not.
			{
				const auto stack = co_await database->execSqlCoro(
					"SELECT instance_id FROM user_items"
					" WHERE user_id = $1 AND item_id = $2 LIMIT 1;",
					identity.userId, it->item_id);
				if (stack.empty())
				{
					co_await database->execSqlCoro(
						"INSERT INTO user_items (user_id, item_id, item_num)"
						" VALUES ($1, $2, $3);",
						identity.userId, it->item_id, it->possession);
				}
				else
				{
					co_await database->execSqlCoro(
						"UPDATE user_items SET item_num = item_num + $1"
						" WHERE instance_id = $2;",
						it->possession, stack[0]["instance_id"].as<int64_t>());
				}
			}
			break;
		case 4:
			co_await database->execSqlCoro(
				"UPDATE user_info SET karma = karma + $1 WHERE id = $2;",
				it->possession, identity.userId);
			break;
		default:
			LOG_WARN << "claimGifts: gift " << giftId << " has unhandled type "
				<< it->type << "; the row is claimed but nothing was paid";
			break;
		}

		++paid;
		LOG_INFO << "claimGifts: " << identity.userId << " claimed gift "
			<< identifyId << " (gift_id " << giftId << ", type " << it->type
			<< ", x" << it->possession << ")";
	}

	co_return paid;
}

} // namespace gme
