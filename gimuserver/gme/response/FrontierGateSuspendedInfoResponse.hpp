#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FrontierGateSuspendedInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "gmeGFd2s"; }

		uint32_t m_FrogateID = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["hBNPQAU0"] = std::to_string(m_FrogateID);
	}
};
RESPONSE_NS_END
