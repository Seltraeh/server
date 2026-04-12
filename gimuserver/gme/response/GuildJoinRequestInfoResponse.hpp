#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildJoinRequestInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "aj38Jk10"; }

		uint32_t m_InviteID = 0;
		std::string m_UserID = "";
		uint32_t m_GuildId = 0;
		std::string m_HandleName = "";
		uint32_t m_TeamLevel = 0;
		std::string m_FriendId = "";
		uint32_t m_UnitImgType = 0;
		uint32_t m_4A6LzBxr = 0;
		std::string m_RequestDate = "";
		uint32_t m_RequestType = 0;
		std::string m_ArenaRankID = "";
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
		std::string m_ArenaRankID = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["7ad38bad"] = std::to_string(m_InviteID);
			v["h7eY3sAK"] = m_UserID;
			v["sD73jd20"] = std::to_string(m_GuildId);
			v["B5JQyV8j"] = m_HandleName;
			v["2Fh3J7ng"] = std::to_string(m_TeamLevel);
			v["98WfKiyA"] = m_FriendId;
			v["2pAyFjmZ"] = std::to_string(m_UnitImgType);
			v["4A6LzBxr"] = std::to_string(m_4A6LzBxr);
			v["SL7ySqi8"] = m_RequestDate;
			v["xvkLFco0"] = std::to_string(m_RequestType);
			v["JmFn3g9t"] = m_ArenaRankID;
			v["e7DK0FQT"] = std::to_string(m_e7DK0FQT);
			v["67CApcti"] = std::to_string(m_67CApcti);
			v["q08xLEsy"] = std::to_string(m_q08xLEsy);
			v["PWXu25cg"] = std::to_string(m_PWXu25cg);
			v["cuIWp89g"] = std::to_string(m_cuIWp89g);
			v["TokWs1B3"] = std::to_string(m_TokWs1B3);
			v["RT4CtH5d"] = std::to_string(m_RT4CtH5d);
			v["t4m1RH6Y"] = std::to_string(m_t4m1RH6Y);
			v["GcMD0hy6"] = std::to_string(m_GcMD0hy6);
			v["e6mY8Z0k"] = std::to_string(m_e6mY8Z0k);
			v["C1HZr3pb"] = std::to_string(m_C1HZr3pb);
			v["X6jf8DUw"] = std::to_string(m_X6jf8DUw);
			v["Ge8Yo32T"] = m_Ge8Yo32T;
			v["3NbeC8AB"] = std::to_string(m_3NbeC8AB);
			v["iEFZ6H19"] = m_iEFZ6H19;
			v["RQ5GnFE2"] = std::to_string(m_RQ5GnFE2);
			v["JmFn3g9t"] = m_ArenaRankID;
	}
};
RESPONSE_NS_END
