#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ColosseumPastRankingResponse : public IResponse
{
	const char* getGroupName() const override { return "dSC5DzuN"; }

	struct Data {
		std::string m_DurationID = "";
		uint32_t m_CurrentPosition = 0;
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
			item["ZW19nYrU"] = data.m_DurationID;
			item["2rqxaZ6K"] = std::to_string(data.m_CurrentPosition);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
