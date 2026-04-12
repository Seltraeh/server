#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FixedReinforcementInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "T_FIXED_REINFORCEMENT"; }

	struct Data {
		std::string m_HandleName = "";
		uint32_t m_4A6LzBxr = 0;
		std::string m_pn16CNah = "";
		uint32_t m_e7DK0FQT = 0;
		uint32_t m_67CApcti = 0;
		uint32_t m_q08xLEsy = 0;
		uint32_t m_PWXu25cg = 0;
		uint32_t m_cuIWp89g = 0;
		uint32_t m_TokWs1B3 = 0;
		uint32_t m_RT4CtH5d = 0;
		uint32_t m_t4m1RH6Y = 0;
		uint32_t m_GcMD0hy6 = 0;
		uint32_t m_e6mY8Z0k = 0;
		uint32_t m_C1HZr3pb = 0;
		uint32_t m_e6mY8Z0k_2 = 0; // duplicate "e6mY8Z0k" JSON key — overwrites above in output
		std::string m_nj9Lw7mV = "";
		uint32_t m_3NbeC8AB = 0;
		uint32_t m_nBTx56W9 = 0;
		std::string m_iEFZ6H19 = "";
		uint32_t m_RQ5GnFE2 = 0;
		std::string m_MissionID = "";
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
			item["B5JQyV8j"] = data.m_HandleName;
			item["4A6LzBxr"] = std::to_string(data.m_4A6LzBxr);
			item["pn16CNah"] = data.m_pn16CNah;
			item["e7DK0FQT"] = std::to_string(data.m_e7DK0FQT);
			item["67CApcti"] = std::to_string(data.m_67CApcti);
			item["q08xLEsy"] = std::to_string(data.m_q08xLEsy);
			item["PWXu25cg"] = std::to_string(data.m_PWXu25cg);
			item["cuIWp89g"] = std::to_string(data.m_cuIWp89g);
			item["TokWs1B3"] = std::to_string(data.m_TokWs1B3);
			item["RT4CtH5d"] = std::to_string(data.m_RT4CtH5d);
			item["t4m1RH6Y"] = std::to_string(data.m_t4m1RH6Y);
			item["GcMD0hy6"] = std::to_string(data.m_GcMD0hy6);
			item["e6mY8Z0k"] = std::to_string(data.m_e6mY8Z0k);
			item["C1HZr3pb"] = std::to_string(data.m_C1HZr3pb);
			item["e6mY8Z0k"] = std::to_string(data.m_e6mY8Z0k_2);
			item["nj9Lw7mV"] = data.m_nj9Lw7mV;
			item["3NbeC8AB"] = std::to_string(data.m_3NbeC8AB);
			item["nBTx56W9"] = std::to_string(data.m_nBTx56W9);
			item["iEFZ6H19"] = data.m_iEFZ6H19;
			item["RQ5GnFE2"] = std::to_string(data.m_RQ5GnFE2);
			item["Ge8Yo32T"] = data.m_MissionID;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
