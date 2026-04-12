#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidBattleResultScoreResponse : public IResponse
{
	const char* getGroupName() const override { return "g71qWtnQ"; }

		std::string m_TPR79fyI = "";
		std::string m_3hPeI1RV = "";
		std::string m_dk19ak67 = "";
		std::string m_i5SNyDli = "";
		std::string m_kyCa5qvm = "";
		std::string m_8o3rdQec = "";
		std::string m_Dak68ck1 = "";
		uint32_t m_yUCruth6 = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["TPR79fyI"] = m_TPR79fyI;
			v["3hPeI1RV"] = m_3hPeI1RV;
			v["dk19ak67"] = m_dk19ak67;
			v["i5SNyDli"] = m_i5SNyDli;
			v["kyCa5qvm"] = m_kyCa5qvm;
			v["8o3rdQec"] = m_8o3rdQec;
			v["Dak68ck1"] = m_Dak68ck1;
			v["yUCruth6"] = std::to_string(m_yUCruth6);
	}
};
RESPONSE_NS_END
