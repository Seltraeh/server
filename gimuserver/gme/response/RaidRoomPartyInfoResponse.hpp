#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidRoomPartyInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "9PS8BLiC"; }

	struct Data {
		std::string m_UserID = "";
		std::string m_UserUnitID = "";
		std::string m_UnitID = "";
		uint32_t m_UnitImgType = 0;
		uint32_t m_DeckNo = 0;
		uint32_t m_MemberType = 0;
		uint32_t m_Disporder = 0;
		uint32_t m_UnitLv = 0;
		uint32_t m_UnitHp = 0;
		uint32_t m_UnitAddHp = 0;
		uint32_t m_UnitExtHp = 0;
		std::string m_EqpItemID = "";
		std::string m_EqpItemID2 = "";
		std::string m_ExtraPassiveSkillID = "";
		std::string m_ExtraPassiveSkillID2 = "";
		uint32_t m_UnitTypeID = 0;
		std::string m_FriendUserID = "";
		uint32_t m_Ep3Flg = 0;
		uint32_t m_Sex = 0;
		uint32_t m_Element = 0;
		std::string m_HairID = "";
		std::string m_ArmID = "";
		uint32_t m_ArmLv = 0;
		uint32_t m_ArmExp = 0;
		std::string m_AbilityInfo = "";
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
			item["edy7fq3L"] = data.m_UserUnitID;
			item["pn16CNah"] = data.m_UnitID;
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			item["zsiAn9P1"] = std::to_string(data.m_DeckNo);
			item["gr48vsdJ"] = std::to_string(data.m_MemberType);
			item["XuJL4pc5"] = std::to_string(data.m_Disporder);
			item["4A6LzBxr"] = std::to_string(data.m_UnitLv);
			item["e7DK0FQT"] = std::to_string(data.m_UnitHp);
			item["cuIWp89g"] = std::to_string(data.m_UnitAddHp);
			item["TokWs1B3"] = std::to_string(data.m_UnitExtHp);
			item["Ge8Yo32T"] = data.m_EqpItemID;
			item["mZA7fH2v"] = data.m_EqpItemID2;
			item["cP83zNsv"] = data.m_ExtraPassiveSkillID;
			item["LjY4DfRg"] = data.m_ExtraPassiveSkillID2;
			item["nBTx56W9"] = std::to_string(data.m_UnitTypeID);
			item["98WfKiyA"] = data.m_FriendUserID;
			item["jkldTrhL"] = std::to_string(data.m_Ep3Flg);
			item["9i2xhMaJ"] = std::to_string(data.m_Sex);
			item["iNy0ZU5M"] = std::to_string(data.m_Element);
			item["btZizNep"] = data.m_HairID;
			item["RVVgyuor"] = data.m_ArmID;
			item["U8FCB2Wj"] = std::to_string(data.m_ArmLv);
			item["NqVAPbLC"] = std::to_string(data.m_ArmExp);
			item["mgNdrCEe"] = data.m_AbilityInfo;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
