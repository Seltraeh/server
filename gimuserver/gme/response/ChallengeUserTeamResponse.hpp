#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeUserTeamResponse : public IResponse
{
	const char* getGroupName() const override { return "kN2i7qds"; }

		uint32_t m_FrogateScore = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["vvXp7Uek"] = std::to_string(m_FrogateScore);
	}
};
RESPONSE_NS_END
