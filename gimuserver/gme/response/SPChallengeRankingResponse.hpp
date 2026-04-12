#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SPChallengeRankingResponse : public IResponse
{
	const char* getGroupName() const override { return "Q63q1b5G"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_TeamLv = 0;
		std::string m_UnitID = "";
		uint32_t m_UnitLv = 0;
		uint32_t m_MaxScore = 0;
		uint32_t m_RankOrder = 0;
		uint32_t m_MaxProgress = 0;
		uint32_t m_InfoType = 0;
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
			item["2Fh3J7ng"] = std::to_string(data.m_TeamLv);
			item["pn16CNah"] = data.m_UnitID;
			item["4A6LzBxr"] = std::to_string(data.m_UnitLv);
			item["U4pMNjy0"] = std::to_string(data.m_MaxScore);
			item["vV4P53m7"] = std::to_string(data.m_RankOrder);
			item["pG2n1A28"] = std::to_string(data.m_MaxProgress);
			item["NH65Wj0f"] = std::to_string(data.m_InfoType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
