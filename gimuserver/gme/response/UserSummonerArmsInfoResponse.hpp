#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserSummonerArmsInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "dhMmbm5p"; }

	struct Data {
		std::string m_UserID = "";
		uint32_t m_Exp = 0;
	};

	std::vector<Data> items;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
		Json::Value arr(Json::arrayValue);
		
		for (const auto& data : items) {
			Json::Value item;
			item["h7eY3sAK"] = data.m_UserID;
			item["d96tuT2E"] = std::to_string(data.m_Exp);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
