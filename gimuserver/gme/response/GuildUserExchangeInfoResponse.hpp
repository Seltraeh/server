#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildUserExchangeInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "nMe3ai17"; }

	struct Data {
		uint32_t m_Yxo3bEic = 0;
		uint32_t m_H6k1LIxC = 0;
		uint32_t m_VDKB0Y5h = 0;
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
			item["Yxo3bEic"] = std::to_string(data.m_Yxo3bEic);
			item["H6k1LIxC"] = std::to_string(data.m_H6k1LIxC);
			item["VDKB0Y5h"] = std::to_string(data.m_VDKB0Y5h);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
