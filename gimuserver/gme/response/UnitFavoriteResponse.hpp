#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UnitFavoriteResponse : public IResponse
{
	const char* getGroupName() const override { return "3kcmQy7B"; }

	struct Data {
		uint32_t m_UserUnitID = 0;
	};

	std::vector<Data> items;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
		Json::Value arr(Json::arrayValue);
		
		for (const auto& data : items) {
			Json::Value item;
			item["edy7fq3L"] = std::to_string(data.m_UserUnitID);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
