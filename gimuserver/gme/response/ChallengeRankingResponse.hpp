#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeRankingResponse : public IResponse
{
	const char* getGroupName() const override { return "Hmiwj75u"; }

	struct Data {
		uint32_t m_TeamLv = 0;
		uint32_t m_UnitLv = 0;
		std::string m_Point1 = "";
		uint32_t m_Point2 = 0;
		uint32_t m_Point3 = 0;
		uint32_t m_RankOrder = 0;
		uint32_t m_InfoType = 0;
		uint32_t m_Rank = 0;
		std::string m_MissionID = "";
		uint32_t m_UnitImgType = 0;
		uint32_t m_Ep3Flg = 0;
		uint32_t m_Sex = 0;
		uint32_t m_Element = 0;
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
			item["2Fh3J7ng"] = std::to_string(data.m_TeamLv);
			item["4A6LzBxr"] = std::to_string(data.m_UnitLv);
			item["U4pMNjy0"] = data.m_Point1;
			item["pG2n1A28"] = std::to_string(data.m_Point2);
			item["2Cn0bWt9"] = std::to_string(data.m_Point3);
			item["vV4P53m7"] = std::to_string(data.m_RankOrder);
			item["NH65Wj0f"] = std::to_string(data.m_InfoType);
			item["9w5e3ZJB"] = std::to_string(data.m_Rank);
			item["j28VNcUW"] = data.m_MissionID;
			item["2pAyFjmZ"] = std::to_string(data.m_UnitImgType);
			item["jkldTrhL"] = std::to_string(data.m_Ep3Flg);
			item["9i2xhMaJ"] = std::to_string(data.m_Sex);
			item["iNy0ZU5M"] = std::to_string(data.m_Element);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
