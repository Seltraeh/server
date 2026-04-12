#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeHrResponse : public IResponse
{
	const char* getGroupName() const override { return "h09mEvDR"; }

	struct Data {
		uint32_t m_HRID = 0;
		std::string m_RankName = "";
		uint32_t m_PresentTyp = 0;
		std::string m_TargetID = "";
		uint32_t m_TargetCnt = 0;
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
			item["Sv80kL5r"] = std::to_string(data.m_HRID);
			item["h6V4weL2"] = data.m_RankName;
			item["30Kw4WBa"] = std::to_string(data.m_PresentTyp);
			item["TdDHf59J"] = data.m_TargetID;
			item["wJsB35iH"] = std::to_string(data.m_TargetCnt);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
