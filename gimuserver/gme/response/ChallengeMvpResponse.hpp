#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeMvpResponse : public IResponse
{
	const char* getGroupName() const override { return "nUmaEC41"; }

	struct Data {
		std::string m_FrohunID = "";
		uint32_t m_RankType = 0;
		std::string m_MissionID = "";
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
			item["c5yZnpB4"] = data.m_FrohunID;
			item["9w5e3ZJB"] = std::to_string(data.m_RankType);
			item["j28VNcUW"] = data.m_MissionID;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
