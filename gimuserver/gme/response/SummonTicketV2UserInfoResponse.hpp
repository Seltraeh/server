#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SummonTicketV2UserInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "a3d5d12i"; }

	struct Data {
		uint32_t m_SummonTicketV2Id = 0;
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
			item["b0D2iq2d"] = std::to_string(data.m_SummonTicketV2Id);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
