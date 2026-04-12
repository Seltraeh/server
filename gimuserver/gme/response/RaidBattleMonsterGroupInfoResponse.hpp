#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidBattleMonsterGroupInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Fx60pCaU"; }

	struct Data {
		std::string m_MonsterID = "";
		uint32_t m_BodyFlg = 0;
		uint32_t m_TotalRecover = 0;
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
			item["7Hf3DnVj"] = std::to_string(data.m_BodyFlg);
			item["jXEdk5J4"] = std::to_string(data.m_TotalRecover);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
