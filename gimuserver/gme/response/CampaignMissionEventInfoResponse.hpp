#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct CampaignMissionEventInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "RseDpY04"; }

		uint32_t m_ReservePartyDispFlg = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["W1EkCyc3"] = std::to_string(m_ReservePartyDispFlg);
	}
};
RESPONSE_NS_END
