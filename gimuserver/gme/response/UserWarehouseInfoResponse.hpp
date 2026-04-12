#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserWarehouseInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "9wjrh74P"; }

		uint32_t m_kixHbe54 = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["kixHbe54"] = std::to_string(m_kixHbe54);
	}
};
RESPONSE_NS_END
