#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct CampaignMissionDeckInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Yusr3Zg5"; }

	struct Data {
		uint32_t m_DeckNum = 0;
		uint32_t m_NowPointNum = 0;
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
			item["7w0inC1R"] = std::to_string(data.m_NowPointNum);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
