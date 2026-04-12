#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SummonerJournalUserMilestoneInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "r3D28bqW"; }

	struct Data {
		uint32_t m_UserID = 0;
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
			item["h7eY3sAK"] = std::to_string(data.m_UserID);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
