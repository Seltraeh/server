#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidMissionRewardInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "4sbU8kdW"; }

	struct Data {
		uint32_t m_PresentType = 0;
		uint32_t m_TargetID = 0;
		uint32_t m_TargetCnt = 0;
		uint32_t m_TargetParam = 0;
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
			item["TdDHf59J"] = std::to_string(data.m_TargetID);
			item["wJsB35iH"] = std::to_string(data.m_TargetCnt);
			item["37moriMq"] = std::to_string(data.m_TargetParam);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
