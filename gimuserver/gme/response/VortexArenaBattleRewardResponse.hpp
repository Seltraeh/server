#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct VortexArenaBattleRewardResponse : public IResponse
{
	const char* getGroupName() const override { return "d6gl3Yk8"; }

	struct Data {
		std::string m_RankID = "";
		uint32_t m_PresentType = 0;
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
			item["da5yD19b"] = data.m_RankID;
			item["30Kw4WBa"] = std::to_string(data.m_PresentType);
			item["wJsB35iH"] = std::to_string(data.m_TargetCnt);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
