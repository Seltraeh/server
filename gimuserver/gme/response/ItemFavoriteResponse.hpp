#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ItemFavoriteResponse : public IResponse
{
	const char* getGroupName() const override { return "VSRPkdId"; }

	struct Data {
		uint32_t m_UserItemID = 0;
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
			item["n6E8iMf3"] = std::to_string(data.m_UserItemID);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
