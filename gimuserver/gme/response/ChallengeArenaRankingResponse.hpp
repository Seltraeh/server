#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaRankingResponse : public IResponse
{
	const char* getGroupName() const override { return "pZWEqax1"; }

	struct Data {
		uint32_t m_BcIqcWDM = 0;
		uint32_t m_4lH05mQr = 0;
		uint32_t m_D9wXQI2V = 0;
		uint32_t m_4A6LzBxr = 0;
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
			item["BcIqcWDM"] = std::to_string(data.m_BcIqcWDM);
			item["4lH05mQr"] = std::to_string(data.m_4lH05mQr);
			item["D9wXQI2V"] = std::to_string(data.m_D9wXQI2V);
			item["4A6LzBxr"] = std::to_string(data.m_4A6LzBxr);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
