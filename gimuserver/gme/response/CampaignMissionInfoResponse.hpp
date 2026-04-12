#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct CampaignMissionInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "2I9V0o6J"; }

	struct Data {
		std::string m_MissionID = "";
		uint32_t m_AttainPercent = 0;
		std::string m_MissionOnFlg = "";
		uint32_t m_State = 0;
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
			item["j28VNcUW"] = data.m_MissionID;
			item["HUo4T7i8"] = std::to_string(data.m_AttainPercent);
			item["JcKMjH64"] = data.m_MissionOnFlg;
			item["j0Uszek2"] = std::to_string(data.m_State);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
