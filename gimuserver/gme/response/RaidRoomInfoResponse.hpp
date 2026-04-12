#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidRoomInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "8iBuZa3n"; }

	struct Data {
		std::string m_RoomID = "";
		std::string m_WorldID = "";
		std::string m_MasterUserID = "";
		std::string m_MasterUserName = "";
		uint32_t m_MasterUserLv = 0;
		std::string m_MasterUserLeaderUnitID = "";
		uint32_t m_MasterUserLeaderUnitLv = 0;
		std::string m_PlaystyleID = "";
		std::string m_TargetID = "";
		uint32_t m_KickFlg = 0;
		uint32_t m_MinRC = 0;
		std::string m_Comment = "";
		std::string m_Password = "";
		uint32_t m_DissolveTime = 0;
		uint32_t m_QuickMatchingFlg = 0;
		uint32_t m_MidwayJoinableFlg = 0;
		uint32_t m_SearchType = 0;
		std::string m_MissionID = "";
		uint32_t m_MissionStartFlg = 0;
		uint32_t m_RoomMemberNum = 0;
		uint32_t m_PasswordFlg = 0;
		uint32_t m_RoomMissionJoinableRestTime = 0;
		std::string m_MinLv = "";
		std::string m_FriendUserID = "";
		std::string m_FriendUserName = "";
		uint32_t m_FriendUserLv = 0;
		std::string m_FriendUserLeaderUnitID = "";
		uint32_t m_FriendUserLeaderUnitLv = 0;
		uint32_t m_EnterableFlg = 0;
		uint32_t m_MasterUnitImgType = 0;
		uint32_t m_FriendUnitImgType = 0;
		uint32_t m_OpeType = 0;
		uint32_t m_QuickStartFlg = 0;
		uint32_t m_Ep3Flg = 0;
		uint32_t m_Sex = 0;
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
			item["43hMuY2I"] = data.m_WorldID;
			item["8ZK3w0Np"] = data.m_MasterUserID;
			item["IAfv98Wd"] = data.m_MasterUserName;
			item["6WKvM4o8"] = std::to_string(data.m_MasterUserLv);
			item["5bBwEo9f"] = data.m_MasterUserLeaderUnitID;
			item["fZgEP96L"] = std::to_string(data.m_MasterUserLeaderUnitLv);
			item["NQR1Vd3t"] = data.m_PlaystyleID;
			item["TdDHf59J"] = data.m_TargetID;
			item["j3sGA1zt"] = std::to_string(data.m_KickFlg);
			item["wYmg52xp"] = std::to_string(data.m_MinRC);
			item["WDUyI85C"] = data.m_Comment;
			item["4WSu1irc"] = data.m_Password;
			item["nJ2Qe3wM"] = std::to_string(data.m_DissolveTime);
			item["KQA1oWj7"] = std::to_string(data.m_QuickMatchingFlg);
			item["1Txpza2u"] = std::to_string(data.m_MidwayJoinableFlg);
			item["ouU5H7qm"] = std::to_string(data.m_SearchType);
			item["j28VNcUW"] = data.m_MissionID;
			item["3tWuc7de"] = std::to_string(data.m_MissionStartFlg);
			item["rPIya39u"] = std::to_string(data.m_RoomMemberNum);
			item["XqMsnE89"] = std::to_string(data.m_PasswordFlg);
			item["4Qf8RePm"] = std::to_string(data.m_RoomMissionJoinableRestTime);
			item["0TfXnP3m"] = data.m_MinLv;
			item["gkYiqB40"] = data.m_FriendUserID;
			item["H5pQK04c"] = data.m_FriendUserName;
			item["usM7W3DS"] = std::to_string(data.m_FriendUserLv);
			item["1ctR6GHC"] = data.m_FriendUserLeaderUnitID;
			item["8Ix0Soup"] = std::to_string(data.m_FriendUserLeaderUnitLv);
			item["C1vG0iKh"] = std::to_string(data.m_EnterableFlg);
			item["i9L42Gx0"] = std::to_string(data.m_MasterUnitImgType);
			item["JmLLHcDv"] = std::to_string(data.m_FriendUnitImgType);
			item["mnZ5K4Ii"] = std::to_string(data.m_OpeType);
			item["GWfpjED4"] = std::to_string(data.m_QuickStartFlg);
			item["jkldTrhL"] = std::to_string(data.m_Ep3Flg);
			item["9i2xhMaJ"] = std::to_string(data.m_Sex);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
