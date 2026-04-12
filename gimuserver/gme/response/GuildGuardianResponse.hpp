#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildGuardianResponse : public IResponse
{
	const char* getGroupName() const override { return "T_GUILD_GUARDIAN"; }

		uint32_t m_GuildUnitID = 0;
		uint32_t m_UnitID = 0;
		uint32_t m_ActualZel = 0;
		uint32_t m_ActualKarma = 0;
		uint32_t m_Level = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["P_GUILD_UNIT_ID"] = std::to_string(m_GuildUnitID);
			v["pn16CNah"] = std::to_string(m_UnitID);
			v["P_ACTUAL_ZEL"] = std::to_string(m_ActualZel);
			v["P_ACTUAL_KARMA"] = std::to_string(m_ActualKarma);
			v["D9wXQI2V"] = std::to_string(m_Level);
	}
};
RESPONSE_NS_END
