#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ChallengeArenaRankingRewardResponse : public IResponse
{
	const char* getGroupName() const override { return "XHNT0LR3"; }

	// The extractor recovered 10 reward tiers that each carry a RewardId (string)
	// and RewardParam (string, except the 10th which is uint32_t). Each tier maps
	// to a distinct obfuscated JSON key, but all share the same semantic setter
	// name in the binary. Indexed 0–9 to avoid duplicate member names.
	struct Data {
		uint32_t m_0Rhjwagb = 0;
		uint32_t m_t6bRQfln = 0;
		uint32_t m_2OfNNfhH = 0;
		uint32_t m_DIOHPpyr = 0;
		uint32_t m_wVzB0Yz6 = 0;
		uint32_t m_25ku1hQC = 0;
		uint32_t m_QXy6fPDm = 0;
		std::string m_RewardId_0 = "";
		uint32_t m_IH1anFgF = 0;
		uint32_t m_kw6U5Juy = 0;
		std::string m_RewardParam_0 = "";
		uint32_t m_Pl0ltUFS = 0;
		std::string m_RewardId_1 = "";
		uint32_t m_5m7eYEoY = 0;
		uint32_t m_7Rw23XKV = 0;
		std::string m_RewardParam_1 = "";
		uint32_t m_1xicWlZF = 0;
		std::string m_RewardId_2 = "";
		uint32_t m_yezEyRRM = 0;
		uint32_t m_H5iyScWN = 0;
		std::string m_RewardParam_2 = "";
		uint32_t m_tbymIkQz = 0;
		std::string m_RewardId_3 = "";
		uint32_t m_UgGNcWkk = 0;
		uint32_t m_1XOKlX5Z = 0;
		std::string m_RewardParam_3 = "";
		uint32_t m_Hk5b8AoG = 0;
		std::string m_RewardId_4 = "";
		uint32_t m_cWqTJ08e = 0;
		uint32_t m_XQ50D7RX = 0;
		std::string m_RewardParam_4 = "";
		uint32_t m_gjRjD2uy = 0;
		std::string m_RewardId_5 = "";
		uint32_t m_kH2EalTY = 0;
		uint32_t m_AI1hCjWl = 0;
		std::string m_RewardParam_5 = "";
		uint32_t m_fmFAhtWv = 0;
		std::string m_RewardId_6 = "";
		uint32_t m_Z1P3w7Xn = 0;
		uint32_t m_Uarr2hXq = 0;
		std::string m_RewardParam_6 = "";
		uint32_t m_6YFE6UYy = 0;
		std::string m_RewardId_7 = "";
		uint32_t m_oLOHIjgo = 0;
		uint32_t m_pghUC2it = 0;
		std::string m_RewardParam_7 = "";
		uint32_t m_95I3mTFK = 0;
		std::string m_RewardId_8 = "";
		uint32_t m_p5kjvjF3 = 0;
		uint32_t m_w9R8EVAl = 0;
		std::string m_RewardParam_8 = "";
		uint32_t m_1OvVA8FM = 0;
		std::string m_RewardId_9 = "";
		uint32_t m_GI62JPYO = 0;
		uint32_t m_RewardParam_9 = 0; // uint32_t on the final tier (different type in binary)
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
			item["0Rhjwagb"] = std::to_string(data.m_0Rhjwagb);
			item["t6bRQfln"] = std::to_string(data.m_t6bRQfln);
			item["2OfNNfhH"] = std::to_string(data.m_2OfNNfhH);
			item["DIOHPpyr"] = std::to_string(data.m_DIOHPpyr);
			item["wVzB0Yz6"] = std::to_string(data.m_wVzB0Yz6);
			item["25ku1hQC"] = std::to_string(data.m_25ku1hQC);
			item["QXy6fPDm"] = std::to_string(data.m_QXy6fPDm);
			item["IEOPY8ax"] = data.m_RewardId_0;
			item["IH1anFgF"] = std::to_string(data.m_IH1anFgF);
			item["kw6U5Juy"] = std::to_string(data.m_kw6U5Juy);
			item["kfY2FBDa"] = data.m_RewardParam_0;
			item["Pl0ltUFS"] = std::to_string(data.m_Pl0ltUFS);
			item["AJCEQfKX"] = data.m_RewardId_1;
			item["5m7eYEoY"] = std::to_string(data.m_5m7eYEoY);
			item["7Rw23XKV"] = std::to_string(data.m_7Rw23XKV);
			item["XkoDKBDM"] = data.m_RewardParam_1;
			item["1xicWlZF"] = std::to_string(data.m_1xicWlZF);
			item["qiYVr95m"] = data.m_RewardId_2;
			item["yezEyRRM"] = std::to_string(data.m_yezEyRRM);
			item["H5iyScWN"] = std::to_string(data.m_H5iyScWN);
			item["DRfVwToL"] = data.m_RewardParam_2;
			item["tbymIkQz"] = std::to_string(data.m_tbymIkQz);
			item["dDkmhizw"] = data.m_RewardId_3;
			item["UgGNcWkk"] = std::to_string(data.m_UgGNcWkk);
			item["1XOKlX5Z"] = std::to_string(data.m_1XOKlX5Z);
			item["Oy4RMVex"] = data.m_RewardParam_3;
			item["Hk5b8AoG"] = std::to_string(data.m_Hk5b8AoG);
			item["bXblisNv"] = data.m_RewardId_4;
			item["cWqTJ08e"] = std::to_string(data.m_cWqTJ08e);
			item["XQ50D7RX"] = std::to_string(data.m_XQ50D7RX);
			item["OSIsGlWw"] = data.m_RewardParam_4;
			item["gjRjD2uy"] = std::to_string(data.m_gjRjD2uy);
			item["c3OHFwLN"] = data.m_RewardId_5;
			item["kH2EalTY"] = std::to_string(data.m_kH2EalTY);
			item["AI1hCjWl"] = std::to_string(data.m_AI1hCjWl);
			item["SK2GyFGi"] = data.m_RewardParam_5;
			item["fmFAhtWv"] = std::to_string(data.m_fmFAhtWv);
			item["ytPCGWQd"] = data.m_RewardId_6;
			item["Z1P3w7Xn"] = std::to_string(data.m_Z1P3w7Xn);
			item["Uarr2hXq"] = std::to_string(data.m_Uarr2hXq);
			item["u8BQ8MVC"] = data.m_RewardParam_6;
			item["6YFE6UYy"] = std::to_string(data.m_6YFE6UYy);
			item["nVCtUew9"] = data.m_RewardId_7;
			item["oLOHIjgo"] = std::to_string(data.m_oLOHIjgo);
			item["pghUC2it"] = std::to_string(data.m_pghUC2it);
			item["F86zmgwG"] = data.m_RewardParam_7;
			item["95I3mTFK"] = std::to_string(data.m_95I3mTFK);
			item["BYiO8s0l"] = data.m_RewardId_8;
			item["p5kjvjF3"] = std::to_string(data.m_p5kjvjF3);
			item["w9R8EVAl"] = std::to_string(data.m_w9R8EVAl);
			item["BJK0o9Wq"] = data.m_RewardParam_8;
			item["1OvVA8FM"] = std::to_string(data.m_1OvVA8FM);
			item["gMNkziJQ"] = data.m_RewardId_9;
			item["GI62JPYO"] = std::to_string(data.m_GI62JPYO);
			item["sRj8n9Zt"] = std::to_string(data.m_RewardParam_9);
			arr.append(item);
		}

		v = arr;
	}
};
RESPONSE_NS_END
