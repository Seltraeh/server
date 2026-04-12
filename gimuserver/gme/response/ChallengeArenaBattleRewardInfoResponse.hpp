#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaBattleRewardInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "CJyIGhAl"; }

		uint32_t m_fM3Zkt4F = 0;
		uint32_t m_IkmC8gG2 = 0;
		uint32_t m_wJsB35iH = 0;
		std::string m_TdDHf59J = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["fM3Zkt4F"] = std::to_string(m_fM3Zkt4F);
			v["IkmC8gG2"] = std::to_string(m_IkmC8gG2);
			v["wJsB35iH"] = std::to_string(m_wJsB35iH);
			v["TdDHf59J"] = m_TdDHf59J;
	}
};
RESPONSE_NS_END
