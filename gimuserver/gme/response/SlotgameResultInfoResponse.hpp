#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SlotgameResultInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "s8r5M6wI"; }

	struct Data {
		std::string m_yT3NBME0 = "";
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
			item["yT3NBME0"] = data.m_yT3NBME0;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
