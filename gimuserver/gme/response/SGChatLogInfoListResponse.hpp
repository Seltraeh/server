#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SGChatLogInfoListResponse : public IResponse
{
	const char* getGroupName() const override { return "em4noAR5"; }

	struct Data {
		std::string m_Xy1mIazx = "";
		uint32_t m_XIvaD6Jp = 0;
		std::string m_h7eY3sAK = "";
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
			item["Xy1mIazx"] = data.m_Xy1mIazx;
			item["XIvaD6Jp"] = std::to_string(data.m_XIvaD6Jp);
			item["h7eY3sAK"] = data.m_h7eY3sAK;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
