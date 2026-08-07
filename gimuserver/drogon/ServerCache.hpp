#pragma once

#include "ServerConfig.hpp"

/*!
* Cache of the server
*/
class ServerCache final : public trantor::NonCopyable
{
public:
	/*!
	* Setup the server cache.
	* @param[in] serverObj Configuration object of the plugin
	*/
	void Setup(const Json::Value& serverObj);

	/*!
	* Gets the banner configuration (dls).
	* @return DLS string
	*/
	inline const auto& dls() const { return m_dls; }

	/*!
	* Gets the server feature configuration.
	* @return Feature string
	*/
	inline const auto& feature() const { return m_feature; }

	/*!
	* Gets the common portion of the initialize response.
	* @return Initialize respose
	*/
	inline const auto& initializeResp() const { return m_initrsp;  }

	/*!
	* Gets the common portion of the user info response.
	* @return User info response
	*/
	inline const auto& userInfoResp() const { return m_userrsp; }

	/*!
	* Gets the cached summon list response.
	* @return GachaList response
	*/
	inline const auto& gachaListRsp() const { return m_gachaListRsp; }

	/*!
	* Gets the cached slot response.
	* @return ControlCenter response
	*/
	inline const auto& braveSlotsResp() const { return m_controlCenterRsp; }

	/*!
	* Server config.
	* @return Server config
	*/
	inline const auto& serverConfig() const { return m_serverConfig; }

	/*!
	* Unit master data (F_UNIT_MST). Empty until deploy/mst/unit_mst.json
	* (hashed-key format, wrapper key "2r9cNSdt") is added and the loader
	* in ServerCache::Setup is uncommented.
	* @return Vector of UnitMst entries
	*/
	inline const auto& unitMst() const { return m_unitMst; }

	/*!
	* Item master data (F_ITEM_MST, wrapper key "2C7LDzYk").  Loaded for
	* server-side item lookup (drop validation, sphere stats).  See
	* mst/item.kdl.
	* @return Vector of ItemMst entries (1668 rows)
	*/
	inline const auto& itemMst() const { return m_itemMst; }

	/*!
	* Grand Mission ("Campaign") master data, one getter per table.  The
	* Campaign handlers use these for mission/reward validation; see
	* mst/grand_mission.kdl for the field maps and
	* tools/MST_PORTING_BACKLOG.md for the 2026-07-19 port pass.
	* @return Vector of the matching GrandMission*Mst entries
	*/
	inline const auto& grandMissionMst() const { return m_grandMissionMst; }
	inline const auto& grandMissionMapMst() const { return m_grandMissionMapMst; }
	inline const auto& grandMissionSpotMst() const { return m_grandMissionSpotMst; }
	inline const auto& grandMissionRouteMst() const { return m_grandMissionRouteMst; }
	inline const auto& grandMissionIconMst() const { return m_grandMissionIconMst; }
	inline const auto& grandMissionTreasureMst() const { return m_grandMissionTreasureMst; }
	inline const auto& grandMissionFlgMst() const { return m_grandMissionFlgMst; }
	inline const auto& grandMissionEndCndMst() const { return m_grandMissionEndCndMst; }
	inline const auto& grandMissionEventMst() const { return m_grandMissionEventMst; }
	inline const auto& grandMissionRewardMst() const { return m_grandMissionRewardMst; }

	/*!
	* Raid Battle master data, one getter per table (13 of 14 — BOSS_PARTS
	* has no response class in the binary).  See mst/raid.kdl for the field
	* maps and tools/MST_PORTING_BACKLOG.md for the 2026-07-19 port pass.
	* @return Vector of the matching Raid*Mst entries
	*/
	inline const auto& raidWorldMst() const { return m_raidWorldMst; }
	inline const auto& raidMapMst() const { return m_raidMapMst; }
	inline const auto& raidRcMst() const { return m_raidRcMst; }
	inline const auto& raidPlaystyleMst() const { return m_raidPlaystyleMst; }
	inline const auto& raidBossMst() const { return m_raidBossMst; }
	inline const auto& raidBossRouteMst() const { return m_raidBossRouteMst; }
	inline const auto& raidMissionMst() const { return m_raidMissionMst; }
	inline const auto& raidMissionBossMst() const { return m_raidMissionBossMst; }
	inline const auto& raidMissionClearCndMst() const { return m_raidMissionClearCndMst; }
	inline const auto& raidMissionPointMst() const { return m_raidMissionPointMst; }
	inline const auto& raidPointMst() const { return m_raidPointMst; }
	inline const auto& raidUserRouteMst() const { return m_raidUserRouteMst; }
	inline const auto& raidBattleGroupMst() const { return m_raidBattleGroupMst; }

