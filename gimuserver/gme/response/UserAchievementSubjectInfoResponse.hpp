#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserAchievementSubjectInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "YTRJLG65"; }

		std::string m_M7SXoc31 = "";
		std::string m_pG2n1A28 = "";
		uint32_t m_9g4hdDJn = 0;
		uint32_t m_0B5bQgZh = 0;
		uint32_t m_VDKB0Y5h = 0;
		uint32_t m_rPk8gtY5 = 0;
		uint32_t m_dJNpLc81 = 0;
		std::string m_AKtVeaZ2 = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["M7SXoc31"] = m_M7SXoc31;
			v["pG2n1A28"] = m_pG2n1A28;
			v["9g4hdDJn"] = std::to_string(m_9g4hdDJn);
			v["0B5bQgZh"] = std::to_string(m_0B5bQgZh);
			v["VDKB0Y5h"] = std::to_string(m_VDKB0Y5h);
			v["rPk8gtY5"] = std::to_string(m_rPk8gtY5);
			v["dJNpLc81"] = std::to_string(m_dJNpLc81);
			v["AKtVeaZ2"] = m_AKtVeaZ2;
	}
};
RESPONSE_NS_END
