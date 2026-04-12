#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct BundlePacksInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "K72eC4nz"; }

	struct Data {
		uint32_t m_ID = 0;
		std::string m_Name = "";
		uint32_t m_DisplayOrder = 0;
		std::string m_StartDate = "";
		uint32_t m_DisplayTopTimer = 0;
		uint32_t m_Cost = 0;
		std::string m_Img = "";
		uint32_t m_TimeLimit = 0;
		uint32_t m_ClaimRemainder = 0;
		std::string m_IapProductId = "";
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
			item["j10diyl9"] = std::to_string(data.m_ID);
			item["23L78yG5"] = data.m_Name;
			item["XuJL4pc5"] = std::to_string(data.m_DisplayOrder);
			item["qA7M9EjP"] = data.m_StartDate;
			item["v7xaO3qi"] = std::to_string(data.m_DisplayTopTimer);
			item["h1g93csR"] = std::to_string(data.m_Cost);
			item["l8DijpA5"] = data.m_Img;
			item["u3bDi9sT"] = std::to_string(data.m_TimeLimit);
			item["uK391dP3"] = std::to_string(data.m_ClaimRemainder);
			item["wXTxs50z"] = data.m_IapProductId;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
