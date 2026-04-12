#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidRoomMemberListResponse : public IResponse
{
	const char* getGroupName() const override { return "b839kdi1"; }

	struct Data {
		uint32_t m_da38cka5 = 0;
		uint32_t m_RoomID = 0;
		std::string m_UserID = "";
		uint32_t m_RoomRank = 0;
		uint32_t m_StartingPoint = 0;
		uint32_t m_NowPoint = 0;
		uint32_t m_ActionPoints = 0;
		uint32_t m_ActionTimer = 0;
		uint32_t m_MaxActionPoints = 0;
		std::string m_ReviveDuration = "";
		uint32_t m_4A6LzBxr = 0;
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
		uint32_t m_X6jf8DUw = 0;
		std::string m_Ge8Yo32T = "";
		uint32_t m_3NbeC8AB = 0;
		std::string m_iEFZ6H19 = "";
		uint32_t m_RQ5GnFE2 = 0;
		uint32_t m_CurrentDeckNum = 0;
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
			item["da38cka5"] = std::to_string(data.m_da38cka5);
			item["8VYd6xSX"] = std::to_string(data.m_RoomID);
			item["h7eY3sAK"] = data.m_UserID;
			item["b9D7bi3y"] = std::to_string(data.m_RoomRank);
			item["tpuBKuJ7"] = std::to_string(data.m_StartingPoint);
			item["FOkXX7Hq"] = std::to_string(data.m_NowPoint);
			item["12bDa98b"] = std::to_string(data.m_ActionPoints);
			item["Wrefr6yA"] = std::to_string(data.m_ActionTimer);
			item["uIe1Bpq9"] = std::to_string(data.m_MaxActionPoints);
			item["jziKq19b"] = data.m_ReviveDuration;
			item["4A6LzBxr"] = std::to_string(data.m_4A6LzBxr);
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
			item["X6jf8DUw"] = std::to_string(data.m_X6jf8DUw);
			item["Ge8Yo32T"] = data.m_Ge8Yo32T;
			item["3NbeC8AB"] = std::to_string(data.m_3NbeC8AB);
			item["iEFZ6H19"] = data.m_iEFZ6H19;
			item["RQ5GnFE2"] = std::to_string(data.m_RQ5GnFE2);
			item["cP83zNsv"] = std::to_string(data.m_CurrentDeckNum);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
