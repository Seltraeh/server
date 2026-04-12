#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ColosseumClassInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "HgYvs3am"; }

	struct Data {
		std::string m_ClassID = "";
		uint32_t m_CurrentPosition = 0;
		std::string m_AtkPartyInfo = "";
		uint32_t m_BattleCnt = 0;
		uint32_t m_WinCnt = 0;
		uint32_t m_AtkSeriesWinCnt = 0;
		uint32_t m_BattleCntAccept = 0;
		uint32_t m_WinCntAccept = 0;
		std::string m_BattleNum = "";
		uint32_t m_TotalCbp = 0;
		uint32_t m_AllWinCnt = 0;
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
			item["3mMAn6L5"] = data.m_ClassID;
			item["2rqxaZ6K"] = std::to_string(data.m_CurrentPosition);
			item["Enmv8X74"] = data.m_AtkPartyInfo;
			item["69vnphig"] = std::to_string(data.m_BattleCnt);
			item["8CEu9Kcm"] = std::to_string(data.m_WinCnt);
			item["7sXWfE8j"] = std::to_string(data.m_AtkSeriesWinCnt);
			item["20iEWRCV"] = std::to_string(data.m_BattleCntAccept);
			item["0rAkzg7L"] = std::to_string(data.m_WinCntAccept);
			item["1kisc6IF"] = data.m_BattleNum;
			item["z1rMbo8n"] = std::to_string(data.m_TotalCbp);
			item["PL0mqDhK"] = std::to_string(data.m_AllWinCnt);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
