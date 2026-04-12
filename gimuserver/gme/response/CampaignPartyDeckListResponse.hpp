#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct CampaignPartyDeckListResponse : public IResponse
{
	const char* getGroupName() const override { return "hT95cq8K"; }

	struct Data {
		uint32_t m_zsiAn9P1 = 0;
		std::string m_GuestID = "";
		uint32_t m_MemberType = 0;
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
			item["zsiAn9P1"] = std::to_string(data.m_zsiAn9P1);
			item["7zyHb5h9"] = data.m_GuestID;
			item["gr48vsdJ"] = std::to_string(data.m_MemberType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
