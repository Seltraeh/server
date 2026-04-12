#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FriendTierTourneyInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "T_USER_TOURNAMENT_RANK_INFO"; }

	struct Data {
		std::string m_TourneyID = "";
		uint32_t m_Rank = 0;
		std::string m_UserID = "";
		uint32_t m_UnitLevel = 0;
		uint32_t m_Points = 0;
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
			item["P_TOURNAMENT_ID"] = data.m_TourneyID;
			item["P_RANK"] = std::to_string(data.m_Rank);
			item["h7eY3sAK"] = data.m_UserID;
			item["4A6LzBxr"] = std::to_string(data.m_UnitLevel);
			item["P_POINTS"] = std::to_string(data.m_Points);
			item["NH65Wj0f"] = std::to_string(data.m_InfoType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
