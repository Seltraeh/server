#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct TierTourneyPointsResponse : public IResponse
{
	const char* getGroupName() const override { return "T_TOURNAMENT_POINTS"; }

	struct Data {
		std::string m_TournamentID = "";
		uint32_t m_Points = 0;
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
			item["P_TOURNAMENT_ID"] = data.m_TournamentID;
			item["P_POINTS"] = std::to_string(data.m_Points);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
