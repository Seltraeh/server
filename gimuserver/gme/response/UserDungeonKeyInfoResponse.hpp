#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserDungeonKeyInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "eFU7Qtb0"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_Possession = 0;
		uint32_t m_LastReceiptDay = 0;
		uint32_t m_ActiveType = 0;
		uint32_t m_ReceiptPossibleFlg = 0;
		uint32_t m_NextReceiptPossibleDate = 0;
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
			item["h7eY3sAK"] = data.m_UserID;
			item["wgV86x1q"] = std::to_string(data.m_Possession);
			item["KI42FwRt"] = std::to_string(data.m_LastReceiptDay);
			item["BY8fZ7M1"] = std::to_string(data.m_ActiveType);
			item["g85qMNxf"] = std::to_string(data.m_ReceiptPossibleFlg);
			item["b6QR1CH5"] = std::to_string(data.m_NextReceiptPossibleDate);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
