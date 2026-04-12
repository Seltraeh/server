#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GachaInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "1IR86sAv"; }

	struct Data {
		std::string m_GachaID = "";
		std::string m_GachaCampainInfo = "";
		uint32_t m_NeedBraveCoin = 0;
		uint32_t m_BtnImg = 0;
		uint32_t m_GachaType = 0;
		uint32_t m_Priority = 0;
		uint32_t m_StartDate = 0;
		uint32_t m_StartHour = 0;
		uint32_t m_EndHour = 0;
		uint32_t m_NeedFreindPoint = 0;
		uint32_t m_OnceDayFlg = 0;
		std::string m_DoorImg = "";
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
			item["7Ffmi96v"] = data.m_GachaID;
			item["8HM3v3gg"] = data.m_GachaCampainInfo;
			item["03UGMHxF"] = std::to_string(data.m_NeedBraveCoin);
			item["W9ABuJj2"] = std::to_string(data.m_BtnImg);
			item["S1oz60Hc"] = std::to_string(data.m_GachaType);
			item["yu18xScw"] = std::to_string(data.m_Priority);
			item["qA7M9EjP"] = std::to_string(data.m_StartDate);
			item["2HY3jpgu"] = std::to_string(data.m_StartHour);
			item["v9TR3cDz"] = std::to_string(data.m_EndHour);
			item["J3stQ7jd"] = std::to_string(data.m_NeedFreindPoint);
			item["4tswNoV9"] = std::to_string(data.m_OnceDayFlg);
			item["uKYf13AH"] = data.m_DoorImg;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
