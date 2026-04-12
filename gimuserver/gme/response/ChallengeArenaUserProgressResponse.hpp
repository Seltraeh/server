#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaUserProgressResponse : public IResponse
{
	const char* getGroupName() const override { return "Ki8Okx9B"; }

		std::string m_h7eY3sAK = "";
		uint32_t m_e34YV1Ey = 0;
		uint32_t m_rX8AFE5T = 0;
		uint32_t m_0Rhjwagb = 0;
		uint32_t m_EAGtIJhK = 0;
		uint32_t m_pG2n1A28 = 0;
		uint32_t m_FqwvgbKS = 0;
		uint32_t m_D9wXQI2V = 0;
		uint32_t m_B5JQyV8j = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["h7eY3sAK"] = m_h7eY3sAK;
			v["e34YV1Ey"] = std::to_string(m_e34YV1Ey);
			v["rX8AFE5T"] = std::to_string(m_rX8AFE5T);
			v["0Rhjwagb"] = std::to_string(m_0Rhjwagb);
			v["EAGtIJhK"] = std::to_string(m_EAGtIJhK);
			v["pG2n1A28"] = std::to_string(m_pG2n1A28);
			v["FqwvgbKS"] = std::to_string(m_FqwvgbKS);
			v["D9wXQI2V"] = std::to_string(m_D9wXQI2V);
			v["B5JQyV8j"] = std::to_string(m_B5JQyV8j);
	}
};
RESPONSE_NS_END
