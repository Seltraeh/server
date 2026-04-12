#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserReleaseInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Dp0MjKAf"; }

		uint32_t m_JHYubaBP = 0;
		uint32_t m_g6PujUgD = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["JHYubaBP"] = std::to_string(m_JHYubaBP);
			v["g6PujUgD"] = std::to_string(m_g6PujUgD);
	}
};
RESPONSE_NS_END
