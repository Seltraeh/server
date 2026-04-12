#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidClearMissionInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "8LF2ohCY"; }

	struct Data {
		std::string m_UesrID = "";
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
			item["h7eY3sAK"] = data.m_UesrID;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
