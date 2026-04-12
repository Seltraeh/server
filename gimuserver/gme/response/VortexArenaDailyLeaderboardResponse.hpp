#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct VortexArenaDailyLeaderboardResponse : public IResponse
{
	const char* getGroupName() const override { return "7eCia0o3"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_UnitLv = 0;
		uint32_t m_ArenaRankID = 0;
		uint32_t m_RankingPoint = 0;
		uint32_t m_BattleCnt = 0;
		uint32_t m_WinCnt = 0;
		uint32_t m_BattleCntAccept = 0;
		uint32_t m_WinCntAccept = 0;
		uint32_t m_Rank = 0;
		uint32_t m_Day = 0;
		std::string m_Date = "";
		uint32_t m_InfoType = 0;
	};

	std::vector<Data> items;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
		Json::Value arr(Json::arrayValue);
		
		for (const auto& data : items) {
			Json::Value item;
			item["h7eY3sAK"] = data.m_UserID;
			item["4A6LzBxr"] = std::to_string(data.m_UnitLv);
			item["da5yD19b"] = std::to_string(data.m_ArenaRankID);
			item["P_POINTS"] = std::to_string(data.m_RankingPoint);
			item["69vnphig"] = std::to_string(data.m_BattleCnt);
			item["8CEu9Kcm"] = std::to_string(data.m_WinCnt);
			item["20iEWRCV"] = std::to_string(data.m_BattleCntAccept);
			item["0rAkzg7L"] = std::to_string(data.m_WinCntAccept);
			item["P_RANK"] = std::to_string(data.m_Rank);
			item["u8iD6ka7"] = std::to_string(data.m_Day);
			item["ieo3yBi7"] = data.m_Date;
			item["NH65Wj0f"] = std::to_string(data.m_InfoType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
