#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaTeamInfoUpdateResponse : public IResponse
{
	const char* getGroupName() const override { return "sZ8IlW1U"; }

		std::string m_Kn51uR4Y = "";
		uint32_t m_DeckNum = 0;
		std::string m_edy7fq3L = "";
		uint32_t m_NowHp = 0;
		uint32_t m_BbGauge = 0;
		uint32_t m_MemberType = 0;
		uint32_t m_Disporder = 0;
		std::string m_DeckNum = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["Kn51uR4Y"] = m_Kn51uR4Y;
			v["zsiAn9P1"] = std::to_string(m_DeckNum);
			v["edy7fq3L"] = m_edy7fq3L;
			v["Umbv916t"] = std::to_string(m_NowHp);
			v["itAtmp8r"] = std::to_string(m_BbGauge);
			v["gr48vsdJ"] = std::to_string(m_MemberType);
			v["4HIy6A1Q"] = std::to_string(m_Disporder);
			v["9PsmH7tz"] = m_DeckNum;
	}
};
RESPONSE_NS_END
