#include "App.hpp"
#include "ServerCache.hpp"
#include "ServerCacheMst.hpp"

#include <gimuserver/utils/BfCrypt.hpp>
#include <gimuserver/utils/JsonFile.hpp>

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <vector>

// Vortex topology bounds, used to collect m_vortexPermits at boot.
//
// Land 99 is the catch-all "special content" land shared by Vortex, Frontier
// Hunter (area 1000000+), Frontier Gate (3000000+) and Grand Quest (5000000+);
// the Vortex block itself is the low area range.  Verified against
// deploy/mst/area_mst.json: 88 land-99 areas sit below 200000 (100000
// "Gathering of Souls" .. 101800 "In the Name of Thunder") and the next one up
// is 700000 "A Dark Ritual", so the cut is wide.
static constexpr int32_t kVortexLandId = 99;
static constexpr int32_t kVortexAreaIdMax = 200000;

// Above this, an id belongs to event/special content rather than the Grand
// Gaia quest line.  Same floor UserInfo's PermitPlace uses for its dense
// ranges; see kSpecialIdFloor there.
static constexpr int32_t kQuestSpecialIdFloor = 100000;

// Parade Garden, and the display_order that floats it to the top of the Vortex
// tile list.
//
// It is where the evolution, sale and enhancing units come from, so it is the
// tile a player wants first, and its shipped order (100) buries it at the
// bottom.
//
// ⚠ THE VORTEX LIST IS SORTED DESCENDING — HIGHEST display_order FIRST.
// The tile order comes from AreaSPSelectScene::setDungeonList, which builds its
// OWN pair<dispOrder, AreaMst*> vector and sorts it at 0x183F2D4 with
// std::__sort<std::greater<...>>.  Do NOT reach for AreaMstList::getActiveList
// @0x1309EDC to answer this: that one sorts with std::__less and is ASCENDING,
// it is a different consumer, and trusting it put this constant at 1 and Parade
// Garden dead last.  99999 is above every shipped land-99 row (the highest is
// 9800, "Year-End Dungeon").
static constexpr int32_t kParadeGardenAreaId = 100600;
static constexpr int32_t kParadeGardenDispOrder = 99999;

// Where the CDN serves area/dungeon banner art from.  Relative because main()
// chdirs into the config file's directory before Drogon starts, and every other
// path in config.json (document_root, mst_root, archive_root) is relative the
// same way.
static constexpr std::string_view kBannerDir = "./game_content/content/dungeon/";

/*!
* An English banner to use in place of a Japanese one, when the set ships both.
*
* Three tiles reach the player with Japanese burnt into the artwork even though
* their MST names are English — "Mysterious Paradise" draws 神秘の楽園,
* "Ultimate Paradise" draws 究極の楽園, "Heavenly Paradise" draws 天上の楽園.
* The global build shipped translated art for exactly these three next to the
* originals (sp_quest_banner_frog{,3,_crystal}_EN.png), so the fix is to point
* at it rather than to drop three of the frog dungeons the player wants.
*
* Probed on disk rather than listed, so art added later is picked up.
*
* @param banner Banner filename from the MST.
* @return The _EN filename when one exists, otherwise banner unchanged.
*/
static std::string LocalisedBanner(const std::string& banner)
{
	if (!banner.ends_with(".png"))
		return banner;

	auto localised = banner.substr(0, banner.size() - 4) + "_EN.png";
	std::error_code ec;
	if (std::filesystem::exists(std::string(kBannerDir) + localised, ec))
		return localised;

	return banner;
}

/*!
* Whether a name is displayable text rather than Japanese or mojibake.
*
* Two shapes have to go.  Some rows are honestly Japanese ("天上の楽園",
* "禁断の石版"); others are Shift-JIS that was decoded as Latin somewhere
* upstream and reached the MST as literal question marks ("?X?g???C???C?Y").
* Neither renders as anything a player can read, and the localisation keys we
* DO want ("MST_DUNGEONS_DUNGEON_100600_NAME") are plain ASCII, so a
* non-ASCII byte or a run of question marks is enough to tell them apart.
*/
static bool IsDisplayableName(std::string_view name)
{
	int questionRun = 0;
	for (const auto ch : name)
	{
		if (static_cast<unsigned char>(ch) > 0x7F)
			return false;
		questionRun = (ch == '?') ? questionRun + 1 : 0;
		if (questionRun >= 3)
			return false;
	}

	return true;
}

