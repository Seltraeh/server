#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserAchievementTradeInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "9j3ALx8I"; }

	struct Data {
		std::string m_Mdgsh04u = "";
		uint32_t m_H6k1LIxC = 0;
		uint32_t m_VDKB0Y5h = 0;
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
			item["Mdgsh04u"] = data.m_Mdgsh04u;
			item["H6k1LIxC"] = std::to_string(data.m_H6k1LIxC);
			item["VDKB0Y5h"] = std::to_string(data.m_VDKB0Y5h);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
