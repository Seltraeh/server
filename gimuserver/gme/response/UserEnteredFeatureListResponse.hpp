#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserEnteredFeatureListResponse : public IResponse
{
	const char* getGroupName() const override { return "2386Diw1"; }

		uint32_t m_e63D1BV0 = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["e63D1BV0"] = std::to_string(m_e63D1BV0);
	}
};
RESPONSE_NS_END
