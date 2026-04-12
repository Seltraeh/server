#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserTournamentInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "T_USER_TOURNAMENT_INFO"; }

	struct Data {
		std::string m_TournamentID = "";
		uint32_t m_TournamentRank = 0;
		uint32_t m_TotalVictoryPoints = 0;
		uint32_t m_ClaimedFlag = 0;
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
			item["P_RANK"] = std::to_string(data.m_TournamentRank);
			item["P_POINTS"] = std::to_string(data.m_TotalVictoryPoints);
			item["P_CLAIM_FLAG"] = std::to_string(data.m_ClaimedFlag);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
