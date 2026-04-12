#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserClearMissionInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "UT1SVg59"; }

		std::string m_UserID = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["h7eY3sAK"] = m_UserID;
	}
};
RESPONSE_NS_END
