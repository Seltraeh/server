#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SPChallengeUserInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Gn2T1wgJ"; }

		std::string m_MissionID = "";
		uint32_t m_Score = 0;
		uint32_t m_StartFlag = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["j28VNcUW"] = m_MissionID;
			v["TPR79fyI"] = std::to_string(m_Score);
			v["QG5kcBz6"] = std::to_string(m_StartFlag);
	}
};
RESPONSE_NS_END