/*!
* Weekday mask for a Vortex dungeon, from its banner filename.
*
* Bit 0 = Monday .. bit 6 = Sunday; 0 means "no weekday token", i.e. the
* dungeon is not part of the rotation and stays permanently open.
*
* The banner is the ONLY day signal the shipped data carries — DungeonMst has
* no day column.  It arrives in the field the KDL calls `room_asset`
* (hash 21VKZo0E), which is a misnomer: every Vortex row holds an
* `sp_quest_banner_*.png`, not a room asset.  Left alone rather than renamed
* because renaming means regenerating all.hpp through the submodule.
*
* Real values are `sp_quest_banner_monday.png`, `..._monday2.png`,
* `..._thursday2.png`, `..._weekend2.png` and friends, so the day token is
* matched as a prefix and any trailing revision digit ignored.  Verified
* against deploy/mst/dungeon_mst.json: of the 110 Vortex dungeons exactly 7
* match, and no non-weekday banner begins with a weekday token.
*/
static uint8_t VortexDayMaskFromBanner(std::string_view banner)
{
	static constexpr std::string_view kPrefix = "sp_quest_banner_";
	if (!banner.starts_with(kPrefix))
		return 0;
	banner.remove_prefix(kPrefix.size());

	struct DayToken { std::string_view token; uint8_t mask; };
	static constexpr DayToken kDays[] = {
		{ "monday",    1 << 0 },
		{ "tuesday",   1 << 1 },
		{ "wednesday", 1 << 2 },
		{ "thursday",  1 << 3 },
		{ "friday",    1 << 4 },
		{ "saturday",  1 << 5 },
		{ "sunday",    1 << 6 },
		// The shipped data has no saturday/sunday banners; the weekend is a
		// single dungeon carrying this token.
		{ "weekend",   (1 << 5) | (1 << 6) },
	};

	for (const auto& [token, mask] : kDays)
		if (banner.starts_with(token))
			return mask;

	return 0;
}

/*!
* Builds a JSON
* @param[in] d Template class to write
* @return Parsed json string
*/
template <typename T>
static std::string BuildJson(const T& d)
{
	std::string buffer{};
	const auto& ec = glz::write_json(d, buffer);
	if (ec)
	{
		throw std::runtime_error("Cannot build a cache JSON, error:\n{}" + glz::format_error(ec, buffer));
	}

	return buffer;
}

