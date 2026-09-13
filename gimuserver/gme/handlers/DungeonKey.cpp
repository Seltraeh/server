#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/PermitPlace.hpp>

#include <charconv>
#include <ctime>
#include <string_view>
#include <vector>

// Vortex dungeon keys — the parade key economy.
//
// The player claims a Metal or Jewel key on the weekdays
// DungeonKeyMst.distribute_days allows (Metal Mon/Tue/Thu/Fri, Jewel Wed, none
// on the weekend), then spends 1 / 3 / 5 of them to open successively better
// parades per DungeonKeyMst.usage_pattern.
//
// Three handlers, all answering with the SAME shape — the post-mutation key
// inventory under eFU7Qtb0.  That is the only dungeon-key response class the
// binary defines (there is no dedicated Receipt or Use response), so the client
// re-reads its whole list from every reply.  The legacy fork returned {} from
// all three, which left the client's key count stale until the next UserInfo.
//
// Field semantics: tools/ida/audits/eFU7Qtb0_audit.txt.
//   GetDistributeDungeonKeyInfo  1mr9UsYz / r0ZA3pn5  — login envelope only
//   DungeonKeyReceipt            WCJE0xe2 / V4pfQo5C  — claim today's key
//   DungeonKeyUse                aGT5S6qZ / 3rPx6tTw  — spend keys on a tier

namespace
{

/// Local calendar reading used for every day comparison in this file.
///
/// Local rather than UTC for the same reason the Vortex rotation in UserInfo is
/// local: "which day is it" has to mean what the player's clock says.
struct CalendarDay
{
	/// ISO weekday, 1 = Monday .. 7 = Sunday.  Matches DungeonKeyMst.distribute_days.
	int isoWeekday = 1;

	/// Days since the Unix epoch.  Used as the once-per-day claim token stored
	/// in last_receipt_day: monotonic, timezone-stable within a session, and
	/// directly comparable without any date maths.
	///
	/// // UNVERIFIED: setLastReceiptDay's encoding is not decoded — the client
	/// may expect a different ordinal.  This value is only ever compared
	/// against itself server-side (today == stored ? already claimed), so a
	/// wrong guess cannot break the claim logic; it could only make a
	/// client-side display of the date wrong.
	int64_t epochDay = 0;

	/// Local midnight of this day, in epoch seconds.
	int64_t midnightEpoch = 0;
};

CalendarDay today()
{
	const auto now = std::time(nullptr);
	std::tm local = *std::localtime(&now);

	CalendarDay day{};
	day.isoWeekday = local.tm_wday == 0 ? 7 : local.tm_wday;

	local.tm_hour = 0;
	local.tm_min = 0;
	local.tm_sec = 0;
	local.tm_isdst = -1;
	const auto midnight = std::mktime(&local);

	day.midnightEpoch = static_cast<int64_t>(midnight);
	day.epochDay = day.midnightEpoch / 86400;
	return day;
}

/// Epoch seconds of the next local midnight on which `key` may be claimed.
///
/// Returns 0 when the key is never distributed (Imp Key: distribute_flag 0 and
/// an empty distribute_days) — there is no next date to name, and 0 is the
/// field's own default so the client sees "no schedule" rather than a date in
/// 1970.
int64_t nextReceiptDate(const ::DungeonKeyMst& key, const CalendarDay& day, bool claimableToday)
{
	if (key.distribute_flag == 0 || key.distribute_days.empty())
		return 0;

	if (claimableToday)
		return day.midnightEpoch;

	// Walk forward from tomorrow; a non-empty schedule always hits within a week.
	for (int ahead = 1; ahead <= 7; ++ahead)
	{
		const int weekday = ((day.isoWeekday - 1 + ahead) % 7) + 1;
		for (const auto allowed : key.distribute_days)
		{
			if (allowed == weekday)
				return day.midnightEpoch + static_cast<int64_t>(ahead) * 86400;
		}
	}

	return 0;
}

/// Formats an epoch-seconds instant the way the client's string date setters
/// expect: "YYYY-MM-DD hh:mm:ss", local time.
///
/// setNextReceiptPossibleDate takes a std::string, not an int — confirmed from
/// the readParam pseudocode in tools/ida/audits/eFU7Qtb0_audit.txt.  Matches
/// UserClearMissionInfo.clear_date, the only other string date this server
/// emits.  Returns "" for 0 so "no schedule" stays empty rather than becoming
/// a date in 1970.
std::string formatWireDate(int64_t epochSeconds)
{
	if (epochSeconds <= 0)
		return {};

	const std::time_t t = static_cast<std::time_t>(epochSeconds);
	std::tm local{};
	localtime_s(&local, &t);

	char buf[24] = {};
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
	return buf;
}

bool distributedOn(const ::DungeonKeyMst& key, int isoWeekday)
{
	if (key.distribute_flag == 0)
		return false;

	for (const auto allowed : key.distribute_days)
	{
		if (allowed == isoWeekday)
			return true;
	}

	return false;
}

/// One rung of DungeonKeyMst.usage_pattern.
// Tier parsing lives in gme::parseUsagePatternTiers (Common.hpp) so PermitPlace
// can reuse it — it also carries the tier's mission ids, which this file needs
// to tell the client what the purchase just opened.


} // namespace

