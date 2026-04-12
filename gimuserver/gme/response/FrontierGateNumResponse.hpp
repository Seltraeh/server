#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FrontierGateNumResponse : public IResponse
{
	const char* getGroupName() const override { return "Mg8K8Y1a"; }

		uint32_t m_FrogateNum = 0;
		uint32_t m_FrogateID = 0;
		uint32_t m_SelSupportID = 0;
		std::string m_PowerUpRateHp = "";
		std::string m_PowerUpRateAtk = "";
		std::string m_PowerUpRateDef = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["2FQdEbG3"] = std::to_string(m_FrogateNum);
			v["hBNPQAU0"] = std::to_string(m_FrogateID);
			v["NungTq5g"] = std::to_string(m_SelSupportID);
			v["9iE6ouVv"] = m_PowerUpRateHp;
			v["llTJ33yj"] = m_PowerUpRateAtk;
			v["fPxrnRT4"] = m_PowerUpRateDef;
	}
};
RESPONSE_NS_END
