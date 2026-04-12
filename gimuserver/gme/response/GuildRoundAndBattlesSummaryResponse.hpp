#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRoundAndBattlesSummaryResponse : public IResponse
{
	const char* getGroupName() const override { return "b56mrqM9"; }

	struct Data {
		uint32_t m_Ranking = 0;
		std::string m_Name = "";
		uint32_t m_RankingType = 0;
		uint32_t m_GuildID = 0;
		std::string m_GuildName = "";
		uint32_t m_GuildLevel = 0;
		uint32_t m_BcpPoints = 0;
		uint32_t m_Ranking2 = 0;
		uint32_t m_UnitLvl = 0;
		uint32_t m_UnitId = 0;
		uint32_t m_UnitImgType = 0;
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
			item["P_RANK"] = std::to_string(data.m_Ranking);
			item["B5JQyV8j"] = data.m_Name;
			item["C6W4Vpow"] = std::to_string(data.m_RankingType);
			item["sD73jd20"] = std::to_string(data.m_GuildID);
			item["s35idar9"] = data.m_GuildName;
			item["osidufj5"] = std::to_string(data.m_GuildLevel);
			item["gE2NN2xi"] = std::to_string(data.m_BcpPoints);
			item["P_RANK"] = std::to_string(data.m_Ranking2);
			item["4A6LzBxr"] = std::to_string(data.m_UnitLvl);
			item["pn16CNah"] = std::to_string(data.m_UnitId);
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
