#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidRoomGuardianPartResponse : public IResponse
{
	const char* getGroupName() const override { return "3quwOva9"; }

	struct Data {
		uint32_t m_RoomGuardPartID = 0;
		uint32_t m_RoomGuardGroupID = 0;
		uint32_t m_RoomGuardPartInfoID = 0;
		uint32_t m_GuardPartID = 0;
		std::string m_CurrentHP = "";
		std::string m_MaxHP = "";
		uint32_t m_ATK = 0;
		uint32_t m_DEF = 0;
		uint32_t m_HEL = 0;
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
			item["BoXxssPt"] = std::to_string(data.m_RoomGuardPartID);
			item["fOwC6mI9"] = std::to_string(data.m_RoomGuardGroupID);
			item["C4Bjk2wp"] = std::to_string(data.m_RoomGuardPartInfoID);
			item["R3DqBi3b"] = std::to_string(data.m_GuardPartID);
			item["e7DK0FQT"] = data.m_CurrentHP;
			item["3WMz78t6"] = data.m_MaxHP;
			item["67CApcti"] = std::to_string(data.m_ATK);
			item["q08xLEsy"] = std::to_string(data.m_DEF);
			item["PWXu25cg"] = std::to_string(data.m_HEL);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
