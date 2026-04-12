#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaShopInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "O3RRXMgK"; }

		uint32_t m_RBseFWyk = 0;
		uint32_t m_ODdetio0 = 0;
		uint32_t m_HFAI8WT4 = 0;
		std::string m_wXTxs50z = "";
		uint32_t m_iuF7lR2o = 0;
		std::string m_M9fJGcI5 = "";
		uint32_t m_8XEFN85n = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["RBseFWyk"] = std::to_string(m_RBseFWyk);
			v["ODdetio0"] = std::to_string(m_ODdetio0);
			v["HFAI8WT4"] = std::to_string(m_HFAI8WT4);
			v["wXTxs50z"] = m_wXTxs50z;
			v["iuF7lR2o"] = std::to_string(m_iuF7lR2o);
			v["M9fJGcI5"] = m_M9fJGcI5;
			v["8XEFN85n"] = std::to_string(m_8XEFN85n);
	}
};
RESPONSE_NS_END
