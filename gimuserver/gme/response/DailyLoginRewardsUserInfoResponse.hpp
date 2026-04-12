#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct DailyLoginRewardsUserInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Drudr2w5"; }

		uint32_t m_XIvaD6Jp = 0;
		uint32_t m_35JXN4Ay = 0;
		uint32_t m_5xStG99s = 0;
		uint32_t m_ad6i23pO = 0;
		std::string m_u8iD6ka7 = "";
		uint32_t m_outas79f = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["XIvaD6Jp"] = std::to_string(m_XIvaD6Jp);
			v["35JXN4Ay"] = std::to_string(m_35JXN4Ay);
			v["5xStG99s"] = std::to_string(m_5xStG99s);
			v["ad6i23pO"] = std::to_string(m_ad6i23pO);
			v["u8iD6ka7"] = m_u8iD6ka7;
			v["outas79f"] = std::to_string(m_outas79f);
	}
};
RESPONSE_NS_END
