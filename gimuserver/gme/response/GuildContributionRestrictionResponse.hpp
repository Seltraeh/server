#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildContributionRestrictionResponse : public IResponse
{
	const char* getGroupName() const override { return "by8ad3ga"; }

		std::string m_h6UL9A1B = "";
		uint32_t m_ba3Dba91 = 0;
		uint32_t m_Najhr8m6 = 0;
		uint32_t m_HTVh8a65 = 0;
		uint32_t m_z3bD10bu = 0;
		uint32_t m_idRYSXQR = 0;
		uint32_t m_dHODYWyi = 0;
		uint32_t m_fE2d6ivS = 0;
		uint32_t m_cEsJzMhy = 0;
		uint32_t m_yba3la1b = 0;
		uint32_t m_b8aMhyLs = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["h6UL9A1B"] = m_h6UL9A1B;
			v["ba3Dba91"] = std::to_string(m_ba3Dba91);
			v["Najhr8m6"] = std::to_string(m_Najhr8m6);
			v["HTVh8a65"] = std::to_string(m_HTVh8a65);
			v["z3bD10bu"] = std::to_string(m_z3bD10bu);
			v["idRYSXQR"] = std::to_string(m_idRYSXQR);
			v["dHODYWyi"] = std::to_string(m_dHODYWyi);
			v["fE2d6ivS"] = std::to_string(m_fE2d6ivS);
			v["cEsJzMhy"] = std::to_string(m_cEsJzMhy);
			v["yba3la1b"] = std::to_string(m_yba3la1b);
			v["b8aMhyLs"] = std::to_string(m_b8aMhyLs);
	}
};
RESPONSE_NS_END
