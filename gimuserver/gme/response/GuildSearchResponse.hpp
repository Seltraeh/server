#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildSearchResponse : public IResponse
{
	const char* getGroupName() const override { return "kj1d80ai"; }

	struct Data {
		uint32_t m_GuildId = 0;
		std::string m_Name = "";
		uint32_t m_GuildArtId = 0;
		uint32_t m_Level = 0;
		uint32_t m_Rank = 0;
		uint32_t m_PrestigePoint = 0;
		uint32_t m_MembersCount = 0;
		std::string m_MasterName = "";
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
			item["sD73jd20"] = std::to_string(data.m_GuildId);
			item["s35idar9"] = data.m_Name;
			item["dDKCN293"] = std::to_string(data.m_GuildArtId);
			item["osidufj5"] = std::to_string(data.m_Level);
			item["P_RANK"] = std::to_string(data.m_Rank);
			item["sc83dkh3"] = std::to_string(data.m_PrestigePoint);
			item["SivJ9sL9"] = std::to_string(data.m_MembersCount);
			item["aBcniqj8"] = data.m_MasterName;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
