#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidRoomFriendGetResponse : public IResponse
{
	const char* getGroupName() const override { return "ft4zY6Uv"; }

		std::string m_UserID = "";
		std::string m_HandleName = "";
		uint32_t m_FriendType = 0;
		uint32_t m_LastLoginDate = 0;
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
		uint32_t m_TodayYale = 0;
		uint32_t m_Favorite = 0;
		uint32_t m_RankingPoint = 0;
		uint32_t m_nBTx56W9 = 0;
		uint32_t m_3NbeC8AB = 0;
		uint32_t m_UnitImgType = 0;
		uint32_t m_DeckNo = 0;
		uint32_t m_Priority = 0;
		uint32_t m_jkldTrhL = 0;
		uint32_t m_9i2xhMaJ = 0;
		uint32_t m_iNy0ZU5M = 0;
		uint32_t m_U8FCB2Wj = 0;
		uint32_t m_ReqTime = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["h7eY3sAK"] = m_UserID;
			v["B5JQyV8j"] = m_HandleName;
			v["96Nxs2WQ"] = std::to_string(m_FriendType);
			v["0CAQ6wUe"] = std::to_string(m_LastLoginDate);
			v["4A6LzBxr"] = std::to_string(m_4A6LzBxr);
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
			v["a1Jp3TVb"] = std::to_string(m_TodayYale);
			v["5JbjC3Pp"] = std::to_string(m_Favorite);
			v["U4pMNjy0"] = std::to_string(m_RankingPoint);
			v["nBTx56W9"] = std::to_string(m_nBTx56W9);
			v["3NbeC8AB"] = std::to_string(m_3NbeC8AB);
			v["2pAyFjmZ"] = std::to_string(m_UnitImgType);
			v["zsiAn9P1"] = std::to_string(m_DeckNo);
			v["yu18xScw"] = std::to_string(m_Priority);
			v["jkldTrhL"] = std::to_string(m_jkldTrhL);
			v["9i2xhMaJ"] = std::to_string(m_9i2xhMaJ);
			v["iNy0ZU5M"] = std::to_string(m_iNy0ZU5M);
			v["U8FCB2Wj"] = std::to_string(m_U8FCB2Wj);
			v["NqVAPbLC"] = std::to_string(m_ReqTime);
	}
};
RESPONSE_NS_END
