#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Grand Quest (the client's "Campaign") helpers shared by the Campaign*
// handlers.  user_campaign_missions doubles as the quest clear-history store
// (MissionEnd writes quest ids there for UT1SVg59), so everything here is
// scoped to the F_GRAND_MISSION_MST ids.

namespace gme
{

/*!
* Whether an id names a Grand Mission (F_GRAND_MISSION_MST).
*/
inline bool isGrandMission(const std::string& missionId)
{
	int32_t id = 0;
	try { id = std::stoi(missionId); }
	catch (const std::exception&) { return false; }

	const auto& mst = theServer()->cache().grandMissionMst();
	return std::any_of(mst.begin(), mst.end(),
		[id](const auto& m) { return m.mission_id == id; });
}

/*!
* EVERY Grand Mission as a 2I9V0o6J entry — state, best completion and the
* once-only rewards already earned (get_reward, JQ23rIvk) for the ones the
* user has played, zeros for the rest.
*
* The whole catalogue has to go out, not just the played rows.
* CampaignMissionSelectScene::setDungeonMstList @0x152E304 looks each Grand
* Mission up in CampaignMissionInfoList and SKIPS the quest tile when there is
* no row, so a player who has never played a Grand Quest was shown the series
* banners with nothing inside them and had no way in — which is exactly what
* an empty list did on 2026-09-12.
*
* mission_on_flg (JcKMjH64) is a comma-separated FLAG LIST, not a boolean:
* CampaignSceneBase::setMissionOnFlg @0x14D9F54 splits it on "," and feeds the
* parts to CampaignMissionEventInfo (the in-mission field quests).  Nothing
* here tracks those flags, so it stays empty rather than claiming flag "1".
*/
inline drogon::Task<std::vector<::CampaignMissionEntry>> loadCampaignMissions(
	const db::Database database,
	const UserIdentity identity)
{
	const auto rows = co_await database->execSqlCoro(
		"SELECT mission_id, attain_percent, state, rewards_got"
		" FROM user_campaign_missions WHERE user_id = $1;",
		identity.userId);

	struct Progress
	{
		int32_t attainPercent = 0;
		int32_t state = 0;
		std::string rewardsGot;
	};
	std::map<std::string, Progress> played;
	for (const auto& row : rows)
	{
		Progress progress{};
		progress.attainPercent = row["attain_percent"].as<int32_t>();
		progress.state = row["state"].as<int32_t>();
		progress.rewardsGot = row["rewards_got"].as<std::string>();
		played.emplace(row["mission_id"].as<std::string>(), std::move(progress));
	}

	std::vector<::CampaignMissionEntry> missions;
	for (const auto& mission : theServer()->cache().grandMissionMst())
	{
		::CampaignMissionEntry entry{};
		entry.mission_id = std::to_string(mission.mission_id);
		if (const auto it = played.find(entry.mission_id); it != played.end())
		{
			entry.attain_percent = it->second.attainPercent;
			entry.state = it->second.state;
			entry.get_reward = it->second.rewardsGot;
		}
		missions.push_back(std::move(entry));
	}
	std::sort(missions.begin(), missions.end(),
		[](const auto& lhs, const auto& rhs) { return lhs.mission_id < rhs.mission_id; });
	co_return missions;
}

/*!
* The Grand Quest item loadout is stored as the JSON of the list the client
* reads back (CampaignItemEntry: NjZ6ds1S / 5EByfWJ4).  An empty or unreadable
* column is an empty loadout.
*/
inline std::vector<::CampaignItemEntry> parseCampaignItems(const std::string& stored)
{
	std::vector<::CampaignItemEntry> items;
	if (stored.empty())
		return items;
	if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(items, stored); ec)
	{
		LOG_WARN << "Campaign: unreadable stored item loadout — treating it as empty";
		items.clear();
	}
	return items;
}

/*!
* Converts the client's item slots (CampaignItemReqEntry) to the stored /
* reply form, dropping empty slots.
*/
inline std::vector<::CampaignItemEntry> toCampaignItems(const std::vector<::CampaignItemReqEntry>& slots)
{
	std::vector<::CampaignItemEntry> items;
	for (const auto& slot : slots)
	{
		if (slot.item_id.empty() || slot.item_id == "0" || slot.item_num <= 0)
			continue;
		::CampaignItemEntry item{};
		item.item_id = slot.item_id;
		item.disp_order = slot.disp_order;
		item.item_num = slot.item_num;
		items.push_back(std::move(item));
	}
	return items;
}

/*!
* Makes sure the user's user_campaign_state row exists, so the run columns
* can be updated without an upsert at every call site.
*/
inline drogon::Task<void> ensureCampaignState(const db::Database database, const UserIdentity identity)
{
	co_await database->execSqlCoro(
		"INSERT OR IGNORE INTO user_campaign_state (user_id) VALUES ($1);",
		identity.userId);
}

/*!
* Records which Grand Mission is being played and opens the run.  Only
* DeckEdit, Save and End name the mission (2I9V0o6J); BattleStart and
* BattleEnd do not, so they read it back from here.
*/
inline drogon::Task<void> openCampaignRun(
	const db::Database database,
	const UserIdentity identity,
	const std::string& missionId)
{
	co_await ensureCampaignState(database, identity);
	if (isGrandMission(missionId))
	{
		co_await database->execSqlCoro(
			"UPDATE user_campaign_state SET active_mission_id = $1, run_open = 1 WHERE user_id = $2;",
			missionId, identity.userId);
	}
}

