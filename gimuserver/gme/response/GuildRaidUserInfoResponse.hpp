#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidUserInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "IG8atK9y"; }

		uint32_t m_RoomID = 0;
		std::string m_PusherChannel = "";
		std::string m_RewardEnable = "";
		std::string m_XoA3vA90 = "";

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["8VYd6xSX"] = std::to_string(m_RoomID);
			v["kjP2h9a7"] = m_PusherChannel;
			v["3Bzw3aeq"] = m_RewardEnable;
			v["XoA3vA90"] = m_XoA3vA90;
	}
};
RESPONSE_NS_END
