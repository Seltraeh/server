#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidMissionBossStatusInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "x7YLyM7H"; }

	struct Data {
		std::string m_MonsterID = "";
		uint32_t m_Status = 0;
		uint32_t m_Evaluate = 0;
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
			item["o49dYfpH"] = data.m_MonsterID;
			item["eYMweF7Y"] = std::to_string(data.m_Status);
			item["ErfrRIAu"] = std::to_string(data.m_Evaluate);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
