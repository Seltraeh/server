#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UnitRequirementResponse : public IResponse
{
	const char* getGroupName() const override { return "z4Die02g"; }

	struct Data {
		std::string m_UnitID = "";
		uint32_t m_RequirementFlag = 0;
		uint32_t m_Series = 0;
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
			item["pn16CNah"] = data.m_UnitID;
			item["x38biA5m"] = std::to_string(data.m_RequirementFlag);
			item["9PsmH7tz"] = std::to_string(data.m_Series);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
