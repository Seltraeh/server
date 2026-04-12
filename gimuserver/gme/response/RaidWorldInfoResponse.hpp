#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidWorldInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "taWh0Pr7"; }

	struct Data {
		std::string m_WorldID = "";
		uint32_t m_CrowdedPercent = 0;
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
			item["43hMuY2I"] = data.m_WorldID;
			item["SyL0XFc6"] = std::to_string(data.m_CrowdedPercent);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
