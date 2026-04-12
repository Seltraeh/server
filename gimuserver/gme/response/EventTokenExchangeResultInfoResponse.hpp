#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct EventTokenExchangeResultInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Sdvs2lds"; }

	struct Data {
		std::string m_Kd3DL39d = "";
		std::string m_Sc3slc04 = "";
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
			item["Kd3DL39d"] = data.m_Kd3DL39d;
			item["Sc3slc04"] = data.m_Sc3slc04;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
