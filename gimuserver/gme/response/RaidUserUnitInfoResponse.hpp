#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidUserUnitInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "CEVQ4p7A"; }

	struct Data {
		std::string m_UserID = "";
		std::string m_edy7fq3L = "";
		std::string m_pn16CNah = "";
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
		std::string m_nj9Lw7mV = "";
		uint32_t m_3NbeC8AB = 0;
		std::string m_iEFZ6H19 = "";
		uint32_t m_RQ5GnFE2 = 0;
		uint32_t m_ReceiveDate = 0;
		uint32_t m_ExtCnt = 0;
		std::string m_0R3qTPK9 = "";
		std::string m_Ge8Yo32T = "";
		std::string m_RXfC31FA = "";
		std::string m_mZA7fH2v = "";
		std::string m_cP83zNsv = "";
		std::string m_LjY4DfRg = "";
		std::string m_AddExtraPassiveSkillID = "";
		uint32_t m_UnitImgType = 0;
		std::string m_FeSkillInfo = "";
		uint32_t m_jkldTrhL = 0;
		uint32_t m_9i2xhMaJ = 0;
		uint32_t m_Element = 0;
		std::string m_btZizNep = "";
		std::string m_RVVgyuor = "";
		uint32_t m_U8FCB2Wj = 0;
		uint32_t m_NqVAPbLC = 0;
		std::string m_mgNdrCEe = "";
		uint32_t m_FriendID = 0;
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
			item["pn16CNah"] = data.m_pn16CNah;
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
			item["nj9Lw7mV"] = data.m_nj9Lw7mV;
			item["3NbeC8AB"] = std::to_string(data.m_3NbeC8AB);
			item["iEFZ6H19"] = data.m_iEFZ6H19;
			item["RQ5GnFE2"] = std::to_string(data.m_RQ5GnFE2);
			item["Bvkx8s6M"] = std::to_string(data.m_ReceiveDate);
			item["5gXxT7LZ"] = std::to_string(data.m_ExtCnt);
			item["0R3qTPK9"] = data.m_0R3qTPK9;
			item["Ge8Yo32T"] = data.m_Ge8Yo32T;
			item["RXfC31FA"] = data.m_RXfC31FA;
			item["mZA7fH2v"] = data.m_mZA7fH2v;
			item["cP83zNsv"] = data.m_cP83zNsv;
			item["LjY4DfRg"] = data.m_LjY4DfRg;
			item["T4rewHa9"] = data.m_AddExtraPassiveSkillID;
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			item["Fnxab5CN"] = data.m_FeSkillInfo;
			item["jkldTrhL"] = std::to_string(data.m_jkldTrhL);
			item["9i2xhMaJ"] = std::to_string(data.m_9i2xhMaJ);
			item["iNy0ZU5M"] = std::to_string(data.m_Element);
			item["btZizNep"] = data.m_btZizNep;
			item["RVVgyuor"] = data.m_RVVgyuor;
			item["U8FCB2Wj"] = std::to_string(data.m_U8FCB2Wj);
			item["NqVAPbLC"] = std::to_string(data.m_NqVAPbLC);
			item["mgNdrCEe"] = data.m_mgNdrCEe;
			item["98WfKiyA"] = std::to_string(data.m_FriendID);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
