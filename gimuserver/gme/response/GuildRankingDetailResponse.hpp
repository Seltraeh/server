#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRankingDetailResponse : public IResponse
{
	const char* getGroupName() const override { return "R6YwbeCB"; }

	// The extractor recovered two m_Name fields (player name vs guild name) and
	// two m_Ranking fields (both map to "P_RANK" — the second is a duplicate key
	// in the serialize and likely refers to the same rank value). Renamed to avoid
	// redefinition errors; serialization preserved exactly as extracted.
	struct Data {
		uint32_t    m_Ranking     = 0; // player rank — "P_RANK"
		std::string m_Name        = ""; // player name — "B5JQyV8j"
		uint32_t    m_RankingType = 0;
		uint32_t    m_GuildID     = 0;
		std::string m_GuildName   = ""; // guild name — "s35idar9" (was duplicate m_Name)
		uint32_t    m_GuildLevel  = 0;
		uint32_t    m_BcpPoints   = 0;
		uint32_t    m_Ranking2    = 0; // duplicate "P_RANK" key — overwrites m_Ranking in JSON
		uint32_t    m_UnitLvl     = 0;
		uint32_t    m_UnitId      = 0;
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
			item["P_RANK"]    = std::to_string(data.m_Ranking);
			item["B5JQyV8j"]  = data.m_Name;
			item["C6W4Vpow"]  = std::to_string(data.m_RankingType);
			item["sD73jd20"]  = std::to_string(data.m_GuildID);
			item["s35idar9"]  = data.m_GuildName;
			item["osidufj5"]  = std::to_string(data.m_GuildLevel);
			item["gE2NN2xi"]  = std::to_string(data.m_BcpPoints);
			item["P_RANK"]    = std::to_string(data.m_Ranking2); // overwrites above — preserved from binary
			item["4A6LzBxr"]  = std::to_string(data.m_UnitLvl);
			item["pn16CNah"]  = std::to_string(data.m_UnitId);
			arr.append(item);
		}

		v = arr;
	}
};
RESPONSE_NS_END
