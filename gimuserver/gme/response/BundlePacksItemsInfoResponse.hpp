#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct BundlePacksItemsInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "yT8fs6jL"; }

		uint32_t m_BundleID = 0;
		uint32_t m_ItemType = 0;
		uint32_t m_ItemID = 0;
		uint32_t m_Qty = 0;
		uint32_t m_Param = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["j10diyl9"] = std::to_string(m_BundleID);
			v["h0K7wjeH"] = std::to_string(m_ItemType);
			v["kixHbe54"] = std::to_string(m_ItemID);
			v["ad3c72d9"] = std::to_string(m_Qty);
			v["2kd649bD"] = std::to_string(m_Param);
	}
};
RESPONSE_NS_END
