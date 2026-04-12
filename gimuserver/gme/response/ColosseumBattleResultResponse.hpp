#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ColosseumBattleResultResponse : public IResponse
{
	const char* getGroupName() const override { return "BZ95Eg3M"; }

	struct Data {
		std::string m_BattleNum = "";
		uint32_t m_BattleType = 0;
		uint32_t m_BattleResult = 0;
		uint32_t m_BattleCrystalNum = 0;
		uint32_t m_HeartCrystalNum = 0;
		uint32_t m_MaxTurnDamage = 0;
		uint32_t m_MaxTurnSparkCnt = 0;
		uint32_t m_SparkCnt = 0;
		uint32_t m_SkillUseCnt = 0;
		uint32_t m_DefeatUnitCnt = 0;
		std::string m_DefeatUnitsInfo = "";
		uint32_t m_AllKnockDown = 0;
		uint32_t m_TimeOut = 0;
		uint32_t m_Perfect = 0;
		uint32_t m_BbKnockDownCnt = 0;
		uint32_t m_SumDamage = 0;
		uint32_t m_SumHeal = 0;
		uint32_t m_SumOverkillAttackCnt = 0;
		uint32_t m_BeforeCbp = 0;
		uint32_t m_AfterCbp = 0;
		uint32_t m_FriendReqState = 0;
		uint32_t m_HpMaxCnt = 0;
		uint32_t m_SkillGageMaxCnt = 0;
		uint32_t m_WeakElemKillCnt = 0;
		uint32_t m_BbUseCnt = 0;
		uint32_t m_SbbUseCnt = 0;
		uint32_t m_MultiKillCnt = 0;
		uint32_t m_MultiKillTotalCnt = 0;
		std::string m_FlnalTurnUnitCnt = "";
		uint32_t m_BonusPoint = 0;
		uint32_t m_BBKoCnt = 0;
		uint32_t m_SBBKoCnt = 0;
		uint32_t m_UBBKoCnt = 0;
		std::string m_HpPartyRest = "";
		uint32_t m_BonusType = 0;
		uint32_t m_BonusParam = 0;
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
			item["1kisc6IF"] = data.m_BattleNum;
			item["2QOPHXW1"] = std::to_string(data.m_BattleType);
			item["3pAQY5KW"] = std::to_string(data.m_BattleResult);
			item["hoG2ieT5"] = std::to_string(data.m_BattleCrystalNum);
			item["6PLsn8xo"] = std::to_string(data.m_HeartCrystalNum);
			item["5NRJQ1LU"] = std::to_string(data.m_MaxTurnDamage);
			item["XP06YWdT"] = std::to_string(data.m_MaxTurnSparkCnt);
			item["U8uZLA34"] = std::to_string(data.m_SparkCnt);
			item["rZQJF5G9"] = std::to_string(data.m_SkillUseCnt);
			item["mFID53JZ"] = std::to_string(data.m_DefeatUnitCnt);
			item["sQ2aMk8B"] = data.m_DefeatUnitsInfo;
			item["G8EebW0o"] = std::to_string(data.m_AllKnockDown);
			item["x3ZgJS9W"] = std::to_string(data.m_TimeOut);
			item["s92RgSwp"] = std::to_string(data.m_Perfect);
			item["XRoD9Hg7"] = std::to_string(data.m_BbKnockDownCnt);
			item["hI68VzHu"] = std::to_string(data.m_SumDamage);
			item["F32xJzsH"] = std::to_string(data.m_SumHeal);
			item["Wnc6A31E"] = std::to_string(data.m_SumOverkillAttackCnt);
			item["43QyxTS5"] = std::to_string(data.m_BeforeCbp);
			item["OUY9jpPA"] = std::to_string(data.m_AfterCbp);
			item["Sx5H0Ikn"] = std::to_string(data.m_FriendReqState);
			item["Ljg1nUro"] = std::to_string(data.m_HpMaxCnt);
			item["tZtlrUHI"] = std::to_string(data.m_SkillGageMaxCnt);
			item["IWh5sCSm"] = std::to_string(data.m_WeakElemKillCnt);
			item["yY8snrWG"] = std::to_string(data.m_BbUseCnt);
			item["hnWoR2fE"] = std::to_string(data.m_SbbUseCnt);
			item["xFH5i4o8"] = std::to_string(data.m_MultiKillCnt);
			item["w6IiLvN8"] = std::to_string(data.m_MultiKillTotalCnt);
			item["gpDCY81t"] = data.m_FlnalTurnUnitCnt;
			item["tvWWL9wI"] = std::to_string(data.m_BonusPoint);
			item["GRCVD1i8"] = std::to_string(data.m_BBKoCnt);
			item["Uy15wSJd"] = std::to_string(data.m_SBBKoCnt);
			item["EfwspXXo"] = std::to_string(data.m_UBBKoCnt);
			item["WDYR11jJ"] = data.m_HpPartyRest;
			item["ciDqCjAF"] = std::to_string(data.m_BonusType);
			item["59b1eE6N"] = std::to_string(data.m_BonusParam);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