	/*!
	* Colosseum (PvP arena) master data — brackets, extra rules, formations
	* and support effects.  Field maps in mst/colosseum.kdl.
	*/
	inline const auto& colosseumClassMst() const { return m_colosseumClassMst; }
	inline const auto& colosseumExtraRuleMst() const { return m_colosseumExtraRuleMst; }
	inline const auto& colosseumFormationMst() const { return m_colosseumFormationMst; }
	inline const auto& colosseumSupportMst() const { return m_colosseumSupportMst; }

	/*!
	* Dual Brave Burst master data — pair catalog and bond recipes.
	* Field maps in mst/dbb.kdl.
	*/
	inline const auto& dbbMst() const { return m_dbbMst; }
	inline const auto& dbbBondRecipeMst() const { return m_dbbBondRecipeMst; }

	/*!
	* Guild master data — level curve, member skills, point exchange shop.
	* Field maps in mst/guild.kdl.
	*/
	inline const auto& guildInfoMst() const { return m_guildInfoMst; }
	inline const auto& guildMemberSkillMst() const { return m_guildMemberSkillMst; }
	inline const auto& guildPointExchangeMst() const { return m_guildPointExchangeMst; }

	/*!
	* Age-banded real-money spend caps.  Field map in mst/purchase.kdl.
	*/
	inline const auto& purchaseAgeLimitMst() const { return m_purchaseAgeLimitMst; }

	/*!
	* Frontier Gate master data — gate catalog and support effects.
	* Field maps in mst/frontier_gate.kdl.
	*/
	inline const auto& frontierGateMst() const { return m_frontierGateMst; }
	inline const auto& frontierGateSupportMst() const { return m_frontierGateSupportMst; }

	/*!
	* Summoner Unit master data — the player avatar's level curve, per-element
	* progression, arm catalog, ability tree, EX skills and illustrations.
	* Field maps in mst/summoner.kdl.
	*/
	inline const auto& summonerAbilityMst() const { return m_summonerAbilityMst; }
	inline const auto& summonerAbilityLevelMst() const { return m_summonerAbilityLevelMst; }
	inline const auto& summonerAbilityUpMst() const { return m_summonerAbilityUpMst; }
	inline const auto& summonerArmMst() const { return m_summonerArmMst; }
	inline const auto& summonerArmElementMst() const { return m_summonerArmElementMst; }
	inline const auto& summonerElementLevelMst() const { return m_summonerElementLevelMst; }
	inline const auto& summonerExSkillMst() const { return m_summonerExSkillMst; }
	inline const auto& summonerImageMst() const { return m_summonerImageMst; }
	inline const auto& summonerLevelMst() const { return m_summonerLevelMst; }

	/*!
	* Skill catalog and its sidecars.  Field maps in mst/skill.kdl (SkillMst)
	* and mst/skill_ext.kdl.
	*/
	inline const auto& skillMst() const { return m_skillMst; }
	inline const auto& skillLevelMst() const { return m_skillLevelMst; }
	inline const auto& leaderSkillMst() const { return m_leaderSkillMst; }

	/*!
	* Unit evolution recipes, standard and Omni.  Field maps in
	* mst/unit_evo.kdl.
	*/
	inline const auto& unitEvoMst() const { return m_unitEvoMst; }
	inline const auto& unitEvoOmniMst() const { return m_unitEvoOmniMst; }
	inline const auto& unitEvoOmniTypeMst() const { return m_unitEvoOmniTypeMst; }
	inline const auto& unitEvoOmniRecipeMst() const { return m_unitEvoOmniRecipeMst; }

