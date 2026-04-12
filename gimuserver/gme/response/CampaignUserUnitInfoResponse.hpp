#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct CampaignUserUnitInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "W6F9ECH5"; }

	struct Data {
		std::string m_UserID = "";
		std::string m_edy7fq3L = "";
		uint32_t m_nBTx56W9 = 0;
		uint32_t m_D9wXQI2V = 0;
		uint32_t m_Exp = 0;
		uint32_t m_TotalExp = 0;
		uint32_t m_e7DK0FQT = 0;
		uint32_t m_cuIWp89g = 0;
		uint32_t m_TokWs1B3 = 0;
		uint32_t m_67CApcti = 0;
		uint32_t m_RT4CtH5d = 0;
		uint32_t m_t4m1RH6Y = 0;
		uint32_t m_q08xLEsy = 0;
		uint32_t m_GcMD0hy6 = 0;
		uint32_t m_e6mY8Z0k = 0;
		uint32_t m_PWXu25cg = 0;
		uint32_t m_C1HZr3pb = 0;
		uint32_t m_X6jf8DUw = 0;
		uint32_t m_Element = 0;
		uint32_t m_3NbeC8AB = 0;
		uint32_t m_RQ5GnFE2 = 0;
		uint32_t m_UnitImgType = 0;
		uint32_t m_jkldTrhL = 0;
		uint32_t m_9i2xhMaJ = 0;
		uint32_t m_U8FCB2Wj = 0;
		uint32_t m_FeSkillInfo = 0;
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
			item["edy7fq3L"] = data.m_edy7fq3L;
			item["nBTx56W9"] = std::to_string(data.m_nBTx56W9);
			item["D9wXQI2V"] = std::to_string(data.m_D9wXQI2V);
			item["d96tuT2E"] = std::to_string(data.m_Exp);
			item["gQInj3H6"] = std::to_string(data.m_TotalExp);
			item["e7DK0FQT"] = std::to_string(data.m_e7DK0FQT);
			item["cuIWp89g"] = std::to_string(data.m_cuIWp89g);
			item["TokWs1B3"] = std::to_string(data.m_TokWs1B3);
			item["67CApcti"] = std::to_string(data.m_67CApcti);
			item["RT4CtH5d"] = std::to_string(data.m_RT4CtH5d);
			item["t4m1RH6Y"] = std::to_string(data.m_t4m1RH6Y);
			item["q08xLEsy"] = std::to_string(data.m_q08xLEsy);
			item["GcMD0hy6"] = std::to_string(data.m_GcMD0hy6);
			item["e6mY8Z0k"] = std::to_string(data.m_e6mY8Z0k);
			item["PWXu25cg"] = std::to_string(data.m_PWXu25cg);
			item["C1HZr3pb"] = std::to_string(data.m_C1HZr3pb);
			item["X6jf8DUw"] = std::to_string(data.m_X6jf8DUw);
			item["iNy0ZU5M"] = std::to_string(data.m_Element);
			item["3NbeC8AB"] = std::to_string(data.m_3NbeC8AB);
			item["RQ5GnFE2"] = std::to_string(data.m_RQ5GnFE2);
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			item["jkldTrhL"] = std::to_string(data.m_jkldTrhL);
			item["9i2xhMaJ"] = std::to_string(data.m_9i2xhMaJ);
			item["U8FCB2Wj"] = std::to_string(data.m_U8FCB2Wj);
			item["NqVAPbLC"] = std::to_string(data.m_FeSkillInfo);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