namespace gme
{

/// Reads the caller's key inventory, seeding any row the MST declares but the
/// player does not have yet, and fills in the two derived response fields.
///
/// Seeding lazily here rather than at account creation keeps the table correct
/// for accounts that predate this feature, and means a future MST gaining a
/// fourth key needs no migration.
drogon::Task<std::vector<::UserDungeonKeyInfo>> dungeonKeyState(
	const db::Database db,
	const UserIdentity identity)
{
	const auto& keyMst = theServer()->cache().dungeonKeyMst();

	auto stored = (co_await db::PacketInterfaceFor<::UserDungeonKeyInfo>::read(
		db,
		"user_dungeon_keys",
		{ db::Lookup("user_id", identity.userId) })).data;

	const auto day = today();

	std::vector<::UserDungeonKeyInfo> state;
	state.reserve(keyMst.size());

	for (const auto& key : keyMst)
	{
		auto match = std::find_if(
			stored.begin(),
			stored.end(),
			[&key](const ::UserDungeonKeyInfo& row) { return row.dungeon_key_id == key.id; });

		::UserDungeonKeyInfo row{};
		if (match != stored.end())
		{
			row = std::move(*match);
		}
		else
		{
			row.user_id = identity.userId;
			row.dungeon_key_id = key.id;
			row.possession = 0;
			row.last_receipt_day = 0;
			row.active_type = 0;

			co_await db::PacketInterfaceFor<::UserDungeonKeyInfo>::insert(db, "user_dungeon_keys", row);
			LOG_INFO << "DungeonKey: seeded row for key " << key.id << " (" << key.name << ")";
		}

		// THE ACTIVE WINDOW.  active_until is an absolute epoch second written
		// by DungeonKeyUse; cnt is what the client actually reads to decide how
		// long the parade stays open, and it was never assigned — so every
		// reply said "0 seconds left" and the parade re-locked the moment it
		// opened, however many keys had been spent.
		//
		// Expiry is evaluated on READ rather than by a timer: nothing has to
		// tick, and a window that lapsed while the server was down is correctly
		// closed the next time anyone asks.
		{
			const auto nowSec = static_cast<int64_t>(
				std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now().time_since_epoch()).count());

			int64_t activeUntil = 0;
			int32_t activeTier = 0;
			try
			{
				const auto rows = co_await db->execSqlCoro(
					"SELECT active_until, active_tier FROM user_dungeon_keys"
					" WHERE user_id = $1 AND dungeon_key_id = $2;",
					identity.userId, static_cast<int32_t>(key.id));
				if (rows.size() > 0)
				{
					activeUntil = rows[0]["active_until"].as<int64_t>();
					activeTier = rows[0]["active_tier"].as<int32_t>();
				}
			}
			catch (const drogon::orm::DrogonDbException& ex)
			{
				LOG_WARN << "DungeonKey: active_until read failed: " << ex.base().what();
			}

			// ⚠ TEST THE TIER, NOT active_type.  The Imp Key's usage_pattern is
			// "1:0:Garden of Imps:102920:" — active_type 0 for a perfectly valid
			// purchase — so keying expiry off active_type left that row claiming
			// an open window for ever.  active_tier is the spend count and is
			// non-zero for every real purchase.
			if (activeTier != 0 && nowSec >= activeUntil)
			{
				// Lapsed — close it and persist, so the next read is cheap and
				// the DB never claims an open parade that is not.
				row.active_type = 0;
				row.cnt = 0;
				try
				{
					co_await db->execSqlCoro(
						"UPDATE user_dungeon_keys SET active_type = 0, active_until = 0, active_tier = 0"
						" WHERE user_id = $1 AND dungeon_key_id = $2;",
						identity.userId, static_cast<int32_t>(key.id));
				}
				catch (const drogon::orm::DrogonDbException& ex)
				{
					LOG_WARN << "DungeonKey: expiry write failed: " << ex.base().what();
				}
			}
			else if (activeTier != 0)
			{
				row.cnt = static_cast<uint32_t>(activeUntil - nowSec);
			}
			else
			{
				row.cnt = 0;
			}
		}

		// Derived, never stored — see the migration comment for why.
		const bool claimable =
			distributedOn(key, day.isoWeekday)
			&& static_cast<int64_t>(row.last_receipt_day) != day.epochDay
			&& static_cast<int32_t>(row.possession) < key.possession_limit;

		row.receipt_possible_flg = claimable ? 1u : 0u;
		row.next_receipt_possible_date =
			formatWireDate(nextReceiptDate(key, day, claimable));

		state.push_back(std::move(row));
	}

