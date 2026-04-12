#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildUserContributionBonusResponse : public IResponse
{
	const char* getGroupName() const override { return "zw8zo15D"; }

		std::string m_h6UL9A1B = "";
		uint32_t m_SkillID = 0;
		uint32_t m_Type = 0;
		uint32_t m_ContCnt = 0;
		std::string m_ciDqCjAF = "";
		uint32_t m_BonusMulp = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["h6UL9A1B"] = m_h6UL9A1B;
			v["ibY38bDn"] = std::to_string(m_SkillID);
			v["1ZF3zLrC"] = std::to_string(m_Type);
			v["FOkXX7Hq"] = std::to_string(m_ContCnt);
			v["ciDqCjAF"] = m_ciDqCjAF;
			v["rwILddWQ"] = std::to_string(m_BonusMulp);
	}
};
RESPONSE_NS_END
