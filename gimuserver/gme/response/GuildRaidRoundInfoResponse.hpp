#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidRoundInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Nebq4d8x"; }

		uint32_t m_SeasonID = 0;
		uint32_t m_RoundID = 0;
		uint32_t m_s385qzx9 = 0;
		std::string m_ServerTime = "";
		std::string m_CurrentPhase = "";
		std::string m_CurrentPhaseEndTime = "";
		std::string m_gE2NN2xi = "";
		std::string m_qXCIfZIk = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["dk39bDa1"] = std::to_string(m_SeasonID);
			v["81tacsfJ"] = std::to_string(m_RoundID);
			v["s385qzx9"] = std::to_string(m_s385qzx9);
			v["z1I0P1Qk"] = m_ServerTime;
			v["djaB081u"] = m_CurrentPhase;
			v["ka8D1i0b"] = m_CurrentPhaseEndTime;
			v["gE2NN2xi"] = m_gE2NN2xi;
			v["qXCIfZIk"] = m_qXCIfZIk;
	}
};
RESPONSE_NS_END
