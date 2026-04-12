#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UnitSelectorGachaUserInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "CGHaOZda"; }

		std::string m_XIvaD6Jp = "";
		uint32_t m_H6k1LIxC = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["XIvaD6Jp"] = m_XIvaD6Jp;
			v["H6k1LIxC"] = std::to_string(m_H6k1LIxC);
	}
};
RESPONSE_NS_END
