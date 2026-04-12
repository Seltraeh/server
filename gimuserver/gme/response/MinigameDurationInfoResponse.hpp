#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct MinigameDurationInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "4xzt8Ra7"; }

	struct Data {
		uint32_t m_s1RXLIA0 = 0;
		std::string m_DurationID = "";
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
			item["s1RXLIA0"] = std::to_string(data.m_s1RXLIA0);
			item["ZW19nYrU"] = data.m_DurationID;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
