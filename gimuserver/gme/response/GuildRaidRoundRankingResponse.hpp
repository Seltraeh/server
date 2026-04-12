#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidRoundRankingResponse : public IResponse
{
	const char* getGroupName() const override { return "mUS1176P"; }

	struct Data {
		uint32_t m_P_RANK = 0;
		uint32_t m_81tacsfJ = 0;
		uint32_t m_gE2NN2xi = 0;
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
			item["P_RANK"] = std::to_string(data.m_P_RANK);
			item["81tacsfJ"] = std::to_string(data.m_81tacsfJ);
			item["gE2NN2xi"] = std::to_string(data.m_gE2NN2xi);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
