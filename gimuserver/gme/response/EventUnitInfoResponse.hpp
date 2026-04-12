#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct EventUnitInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "49cks405"; }

	struct Data {
		std::string m_Slkc395l = "";
		uint32_t m_iR5yc6eW = 0;
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
			item["Slkc395l"] = data.m_Slkc395l;
			item["iR5yc6eW"] = std::to_string(data.m_iR5yc6eW);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
