#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ColosseumArchiveInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "7UVKrgle"; }

	struct Data {
		std::string m_ClassID = "";
		uint32_t m_ArenaDefeatUnitCnt = 0;
		uint32_t m_AtkCnt = 0;
		uint32_t m_AtkWinCnt = 0;
		uint32_t m_AtkSeriesWinCnt = 0;
		uint32_t m_DefCnt = 0;
		uint32_t m_DefWinCnt = 0;
		uint32_t m_DefEsriesWinCnt = 0;
		uint32_t m_MaxAtkSeriesWinCnt = 0;
		uint32_t m_MaxDefSeriesWinCnt = 0;
		uint32_t m_MaxTrnDamage = 0;
		uint32_t m_MaxTurnSpark = 0;
		uint32_t m_SumB_CrystalCnt = 0;
		uint32_t m_SumH_CrystalCnt = 0;
		uint32_t m_SumDefeatFireUnitCnt = 0;
		uint32_t m_SumDefeatWaterUnitCnt = 0;
		uint32_t m_SumDefeatTreeUnitCnt = 0;
		uint32_t m_SumDefeatThunderUnitCnt = 0;
		uint32_t m_SumDefeatSaintUnitCnt = 0;
		uint32_t m_SumDefeatDarkUnitCnt = 0;
		uint32_t m_SumDefeatRare1UnitCnt = 0;
		uint32_t m_SumDefeatRare2UnitCnt = 0;
		uint32_t m_SumDefeatRare3UnitCnt = 0;
		uint32_t m_SumDefeatRare4UnitCnt = 0;
		uint32_t m_SumDefeatRare5UnitCnt = 0;
		uint32_t m_SumDefeatRare6UnitCnt = 0;
		uint32_t m_SumDefeatRare7UnitCnt = 0;
		uint32_t m_SumDefeatRare8UnitCnt = 0;
		uint32_t m_SumallKnockDownCnt = 0;
		uint32_t m_SumTimeountWinCnt = 0;
		uint32_t m_SumPerfectWinCnt = 0;
		uint32_t m_SumBB_KnockDownCnt = 0;
		uint32_t m_TotalCBP = 0;
		std::string m_SumDamage = "";
		std::string m_SumHeal = "";
		uint32_t m_SumOverkillAttackCnt = 0;
		uint32_t m_SumSpark = 0;
		uint32_t m_SumKkillCnt = 0;
		uint32_t m_FirstWinCnt = 0;
		uint32_t m_SecondWinCnt = 0;
		uint32_t m_AllWinCnt = 0;
		uint32_t m_AllSeriesWinCnt = 0;
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
			item["3mMAn6L5"] = data.m_ClassID;
			item["mFID53JZ"] = std::to_string(data.m_ArenaDefeatUnitCnt);
			item["iw7W8xek"] = std::to_string(data.m_AtkCnt);
			item["35ymTv4j"] = std::to_string(data.m_AtkWinCnt);
			item["7sXWfE8j"] = std::to_string(data.m_AtkSeriesWinCnt);
			item["74wNK1Me"] = std::to_string(data.m_DefCnt);
			item["ht7jHe2L"] = std::to_string(data.m_DefWinCnt);
			item["y0eUdi1I"] = std::to_string(data.m_DefEsriesWinCnt);
			item["KuZ6k5Vp"] = std::to_string(data.m_MaxAtkSeriesWinCnt);
			item["72sMkn1N"] = std::to_string(data.m_MaxDefSeriesWinCnt);
			item["0p8BtREA"] = std::to_string(data.m_MaxTrnDamage);
			item["R7eJ1Tfv"] = std::to_string(data.m_MaxTurnSpark);
			item["8bAdgn37"] = std::to_string(data.m_SumB_CrystalCnt);
			item["I5Uin24C"] = std::to_string(data.m_SumH_CrystalCnt);
			item["nRms5i9J"] = std::to_string(data.m_SumDefeatFireUnitCnt);
			item["Cvp53GKJ"] = std::to_string(data.m_SumDefeatWaterUnitCnt);
			item["T2qI34Rm"] = std::to_string(data.m_SumDefeatTreeUnitCnt);
			item["Ff6g3CNv"] = std::to_string(data.m_SumDefeatThunderUnitCnt);
			item["3EmVG6Ts"] = std::to_string(data.m_SumDefeatSaintUnitCnt);
			item["3uUIPEj5"] = std::to_string(data.m_SumDefeatDarkUnitCnt);
			item["uXrLT17C"] = std::to_string(data.m_SumDefeatRare1UnitCnt);
			item["8dzSxWA2"] = std::to_string(data.m_SumDefeatRare2UnitCnt);
			item["8TL5Zmuw"] = std::to_string(data.m_SumDefeatRare3UnitCnt);
			item["0XocvSM2"] = std::to_string(data.m_SumDefeatRare4UnitCnt);
			item["Ivk81jQh"] = std::to_string(data.m_SumDefeatRare5UnitCnt);
			item["7Su2qZFv"] = std::to_string(data.m_SumDefeatRare6UnitCnt);
			item["mCDR82iQ"] = std::to_string(data.m_SumDefeatRare7UnitCnt);
			item["u9d8ye9H"] = std::to_string(data.m_SumDefeatRare8UnitCnt);
			item["Gj4zJIB3"] = std::to_string(data.m_SumallKnockDownCnt);
			item["Z0X7BVdF"] = std::to_string(data.m_SumTimeountWinCnt);
			item["J7u8prxX"] = std::to_string(data.m_SumPerfectWinCnt);
			item["Ut8sN0oM"] = std::to_string(data.m_SumBB_KnockDownCnt);
			item["z1rMbo8n"] = std::to_string(data.m_TotalCBP);
			item["hI68VzHu"] = data.m_SumDamage;
			item["F32xJzsH"] = data.m_SumHeal;
			item["Wnc6A31E"] = std::to_string(data.m_SumOverkillAttackCnt);
			item["kHbYZ9w1"] = std::to_string(data.m_SumSpark);
			item["4vx39EIB"] = std::to_string(data.m_SumKkillCnt);
			item["uwrMXq0v"] = std::to_string(data.m_FirstWinCnt);
			item["u4S8MyDD"] = std::to_string(data.m_SecondWinCnt);
			item["PL0mqDhK"] = std::to_string(data.m_AllWinCnt);
			item["Txb2idHz"] = std::to_string(data.m_AllSeriesWinCnt);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
