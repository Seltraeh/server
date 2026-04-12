#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidBattleLogListResponse : public IResponse
{
	const char* getGroupName() const override { return "M9ctnu4Q"; }

		uint32_t m_MsgID = 0;
		uint32_t m_MsgReadability = 0;
		uint32_t m_GuildID = 0;
		std::string m_MsgType = "";
		uint32_t m_MsgContentID = 0;
		std::string m_HandleName = "";
		uint32_t m_MapPointID = 0;
		uint32_t m_MapGuardianType = 0;
		std::string m_CurrentHP = "";
		uint32_t m_GuardianBuffID = 0;
		uint32_t m_GuardianDebuffID = 0;
		uint32_t m_TeamBuffID = 0;
		uint32_t m_TeamDebuffID = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["78Zema70"] = std::to_string(m_MsgID);
			v["ykppaAss"] = std::to_string(m_MsgReadability);
			v["sD73jd20"] = std::to_string(m_GuildID);
			v["3e9aGpus"] = m_MsgType;
			v["33NJQWdR"] = std::to_string(m_MsgContentID);
			v["B5JQyV8j"] = m_HandleName;
			v["me7eDiXs"] = std::to_string(m_MapPointID);
			v["nr49L6DW"] = std::to_string(m_MapGuardianType);
			v["e7DK0FQT"] = m_CurrentHP;
			v["QXYAv9L7"] = std::to_string(m_GuardianBuffID);
			v["m5RPduay"] = std::to_string(m_GuardianDebuffID);
			v["KeKKpEAW"] = std::to_string(m_TeamBuffID);
			v["fsZWDyW2"] = std::to_string(m_TeamDebuffID);
	}
};
RESPONSE_NS_END
