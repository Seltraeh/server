#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildBattleScoreInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "lHDysPRp"; }

		std::string m_gE2NN2xi = "";
		std::string m_qXCIfZIk = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["gE2NN2xi"] = m_gE2NN2xi;
			v["qXCIfZIk"] = m_qXCIfZIk;
	}
};
RESPONSE_NS_END
