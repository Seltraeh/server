#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRankingResponse : public IResponse
{
	const char* getGroupName() const override { return "g7hx43Pq"; }

	struct Data {
		std::string m_UserId = "";
		std::string m_Score = "";
		std::string m_HandleName = "";
		uint32_t m_Progress = 0;
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
			item["h7eY3sAK"] = data.m_UserId;
			item["TPR79fyI"] = data.m_Score;
			item["B5JQyV8j"] = data.m_HandleName;
			item["pG2n1A28"] = std::to_string(data.m_Progress);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
