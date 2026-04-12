#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct EventTokenInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "l234vdKs"; }

	struct Data {
		uint32_t m_Slkc395l = 0;
		std::string m_s35idar9 = "";
		uint32_t m_lDk4hv20 = 0;
		std::string m_Timeleft = "";
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
			item["Slkc395l"] = std::to_string(data.m_Slkc395l);
			item["s35idar9"] = data.m_s35idar9;
			item["lDk4hv20"] = std::to_string(data.m_lDk4hv20);
			item["fE2d6ivS"] = data.m_Timeleft;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
