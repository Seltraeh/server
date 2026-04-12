#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserUnitResummonInfoSgResponse : public IResponse
{
	const char* getGroupName() const override { return "sdu3jvhH"; }

		std::string m_DoYmd = "";
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
		std::string m_ExtPlusCnt = "";
		uint32_t m_3NbeC8AB = 0;
		std::string m_iEFZ6H19 = "";
		uint32_t m_RQ5GnFE2 = 0;
		std::string m_0R3qTPK9 = "";
		uint32_t m_ReceiveDate = 0;
		std::string m_GachaEffectId = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["edy7fq3L"] = m_DoYmd;
			v["nBTx56W9"] = std::to_string(m_nBTx56W9);
			v["D9wXQI2V"] = std::to_string(m_D9wXQI2V);
			v["d96tuT2E"] = std::to_string(m_Exp);
			v["gQInj3H6"] = std::to_string(m_TotalExp);
			v["e7DK0FQT"] = std::to_string(m_e7DK0FQT);
			v["cuIWp89g"] = std::to_string(m_cuIWp89g);
			v["TokWs1B3"] = std::to_string(m_TokWs1B3);
			v["67CApcti"] = std::to_string(m_67CApcti);
			v["RT4CtH5d"] = std::to_string(m_RT4CtH5d);
			v["t4m1RH6Y"] = std::to_string(m_t4m1RH6Y);
			v["q08xLEsy"] = std::to_string(m_q08xLEsy);
			v["GcMD0hy6"] = std::to_string(m_GcMD0hy6);
			v["e6mY8Z0k"] = std::to_string(m_e6mY8Z0k);
			v["PWXu25cg"] = std::to_string(m_PWXu25cg);
			v["C1HZr3pb"] = std::to_string(m_C1HZr3pb);
			v["X6jf8DUw"] = std::to_string(m_X6jf8DUw);
			v["xujp1nz2"] = m_ExtPlusCnt;
			v["3NbeC8AB"] = std::to_string(m_3NbeC8AB);
			v["iEFZ6H19"] = m_iEFZ6H19;
			v["RQ5GnFE2"] = std::to_string(m_RQ5GnFE2);
			v["0R3qTPK9"] = m_0R3qTPK9;
			v["Bvkx8s6M"] = std::to_string(m_ReceiveDate);
			v["u0vkt9yH"] = m_GachaEffectId;
	}
};
RESPONSE_NS_END
