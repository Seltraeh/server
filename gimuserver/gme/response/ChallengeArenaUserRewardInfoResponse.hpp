#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaUserRewardInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "FmYdRkjQ"; }

		uint32_t m_IkmC8gG2 = 0;
		uint32_t m_r5ulZTZv = 0;
		std::string m_TdDHf59J = "";
		uint32_t m_29MgiJIQ = 0;
		uint32_t m_wJsB35iH = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["IkmC8gG2"] = std::to_string(m_IkmC8gG2);
			v["r5ulZTZv"] = std::to_string(m_r5ulZTZv);
			v["TdDHf59J"] = m_TdDHf59J;
			v["29MgiJIQ"] = std::to_string(m_29MgiJIQ);
			v["wJsB35iH"] = std::to_string(m_wJsB35iH);
	}
};
RESPONSE_NS_END