/*!
* The Grand Quest map routes, minus any edge that leads nowhere.
*
* F_GRAND_MISSION_ROUTE_MST ships one broken row: route 322 on mission 5000002
* runs from spot 20 to spot 24, and that mission's spot table stops at 23, so
* the destination does not exist.  CampaignFieldScene resolves a route's ends
* against CampaignMapPointMstList, so an edge to a missing spot is a null
* lookup on the one tap that takes it -- and spot 20 still has route 323 to 21,
* so dropping it strands nothing.
*
* Filtered here rather than edited into the MST: the table is shipped data, and
* this also guards the same class of drift in any future row.
*/
inline std::vector<::GrandMissionRouteMst> campaignRoutes()
{
	const auto& cache = theServer()->cache();

	std::map<int32_t, std::set<int32_t>> spots;
	for (const auto& spot : cache.grandMissionSpotMst())
		spots[spot.mission_id].insert(spot.point_num);

	std::vector<::GrandMissionRouteMst> routes;
	for (const auto& route : cache.grandMissionRouteMst())
	{
		const auto it = spots.find(route.mission_id);
		if (it != spots.end()
			&& (!it->second.contains(route.point_num_start)
				|| !it->second.contains(route.point_num_end)))
		{
			LOG_WARN << "Grand Quest route " << route.id << " on mission " << route.mission_id
				<< " joins " << route.point_num_start << " -> " << route.point_num_end
				<< ", which is not a spot on that map; dropped";
			continue;
		}
		routes.push_back(route);
	}
	return routes;
}

/*!
* Fills the ten Grand Quest master tables on any response that carries them.
*
* Templated on the response because two requests need them and their generated
* structs are unrelated types: CampaignMissionGet (the menu, and the one that
* MUST have them -- see CampaignMissionGetResp in net/handlers.kdl) and
* CampaignStart (the run).
*/
template <typename Resp>
inline void fillCampaignMst(Resp& resp)
{
	const auto& cache = theServer()->cache();
	resp.mission_mst      = cache.grandMissionMst();
	resp.map_mst          = cache.grandMissionMapMst();
	resp.spot_mst         = cache.grandMissionSpotMst();
	resp.route_mst        = campaignRoutes();
	resp.icon_mst         = cache.grandMissionIconMst();
	resp.treasure_mst     = cache.grandMissionTreasureMst();
	resp.flg_mst          = cache.grandMissionFlgMst();
	resp.end_cnd_mst      = cache.grandMissionEndCndMst();
	resp.event_mst        = cache.grandMissionEventMst();
	resp.reward_bonus_mst = cache.grandMissionRewardMst();
}

/*!
* Stores where each campaign deck stands on the Grand Quest map.
*
* The client owns the position while a run is on screen and reports it back in
* the `Yusr3Zg5` group of CampaignSave, CampaignBattleEnd and CampaignEnd
* (createBodySaveDataPos @0x13B4170 -- deck, spot, arrival order).  Nothing was
* reading it, so the party marker always came back at point 0 on the next
* CampaignStart.
*
* @param database Database client or transaction to use.
* @param identity Resolved user identity to write.
* @param decks    The deck rows exactly as the client sent them; an empty list
*                 is a no-op rather than a wipe, because the plain Continue
*                 bodies omit the group entirely.
*/
inline drogon::Task<void> storeCampaignDeckPos(
	const db::Database database,
	const UserIdentity identity,
	const std::vector<::CampaignDeckPosEntry>& decks)
{
	for (const auto& deck : decks)
	{
		co_await database->execSqlCoro(
			"INSERT INTO user_campaign_deck_pos (user_id, deck_num, now_point_num, arrival_order)"
			" VALUES ($1, $2, $3, $4)"
			" ON CONFLICT(user_id, deck_num) DO UPDATE SET"
			" now_point_num = $3, arrival_order = $4;",
			identity.userId,
			static_cast<int32_t>(deck.deck_num),
			static_cast<int32_t>(deck.now_point_num),
			static_cast<int32_t>(deck.arrival_order));
	}
}

/*!
* The stored deck positions as `Yusr3Zg5` rows, one per deck slot.
*
* Ten rows always go out (the panel renders a slot per deck) with the stored
* position where there is one and zeros otherwise -- a REPLACE list, so a short
* reply would silently drop the slots it left out.
*/
inline drogon::Task<std::vector<::CampaignStartMissionDeckEntry>> loadCampaignDeckPos(
	const db::Database database,
	const UserIdentity identity)
{
	constexpr int32_t kCampaignDeckSlots = 10;

	const auto rows = co_await database->execSqlCoro(
		"SELECT deck_num, now_point_num, arrival_order FROM user_campaign_deck_pos"
		" WHERE user_id = $1;",
		identity.userId);

	std::map<int32_t, std::pair<int32_t, int32_t>> stored;
	for (const auto& row : rows)
		stored[row["deck_num"].as<int32_t>()] =
			{ row["now_point_num"].as<int32_t>(), row["arrival_order"].as<int32_t>() };

	std::vector<::CampaignStartMissionDeckEntry> decks;
	for (int32_t slot = 0; slot < kCampaignDeckSlots; ++slot)
	{
		::CampaignStartMissionDeckEntry deck{};
		deck.deck_num = slot;
		if (const auto it = stored.find(slot); it != stored.end())
		{
			deck.now_point_num = it->second.first;
			deck.arrival_order = it->second.second;
		}
		decks.push_back(std::move(deck));
	}
	co_return decks;
}

} // namespace gme