	/*!
	* Unit side-tables — types, extras, EP3 costs, animation manifests,
	* comments and the Frontier Evolution tree.  Field maps in
	* mst/unit_ext.kdl and mst/mission_ep3.kdl.
	*/
	inline const auto& unitTypeMst() const { return m_unitTypeMst; }
	inline const auto& unitExtMst() const { return m_unitExtMst; }
	inline const auto& unitEp3Mst() const { return m_unitEp3Mst; }
	inline const auto& unitCgsMst() const { return m_unitCgsMst; }
	inline const auto& unitCommentMst() const { return m_unitCommentMst; }
	inline const auto& unitFeSkillMst() const { return m_unitFeSkillMst; }
	inline const auto& unitFeCategoryMst() const { return m_unitFeCategoryMst; }
	inline const auto& missionEp3Mst() const { return m_missionEp3Mst; }

	/*!
	* World geography — gates, areas, dungeons.  Field maps in mst/area.kdl.
	*/
	inline const auto& gateMst() const { return m_gateMst; }
	inline const auto& areaMst() const { return m_areaMst; }
	inline const auto& dungeonMst() const { return m_dungeonMst; }

	/*!
	* Shop, medal, help-detail and fixed-PvP tables.  Field maps in
	* mst/shop.kdl.
	*/
	inline const auto& shopItemMst() const { return m_shopItemMst; }
	inline const auto& medalMst() const { return m_medalMst; }
	inline const auto& helpDetailMst() const { return m_helpDetailMst; }
	inline const auto& pvpFixedSettingMst() const { return m_pvpFixedSettingMst; }

private:
	/*!
	* DLS cached JSON.
	*/
	std::string m_dls;

	/*!
	* Cached data of response.
	*/
	FeatureCheck m_feature{};

	/*!
	* Cached common data of the Initialize response
	*/
	InitializeResp m_initrsp{};

	/*!
	* Cached slot response
	*/
	SlotGameInfoR m_controlCenterRsp{};

	/*!
	* Server configuration.
	*/
	ServerConfig m_serverConfig;

	/*!
	* User info response.
	*/
	UserInfoResp m_userrsp{};

	/*!
	* Summon list response.
	*/
	GachaListResp m_gachaListRsp{};

	/*!
	* Unit master data, keyed/iterated by Unit handler ports.
	*/
	std::vector<UnitMst> m_unitMst;

	/*!
	* Item master data (wrapper key "2C7LDzYk"), looked up by item_id.
	* Loaded from deploy/mst/item_mst.json.
	*/
	std::vector<ItemMst> m_itemMst;

	/*!
	* Grand Mission ("Campaign") master data, loaded from
	* deploy/mst/grand_mission_*.json (wrapper keys documented in
	* mst/grand_mission.kdl).
	*/
	std::vector<GrandMissionMst> m_grandMissionMst;
	std::vector<GrandMissionMapMst> m_grandMissionMapMst;
	std::vector<GrandMissionSpotMst> m_grandMissionSpotMst;
	std::vector<GrandMissionRouteMst> m_grandMissionRouteMst;
	std::vector<GrandMissionIconMst> m_grandMissionIconMst;
	std::vector<GrandMissionTreasureMst> m_grandMissionTreasureMst;
	std::vector<GrandMissionFlgMst> m_grandMissionFlgMst;
	std::vector<GrandMissionEndCndMst> m_grandMissionEndCndMst;
	std::vector<GrandMissionEventMst> m_grandMissionEventMst;
	std::vector<GrandMissionRewardMst> m_grandMissionRewardMst;

	/*!
	* Raid Battle master data, loaded from deploy/mst/raid_*.json
	* (wrapper keys documented in mst/raid.kdl).
	*/
	std::vector<RaidWorldMst> m_raidWorldMst;
	std::vector<RaidMapMst> m_raidMapMst;
	std::vector<RaidRcMst> m_raidRcMst;
	std::vector<RaidPlaystyleMst> m_raidPlaystyleMst;
	std::vector<RaidBossMst> m_raidBossMst;
	std::vector<RaidBossRouteMst> m_raidBossRouteMst;
	std::vector<RaidMissionMst> m_raidMissionMst;
	std::vector<RaidMissionBossMst> m_raidMissionBossMst;
	std::vector<RaidMissionClearCndMst> m_raidMissionClearCndMst;
	std::vector<RaidMissionPointMst> m_raidMissionPointMst;
	std::vector<RaidPointMst> m_raidPointMst;
	std::vector<RaidUserRouteMst> m_raidUserRouteMst;
	std::vector<RaidBattleGroupMst> m_raidBattleGroupMst;

