#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildCreateCostResponse : public IResponse
{
	const char* getGroupName() const override { return "jKeiqDbl"; }

		uint32_t m_CurrencyType = 0;
		uint32_t m_RequiredAmount = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["ODdetio0"] = std::to_string(m_CurrencyType);
			v["HFAI8WT4"] = std::to_string(m_RequiredAmount);
	}
};
RESPONSE_NS_END
