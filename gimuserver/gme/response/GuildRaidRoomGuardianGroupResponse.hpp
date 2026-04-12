#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidRoomGuardianGroupResponse : public IResponse
{
	const char* getGroupName() const override { return "71ocHdQe"; }

	struct Data {
		uint32_t m_RoomGuardGroupID = 0;
		uint32_t m_GuardGroupInfoID = 0;
		uint32_t m_GuardBattleLevel = 0;
		uint32_t m_GuardGroupID = 0;
		std::string m_938AbXie = "";
		uint32_t m_Lv = 0;
		uint32_t m_RoomID = 0;
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
			item["fOwC6mI9"] = std::to_string(data.m_RoomGuardGroupID);
			item["vn83gRn8"] = std::to_string(data.m_GuardGroupInfoID);
			item["w37afraZ"] = std::to_string(data.m_GuardBattleLevel);
			item["3Da8bm3b"] = std::to_string(data.m_GuardGroupID);
			item["938AbXie"] = data.m_938AbXie;
			item["D9wXQI2V"] = std::to_string(data.m_Lv);
			item["8VYd6xSX"] = std::to_string(data.m_RoomID);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
