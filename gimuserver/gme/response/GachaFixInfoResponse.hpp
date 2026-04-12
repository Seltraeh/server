#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct GachaFixInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "8JEzC89y"; }

	struct Data {
		std::string m_GachaID = "";
		uint32_t m_GachaType = 0;
		uint32_t m_FriendPoint = 0;
		uint32_t m_BraveCoin = 0;
		uint32_t m_DrawCnt = 0;
		uint32_t m_Rare = 0;
		uint32_t m_UnitID = 0;
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
			item["7Ffmi96v"] = data.m_GachaID;
			item["S1oz60Hc"] = std::to_string(data.m_GachaType);
			item["J3stQ7jd"] = std::to_string(data.m_FriendPoint);
			item["03UGMHxF"] = std::to_string(data.m_BraveCoin);
			item["d04gRmkE"] = std::to_string(data.m_DrawCnt);
			item["7ofj5xa1"] = std::to_string(data.m_Rare);
			item["pn16CNah"] = std::to_string(data.m_UnitID);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
