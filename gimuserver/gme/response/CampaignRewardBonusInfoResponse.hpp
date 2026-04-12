#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct CampaignRewardBonusInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "p04iC2wr"; }

	struct Data {
		std::string m_RewardID = "";
		uint32_t m_PresentType = 0;
		std::string m_TargetID = "";
		uint32_t m_TargetCnt = 0;
		std::string m_TargetParam = "";
		uint32_t m_RewardType = 0;
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
			item["N1b6SUW4"] = data.m_RewardID;
			item["30Kw4WBa"] = std::to_string(data.m_PresentType);
			item["TdDHf59J"] = data.m_TargetID;
			item["wJsB35iH"] = std::to_string(data.m_TargetCnt);
			item["37moriMq"] = data.m_TargetParam;
			item["IkmC8gG2"] = std::to_string(data.m_RewardType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
