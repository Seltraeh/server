#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FrontierGateUserUnitInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "52xDBRGr"; }

	struct Data {
		std::string m_UserID = "";
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
		std::string m_LeaderSkillID = "";
		uint32_t m_3NbeC8AB = 0;
		std::string m_iEFZ6H19 = "";
		uint32_t m_RQ5GnFE2 = 0;
		std::string m_AddExtraPassiveSkillID = "";
		uint32_t m_UnitImgType = 0;
		uint32_t m_FeSkillInfo = 0;
		uint32_t m_jkldTrhL = 0;
		uint32_t m_9i2xhMaJ = 0;
		uint32_t m_Element = 0;
		std::string m_btZizNep = "";
		uint32_t m_U8FCB2Wj = 0;
		uint32_t m_NqVAPbLC = 0;
		std::string m_mgNdrCEe = "";
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
			item["oS3kTZ2W"] = data.m_LeaderSkillID;
			item["3NbeC8AB"] = std::to_string(data.m_3NbeC8AB);
			item["iEFZ6H19"] = data.m_iEFZ6H19;
			item["RQ5GnFE2"] = std::to_string(data.m_RQ5GnFE2);
			item["Ge8Yo32T"] = data.m_AddExtraPassiveSkillID;
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			item["49sa3sld"] = std::to_string(data.m_FeSkillInfo);
			item["jkldTrhL"] = std::to_string(data.m_jkldTrhL);
			item["9i2xhMaJ"] = std::to_string(data.m_9i2xhMaJ);
			item["iNy0ZU5M"] = std::to_string(data.m_Element);
			item["btZizNep"] = data.m_btZizNep;
			item["U8FCB2Wj"] = std::to_string(data.m_U8FCB2Wj);
			item["NqVAPbLC"] = std::to_string(data.m_NqVAPbLC);
			item["mgNdrCEe"] = data.m_mgNdrCEe;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
