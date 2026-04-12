#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildGuardianSkillResponse : public IResponse
{
	const char* getGroupName() const override { return "FxewAaR2"; }

		uint32_t m_GuardASInfoID = 0;
		uint32_t m_SkillID = 0;
		uint32_t m_SkillLevel = 0;
		uint32_t m_ActualZel = 0;
		uint32_t m_ActualKarma = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["mXlvqgFU"] = std::to_string(m_GuardASInfoID);
			v["yu35Bdwr"] = std::to_string(m_SkillID);
			v["D9wXQI2V"] = std::to_string(m_SkillLevel);
			v["Najhr8m6"] = std::to_string(m_ActualZel);
			v["HTVh8a65"] = std::to_string(m_ActualKarma);
	}
};
RESPONSE_NS_END
