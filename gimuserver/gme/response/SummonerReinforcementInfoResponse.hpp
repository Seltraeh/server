#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SummonerReinforcementInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "6gEp9sFW"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_FriendType = 0;
		uint32_t m_LastLoginDate = 0;
		uint32_t m_Sex = 0;
		uint32_t m_Element = 0;
		std::string m_ArmID = "";
		uint32_t m_D9wXQI2V = 0;
		uint32_t m_e7DK0FQT = 0;
		uint32_t m_67CApcti = 0;
		uint32_t m_q08xLEsy = 0;
		uint32_t m_PWXu25cg = 0;
		std::string m_AbilityInfo = "";
		uint32_t m_ArmLevel = 0;
		uint32_t m_ArmExp = 0;
		std::string m_EqpExSkillID1 = "";
		uint32_t m_FriendPoint = 0;
		uint32_t m_NormalFriendPoint = 0;
		uint32_t m_TeamLevel = 0;
		std::string m_FriendMessage = "";
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
			item["96Nxs2WQ"] = std::to_string(data.m_FriendType);
			item["0CAQ6wUe"] = std::to_string(data.m_LastLoginDate);
			item["9i2xhMaJ"] = std::to_string(data.m_Sex);
			item["iNy0ZU5M"] = std::to_string(data.m_Element);
			item["RVVgyuor"] = data.m_ArmID;
			item["D9wXQI2V"] = std::to_string(data.m_D9wXQI2V);
			item["e7DK0FQT"] = std::to_string(data.m_e7DK0FQT);
			item["67CApcti"] = std::to_string(data.m_67CApcti);
			item["q08xLEsy"] = std::to_string(data.m_q08xLEsy);
			item["PWXu25cg"] = std::to_string(data.m_PWXu25cg);
			item["mgNdrCEe"] = data.m_AbilityInfo;
			item["U8FCB2Wj"] = std::to_string(data.m_ArmLevel);
			item["NqVAPbLC"] = std::to_string(data.m_ArmExp);
			item["Ge8Yo32T"] = data.m_EqpExSkillID1;
			item["J3stQ7jd"] = std::to_string(data.m_FriendPoint);
			item["w28VsQ7h"] = std::to_string(data.m_NormalFriendPoint);
			item["2Fh3J7ng"] = std::to_string(data.m_TeamLevel);
			item["bM7RLu5K"] = data.m_FriendMessage;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
