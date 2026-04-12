#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FriendInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "tojMy68W"; }

	struct Data {
		std::string m_UserID = "";
		std::string m_HandleName = "";
		uint32_t m_TeamLv = 0;
		uint32_t m_FriendType = 0;
		uint32_t m_LastLoginDate = 0;
		std::string m_pn16CNah = "";
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
		std::string m_WantGift = "";
		uint32_t m_Favorite = 0;
		std::string m_Ge8Yo32T = "";
		std::string m_mZA7fH2v = "";
		std::string m_FriendID = "";
		std::string m_FriendMessage = "";
		uint32_t m_FriendMessageChangeTime = 0;
		std::string m_ArenaRankID = "";
		uint32_t m_RankingPoint = 0;
		uint32_t m_nBTx56W9 = 0;
		std::string m_nj9Lw7mV = "";
		uint32_t m_3NbeC8AB = 0;
		std::string m_iEFZ6H19 = "";
		uint32_t m_RQ5GnFE2 = 0;
		std::string m_ReqTime = "";
		uint32_t m_ElapsedAgreeTime = 0;
		uint32_t m_7x3pPB2C = 0;
		uint32_t m_Sv80kL5r = 0;
		std::string m_cP83zNsv = "";
		std::string m_LjY4DfRg = "";
		uint32_t m_UnitImgType = 0;
		uint32_t m_DeckNo = 0;
		uint32_t m_Priority = 0;
		uint32_t m_jkldTrhL = 0;
		uint32_t m_9i2xhMaJ = 0;
		uint32_t m_iNy0ZU5M = 0;
		std::string m_btZizNep = "";
		std::string m_RVVgyuor = "";
		uint32_t m_U8FCB2Wj = 0;
		uint32_t m_NqVAPbLC = 0;
		uint32_t m_GuildID = 0;
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
			item["B5JQyV8j"] = data.m_HandleName;
			item["2Fh3J7ng"] = std::to_string(data.m_TeamLv);
			item["96Nxs2WQ"] = std::to_string(data.m_FriendType);
			item["0CAQ6wUe"] = std::to_string(data.m_LastLoginDate);
			item["pn16CNah"] = data.m_pn16CNah;
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
			item["a1Jp3TVb"] = std::to_string(data.m_TodayYale);
			item["s2WnRw9N"] = data.m_WantGift;
			item["5JbjC3Pp"] = std::to_string(data.m_Favorite);
			item["Ge8Yo32T"] = data.m_Ge8Yo32T;
			item["mZA7fH2v"] = data.m_mZA7fH2v;
			item["98WfKiyA"] = data.m_FriendID;
			item["bM7RLu5K"] = data.m_FriendMessage;
			item["8DtoZdXE"] = std::to_string(data.m_FriendMessageChangeTime);
			item["JmFn3g9t"] = data.m_ArenaRankID;
			item["U4pMNjy0"] = std::to_string(data.m_RankingPoint);
			item["nBTx56W9"] = std::to_string(data.m_nBTx56W9);
			item["nj9Lw7mV"] = data.m_nj9Lw7mV;
			item["3NbeC8AB"] = std::to_string(data.m_3NbeC8AB);
			item["iEFZ6H19"] = data.m_iEFZ6H19;
			item["RQ5GnFE2"] = std::to_string(data.m_RQ5GnFE2);
			item["3InKeya4"] = data.m_ReqTime;
			item["paND1zM8"] = std::to_string(data.m_ElapsedAgreeTime);
			item["7x3pPB2C"] = std::to_string(data.m_7x3pPB2C);
			item["Sv80kL5r"] = std::to_string(data.m_Sv80kL5r);
			item["cP83zNsv"] = data.m_cP83zNsv;
			item["LjY4DfRg"] = data.m_LjY4DfRg;
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			item["zsiAn9P1"] = std::to_string(data.m_DeckNo);
			item["yu18xScw"] = std::to_string(data.m_Priority);
			item["jkldTrhL"] = std::to_string(data.m_jkldTrhL);
			item["9i2xhMaJ"] = std::to_string(data.m_9i2xhMaJ);
			item["iNy0ZU5M"] = std::to_string(data.m_iNy0ZU5M);
			item["btZizNep"] = data.m_btZizNep;
			item["RVVgyuor"] = data.m_RVVgyuor;
			item["U8FCB2Wj"] = std::to_string(data.m_U8FCB2Wj);
			item["NqVAPbLC"] = std::to_string(data.m_NqVAPbLC);
			item["mgNdrCEe"] = std::to_string(data.m_GuildID);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
