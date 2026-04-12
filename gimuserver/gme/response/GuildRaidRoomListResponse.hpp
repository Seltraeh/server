#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidRoomListResponse : public IResponse
{
	const char* getGroupName() const override { return "b9Dq1xzi"; }

	struct Data {
		uint32_t m_RoomID = 0;
		uint32_t m_DifficultyID = 0;
		uint32_t m_SeasonID = 0;
		uint32_t m_MemberCount = 0;
		std::string m_MasterUserID = "";
		uint32_t m_CampInterval = 0;
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
			item["8VYd6xSX"] = std::to_string(data.m_RoomID);
			item["978aBi2C"] = std::to_string(data.m_DifficultyID);
			item["dk39bDa1"] = std::to_string(data.m_SeasonID);
			item["ceak3Pxn"] = std::to_string(data.m_MemberCount);
			item["h7eY3sAK"] = data.m_MasterUserID;
			item["8ubiaz8i"] = std::to_string(data.m_CampInterval);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
