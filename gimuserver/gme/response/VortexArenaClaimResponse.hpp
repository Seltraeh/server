#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct VortexArenaClaimResponse : public IResponse
{
	const char* getGroupName() const override { return "x38dT257"; }

		uint32_t m_IsReceived = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["jY38iP5e"] = std::to_string(m_IsReceived);
	}
};
RESPONSE_NS_END
