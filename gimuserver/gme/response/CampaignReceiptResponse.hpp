#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct CampaignReceiptResponse : public IResponse
{
	const char* getGroupName() const override { return "4MCxgS5p"; }

		std::string m_pCIRMw04 = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["pCIRMw04"] = m_pCIRMw04;
	}
};
RESPONSE_NS_END
