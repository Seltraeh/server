#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeMisUserInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "cv9rC2KN"; }

		std::string m_MissionID = "";
		std::string m_Score = "";
		uint32_t m_StartFlag = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["j28VNcUW"] = m_MissionID;
			v["TPR79fyI"] = m_Score;
			v["QG5kcBz6"] = std::to_string(m_StartFlag);
	}
};
RESPONSE_NS_END
