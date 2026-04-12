#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaOpponentTeamInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Tmx1rq96"; }

		std::string m_h7eY3sAK = "";
		uint32_t m_Umbv916t = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["h7eY3sAK"] = m_h7eY3sAK;
			v["Umbv916t"] = std::to_string(m_Umbv916t);
	}
};
RESPONSE_NS_END
