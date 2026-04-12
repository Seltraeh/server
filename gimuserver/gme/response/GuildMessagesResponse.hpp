#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildMessagesResponse : public IResponse
{
	const char* getGroupName() const override { return "T_GUILD_MESSAGES"; }

		uint32_t m_GuildId = 0;
		uint32_t m_MessageID = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["sD73jd20"] = std::to_string(m_GuildId);
			v["bpY1eo2c"] = std::to_string(m_MessageID);
	}
};
RESPONSE_NS_END
