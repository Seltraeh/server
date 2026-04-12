#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidMissionRCUpInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "awQFCW03"; }

		uint32_t m_RcUPFlag = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["UJTR1h5r"] = std::to_string(m_RcUPFlag);
	}
};
RESPONSE_NS_END
