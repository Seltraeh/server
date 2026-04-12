#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FGPlusBattleInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "P7uSpube"; }

		uint32_t m_TPR79fyI = 0;
		uint32_t m_pG2n1A28 = 0;
		uint32_t m_OdInfo = 0;
		uint32_t m_PYbfxpTp = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["TPR79fyI"] = std::to_string(m_TPR79fyI);
			v["pG2n1A28"] = std::to_string(m_pG2n1A28);
			v["NungTq5g"] = std::to_string(m_OdInfo);
			v["PYbfxpTp"] = std::to_string(m_PYbfxpTp);
	}
};
RESPONSE_NS_END
