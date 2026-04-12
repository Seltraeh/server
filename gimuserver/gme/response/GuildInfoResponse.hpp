#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "IkdSufj5"; }

		uint32_t m_GuildId = 0;
		std::string m_Name = "";
		uint32_t m_GuildArtId = 0;
		uint32_t m_Level = 0;
		uint32_t m_Rank = 0;
		uint32_t m_PrestigePoint = 0;
		uint32_t m_MembersCount = 0;
		std::string m_MasterName = "";
		uint32_t m_Experience = 0;
		uint32_t m_ExperienceObtained = 0;
		uint32_t m_BoardFlag = 0;
		std::string m_CreateDate = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["sD73jd20"] = std::to_string(m_GuildId);
			v["s35idar9"] = m_Name;
			v["dDKCN293"] = std::to_string(m_GuildArtId);
			v["osidufj5"] = std::to_string(m_Level);
			v["P_RANK"] = std::to_string(m_Rank);
			v["sc83dkh3"] = std::to_string(m_PrestigePoint);
			v["SivJ9sL9"] = std::to_string(m_MembersCount);
			v["aBcniqj8"] = m_MasterName;
			v["Siv49s0l"] = std::to_string(m_Experience);
			v["39bDai1y"] = std::to_string(m_ExperienceObtained);
			v["b6D19bg9"] = std::to_string(m_BoardFlag);
			v["o94cnA8P"] = m_CreateDate;
	}
};
RESPONSE_NS_END
