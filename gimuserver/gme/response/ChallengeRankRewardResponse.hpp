#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeRankRewardResponse : public IResponse
{
	const char* getGroupName() const override { return "p73SJiWv"; }

	struct Data {
		std::string m_FrohunID = "";
		uint32_t m_InfoType = 0;
		uint32_t m_RankType = 0;
		std::string m_MissionID = "";
		uint32_t m_HRID = 0;
		uint32_t m_RewardType = 0;
		std::string m_TargetID = "";
		uint32_t m_TargetCnt = 0;
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
			item["c5yZnpB4"] = data.m_FrohunID;
			item["NH65Wj0f"] = std::to_string(data.m_InfoType);
			item["9w5e3ZJB"] = std::to_string(data.m_RankType);
			item["j28VNcUW"] = data.m_MissionID;
			item["Sv80kL5r"] = std::to_string(data.m_HRID);
			item["30Kw4WBa"] = std::to_string(data.m_RewardType);
			item["TdDHf59J"] = data.m_TargetID;
			item["wJsB35iH"] = std::to_string(data.m_TargetCnt);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
