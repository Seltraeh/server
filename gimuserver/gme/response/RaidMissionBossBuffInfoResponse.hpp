#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidMissionBossBuffInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "HQtqJGor"; }

	struct Data {
		std::string m_MonsterID = "";
		uint32_t m_BuffID = 0;
		uint32_t m_Evaluate = 0;
		std::string m_Param = "";
		uint32_t m_UbbFlg = 0;
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
			item["Jq2sQQJd"] = std::to_string(data.m_BuffID);
			item["ErfrRIAu"] = std::to_string(data.m_Evaluate);
			item["t5R47iwj"] = data.m_Param;
			item["roe7Ewt4"] = std::to_string(data.m_UbbFlg);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
