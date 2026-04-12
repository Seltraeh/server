#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildBoardInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "adf38ba1"; }

	struct Data {
		uint32_t m_GuildBoardID = 0;
		uint32_t m_GuildID = 0;
		std::string m_UserID = "";
		uint32_t m_TeamLevel = 0;
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
			item["Kd8i2bKe"] = std::to_string(data.m_GuildBoardID);
			item["sD73jd20"] = std::to_string(data.m_GuildID);
			item["h7eY3sAK"] = data.m_UserID;
			item["2Fh3J7ng"] = std::to_string(data.m_TeamLevel);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