void ServerCache::Setup(const Json::Value& serverObj)
{
	const auto& mstRoot = serverObj["mst_root"].asString();
	// Not every file the server loads at boot is a decoded MST.  features/
	// brave_slots/notice_info are server config and response fixtures, so they
	// live in system_root while mst_root holds only reference tables.
	const auto& systemRoot = serverObj.get("system_root", "./system").asString();

	// fps_cap is the client-side render cap delivered to the offline-proxy
	// libcurl shim via the /offline_mod/fps_cap endpoint. Default 60 (matches
	// the proxy's compile-time default); 0 disables the cap entirely.
	m_serverConfig.fpsCap = serverObj.get("fps_cap", 60u).asUInt();

	// See ServerConfig::vortexWeekendOpensAll — off by default because the
	// shipped MST gives the weekend its own dungeon rather than opening the
	// weekday ones.
	m_serverConfig.vortexWeekendOpensAll = serverObj.get("vortex_weekend_opens_all", false).asBool();

	{
		GameDls dls{};
		dls.game_ip = GetDrogonBindHostname();
		dls.resource_ip = dls.game_ip;
		dls.version = serverObj["game_version"].asUInt();
		dls.gumilive_ip = dls.game_ip + "/";
		dls.bg_image = serverObj["wallpaper_banner"].asString();

		const auto dlsJson = BuildJson(dls);
		const auto sree = BfCrypt::BuildSREE(dlsJson);

		// build SREE crypted JSON rather than constructing it everytime in the dls controller
		if (sree.has_value())
		{
			m_dls = BuildJson(sree.value());
		}
		else
		{
			throw std::runtime_error("Cannot encrypt SREE cache json");
		}
	}

	m_feature = LoadJson<FeatureCheck>(systemRoot, "features.json");
	m_controlCenterRsp = LoadJson<SlotGameInfoR>(systemRoot, "brave_slots.json");

	{
		// Cache: Initialize response
		m_initrsp.login_campagin = LoadJson<LoginCampaignMst>(mstRoot, "login_campaign_mst.json");
		m_initrsp.login_campaign_reward = LoadJson<LoginCampaignRewardCache>(mstRoot, "login_campaign_reward_mst.json").data;
		m_initrsp.progression = LoadJson<UserLevelMstCache>(mstRoot, "user_level_mst.json").data;
		m_initrsp.mst = LoadJson<VersionInfoCache>(mstRoot, "version_info_mst.json").data;
		m_initrsp.town_facility = LoadJson<TownFacilityMstCache>(mstRoot, "town_facility_mst.json").data;
		m_initrsp.town_facility_lv = LoadJson<TownFacilityLvMstCache>(mstRoot, "town_facility_lv_mst.json").data;
		m_initrsp.town_location = LoadJson<TownLocationMstCache>(mstRoot, "town_location_mst.json").data;
		m_initrsp.town_location_lv = LoadJson<TownLocationLvMstCache>(mstRoot, "town_location_lv_mst.json").data;
		m_initrsp.dungeon_keys = LoadJson<DungeonKeyMstCache>(mstRoot, "dungeon_key_mst.json").data;
		m_initrsp.arena_ranks = LoadJson<ArenaRankMstCache>(mstRoot, "arena_rank_mst.json").data;
		m_initrsp.gacha_effects = LoadJson<GachaEffectMstCache>(mstRoot, "gacha_effect_mst.json").data;
		m_initrsp.gachas = LoadJson<GachaMstCache>(mstRoot, "gacha_mst.json").data;
		m_initrsp.npcs = LoadJson<NpcMstCache>(mstRoot, "npc_mst.json").data;
		m_initrsp.banner_info = LoadJson <BannerInfoMstCache>(mstRoot, "banner_info_mst.json").data;
		m_initrsp.extra_passive_skills = LoadJson<ExtraPassiveSkillMstCache>(mstRoot, "extra_passive_skill_mst.json").data;
		m_initrsp.notice_info = LoadJson<NoticeInfo>(systemRoot, "notice_info.json");
		m_initrsp.defines = LoadJson<DefineMst>(mstRoot, "defines_mst.json");
		m_initrsp.video_ad_slots = LoadJson<VideoAdsSlotGameInfo>(mstRoot, "video_ad_slot_game_info_mst.json");
		m_initrsp.exp_pattern = LoadJson<UnitExpPatternMstCache>(mstRoot, "unit_exp_pattern_mst.json").data;
		m_initrsp.receipe = LoadJson<ReceipeMstCache>(mstRoot, "recipe_mst.json").data;
		m_initrsp.trophy = LoadJson<TrophyMstCache>(mstRoot, "trophy_mst.json").data;
		m_initrsp.trophy_group = LoadJson<TrophyGroupMstCache>(mstRoot, "trophy_group_mst.json").data;
		m_initrsp.trophy_grade = LoadJson<TrophyGradeMstCache>(mstRoot, "trophy_grade_mst.json").data;
		m_initrsp.information = LoadJson<InformationMstCache>(mstRoot, "information_mst.json").data;
		m_initrsp.help = LoadJson<HelpMstCache>(mstRoot, "help_mst.json").data;
		m_initrsp.help_sub = LoadJson<HelpSubMstCache>(mstRoot, "help_sub_mst.json").data;
		m_initrsp.url = LoadJson<UrlMstCache>(mstRoot, "url_mst.json").data;
		m_initrsp.challenge = LoadJson<ChallengeMstCache>(mstRoot, "challenge_mst.json").data;

		// Keep one Frontier Hunter event running.
		//
		// All 97 rows are real 2014-2022 event windows and every one of them is
		// long over against a 2026 clock — the newest, id 97, ended
		// 2022-04-27.  With no live event the Survey Office has nothing to show
		// and, on the working theory, no Hunter Orb allowance either (the orbs
		// are Frontier Hunter's currency, which Frontier Gate shares — see the
		// in-game intro at content/event/randall_FG.txt).
		//
		// Same call as the Frontier Gate windows: this is an offline
		// preservation server with no event calendar, so the newest event is
		// advertised as permanently running rather than the catalog being left
		// entirely in the past.  Only the LAST row is touched; the other 96 keep
		// their historical windows so the event history stays intact.
		//
		// To go back to authentic windows, delete this block — nothing else
		// depends on it.
		if (!m_initrsp.challenge.empty())
		{
			auto& active = m_initrsp.challenge.back();
			const auto opened = std::chrono::time_point_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::from_time_t(1420070400));   // 2015-01-01
			const auto closes = std::chrono::time_point_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::from_time_t(2145916800));   // 2038-01-01

			active.start_data = opened;
			active.end_data = closes;
			active.cnt_start_data = opened;
			active.cnt_end_data = closes;
			active.rank_start_data = opened;
			active.rank_end_data = closes;
			m_activeChallengeId = active.id;

			LOG_INFO << "ServerCache: Frontier Hunter event " << active.id
			         << " advertised as running (all 97 rows expired by 2022)";
		}
		m_initrsp.challenge_hr = LoadJson<ChallengeHrMstCache>(mstRoot, "challenge_hr_mst.json").data;
		m_initrsp.challenge_mis = LoadJson<ChallengeMisMstCache>(mstRoot, "chlng_mission_mst.json").data;
		m_initrsp.challenge_grade = LoadJson<ChallengeGradeMstCache>(mstRoot, "chlng_mission_grade_mst.json").data;
		m_initrsp.challenge_reward = LoadJson<ChallengeRewardMstCache>(mstRoot, "chlng_mission_reward_mst.json").data;
		m_initrsp.challenge_item = LoadJson<ChallengeItemMstCache>(mstRoot, "chlng_mission_item_set_mst.json").data;
		m_initrsp.challenge_rank_reward = LoadJson<ChallengeRankRewardMstCache>(mstRoot, "challenge_rank_reward_mst.json").data;
		m_initrsp.challenge_mvp = LoadJson<ChallengeMvpMstCache>(mstRoot, "challenge_mvp_mst.json").data;
		m_initrsp.interactive_banner = LoadJson<InteractiveBannerInfoMstCache>(mstRoot, "interactive_banner_info_mst.json").data;
		m_initrsp.sound = LoadJson<SoundMstCache>(mstRoot, "sound_mst.json").data;
		
		// cache: UserInfo response
		m_userrsp.notice_info = m_initrsp.notice_info;
		m_userrsp.video_ad_region = LoadJson<VideoAdRegionCache>(mstRoot, "video_ad_region_mst.json").data;
		m_userrsp.video_ad_info = LoadJson<VideoAdInfoCache>(mstRoot, "video_ad_info_mst.json").data;
		m_userrsp.excluded_dungeon_missions = LoadJson<ExcludedDungeonMissionMstCache>(mstRoot, "excluded_dungeon_mission_mst.json").data;
		m_userrsp.gift = LoadJson<GiftItemMstCache>(mstRoot, "gift_item_mst.json").data;
		m_userrsp.general_event = LoadJson<GeneralEventMstCache>(mstRoot, "general_event_mst.json").data;
		m_userrsp.first_desc = LoadJson<FirstDescMstCache>(mstRoot, "first_desc_mst.json").data;
		m_userrsp.summon_ticket_v2 = LoadJson<SummonTicketV2MstCache>(mstRoot, "summon_ticket_v2_mst.json").data;
		m_userrsp.resummon_gacha = LoadJson<ResummonGachaMstCache>(mstRoot, "resummon_gacha_mst.json").data;

		m_unitMst = LoadJson<UnitMstCache>(mstRoot, "unit_mst.json").data;
		m_itemMst = LoadJson<ItemMstCache>(mstRoot, "item_mst.json").data;

		// Grand Mission ("Campaign") master data — wrapper keys and field
		// maps documented in mst/grand_mission.kdl.
		m_grandMissionMst = LoadJson<GrandMissionMstCache>(mstRoot, "grand_mission_mst.json").data;
		m_grandMissionMapMst = LoadJson<GrandMissionMapMstCache>(mstRoot, "grand_mission_map_mst.json").data;
		m_grandMissionSpotMst = LoadJson<GrandMissionSpotMstCache>(mstRoot, "grand_mission_spot_mst.json").data;
		m_grandMissionRouteMst = LoadJson<GrandMissionRouteMstCache>(mstRoot, "grand_mission_route_mst.json").data;
		m_grandMissionIconMst = LoadJson<GrandMissionIconMstCache>(mstRoot, "grand_mission_icon_mst.json").data;
		m_grandMissionTreasureMst = LoadJson<GrandMissionTreasureMstCache>(mstRoot, "grand_mission_treasure_mst.json").data;
		m_grandMissionFlgMst = LoadJson<GrandMissionFlgMstCache>(mstRoot, "grand_mission_flg_mst.json").data;
		m_grandMissionEndCndMst = LoadJson<GrandMissionEndCndMstCache>(mstRoot, "grand_mission_end_cnd_mst.json").data;
		m_grandMissionEventMst = LoadJson<GrandMissionEventMstCache>(mstRoot, "grand_mission_event_mst.json").data;
		m_grandMissionRewardMst = LoadJson<GrandMissionRewardMstCache>(mstRoot, "grand_mission_reward_mst.json").data;

		// Raid Battle master data — wrapper keys and field maps documented
		// in mst/raid.kdl (BOSS_PARTS has no response class; not loaded).
		m_raidWorldMst = LoadJson<RaidWorldMstCache>(mstRoot, "raid_world_mst.json").data;
		m_raidMapMst = LoadJson<RaidMapMstCache>(mstRoot, "raid_map_mst.json").data;
		m_raidRcMst = LoadJson<RaidRcMstCache>(mstRoot, "raid_rc_mst.json").data;
		m_raidPlaystyleMst = LoadJson<RaidPlaystyleMstCache>(mstRoot, "raid_playstyle_mst.json").data;
		m_raidBossMst = LoadJson<RaidBossMstCache>(mstRoot, "raid_boss_mst.json").data;
		m_raidBossRouteMst = LoadJson<RaidBossRouteMstCache>(mstRoot, "raid_boss_route_mst.json").data;
		m_raidMissionMst = LoadJson<RaidMissionMstCache>(mstRoot, "raid_mission_mst.json").data;
		m_raidMissionBossMst = LoadJson<RaidMissionBossMstCache>(mstRoot, "raid_mission_boss_mst.json").data;
		m_raidMissionClearCndMst = LoadJson<RaidMissionClearCndMstCache>(mstRoot, "raid_mission_clear_cnd_mst.json").data;
		m_raidMissionPointMst = LoadJson<RaidMissionPointMstCache>(mstRoot, "raid_mission_point_mst.json").data;
		m_raidPointMst = LoadJson<RaidPointMstCache>(mstRoot, "raid_point_mst.json").data;
		m_raidUserRouteMst = LoadJson<RaidUserRouteMstCache>(mstRoot, "raid_user_route_mst.json").data;
		m_raidBattleGroupMst = LoadJson<RaidBattleGroupMstCache>(mstRoot, "raid_battle_group_mst.json").data;
		// Colosseum (PvP arena) master data — wrapper keys and field maps
		// documented in mst/colosseum.kdl.
		m_colosseumClassMst = LoadJson<ColosseumClassMstCache>(mstRoot, "colosseum_class_mst.json").data;
		m_colosseumExtraRuleMst = LoadJson<ColosseumExtraRuleMstCache>(mstRoot, "colosseum_extra_rule_mst.json").data;
		m_colosseumFormationMst = LoadJson<ColosseumFormationMstCache>(mstRoot, "colosseum_formation_mst.json").data;
		m_colosseumSupportMst = LoadJson<ColosseumSupportMstCache>(mstRoot, "colosseum_support_mst.json").data;

		// Dual Brave Burst master data — see mst/dbb.kdl.
		m_dbbMst = LoadJson<DbbMstCache>(mstRoot, "dbb_mst.json").data;
		m_dbbBondRecipeMst = LoadJson<DbbBondRecipeMstCache>(mstRoot, "dbb_bond_recipe_mst.json").data;

		// Guild master data — see mst/guild.kdl (ART and SKILL_DETAILS remain
		// unported; their response classes are ambiguous).
		m_guildInfoMst = LoadJson<GuildInfoMstCache>(mstRoot, "guild_info_mst.json").data;
		m_guildMemberSkillMst = LoadJson<GuildMemberSkillMstCache>(mstRoot, "guild_member_skill_mst.json").data;
		m_guildPointExchangeMst = LoadJson<GuildPointExchangeMstCache>(mstRoot, "guild_point_exchange_mst.json").data;

		// Purchase age-band spend caps — see mst/purchase.kdl.
		m_purchaseAgeLimitMst = LoadJson<PurchaseAgeLimitMstCache>(mstRoot, "purchase_age_limit_mst.json").data;

		// Frontier Gate master data — see mst/frontier_gate.kdl (REWARD and
		// AREA remain unported; no response class in the export).
		m_frontierGateMst = LoadJson<FrontierGateMstCache>(mstRoot, "frontier_gate_mst.json").data;
		m_frontierGateSupportMst = LoadJson<FrontierGateSupportMstCache>(mstRoot, "frontier_gate_support_mst.json").data;

		// Build the dungeon -> missions index, then drop the rows.  Only the id
		// and dungeon_id columns survive; see missionsByDungeon() for why this
		// is not the mission_mst cache that c49782e removed.
		{
			const auto missions = LoadJson<MissionMstCache>(mstRoot, "mission_mst.json").data;
			for (const auto& mission : missions)
			{
				m_missionsByDungeon[mission.dungeon_id].push_back(mission.id);

				// First-clear rewards, kept for the 787 rows that have any.
				// Two ints and a short string wide -- the same reasoning as the
				// indexes below, rather than reviving the whole row cache.
				if (!mission.clear_rewards.empty())
					m_missionClearRewards.emplace(mission.id, mission.clear_rewards);

				// Mission -> its quest dungeon, for the clear Gem (see
				// missionQuestDungeon()).  Grand Gaia only: land 99 is the
				// special-content catch-all (Vortex, Frontier Gate, Frontier
				// Hunter, Grand Quest) and none of those are the "Quest map"
				// the reward belongs to.  The id floor keeps event and collab
				// numbering out for the same reason.
				if (mission.land_id > 0 && mission.land_id < kVortexLandId
					&& mission.area_id > 0 && mission.area_id < kQuestSpecialIdFloor
					&& mission.dungeon_id > 0 && mission.dungeon_id < kQuestSpecialIdFloor
					&& mission.id > 0 && mission.id < kQuestSpecialIdFloor)
				{
					m_missionQuestDungeon.emplace(mission.id, mission.dungeon_id);
				}

				// Prerequisites, for PermitPlace's progression gate.  Kept as
				// its own index for the same reason as the one above: the
				// rows themselves are dropped, and this is two ints wide.
				std::vector<int32_t> needs;
				for (const auto need : mission.need_mission_id)
					if (need != 0)
						needs.push_back(need);
				if (!needs.empty())
					m_missionNeeds.emplace(mission.id, std::move(needs));
			}
			for (auto& [dungeonId, ids] : m_missionsByDungeon)
				std::sort(ids.begin(), ids.end());
			LOG_INFO << "ServerCache: indexed " << missions.size() << " missions across "
			         << m_missionsByDungeon.size() << " dungeons";

			{
				std::set<int32_t> gemDungeons;
				for (const auto& [missionId, dungeonId] : m_missionQuestDungeon)
					gemDungeons.insert(dungeonId);
				LOG_INFO << "ServerCache: " << gemDungeons.size()
				         << " quest dungeon(s) pay a clear Gem, covering "
				         << m_missionQuestDungeon.size() << " mission(s)";
			}

			// Everything PermitPlace must allow for Frontier Gate to be
			// enterable.  UserInfo's dense ranges cover the Grand Gaia
			// numbering space only, and Frontier Gate lives entirely outside
			// it: gate 91's mission 9010001 sits in land 99, area 3000001,
			// dungeon 9000002 — none of which fall in lands 1-2, areas 1-1000,
			// missions 1-4000 or dungeons 1-2000.
			//
			// Verified 2026-08-07: permitting the gates' dungeons alone was not
			// enough; the mission loaded its assets and then crashed because
			// its parent topology was unreachable.  That is the same failure
			// the area note in UserInfo.cpp describes, one level deeper.
			//
			// Collected per gate rather than as a blanket widening so the
			// permit list grows by the ~100 ids Frontier Gate actually needs
			// instead of thousands.
			std::unordered_map<int32_t, const MissionMst*> missionById;
			missionById.reserve(missions.size());
			for (const auto& mission : missions)
				missionById.emplace(mission.id, &mission);

			for (const auto& gate : m_frontierGateMst)
			{
				if (gate.dungeon_id != 0)
					m_frontierGatePermits.dungeons.insert(gate.dungeon_id);
				if (gate.need_mission_id != 0)
					m_frontierGatePermits.missions.insert(gate.need_mission_id);

				// Guard the 0 case as well as the miss: MissionMst's sentinel
				// row is id 0 in dungeon 0, so a gate with no dungeon would
				// otherwise pull that sentinel into the permit list.
				if (gate.dungeon_id == 0)
					continue;

				const auto it = m_missionsByDungeon.find(gate.dungeon_id);
				if (it == m_missionsByDungeon.end())
					continue;

				for (const auto missionId : it->second)
				{
					m_frontierGatePermits.missions.insert(missionId);
					const auto mit = missionById.find(missionId);
					if (mit == missionById.end())
						continue;
					if (mit->second->area_id != 0)
						m_frontierGatePermits.areas.insert(mit->second->area_id);
					if (mit->second->land_id != 0)
						m_frontierGatePermits.lands.insert(mit->second->land_id);
				}
			}

			LOG_INFO << "ServerCache: Frontier Gate permits — "
			         << m_frontierGatePermits.lands.size() << " land(s), "
			         << m_frontierGatePermits.areas.size() << " area(s), "
			         << m_frontierGatePermits.dungeons.size() << " dungeon(s), "
			         << m_frontierGatePermits.missions.size() << " mission(s)";
		}

		// Summoner Unit master data — see mst/summoner.kdl (ARM_LEVEL and
		// ARM_PASSIVE remain unported; no response class in the export).
		m_summonerAbilityMst = LoadJson<SummonerAbilityMstCache>(mstRoot, "summoner_ability_mst.json").data;
		m_summonerAbilityLevelMst = LoadJson<SummonerAbilityLevelMstCache>(mstRoot, "summoner_ability_level_mst.json").data;
		m_summonerAbilityUpMst = LoadJson<SummonerAbilityUpMstCache>(mstRoot, "summoner_ability_up_mst.json").data;
		m_summonerArmMst = LoadJson<SummonerArmMstCache>(mstRoot, "summoner_arm_mst.json").data;
		m_summonerArmElementMst = LoadJson<SummonerArmElementMstCache>(mstRoot, "summoner_arm_element_mst.json").data;
		m_summonerElementLevelMst = LoadJson<SummonerElementLevelMstCache>(mstRoot, "summoner_element_level_mst.json").data;
		m_summonerExSkillMst = LoadJson<SummonerExSkillMstCache>(mstRoot, "summoner_ex_skill_mst.json").data;
		m_summonerImageMst = LoadJson<SummonerImageMstCache>(mstRoot, "summoner_image_mst.json").data;
		m_summonerLevelMst = LoadJson<SummonerLevelMstCache>(mstRoot, "summoner_level_mst.json").data;

		// Skill tables — SkillMst's schema already lived in mst/skill.kdl;
		// the sidecars are in mst/skill_ext.kdl.
		m_skillMst = LoadJson<SkillMstCache>(mstRoot, "skill_mst.json").data;
		m_skillLevelMst = LoadJson<SkillLevelMstCache>(mstRoot, "skill_level_mst.json").data;
		m_leaderSkillMst = LoadJson<LeaderSkillMstCache>(mstRoot, "leader_skill_mst.json").data;

		// Unit evolution — see mst/unit_evo.kdl.
		m_unitEvoMst = LoadJson<UnitEvoMstCache>(mstRoot, "unit_evo_mst.json").data;
		m_unitEvoOmniMst = LoadJson<UnitEvoOmniMstCache>(mstRoot, "unit_evo_omni_mst.json").data;
		m_unitEvoOmniTypeMst = LoadJson<UnitEvoOmniTypeMstCache>(mstRoot, "unit_evo_omni_type_mst.json").data;
		m_unitEvoOmniRecipeMst = LoadJson<UnitEvoOmniRecipeMstCache>(mstRoot, "unit_evo_omni_recipe_mst.json").data;

		// Unit side-tables — see mst/unit_ext.kdl (MissionEp3Mst's schema
		// already lived in mst/mission_ep3.kdl).
		m_unitTypeMst = LoadJson<UnitTypeMstCache>(mstRoot, "unit_type_mst.json").data;
		m_unitExtMst = LoadJson<UnitExtMstCache>(mstRoot, "unit_ext_mst.json").data;
		m_unitEp3Mst = LoadJson<UnitEp3MstCache>(mstRoot, "unit_ep3_mst.json").data;
		m_unitCgsMst = LoadJson<UnitCgsMstCache>(mstRoot, "unit_cgs_mst.json").data;
		m_unitCommentMst = LoadJson<UnitCommentMstCache>(mstRoot, "unit_comment_mst.json").data;
		m_unitFeSkillMst = LoadJson<UnitFeSkillMstCache>(mstRoot, "unit_fe_skill_mst.json").data;
		m_unitFeCategoryMst = LoadJson<UnitFeCategoryMstCache>(mstRoot, "unit_fe_category_mst.json").data;
		m_missionEp3Mst = LoadJson<MissionEp3MstCache>(mstRoot, "mission_ep3_mst.json").data;

		// World geography — see mst/area.kdl.
		m_gateMst = LoadJson<GateMstCache>(mstRoot, "gate_mst.json").data;
		m_areaMst = LoadJson<AreaMstCache>(mstRoot, "area_mst.json").data;
		m_dungeonMst = LoadJson<DungeonMstCache>(mstRoot, "dungeon_mst.json").data;

		// Which of the 88 Vortex areas are worth a tile.  Three ways to lose
		// one, all of them things the player sees as clutter:
		//
		//  - nothing behind it.  Ten areas have no dungeon at all, because
		//    this data version consolidated the weekday dungeons under area
		//    100001 "Enhancement" and left the old per-day area shells
		//    ("Garden of God", "Cave of Greed", "Souls Training Ground", ...)
		//    empty.  They still draw, and open onto nothing.
		//  - Japanese or mojibake naming.  34 areas, mostly the untranslated
		//    collab dungeons (Tales of, FFBE, Guilty Gear, Megami Tensei) plus
		//    the 至高の楽園 run.
		//  - the same tile again.  13 areas repeat a tile the player already
		//    has: "Heavenly Paradise" is six areas, "Ultimate Paradise" four,
		//    "Year-End Dungeon" three, and "Winter Paradise" arrives three
		//    times over as Travellers / Vortex Trials / Seasons Past.
		//
		// Duplicates are matched on the BANNER SET rather than the area name,
		// because the Winter Paradise trio carries three different area names
		// and plate images over one identical dungeon.  The lowest
		// display_order in each group is the one kept, so the survivor is
		// wherever the player already expected the tile to be.
		std::map<std::vector<std::string>, int32_t> bannersSeen;
		{
			std::map<int32_t, std::vector<const DungeonMst*>> vortexDungeons;
			for (const auto& dungeon : m_dungeonMst)
				if (dungeon.land_id == kVortexLandId && dungeon.area_id < kVortexAreaIdMax)
					vortexDungeons[dungeon.area_id].push_back(&dungeon);

			std::vector<const AreaMst*> vortexAreas;
			for (const auto& area : m_areaMst)
				if (area.land_id == kVortexLandId && area.area_id < kVortexAreaIdMax)
					vortexAreas.push_back(&area);

			std::sort(vortexAreas.begin(), vortexAreas.end(),
				[](const AreaMst* lhs, const AreaMst* rhs) {
					return std::tie(lhs->display_order, lhs->area_id)
					     < std::tie(rhs->display_order, rhs->area_id);
				});

			size_t empty = 0, untranslated = 0, duplicate = 0;
			for (const auto* area : vortexAreas)
			{
				const auto it = vortexDungeons.find(area->area_id);
				if (it == vortexDungeons.end())
				{
					m_vortexHiddenAreas.insert(area->area_id);
					++empty;
					continue;
				}

				const auto& dungeons = it->second;
				const auto readable = IsDisplayableName(area->name)
					&& std::all_of(dungeons.begin(), dungeons.end(),
						[](const DungeonMst* d) { return IsDisplayableName(d->name); });
				if (!readable)
				{
					m_vortexHiddenAreas.insert(area->area_id);
					++untranslated;
					continue;
				}

				std::vector<std::string> banners;
				banners.reserve(dungeons.size());
				for (const auto* dungeon : dungeons)
					banners.push_back(dungeon->room_asset);
				std::sort(banners.begin(), banners.end());

				if (!bannersSeen.emplace(std::move(banners), area->area_id).second)
				{
					m_vortexHiddenAreas.insert(area->area_id);
					++duplicate;
					continue;
				}
			}

			LOG_INFO << "ServerCache: Vortex tiles — "
			         << (vortexAreas.size() - m_vortexHiddenAreas.size()) << " kept of "
			         << vortexAreas.size() << " (" << empty << " empty, "
			         << untranslated << " untranslated, " << duplicate << " duplicate)";
		}

		// The area table the client is served, which is what actually draws the
		// Vortex list: the hidden areas are dropped, Parade Garden is floated
		// to the top, and any tile with translated art gets it.  Every other
		// row is passed through untouched — the key is a full replace (see
		// UserInfoResp::area_mst), so anything missing here stops existing for
		// the client, Grand Gaia included.
		{
			size_t localised = 0;
			m_clientAreaMst.reserve(m_areaMst.size());
			for (const auto& area : m_areaMst)
			{
				if (m_vortexHiddenAreas.contains(area.area_id))
					continue;

				auto& emitted = m_clientAreaMst.emplace_back(area);
				if (emitted.area_id == kParadeGardenAreaId)
					emitted.display_order = kParadeGardenDispOrder;

				auto banner = LocalisedBanner(emitted.plate_img);
				if (banner != emitted.plate_img)
				{
					emitted.plate_img = std::move(banner);
					++localised;
				}
			}

			LOG_INFO << "ServerCache: serving " << m_clientAreaMst.size()
			         << " area(s) to the client, " << localised
			         << " with translated banner art";
		}

		// Everything PermitPlace must allow before the Vortex draws a tile.
		// See vortexPermits() for why this is scoped rather than a widening.
		//
		// Runs here, after the geography load, rather than in the mission block
		// above: the mission rows are dropped there, but m_missionsByDungeon
		// survives and is all this needs to walk dungeon -> missions.
		{
			for (const auto& area : m_areaMst)
				if (area.land_id == kVortexLandId && area.area_id < kVortexAreaIdMax
					&& !m_vortexHiddenAreas.contains(area.area_id))
					m_vortexPermits.areas.insert(area.area_id);

			for (const auto& dungeon : m_dungeonMst)
			{
				if (dungeon.land_id != kVortexLandId || dungeon.area_id >= kVortexAreaIdMax)
					continue;

				// A hidden area has no tile, so nothing inside it is reachable.
				// Leaving its dungeons permitted would not draw anything, but it
				// would keep them in PermitPlace for no reason.
				if (m_vortexHiddenAreas.contains(dungeon.area_id))
					continue;

				// The area is permitted unconditionally even for a rotating
				// dungeon, so the area tile stays put and only the dungeon
				// inside it comes and goes.
				m_vortexPermits.areas.insert(dungeon.area_id);

				const auto dayMask = VortexDayMaskFromBanner(dungeon.room_asset);

				// Not in the rotation -> permanently open.
				if (dayMask == 0)
					m_vortexPermits.dungeons.insert(dungeon.dungeon_id);

				const auto it = m_missionsByDungeon.find(dungeon.dungeon_id);
				const auto* missionIds =
					it == m_missionsByDungeon.end() ? nullptr : &it->second;

				if (dayMask == 0)
				{
					if (missionIds)
						for (const auto missionId : *missionIds)
							m_vortexPermits.missions.insert(missionId);
					continue;
				}

				for (size_t day = 0; day < m_vortexDayPermits.size(); ++day)
				{
					if ((dayMask & (1u << day)) == 0)
						continue;

					auto& permits = m_vortexDayPermits[day];
					permits.dungeons.insert(dungeon.dungeon_id);
					if (missionIds)
						for (const auto missionId : *missionIds)
							permits.missions.insert(missionId);
				}
			}

			// Player recollection is that weekends opened every weekday
			// dungeon; the shipped MST only gives the weekend its own.  Fold
			// Mon-Fri into Sat/Sun when the operator asks for the former.
			if (m_serverConfig.vortexWeekendOpensAll)
			{
				for (size_t day = 5; day < m_vortexDayPermits.size(); ++day)
					for (size_t weekday = 0; weekday < 5; ++weekday)
					{
						m_vortexDayPermits[day].dungeons.insert(
							m_vortexDayPermits[weekday].dungeons.begin(),
							m_vortexDayPermits[weekday].dungeons.end());
						m_vortexDayPermits[day].missions.insert(
							m_vortexDayPermits[weekday].missions.begin(),
							m_vortexDayPermits[weekday].missions.end());
					}
			}

			if (!m_vortexPermits.areas.empty())
				m_vortexPermits.lands.insert(kVortexLandId);

			LOG_INFO << "ServerCache: Vortex permits — "
			         << m_vortexPermits.lands.size() << " land(s), "
			         << m_vortexPermits.areas.size() << " area(s), "
			         << m_vortexPermits.dungeons.size() << " always-open dungeon(s), "
			         << m_vortexPermits.missions.size() << " always-open mission(s)";

			static constexpr std::string_view kDayNames[] = {
				"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
			};
			for (size_t day = 0; day < m_vortexDayPermits.size(); ++day)
				LOG_INFO << "ServerCache: Vortex rotation " << kDayNames[day] << " — "
				         << m_vortexDayPermits[day].dungeons.size() << " dungeon(s), "
				         << m_vortexDayPermits[day].missions.size() << " mission(s)"
				         << (m_serverConfig.vortexWeekendOpensAll && day >= 5
				             ? "  (weekend-opens-all)" : "");
		}

		// Shop / medal / help / fixed-PvP singletons — see mst/shop.kdl.
		m_shopItemMst = LoadJson<ShopItemMstCache>(mstRoot, "shop_item_mst.json").data;
		m_medalMst = LoadJson<MedalMstCache>(mstRoot, "medal_mst.json").data;
		m_helpDetailMst = LoadJson<HelpDetailMstCache>(mstRoot, "help_detail_mst.json").data;
		m_pvpFixedSettingMst = LoadJson<PvpFixedSettingMstCache>(mstRoot, "pvp_fixed_setting_mst.json").data;

		// NOTE: gacha_info_mst.json is deliberately NOT loaded here.  The
		// GachaInfoMst struct in mst/gacha.kdl is the per-request RESPONSE
		// shape (12 fields) and GachaArchiver serves it at request time; the
		// MST file has 23 columns, so a strict load into that struct throws.

		// Local-only MST loads, if this checkout has any.  Runs last so it can
		// rely on everything above; see ServerCacheLocalMembers.inl for storage.
		// After ADDING the file, touch this file: when it was absent nothing
		// recorded a dependency on it, so the build won't otherwise notice.
#if __has_include("ServerCacheLocalLoad.inl")
	#include "ServerCacheLocalLoad.inl"
#endif

		// cache: GachaList response (gacha_info comes from GachaArchiver at
		// request time; only the category banners are cached here)
		m_gachaListRsp.gacha_categories = LoadJson<GachaCategoryCache>(mstRoot, "gacha_category_mst.json").data;

		// TODO(arves): move this to generated per-used as there's no support for the claim
		m_initrsp.daily_task_bonuses = LoadJson<DailyTaskBonusMst>(mstRoot, "daily_task_bonus_mst.json");
		m_initrsp.daily_task_prizes = LoadJson<DailyTaskPrizeMstCache>(mstRoot, "daily_task_prize_mst.json").data;
		m_initrsp.daily_tasks = LoadJson<DailyTaskMstCache>(mstRoot, "daily_task_mst.json").data;
		// ---
	}
}

