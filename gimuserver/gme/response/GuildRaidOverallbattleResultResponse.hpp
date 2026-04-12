#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GuildRaidOverallbattleResultResponse : public IResponse
{
	const char* getGroupName() const override { return "YcDPfGlN"; }

	struct Data {
		uint32_t m_dk39bDa1 = 0;
		uint32_t m_81tacsfJ = 0;
		uint32_t m_s385qzx9 = 0;
		uint32_t m_8VYd6xSX = 0;
		uint32_t m_gE2NN2xi = 0;
		uint32_t m_HCZs5dMf = 0;
		std::string m_BsBkDpYK = "";
		uint32_t m_aULFPSiQ = 0;
		uint32_t m_qXCIfZIk = 0;
		uint32_t m_phRQ6Koc = 0;
		uint32_t m_Z630LOdW = 0;
		std::string m_uR6vbPRA = "";
		uint32_t m_978aBi2C = 0;
		uint32_t m_phRQ6Koc_2 = 0; // duplicate "phRQ6Koc" JSON key — overwrites above in output
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
			item["dk39bDa1"] = std::to_string(data.m_dk39bDa1);
			item["81tacsfJ"] = std::to_string(data.m_81tacsfJ);
			item["s385qzx9"] = std::to_string(data.m_s385qzx9);
			item["8VYd6xSX"] = std::to_string(data.m_8VYd6xSX);
			item["gE2NN2xi"] = std::to_string(data.m_gE2NN2xi);
			item["HCZs5dMf"] = std::to_string(data.m_HCZs5dMf);
			item["BsBkDpYK"] = data.m_BsBkDpYK;
			item["aULFPSiQ"] = std::to_string(data.m_aULFPSiQ);
			item["qXCIfZIk"] = std::to_string(data.m_qXCIfZIk);
			item["phRQ6Koc"] = std::to_string(data.m_phRQ6Koc);
			item["Z630LOdW"] = std::to_string(data.m_Z630LOdW);
			item["uR6vbPRA"] = data.m_uR6vbPRA;
			item["978aBi2C"] = std::to_string(data.m_978aBi2C);
			item["phRQ6Koc"] = std::to_string(data.m_phRQ6Koc_2);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
