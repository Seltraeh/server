#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildUserPartyDeckInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "IxiGlf2f"; }

	struct Data {
		uint32_t m_PartyDeckInfoID = 0;
		std::string m_UserID = "";
		uint32_t m_MemberType = 0;
		std::string m_pn16CNah = "";
		uint32_t m_4A6LzBxr = 0;
		uint32_t m_Disporder = 0;
		uint32_t m_MaxHp = 0;
		uint32_t m_NowHp = 0;
		uint32_t m_BbGauge = 0;
		uint32_t m_SbGauge = 0;
		uint32_t m_Hel = 0;
		std::string m_FriendID = "";
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
			item["b6adb7ik"] = std::to_string(data.m_PartyDeckInfoID);
			item["h7eY3sAK"] = data.m_UserID;
			item["gr48vsdJ"] = std::to_string(data.m_MemberType);
			item["pn16CNah"] = data.m_pn16CNah;
			item["4A6LzBxr"] = std::to_string(data.m_4A6LzBxr);
			item["XuJL4pc5"] = std::to_string(data.m_Disporder);
			item["3WMz78t6"] = std::to_string(data.m_MaxHp);
			item["a3qJ6QhX"] = std::to_string(data.m_NowHp);
			item["gU2xtQ0V"] = std::to_string(data.m_BbGauge);
			item["nIGZ1X9C"] = std::to_string(data.m_SbGauge);
			item["PWXu25cg"] = std::to_string(data.m_Hel);
			item["98WfKiyA"] = data.m_FriendID;
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
