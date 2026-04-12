#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidRoomUserInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "uwH62EZy"; }

	struct Data {
		std::string m_RoomID = "";
		uint32_t m_ReadyInfo = 0;
		std::string m_UserName = "";
		uint32_t m_UserRC = 0;
		uint32_t m_UserLv = 0;
		uint32_t m_UserAubeNum = 0;
		uint32_t m_MissionStatus = 0;
		uint32_t m_KickedFlg = 0;
		uint32_t m_ActiveStatus = 0;
		uint32_t m_SortieFlg = 0;
		uint32_t m_CurrentRoomMissionID = 0;
		uint32_t m_UnitImgType = 0;
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
			item["8VYd6xSX"] = data.m_RoomID;
			item["uX87tAZB"] = std::to_string(data.m_ReadyInfo);
			item["B5JQyV8j"] = data.m_UserName;
			item["7x3pPB2C"] = std::to_string(data.m_UserRC);
			item["D9wXQI2V"] = std::to_string(data.m_UserLv);
			item["C45mRxnZ"] = std::to_string(data.m_UserAubeNum);
			item["j3g5P4cq"] = std::to_string(data.m_MissionStatus);
			item["D2ZKtGR9"] = std::to_string(data.m_KickedFlg);
			item["BY8fZ7M1"] = std::to_string(data.m_ActiveStatus);
			item["Tq86EG0N"] = std::to_string(data.m_SortieFlg);
			item["FV6MU5nQ"] = std::to_string(data.m_CurrentRoomMissionID);
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
