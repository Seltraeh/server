#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct VideoAdInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "j129kD6r"; }

		uint32_t m_VideoID = 0;
		uint32_t m_AvailableValue = 0;
		uint32_t m_regionID = 0;
		uint32_t m_VideoEnabled = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["k3ab6D82"] = std::to_string(m_VideoID);
			v["Diwl3b56"] = std::to_string(m_AvailableValue);
			v["Y3de0n2p"] = std::to_string(m_regionID);
			v["26adZ1iy"] = std::to_string(m_VideoEnabled);
	}
};
RESPONSE_NS_END
