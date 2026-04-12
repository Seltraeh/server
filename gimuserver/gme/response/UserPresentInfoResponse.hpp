#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserPresentInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "sEA41vFK"; }

	struct Data {
		std::string m_PresentID = "";
		uint32_t m_PresentType = 0;
		std::string m_TargetID = "";
		uint32_t m_TargetCnt = 0;
		std::string m_TargetParam = "";
		uint32_t m_ReceiptType = 0;
		uint32_t m_PresentDate = 0;
		std::string m_PresentDateStr = "";
		uint32_t m_ReceiptDate = 0;
		std::string m_ReceiptDateStr = "";
		uint32_t m_IsReceipt = 0;
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
			item["S1B82FHK"] = data.m_PresentID;
			item["30Kw4WBa"] = std::to_string(data.m_PresentType);
			item["TdDHf59J"] = data.m_TargetID;
			item["wJsB35iH"] = std::to_string(data.m_TargetCnt);
			item["37moriMq"] = data.m_TargetParam;
			item["i1WQkh4G"] = std::to_string(data.m_ReceiptType);
			item["fAi8Th5s"] = std::to_string(data.m_PresentDate);
			item["ya7UHG9v"] = data.m_PresentDateStr;
			item["0wmDb9vi"] = std::to_string(data.m_ReceiptDate);
			item["vo1GT3Rs"] = data.m_ReceiptDateStr;
			item["DbMVG16I"] = std::to_string(data.m_IsReceipt);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
