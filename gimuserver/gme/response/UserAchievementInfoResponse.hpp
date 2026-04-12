#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserAchievementInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Bnc4LpM8"; }

		uint32_t m_idfCDG70 = 0;
		uint32_t m_HjgTfx8A = 0;
		uint32_t m_CjaJUSrs = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["idfCDG70"] = std::to_string(m_idfCDG70);
			v["HjgTfx8A"] = std::to_string(m_HjgTfx8A);
			v["CjaJUSrs"] = std::to_string(m_CjaJUSrs);
	}
};
RESPONSE_NS_END
