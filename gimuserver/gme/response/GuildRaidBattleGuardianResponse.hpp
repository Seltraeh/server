#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidBattleGuardianResponse : public IResponse
{
	const char* getGroupName() const override { return "9b7aDa71"; }

	struct Data {
		uint32_t m_BattleGuardianID = 0;
		uint32_t m_RoomGuardGroupID = 0;
		uint32_t m_RoomID = 0;
		uint32_t m_MapPointID = 0;
		std::string m_GuardType = "";
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
			item["8b6adw7h"] = std::to_string(data.m_BattleGuardianID);
			item["fOwC6mI9"] = std::to_string(data.m_RoomGuardGroupID);
			item["8VYd6xSX"] = std::to_string(data.m_RoomID);
			item["me7eDiXs"] = std::to_string(data.m_MapPointID);
			item["938AbXie"] = data.m_GuardType;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
