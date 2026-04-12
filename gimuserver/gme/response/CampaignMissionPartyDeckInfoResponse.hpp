#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct CampaignMissionPartyDeckInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "w5vHLT0q"; }

	struct Data {
		uint32_t m_DeckNum = 0;
		std::string m_FriendID = "";
		uint32_t m_MemberType = 0;
		uint32_t m_Disporder = 0;
		uint32_t m_NowHp = 0;
		uint32_t m_MaxHp = 0;
		uint32_t m_BbGauge = 0;
		uint32_t m_SbGauge = 0;
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
			item["zsiAn9P1"] = std::to_string(data.m_DeckNum);
			item["edy7fq3L"] = data.m_FriendID;
			item["gr48vsdJ"] = std::to_string(data.m_MemberType);
			item["XuJL4pc5"] = std::to_string(data.m_Disporder);
			item["a3qJ6QhX"] = std::to_string(data.m_NowHp);
			item["3WMz78t6"] = std::to_string(data.m_MaxHp);
			item["gU2xtQ0V"] = std::to_string(data.m_BbGauge);
			item["nIGZ1X9C"] = std::to_string(data.m_SbGauge);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
