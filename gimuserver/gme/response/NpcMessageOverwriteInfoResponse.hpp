#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct NpcMessageOverwriteInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "yNnvj59x"; }

	struct Data {
		std::string m_LimitTime = "";
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
			item["s35idar9"] = data.m_LimitTime;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
