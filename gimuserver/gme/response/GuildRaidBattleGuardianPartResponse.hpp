#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidBattleGuardianPartResponse : public IResponse
{
	const char* getGroupName() const override { return "yDa71iOw"; }

	struct Data {
		uint32_t m_BattleGuardPartID = 0;
		uint32_t m_BattleGuardID = 0;
		uint32_t m_RoomGuardPartID = 0;
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
			item["9jKlw91z"] = std::to_string(data.m_BattleGuardPartID);
			item["8b6adw7h"] = std::to_string(data.m_BattleGuardID);
			item["BoXxssPt"] = std::to_string(data.m_RoomGuardPartID);
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
