#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct MinigamePastRankingInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "C6Au3tzm"; }

	struct Data {
		uint32_t m_s1RXLIA0 = 0;
		std::string m_ZW19nYrU = "";
		uint32_t m_TPR79fyI = 0;
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
			item["s1RXLIA0"] = std::to_string(data.m_s1RXLIA0);
			item["ZW19nYrU"] = data.m_ZW19nYrU;
			item["TPR79fyI"] = std::to_string(data.m_TPR79fyI);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
