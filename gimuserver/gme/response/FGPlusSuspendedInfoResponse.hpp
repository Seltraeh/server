#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FGPlusSuspendedInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "spuPre2e"; }

		uint32_t m_hBNPQAU0 = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["hBNPQAU0"] = std::to_string(m_hBNPQAU0);
	}
};
RESPONSE_NS_END
