#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeRewardResponse : public IResponse
{
	const char* getGroupName() const override { return "ZSC3t4Fv"; }

		uint32_t m_RewKind = 0;
		uint32_t m_RewValue = 0;
		uint32_t m_RewType = 0;
		std::string m_RewTarID = "";
		uint32_t m_RewTarCnt = 0;
		std::string m_RewParam = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["IkmC8gG2"] = std::to_string(m_RewKind);
			v["empaR60j"] = std::to_string(m_RewValue);
			v["30Kw4WBa"] = std::to_string(m_RewType);
			v["TdDHf59J"] = m_RewTarID;
			v["wJsB35iH"] = std::to_string(m_RewTarCnt);
			v["37moriMq"] = m_RewParam;
	}
};
RESPONSE_NS_END
