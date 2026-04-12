#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserTownLocationInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "yj46Q2xw"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_LocationID = 0;
		uint32_t m_Lv = 0;
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
			item["h7eY3sAK"] = data.m_UserID;
			item["un80kW9Y"] = std::to_string(data.m_LocationID);
			item["D9wXQI2V"] = std::to_string(data.m_Lv);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
