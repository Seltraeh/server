#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct VideoAdRegionResponse : public IResponse
{
	const char* getGroupName() const override { return "bpD29eiQ"; }

		uint32_t m_RegionID = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["Y3de0n2p"] = std::to_string(m_RegionID);
	}
};
RESPONSE_NS_END
