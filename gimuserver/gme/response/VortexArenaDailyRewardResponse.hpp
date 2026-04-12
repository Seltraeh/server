#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct VortexArenaDailyRewardResponse : public IResponse
{
	const char* getGroupName() const override { return "93Dimd1w"; }

	struct Data {
		std::string m_TournamentID = "";
		uint32_t m_MinRank = 0;
		uint32_t m_MaxRank = 0;
		uint32_t m_Type = 0;
		uint32_t m_Quantity = 0;
		std::string m_RewardID = "";
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
			item["m31NipQe"] = data.m_TournamentID;
			item["u7Ijep5o"] = std::to_string(data.m_MinRank);
			item["4BtYr6Eg"] = std::to_string(data.m_MaxRank);
			item["30Kw4WBa"] = std::to_string(data.m_Type);
			item["wJsB35iH"] = std::to_string(data.m_Quantity);
			item["TdDHf59J"] = data.m_RewardID;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
