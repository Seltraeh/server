#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRecomendedMemberResponse : public IResponse
{
	const char* getGroupName() const override { return "fRaBu6et"; }

	struct Data {
		std::string m_UserName = "";
		uint32_t m_UserLevel = 0;
		uint32_t m_FriendUserImageType = 0;
		uint32_t m_FriendUserUnitID = 0;
		uint32_t m_FriendUserUnitLevel = 0;
		std::string m_LastOnlineTime = "";
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
			item["B5JQyV8j"] = data.m_UserName;
			item["LKSVU2sl"] = std::to_string(data.m_UserLevel);
			item["JmLLHcDv"] = std::to_string(data.m_FriendUserImageType);
			item["1ctR6GHC"] = std::to_string(data.m_FriendUserUnitID);
			item["8Ix0Soup"] = std::to_string(data.m_FriendUserUnitLevel);
			item["93nclsc8"] = data.m_LastOnlineTime;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
