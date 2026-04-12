#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidRoomMissionUserPerformanceInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "4FVXsAw8"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_PorposeType = 0;
		uint32_t m_MissionBossID = 0;
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
			item["h7eY3sAK"] = data.m_UserID;
			item["5V6Mrt8A"] = std::to_string(data.m_PorposeType);
			item["SP29fLtH"] = std::to_string(data.m_MissionBossID);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
