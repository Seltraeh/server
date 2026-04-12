#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildGuardianGroupInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "m67Di3wq"; }

	struct Data {
		uint32_t m_GroupInfoID = 0;
		uint32_t m_GuardGroupID = 0;
		uint32_t m_GuardianFusionType = 0;
		uint32_t m_UnitType = 0;
		uint32_t m_Lv = 0;
		uint32_t m_Exp = 0;
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
			item["vn83gRn8"] = std::to_string(data.m_GroupInfoID);
			item["3Da8bm3b"] = std::to_string(data.m_GuardGroupID);
			item["iXvbWEyO"] = std::to_string(data.m_GuardianFusionType);
			item["1ZF3zLrC"] = std::to_string(data.m_UnitType);
			item["D9wXQI2V"] = std::to_string(data.m_Lv);
			item["d96tuT2E"] = std::to_string(data.m_Exp);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
