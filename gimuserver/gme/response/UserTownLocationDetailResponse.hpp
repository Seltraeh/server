#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserTownLocationDetailResponse : public IResponse
{
	const char* getGroupName() const override { return "s8TCo2MS"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_LocationID = 0;
		std::string m_Startdate = "";
		uint32_t m_TapCnt = 0;
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
			item["qA7M9EjP"] = data.m_Startdate;
			item["mDaE3t6A"] = std::to_string(data.m_TapCnt);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
