#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserChronologyInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "tjTwQOEb"; }

	struct Data {
		uint32_t m_5wtBEocP = 0;
		uint32_t m_NEcmfMyo = 0;
		uint32_t m_dJNpLc81 = 0;
		std::string m_3VftdE9r = "";
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
			item["5wtBEocP"] = std::to_string(data.m_5wtBEocP);
			item["NEcmfMyo"] = std::to_string(data.m_NEcmfMyo);
			item["dJNpLc81"] = std::to_string(data.m_dJNpLc81);
			item["3VftdE9r"] = data.m_3VftdE9r;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
