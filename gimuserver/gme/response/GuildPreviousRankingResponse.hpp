#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildPreviousRankingResponse : public IResponse
{
	const char* getGroupName() const override { return "LoLJTlaG"; }

	struct Data {
		uint32_t m_RankingType = 0;
		uint32_t m_GuildID = 0;
		std::string m_Name = "";
		uint32_t m_PrestigePoints = 0;
		uint32_t m_Ranking = 0;
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
			item["C6W4Vpow"] = std::to_string(data.m_RankingType);
			item["sD73jd20"] = std::to_string(data.m_GuildID);
			item["s35idar9"] = data.m_Name;
			item["sc83dkh3"] = std::to_string(data.m_PrestigePoints);
			item["P_RANK"] = std::to_string(data.m_Ranking);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