	co_return state;
}

} // namespace gme

namespace
{

/// Every dungeon-key handler answers with the same body.
std::string keyStateBody(const std::vector<::UserDungeonKeyInfo>& state)
{
	::DungeonKeyInfoResp resp{};
	resp.keys = state;
	// The key-ready badge.  BadgeInfo::setDungeonKeyNum is written ONLY by
	// BadgeInfoResponse, so a claim has to re-send the count or the badge stays
	// lit until the next login (BadgeKeyCount in net/badge_info.kdl).
	resp.badge.dungeon_key_num = gme::claimableKeyCount(state);
	return glz::write_json(resp).value_or("{}");
}

} // namespace

// GetDistributeDungeonKeyInfo (1mr9UsYz / r0ZA3pn5) — fired when the client
// opens the Administration Office.  Carries no fields of its own; the answer is
// entirely a function of the resolved user plus today's date.
HANDLEF(GetDistributeDungeonKeyInfo)
{
	LOG_INFO << "GetDistributeDungeonKeyInfo: " << json;

	::GetDistributeDungeonKeyInfoReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GetDistributeDungeonKeyInfo: parse error: " << glz::format_error(ec, json);
	}

	const auto db = theDb();
	const auto identity = (co_await gme::getUserIdentity(db, req.login_info)).nonEmpty();

	const auto state = co_await gme::dungeonKeyState(db, identity);
	co_return HandleResult::success(keyStateBody(state));
}

// DungeonKeyReceipt (WCJE0xe2 / V4pfQo5C) — "Receive Key" in the Akras
// Summoners' Hall Administration Office.
HANDLEF(DungeonKeyReceipt)
{
	LOG_INFO << "DungeonKeyReceipt: " << json;

	::DungeonKeyReceiptReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "DungeonKeyReceipt: parse error: " << glz::format_error(ec, json);
	}

	const auto db = theDb();
	const auto identity = (co_await gme::getUserIdentity(db, req.login_info)).nonEmpty();

	const auto requestedId = req.receipt.dungeon_key_id;
	LOG_INFO << "DungeonKeyReceipt: key " << requestedId << " (i1WQkh4G=" << req.receipt.unk << ")";

	const auto& keyMst = theServer()->cache().dungeonKeyMst();
	const auto key = std::find_if(
		keyMst.begin(),
		keyMst.end(),
		[requestedId](const ::DungeonKeyMst& mst) { return mst.id == requestedId; });

	if (key == keyMst.end())
	{
		// Unknown key id: answer with the unchanged inventory rather than an
		// error.  A closed session triggers the client retry loop (handbook §5).
		LOG_WARN << "DungeonKeyReceipt: no DungeonKeyMst row for id " << requestedId;
		co_return HandleResult::success(keyStateBody(co_await gme::dungeonKeyState(db, identity)));
	}

	auto state = co_await gme::dungeonKeyState(db, identity);
	auto row = std::find_if(
		state.begin(),
		state.end(),
		[requestedId](const ::UserDungeonKeyInfo& entry) { return entry.dungeon_key_id == requestedId; });

	// receipt_possible_flg was computed by dungeonKeyState against the calendar,
	// so re-deriving the eligibility rules here would be a second place to keep
	// them right.  Trust the flag.
	if (row == state.end() || row->receipt_possible_flg == 0)
	{
		LOG_INFO << "DungeonKeyReceipt: key " << requestedId << " not claimable today — no change";
		co_return HandleResult::success(keyStateBody(state));
	}

	const auto day = today();
	const auto granted = key->distribute_count.value_or(1);
	const auto limit = static_cast<uint32_t>(key->possession_limit);

	row->possession = std::min(row->possession + static_cast<uint32_t>(granted), limit);
	row->last_receipt_day = static_cast<uint32_t>(day.epochDay);
	row->receipt_possible_flg = 0;
	row->next_receipt_possible_date = formatWireDate(nextReceiptDate(*key, day, false));

	co_await db::PacketInterfaceFor<::UserDungeonKeyInfo>::update(
		db,
		"user_dungeon_keys",
		*row,
		{
			db::Lookup("user_id", identity.userId),
			db::Lookup("dungeon_key_id", requestedId),
		});

	LOG_INFO << "DungeonKeyReceipt: granted " << granted << " x " << key->name
	         << " — now holding " << row->possession << "/" << limit;

	co_return HandleResult::success(keyStateBody(state));
}

