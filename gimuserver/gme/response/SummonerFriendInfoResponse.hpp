#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SummonerFriendInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "PAd9aS1H"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_Sex = 0;
		uint32_t m_Element = 0;
		std::string m_ArmID = "";
		std::string m_HairID = "";
		uint32_t m_SummonerLv = 0;
		uint32_t m_e7DK0FQT = 0;
		uint32_t m_67CApcti = 0;
		uint32_t m_q08xLEsy = 0;
		uint32_t m_PWXu25cg = 0;
		std::string m_AbilityInfo = "";
		uint32_t m_ArmLv = 0;
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
			item["9i2xhMaJ"] = std::to_string(data.m_Sex);
			item["iNy0ZU5M"] = std::to_string(data.m_Element);
			item["RVVgyuor"] = data.m_ArmID;
			item["btZizNep"] = data.m_HairID;
			item["D9wXQI2V"] = std::to_string(data.m_SummonerLv);
			item["e7DK0FQT"] = std::to_string(data.m_e7DK0FQT);
			item["67CApcti"] = std::to_string(data.m_67CApcti);
			item["q08xLEsy"] = std::to_string(data.m_q08xLEsy);
			item["PWXu25cg"] = std::to_string(data.m_PWXu25cg);
			item["mgNdrCEe"] = data.m_AbilityInfo;
			item["U8FCB2Wj"] = std::to_string(data.m_ArmLv);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
