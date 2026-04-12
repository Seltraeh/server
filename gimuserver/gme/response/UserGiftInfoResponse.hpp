#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserGiftInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "30uygM9m"; }

	struct Data {
		std::string m_GiftIdentifyID = "";
		std::string m_UserIDFrom = "";
		std::string m_UserIDTo = "";
		uint32_t m_GiftID = 0;
		uint32_t m_GiftType = 0;
		std::string m_ItemID = "";
		uint32_t m_Possession = 0;
		std::string m_GiftDate = "";
		uint32_t m_GiftYmd = 0;
		uint32_t m_RecieveFlg = 0;
		std::string m_UnitID = "";
		std::string m_Handlename = "";
		uint32_t m_UnitImgType = 0;
		uint32_t m_Ep3Flg = 0;
		uint32_t m_Sex = 0;
		uint32_t m_Element = 0;
		std::string m_HairID = "";
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
			item["gNE76SLp"] = data.m_GiftIdentifyID;
			item["yDiG31Fe"] = data.m_UserIDFrom;
			item["1px2dbX3"] = data.m_UserIDTo;
			item["eDj82kMN"] = std::to_string(data.m_GiftID);
			item["2bvT9hKt"] = std::to_string(data.m_GiftType);
			item["kixHbe54"] = data.m_ItemID;
			item["wgV86x1q"] = std::to_string(data.m_Possession);
			item["aHSvC81N"] = data.m_GiftDate;
			item["q5Y4gUnb"] = std::to_string(data.m_GiftYmd);
			item["fa6WwE9u"] = std::to_string(data.m_RecieveFlg);
			item["pn16CNah"] = data.m_UnitID;
			item["B5JQyV8j"] = data.m_Handlename;
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			item["jkldTrhL"] = std::to_string(data.m_Ep3Flg);
			item["9i2xhMaJ"] = std::to_string(data.m_Sex);
			item["iNy0ZU5M"] = std::to_string(data.m_Element);
			item["btZizNep"] = data.m_HairID;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