// DungeonKeyUse (aGT5S6qZ / 3rPx6tTw) — spend keys to open a parade tier.
HANDLEF(DungeonKeyUse)
{
	LOG_INFO << "DungeonKeyUse: " << json;

	::DungeonKeyUseReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "DungeonKeyUse: parse error: " << glz::format_error(ec, json);
	}

	const auto database = theDb();
	const auto identity = (co_await gme::getUserIdentity(database, req.login_info)).nonEmpty();
	// Read the balance, spend, and build the complete response on one transaction.
	// A state/serialization failure must not leave a partial purchase behind.
	auto db = co_await database->newTransactionCoro();
	try
	{
		const auto requestedId = req.use.dungeon_key_id;
		const auto useCount = req.use.use_count;
		LOG_INFO << "DungeonKeyUse: key " << requestedId << " x " << useCount;

		auto state = co_await gme::dungeonKeyState(db, identity);

		const auto& keyMst = theServer()->cache().dungeonKeyMst();
		const auto key = std::find_if(
			keyMst.begin(),
			keyMst.end(),
			[requestedId](const ::DungeonKeyMst& mst) { return mst.id == requestedId; });

		if (key == keyMst.end())
		{
			LOG_WARN << "DungeonKeyUse: no DungeonKeyMst row for id " << requestedId;
			co_return HandleResult::success(keyStateBody(state));
		}

		auto row = std::find_if(
			state.begin(),
			state.end(),
			[requestedId](const ::UserDungeonKeyInfo& entry) { return entry.dungeon_key_id == requestedId; });

		// The spend must match a real rung of the ladder, not just any number the
		// client sends — otherwise a crafted request could drain an arbitrary count
		// or set active_type to something the MST never offers.
		const auto tiers = gme::parseUsagePatternTiers(key->usage_pattern);
		const auto tier = std::find_if(
			tiers.begin(),
			tiers.end(),
			[useCount](const gme::UsagePatternTier& candidate) { return candidate.keysRequired == useCount; });

		if (row == state.end() || tier == tiers.end())
		{
			LOG_WARN << "DungeonKeyUse: " << useCount << " is not a tier of \"" << key->usage_pattern
			         << "\" — no change";
			co_return HandleResult::success(keyStateBody(state));
		}

		if (row->possession < static_cast<uint32_t>(tier->keysRequired))
		{
			LOG_INFO << "DungeonKeyUse: holding " << row->possession << ", tier needs "
			         << tier->keysRequired << " — no change";
			co_return HandleResult::success(keyStateBody(state));
		}

		row->possession -= static_cast<uint32_t>(tier->keysRequired);
		row->active_type = static_cast<uint32_t>(tier->activeType);

		// Open the parade for DungeonKeyMst.limit_sec (1800 = the 30 minutes the
		// live game gave you), stored as an absolute instant.  cnt goes out as the
		// remaining seconds; without it the client closes the parade immediately.
		const auto nowSec = static_cast<int64_t>(
			std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()).count());
		const int64_t activeUntil = nowSec + std::max<int32_t>(key->limit_sec, 0);
		row->cnt = static_cast<uint32_t>(activeUntil - nowSec);

		co_await db->execSqlCoro(
			"UPDATE user_dungeon_keys SET active_until = $1, active_tier = $2"
			" WHERE user_id = $3 AND dungeon_key_id = $4;",
			activeUntil, tier->keysRequired,
			identity.userId, static_cast<int32_t>(requestedId));

		co_await db::PacketInterfaceFor<::UserDungeonKeyInfo>::update(
			db,
			"user_dungeon_keys",
			*row,
			{
				db::Lookup("user_id", identity.userId),
				db::Lookup("dungeon_key_id", requestedId),
			});

		LOG_INFO << "DungeonKeyUse: spent " << tier->keysRequired << " x " << key->name
		         << " — active_type " << row->active_type << ", " << row->possession << " left";

		// A fresh replacement also works before UserInfo and drops expired tiers.
		::DungeonKeyUseResp resp{};
		resp.keys = state;
		std::string body;
		if (const auto error = glz::write_json(resp, body); error)
			throw std::runtime_error(glz::format_error(error, body));
		gme::injectPermitPlace(body, co_await gme::buildPermitPlace(db, identity));

		co_return HandleResult::success(body);
	}
	catch (...)
	{
		db->rollback();
		throw;
	}
}
