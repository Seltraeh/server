#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildLeaderSkillResponse : public IResponse
{
	const char* getGroupName() const override { return "18Dak67b"; }

	struct Data {
		uint32_t m_UnitId = 0;
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
			item["pn16CNah"] = std::to_string(data.m_UnitId);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
