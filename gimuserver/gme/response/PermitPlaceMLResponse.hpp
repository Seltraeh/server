#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct PermitPlaceMLResponse : public IResponse
{
	const char* getGroupName() const override { return "Y73mHKS8"; }

		std::string m_Type = "";
		uint32_t m_TimeLimit = 0;
		uint32_t m_QuestBonusType = 0;
		std::string m_QuestBonusRate = "";
		uint32_t m_QuestBonusTimeLimit = 0;
		uint32_t m_PeriodText = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["0Cq2AlXW"] = m_Type;
			v["qY49LBjw"] = std::to_string(m_TimeLimit);
			v["nA95Bdj6"] = std::to_string(m_QuestBonusType);
			v["5Z1LNoyH"] = m_QuestBonusRate;
			v["5ry19G4Y"] = std::to_string(m_QuestBonusTimeLimit);
			v["s2gM3deu"] = std::to_string(m_PeriodText);
	}
};
RESPONSE_NS_END
