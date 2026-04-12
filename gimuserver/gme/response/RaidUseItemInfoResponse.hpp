#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidUseItemInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "S2fpqc7K"; }

	struct Data {
		std::string m_MissionUserItemID = "";
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
			item["bL9t7ABR"] = data.m_MissionUserItemID;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
