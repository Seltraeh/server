#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidServeInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "LApo1M3n"; }

		std::string m_h7eY3sAK = "";
		uint32_t m_gcTR4MV0 = 0;
		uint32_t m_V4Qc7mL8 = 0;
		uint32_t m_w4KCZqs2 = 0;
		std::string m_vIf2wG43 = "";
		std::string m_7eHKS3Nn = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["h7eY3sAK"] = m_h7eY3sAK;
			v["gcTR4MV0"] = std::to_string(m_gcTR4MV0);
			v["V4Qc7mL8"] = std::to_string(m_V4Qc7mL8);
			v["w4KCZqs2"] = std::to_string(m_w4KCZqs2);
			v["vIf2wG43"] = m_vIf2wG43;
			v["7eHKS3Nn"] = m_7eHKS3Nn;
	}
};
RESPONSE_NS_END
