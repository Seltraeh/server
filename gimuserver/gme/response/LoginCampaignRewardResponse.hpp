#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct LoginCampaignRewardResponse : public IResponse
{
	const char* getGroupName() const override { return "bD18x9Ti"; }

	struct Data {
		std::string m_LoginCampaignId = "";
		uint32_t m_RewardDay = 0;
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
			item["H1Dkq93v"] = data.m_LoginCampaignId;
			item["n0He37p1"] = std::to_string(data.m_RewardDay);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
