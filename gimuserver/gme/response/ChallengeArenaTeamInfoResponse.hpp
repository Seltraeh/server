#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaTeamInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "W5F5xe9v"; }

	struct Data {
		uint32_t m_DeckNum = 0;
		std::string m_edy7fq3L = "";
		uint32_t m_NowHp = 0;
		uint32_t m_BbGauge = 0;
		uint32_t m_MemberType = 0;
		uint32_t m_Disporder = 0;
		std::string m_9PsmH7tz = "";
		std::string m_NowHp_str = ""; // distinct from uint32_t m_NowHp above; different JSON key
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
			item["edy7fq3L"] = data.m_edy7fq3L;
			item["Umbv916t"] = std::to_string(data.m_NowHp);
			item["itAtmp8r"] = std::to_string(data.m_BbGauge);
			item["gr48vsdJ"] = std::to_string(data.m_MemberType);
			item["4HIy6A1Q"] = std::to_string(data.m_Disporder);
			item["9PsmH7tz"] = data.m_9PsmH7tz;
			item["NM2tt2eS"] = data.m_NowHp_str;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
