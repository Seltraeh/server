#pragma once

#include <vector>
#include <json/json.h>
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

	// Item seed list loaded from item_master.json
	struct SeedItem { int id = 0; int quantity = 0; };
	const std::vector<SeedItem>& GetSeedItems() const { return m_seedItems; }

	// All mission IDs from mission_master.json (used to mark all missions as cleared)
	const std::vector<int>& GetMissionIds() const { return m_missionIds; }

private:
	void LoadGacha(const std::string& path);
	void LoadExtraSkillPassive(const std::string& path);
	void LoadUrl(const std::string& path);
	void LoadReceipes(const std::string& path);
	void LoadProgressionInfo(const std::string& path);
	void LoadSeedItems(const std::string& basePath);
	void LoadMissionIds(const std::string& basePath);

	// cached jsons
	Json::Value m_initMst;
	Json::Value m_userInfoMst;
	Json::Value m_gatchaInfo;

	// caches classes
	Response::VideoAdsSlotgameInfo m_videoAdsSlot;
	Response::UserLevelMst m_progressions;
	::DailyTaskConfig m_dailyTask;
	::StartInfo m_startInfo;

	// slim master data extracted from bravefrontier_data
	std::vector<SeedItem> m_seedItems;
	std::vector<int>      m_missionIds;

};

