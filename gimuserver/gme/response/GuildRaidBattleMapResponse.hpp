#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidBattleMapResponse : public IResponse
{
	const char* getGroupName() const override { return "7boad17y"; }

	struct Data {
		uint32_t m_BattleMapID = 0;
		uint32_t m_MapPointID = 0;
		std::string m_Type = "";
		uint32_t m_GuardianID = 0;
		uint32_t m_RoomID = 0;
		std::string m_Located = "";
		uint32_t m_BuffID = 0;
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
			item["3ka7z8a1"] = std::to_string(data.m_BattleMapID);
			item["me7eDiXs"] = std::to_string(data.m_MapPointID);
			item["1ZF3zLrC"] = data.m_Type;
			item["8b6adw7h"] = std::to_string(data.m_GuardianID);
			item["8VYd6xSX"] = std::to_string(data.m_RoomID);
			item["j8iZk1wb"] = data.m_Located;
			item["Jq2sQQJd"] = std::to_string(data.m_BuffID);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
