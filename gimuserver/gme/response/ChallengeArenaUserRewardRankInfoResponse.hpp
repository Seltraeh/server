#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaUserRewardRankInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "UahnBfGc"; }

		uint32_t m_IkmC8gG2 = 0;
		uint32_t m_BcIqcWDM = 0;
		uint32_t m_TPR79fyI = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["IkmC8gG2"] = std::to_string(m_IkmC8gG2);
			v["BcIqcWDM"] = std::to_string(m_BcIqcWDM);
			v["TPR79fyI"] = std::to_string(m_TPR79fyI);
	}
};
RESPONSE_NS_END
