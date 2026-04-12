#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidGetChatLogResponse : public IResponse
{
	const char* getGroupName() const override { return "WCA9VhL3"; }

		uint32_t m_LogId = 0;
		uint32_t m_LogDate = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["NLKce95n"] = std::to_string(m_LogId);
			v["ToSxkU34"] = std::to_string(m_LogDate);
	}
};
RESPONSE_NS_END
