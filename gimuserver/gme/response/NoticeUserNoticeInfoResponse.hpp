#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct NoticeUserNoticeInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "CihCiL05"; }

		uint32_t m_h7eY3sAK = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["h7eY3sAK"] = std::to_string(m_h7eY3sAK);
	}
};
RESPONSE_NS_END
