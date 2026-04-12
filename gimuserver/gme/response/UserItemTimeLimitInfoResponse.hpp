#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserItemTimeLimitInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "YmaEqtA8"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_UseCnt = 0;
		uint32_t m_EndDate = 0;
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
			item["phe9Ta54"] = std::to_string(data.m_UseCnt);
			item["SzV0Nps7"] = std::to_string(data.m_EndDate);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
