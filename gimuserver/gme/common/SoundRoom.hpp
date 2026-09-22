#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <deque>
#include <vector>

// The Music House — the town's jukebox, 108 BGM tracks bought with Zel.
//
// The purchase happens entirely inside the client.  MyTownSoundRoomScene::
// confirmAnswerYes @0x1902514 reads SoundMst::getAmount for the price, calls
// UserTeamInfo::getZel/setZel to pay it, builds a UserSoundInfo and addObject's
// it straight into UserSoundInfoList, records it through
// UserState::addBuySoundList, and queues a TownUpdate.  The server learns about
// it only as a comma list of sound ids under `LzKDI2i7` on that request.
//
// Validate and persist the reported purchases; the
// reason that matters is `d98mjNDc`: UserSoundInfoResponse::readParam
// @0x1401614 calls removeAllObjects at row 0 and addObject per row, a full
// replace.  This server modelled the block as a one-element wrapper and never
// filled it, so every launch replaced the player's jukebox with nothing —
// tracks they had paid Zel for were simply gone.
//
// THE ZEL IS CHARGED SERVER-SIDE, and that is not double-charging.  The
// client's decrement is local only; TownUpdateResp's own doc says the next
// UserInfo carries the authoritative Zel, which would hand it straight back.
// Persisting the track without charging would make every track after the first
// relaunch free.
//
// Readers of the list: MyTownSoundRoomScene (which tracks are playable, plus
// getCount for the list) and SandbagChangeSoundScene (picking battle BGM).

namespace gme
{

/*!
* Every Music House track a player owns, as `d98mjNDc` rows.
*
* A FULL REPLACE on the client, so this is the complete set every time. An
* empty result is correct for a player who has bought nothing — the block then
* goes out as `[]`, which never reaches readParam and leaves the client's own
* (also empty) list alone.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity.
* @return One row per owned track, in id order.
*/
inline drogon::Task<std::vector<::UserSoundInfo>> loadOwnedSounds(
	const db::Database database,
	const UserIdentity identity)
{
	std::vector<::UserSoundInfo> owned;

	const auto rows = co_await database->execSqlCoro(
		"SELECT sound_id FROM user_sounds WHERE user_id = $1 ORDER BY sound_id;",
		identity.userId);
	owned.reserve(rows.size());
	for (const auto& row : rows)
	{
		::UserSoundInfo info = {};
		info.user_id = identity.userId;
		info.sound_id = row["sound_id"].as<int32_t>();
		owned.push_back(std::move(info));
	}

	co_return owned;
}

/*!
* Records tracks the client reports having bought, charging each one's price.
*
* Ids the player already owns are skipped rather than charged twice — the
* client resends its whole `getBuySoundList` until the flush succeeds, and a
* retried TownUpdate must not bill again. An id that is not in SoundMst is
* refused outright rather than granted for free.
*
* Zel is charged for what the player can actually afford, cheapest first, so a
* batch that overshoots the balance still grants a sensible prefix instead of
* failing whole or going negative.
*
* Caller must hold a transaction covering the grant and payment. Exceptions
* propagate so the caller can roll back; do not report partial success on DB failure.
* @param database Caller-owned transaction.
* @param identity Resolved user identity.
* @param soundIds The `LzKDI2i7` list from a TownUpdate request.  A deque
*                 because that is what the generated comma-separated array is.
* @return How many tracks were newly granted.
*/
inline drogon::Task<int32_t> recordBoughtSounds(
	const db::Database database,
	const UserIdentity identity,
	const std::deque<int32_t> soundIds)
{
	if (soundIds.empty())
	{
		co_return 0;
	}

	int32_t granted = 0;
	const auto ownedRows = co_await database->execSqlCoro(
		"SELECT sound_id FROM user_sounds WHERE user_id = $1;", identity.userId);
	std::vector<int32_t> owned;
	for (const auto& row : ownedRows)
		owned.push_back(row["sound_id"].as<int32_t>());

	const auto& catalogue = theServer()->cache().initializeResp().sound;

	// Price each requested id, dropping the ones already owned or unknown.
	std::vector<std::pair<int32_t, int32_t>> toBuy;   // price, id
	for (const auto id : soundIds)
	{
		if (std::find(owned.begin(), owned.end(), id) != owned.end())
			continue;
		const auto track = std::find_if(catalogue.begin(), catalogue.end(),
			[id](const ::SoundMst& s) { return s.id == id; });
		if (track == catalogue.end() || track->amount < 0)
		{
			LOG_WARN << "recordBoughtSounds: " << identity.userId
				<< " claimed sound " << id << ", which is not in SoundMst; refused";
			continue;
		}
		toBuy.emplace_back(track->amount, id);
		owned.push_back(id);   // a duplicate inside one batch is still one purchase
	}
	if (toBuy.empty())
	{
		co_return 0;
	}
	std::sort(toBuy.begin(), toBuy.end());

	const auto balanceRows = co_await database->execSqlCoro(
		"SELECT zel FROM user_info WHERE id = $1;", identity.userId);
	auto zel = balanceRows.empty() ? int64_t{ 0 } : balanceRows[0]["zel"].as<int64_t>();

	for (const auto& [price, id] : toBuy)
	{
		if (zel < price)
		{
			LOG_WARN << "recordBoughtSounds: " << identity.userId << " reported buying sound "
				<< id << " for " << price << " Zel but holds " << zel << "; not granted";
			continue;
		}
		zel -= price;
		co_await database->execSqlCoro(
			"INSERT OR IGNORE INTO user_sounds (user_id, sound_id) VALUES ($1, $2);",
			identity.userId, id);
		++granted;
	}

	if (granted > 0)
	{
		co_await database->execSqlCoro(
			"UPDATE user_info SET zel = $1 WHERE id = $2;", zel, identity.userId);
		LOG_INFO << "recordBoughtSounds: " << identity.userId << " bought " << granted
			<< " Music House track(s); " << zel << " Zel left";
	}

	co_return granted;
}

} // namespace gme
