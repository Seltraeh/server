#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct EventTokenExchangeInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "c2Sls4DD"; }

	struct Data {
		std::string m_Kd3DL39d = "";
		uint32_t m_XuJL4pc5 = 0;
		uint32_t m_3EWLm0sA = 0;
		uint32_t m_S8rdp9zk = 0;
		std::string m_TradeInfo = "";
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
			item["Kd3DL39d"] = data.m_Kd3DL39d;
			item["XuJL4pc5"] = std::to_string(data.m_XuJL4pc5);
			item["3EWLm0sA"] = std::to_string(data.m_3EWLm0sA);
			item["S8rdp9zk"] = std::to_string(data.m_S8rdp9zk);
			item["qBAb07rh"] = data.m_TradeInfo;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
