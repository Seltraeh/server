#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ColosseumRewardCategoryInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "1x6T2Trz"; }

	struct Data {
		std::string m_StageID = "";
		uint32_t m_DispOrder = 0;
		uint32_t m_ImgType = 0;
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
			item["mQhKzRxu"] = data.m_StageID;
			item["XuJL4pc5"] = std::to_string(data.m_DispOrder);
			item["2pAyFjmZ"] = std::to_string(data.m_ImgType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
