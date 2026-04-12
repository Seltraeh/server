#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildGuardianSummonResponse : public IResponse
{
	const char* getGroupName() const override { return "asl26Gjz"; }

		uint32_t m_Najhr8m6 = 0;
		uint32_t m_HTVh8a65 = 0;
		uint32_t m_mn5Tj3fz = 0;
		uint32_t m_20qd9shE = 0;
		std::string m_Al1icBG = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["Najhr8m6"] = std::to_string(m_Najhr8m6);
			v["HTVh8a65"] = std::to_string(m_HTVh8a65);
			v["mn5Tj3fz"] = std::to_string(m_mn5Tj3fz);
			v["20qd9shE"] = std::to_string(m_20qd9shE);
			v["Al1icBG"] = m_Al1icBG;
	}
};
RESPONSE_NS_END
