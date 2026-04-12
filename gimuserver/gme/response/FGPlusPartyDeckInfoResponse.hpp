#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FGPlusPartyDeckInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "cr6qeFUP"; }

	struct Data {
		uint32_t m_zsiAn9P1 = 0;
		std::string m_edy7fq3L = "";
		uint32_t m_gr48vsdJ = 0;
		uint32_t m_XuJL4pc5 = 0;
		uint32_t m_a3qJ6QhX = 0;
		uint32_t m_3WMz78t6 = 0;
		uint32_t m_gU2xtQ0V = 0;
		uint32_t m_nIGZ1X9C = 0;
		std::string m_P_ACTIVE_FLAG = "";
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
			item["zsiAn9P1"] = std::to_string(data.m_zsiAn9P1);
			item["edy7fq3L"] = data.m_edy7fq3L;
			item["gr48vsdJ"] = std::to_string(data.m_gr48vsdJ);
			item["XuJL4pc5"] = std::to_string(data.m_XuJL4pc5);
			item["a3qJ6QhX"] = std::to_string(data.m_a3qJ6QhX);
			item["3WMz78t6"] = std::to_string(data.m_3WMz78t6);
			item["gU2xtQ0V"] = std::to_string(data.m_gU2xtQ0V);
			item["nIGZ1X9C"] = std::to_string(data.m_nIGZ1X9C);
			item["P_ACTIVE_FLAG"] = data.m_P_ACTIVE_FLAG;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
