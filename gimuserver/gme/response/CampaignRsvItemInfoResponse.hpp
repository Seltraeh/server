#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct CampaignRsvItemInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "5EByfWJ4"; }

	struct Data {
		std::string m_ItemID = "";
		uint32_t m_DispOrder = 0;
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
			item["kixHbe54"] = data.m_ItemID;
			item["XuJL4pc5"] = std::to_string(data.m_DispOrder);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
