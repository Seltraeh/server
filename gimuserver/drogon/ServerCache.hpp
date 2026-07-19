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
	* Unit master data (F_UNIT_MST). Empty until deploy/system/unit_mst.json
	* (hashed-key format, wrapper key "2r9cNSdt") is added and the loader
	* in ServerCache::Setup is uncommented.
	* @return Vector of UnitMst entries
	*/
	inline const auto& unitMst() const { return m_unitMst; }

	/*!
	* Mission master data (F_MISSION_MST_1 + F_MISSION_MST_2 merged).
	* Wire wrapper key "oXeC1Ak9".  Server uses this to look up per-mission
	* energy cost (69vnphig), exp reward (d96tuT2E), zel/karma rewards
	* (Rs7bCE3t/HTVh8a65), battle group ids (8f4NYKxb), etc. at handler
	* time so MissionStart no longer hardcodes -10 energy and MissionEnd
	* no longer fakes 100 exp / 500 zel / 100 karma.  See mst/mission.kdl
	* for the full field map.
	* @return Vector of MissionMst entries (3433 rows from the 21900 game data)
	*/
	inline const auto& missionMst() const { return m_missionMst; }

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
	* Mission master data (wrapper key "oXeC1Ak9"), looked up by mission_id
	* in MissionStart / MissionEnd.  Loaded from deploy/system/mission_mst.json.
	*/
	std::vector<MissionMst> m_missionMst;

	/*!
	* Item master data (wrapper key "2C7LDzYk"), looked up by item_id.
	* Loaded from deploy/system/item_mst.json.
	*/
	std::vector<ItemMst> m_itemMst;

	/*!
	* Grand Mission ("Campaign") master data, loaded from
	* deploy/system/grand_mission_*.json (wrapper keys documented in
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
};
