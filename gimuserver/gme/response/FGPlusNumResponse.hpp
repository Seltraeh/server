#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FGPlusNumResponse : public IResponse
{
	const char* getGroupName() const override { return "Che7rAre"; }

		uint32_t m_2FQdEbG3 = 0;
		uint32_t m_hBNPQAU0 = 0;
		uint32_t m_NungTq5g = 0;
		std::string m_9iE6ouVv = "";
		std::string m_llTJ33yj = "";
		std::string m_fPxrnRT4 = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["2FQdEbG3"] = std::to_string(m_2FQdEbG3);
			v["hBNPQAU0"] = std::to_string(m_hBNPQAU0);
			v["NungTq5g"] = std::to_string(m_NungTq5g);
			v["9iE6ouVv"] = m_9iE6ouVv;
			v["llTJ33yj"] = m_llTJ33yj;
			v["fPxrnRT4"] = m_fPxrnRT4;
	}
};
RESPONSE_NS_END