	// Colosseum — mst/colosseum.kdl
	std::vector<ColosseumClassMst> m_colosseumClassMst;
	std::vector<ColosseumExtraRuleMst> m_colosseumExtraRuleMst;
	std::vector<ColosseumFormationMst> m_colosseumFormationMst;
	std::vector<ColosseumSupportMst> m_colosseumSupportMst;

	// Dual Brave Burst — mst/dbb.kdl
	std::vector<DbbMst> m_dbbMst;
	std::vector<DbbBondRecipeMst> m_dbbBondRecipeMst;

	// Guild — mst/guild.kdl
	std::vector<GuildInfoMst> m_guildInfoMst;
	std::vector<GuildMemberSkillMst> m_guildMemberSkillMst;
	std::vector<GuildPointExchangeMst> m_guildPointExchangeMst;

	// Purchase age caps — mst/purchase.kdl
	std::vector<PurchaseAgeLimitMst> m_purchaseAgeLimitMst;

	// Frontier Gate — mst/frontier_gate.kdl
	std::vector<FrontierGateMst> m_frontierGateMst;
	std::vector<FrontierGateSupportMst> m_frontierGateSupportMst;

	// Summoner Unit — mst/summoner.kdl
	std::vector<SummonerAbilityMst> m_summonerAbilityMst;
	std::vector<SummonerAbilityLevelMst> m_summonerAbilityLevelMst;
	std::vector<SummonerAbilityUpMst> m_summonerAbilityUpMst;
	std::vector<SummonerArmMst> m_summonerArmMst;
	std::vector<SummonerArmElementMst> m_summonerArmElementMst;
	std::vector<SummonerElementLevelMst> m_summonerElementLevelMst;
	std::vector<SummonerExSkillMst> m_summonerExSkillMst;
	std::vector<SummonerImageMst> m_summonerImageMst;
	std::vector<SummonerLevelMst> m_summonerLevelMst;

	// Skills — mst/skill.kdl + mst/skill_ext.kdl
	std::vector<SkillMst> m_skillMst;
	std::vector<SkillLevelMst> m_skillLevelMst;
	std::vector<LeaderSkillMst> m_leaderSkillMst;

	// Unit evolution — mst/unit_evo.kdl
	std::vector<UnitEvoMst> m_unitEvoMst;
	std::vector<UnitEvoOmniMst> m_unitEvoOmniMst;
	std::vector<UnitEvoOmniTypeMst> m_unitEvoOmniTypeMst;
	std::vector<UnitEvoOmniRecipeMst> m_unitEvoOmniRecipeMst;

	// Unit side-tables — mst/unit_ext.kdl + mst/mission_ep3.kdl
	std::vector<UnitTypeMst> m_unitTypeMst;
	std::vector<UnitExtMst> m_unitExtMst;
	std::vector<UnitEp3Mst> m_unitEp3Mst;
	std::vector<UnitCgsMst> m_unitCgsMst;
	std::vector<UnitCommentMst> m_unitCommentMst;
	std::vector<UnitFeSkillMst> m_unitFeSkillMst;
	std::vector<UnitFeCategoryMst> m_unitFeCategoryMst;
	std::vector<MissionEp3Mst> m_missionEp3Mst;

	// World geography — mst/area.kdl
	std::vector<GateMst> m_gateMst;
	std::vector<AreaMst> m_areaMst;
	std::vector<DungeonMst> m_dungeonMst;

	// Shop / medal / help / fixed-PvP — mst/shop.kdl
	std::vector<ShopItemMst> m_shopItemMst;
	std::vector<MedalMst> m_medalMst;
	std::vector<HelpDetailMst> m_helpDetailMst;
	std::vector<PvpFixedSettingMst> m_pvpFixedSettingMst;

// Local-only members and getters, if this checkout has any.  Must be last in
// the class: the include manages its own access specifiers and leaves the
// class in whatever mode it ends on.
// After ADDING the file, touch this header: when it was absent nothing
// recorded a dependency on it, so the build won't otherwise notice.
#if __has_include("ServerCacheLocalMembers.inl")
	#include "ServerCacheLocalMembers.inl"
#endif
};
