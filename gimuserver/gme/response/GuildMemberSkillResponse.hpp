#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildMemberSkillResponse : public IResponse
{
	const char* getGroupName() const override { return "dag38b71"; }

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
			v["ibY38bDn"] = std::to_string(m_SkillID);
			v["3NbeC8AB"] = std::to_string(m_SkillLevel);
			v["Najhr8m6"] = std::to_string(m_ActualZel);
			v["HTVh8a65"] = std::to_string(m_ActualKarma);
	}
};
RESPONSE_NS_END
