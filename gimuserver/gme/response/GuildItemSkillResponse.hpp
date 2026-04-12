#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildItemSkillResponse : public IResponse
{
	const char* getGroupName() const override { return "a3a8ck16"; }

	struct Data {
		uint32_t m_SphereID = 0;
		std::string m_ProcessId = "";
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
			item["jD18ry3B"] = std::to_string(data.m_SphereID);
			item["hjAy9St3"] = data.m_ProcessId;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
