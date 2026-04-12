#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GachaBonusGateResponse : public IResponse
{
	const char* getGroupName() const override { return "a36Dai1b"; }

		std::string m_GachaId = "";
		uint32_t m_TimeLeft = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["b3heg81m"] = m_GachaId;
			v["fE2d6ivS"] = std::to_string(m_TimeLeft);
	}
};
RESPONSE_NS_END
