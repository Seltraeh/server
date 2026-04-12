#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FeatureGatingInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "2375D38i"; }

		uint32_t m_FeatureID = 0;
		std::string m_FeatureName = "";
		uint32_t m_ReqID = 0;
		uint32_t m_ReqValue = 0;
		uint32_t m_DungeonID = 0;
		uint32_t m_StartDate = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["e63D1BV0"] = std::to_string(m_FeatureID);
			v["De5dk137"] = m_FeatureName;
			v["3NHiD5Ei"] = std::to_string(m_ReqID);
			v["37DXiFyl"] = std::to_string(m_ReqValue);
			v["MHx05sXt"] = std::to_string(m_DungeonID);
			v["qA7M9EjP"] = std::to_string(m_StartDate);
	}
};
RESPONSE_NS_END
