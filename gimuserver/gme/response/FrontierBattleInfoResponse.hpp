#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FrontierBattleInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "eIQ79KO2"; }

		uint32_t m_Score = 0;
		uint32_t m_Progress = 0;
		uint32_t m_SupportID = 0;
		uint32_t m_NowScore = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["TPR79fyI"] = std::to_string(m_Score);
			v["pG2n1A28"] = std::to_string(m_Progress);
			v["NungTq5g"] = std::to_string(m_SupportID);
			v["PYbfxpTp"] = std::to_string(m_NowScore);
	}
};
RESPONSE_NS_END
