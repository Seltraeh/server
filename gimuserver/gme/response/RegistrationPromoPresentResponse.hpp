#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RegistrationPromoPresentResponse : public IResponse
{
	const char* getGroupName() const override { return "3b6da94Y"; }

	struct Data {
		uint32_t m_PresentType = 0;
		std::string m_TargetID = "";
		uint32_t m_TargetCnt = 0;
		std::string m_TargetParam = "";
		uint32_t m_SubscriptionType = 0;
		uint32_t m_SubscriptionDayRange = 0;
		std::string m_PromoStartDay = "";
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
			item["30Kw4WBa"] = std::to_string(data.m_PresentType);
			item["TdDHf59J"] = data.m_TargetID;
			item["wJsB35iH"] = std::to_string(data.m_TargetCnt);
			item["37moriMq"] = data.m_TargetParam;
			item["b63Hk2lT"] = std::to_string(data.m_SubscriptionType);
			item["D2Br45nw"] = std::to_string(data.m_SubscriptionDayRange);
			item["ad6D2iby"] = data.m_PromoStartDay;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
