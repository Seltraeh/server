#pragma once

#include <json/json.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "DailyTaskConfig.hpp"
#include "StartInfo.hpp"
#include <gimuserver/gme/response/VideoAdsSlotgameInfo.hpp>
#include <gimuserver/gme/response/UserLevelMst.hpp>

class MstConfig
{
public:
	void LoadAllTables(const std::string& basePath);
	void CopyInitializeMstTo(Json::Value& v) const
	{
		for (auto srcIt = m_initMst.begin(); srcIt != m_initMst.end(); ++srcIt)
			v[srcIt.name()] = *srcIt;
	}

	void CopyUserInfoMstTo(Json::Value& v) const
	{
		for (auto srcIt = m_userInfoMst.begin(); srcIt != m_userInfoMst.end(); ++srcIt)
			v[srcIt.name()] = *srcIt;
	}

	void CopyGachaInfoTo(Json::Value& v) const {
		for (auto srcIt = m_gatchaInfo.begin(); srcIt != m_gatchaInfo.end(); ++srcIt)
			v[srcIt.name()] = *srcIt;
	}

	const auto& DailyTaskConfig() const { return m_dailyTask; }
	const auto& StartInfo() const { return m_startInfo; }
	const auto& GetAdsSlotInfo() const { return m_videoAdsSlot; }
	const auto& GetProgressionInfo() const { return m_progressions; }

	// Item MST: (itemId string, maxStack int) for all warehouse-seeded items.
	struct ItemMstEntry { std::string itemId; int maxStack; int sphereType; };
	const std::vector<ItemMstEntry>& GetItemMstEntries() const { return m_itemMst; }

	// Returns the itemSphereType for the given item ID, or 0 if not found / not a sphere.
	int GetItemSphereType(const std::string& itemId) const
	{
		for (const auto& e : m_itemMst)
			if (e.itemId == itemId) return e.sphereType;
		return 0;
	}

	// Unit MST: base/lord stats + level cap + exp pattern ID for fusion calculations.
	struct UnitMstData {
		std::string unitId;
		int expPatternId = 10;
		int maxLevel     = 100;
		int xpBoost      = 0;    // unitAdditionalXpBoost (adjustExp in client code)
		int sellPrice    = 500;
		int cost         = 1;    // unitCost — used in fusion XP formula: cost*2
		int rare         = 1;    // rarity (1-8) — used in fusion XP rarity bonus
		int element      = 1;    // element id (1=fire..6=dark) — used for same-element 1.5x bonus
		int baseHp = 0,  lordHp = 0;
		int baseAtk = 0, lordAtk = 0;
		int baseDef = 0, lordDef = 0;
		int baseRec = 0, lordRec = 0;
	};

	// Returns unit MST data by unitId (no _100 suffix), or nullptr if not found.
	const UnitMstData* GetUnitMstData(const std::string& unitId) const
	{
		auto it = m_unitMst.find(unitId);
		return it != m_unitMst.end() ? &it->second : nullptr;
	}

	// Returns the level a unit would be at for the given patternId and cumulative totalExp,
	// capped to maxLevel.
	int GetLevelFromTotalExp(int patternId, int maxLevel, int totalExp) const;

	// Returns the cumulative needExp threshold for the given level in the given pattern.
	int GetExpForLevel(int patternId, int level) const;

private:
	void LoadGacha(const std::string& path);
	void LoadItemMst(const std::string& basePath);
	void LoadUnitExpPatterns(const std::string& basePath);
	void LoadUnitMst(const std::string& basePath);
	void LoadExtraSkillPassive(const std::string& path);
	void LoadUrl(const std::string& path);
	void LoadReceipes(const std::string& path);
	void LoadProgressionInfo(const std::string& path);

	// cached jsons
	Json::Value m_initMst;
	Json::Value m_userInfoMst;
	Json::Value m_gatchaInfo;

	// caches classes
	Response::VideoAdsSlotgameInfo m_videoAdsSlot;
	Response::UserLevelMst m_progressions;
	::DailyTaskConfig m_dailyTask;
	::StartInfo m_startInfo;
	std::vector<ItemMstEntry> m_itemMst;

	// patternId → sorted vector of (level, needExp) pairs
	std::unordered_map<int, std::vector<std::pair<int,int>>> m_unitExpPatterns;
	std::unordered_map<std::string, UnitMstData> m_unitMst;
};

