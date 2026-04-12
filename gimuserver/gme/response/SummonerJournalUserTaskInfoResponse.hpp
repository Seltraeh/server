#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SummonerJournalUserTaskInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "da38tRai"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_Progress = 0;
		uint32_t m_ClaimStatus = 0;
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
			item["pG2n1A28"] = std::to_string(data.m_Progress);
			item["2g6adYig"] = std::to_string(data.m_ClaimStatus);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
