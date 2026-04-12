#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct EventSkillInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "j2389b5D"; }

		std::string m_EventSkillId = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["nj9Lw7mV"] = m_EventSkillId;
	}
};
RESPONSE_NS_END
