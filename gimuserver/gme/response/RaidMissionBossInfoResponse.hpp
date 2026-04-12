#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidMissionBossInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Ts2zJ6gj"; }

		std::string m_MissionBossID = "";
		uint32_t m_BossNowHp = 0;
		uint32_t m_BossCurrentPoint = 0;
		uint32_t m_BossPosX = 0;
		uint32_t m_BossPosY = 0;
		uint32_t m_BossGoalPosX = 0;
		uint32_t m_BossGoalPosY = 0;
		uint32_t m_VisibleFlg = 0;

	void Serialize(Json::Value& v) const {
		Json::Value data;
		SerializeFields(data, 0);
		v[getGroupName()] = data;
	}

protected:
	void SerializeFields(Json::Value& v, size_t) const override {
			v["SP29fLtH"] = m_MissionBossID;
			v["4x7IS9Vg"] = std::to_string(m_BossNowHp);
			v["7Ex5mKRP"] = std::to_string(m_BossCurrentPoint);
			v["5NAb7ds3"] = std::to_string(m_BossPosX);
			v["9P1MbHx5"] = std::to_string(m_BossPosY);
			v["hsjMq7i6"] = std::to_string(m_BossGoalPosX);
			v["g57HTBUa"] = std::to_string(m_BossGoalPosY);
			v["7z6dqvuc"] = std::to_string(m_VisibleFlg);
	}
};
RESPONSE_NS_END
