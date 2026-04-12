#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserUnitDbbInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "sxorQ3Mb"; }

	struct Data {
		std::string m_UserUnitId = "";
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
			item["edy7fq3L"] = data.m_UserUnitId;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
