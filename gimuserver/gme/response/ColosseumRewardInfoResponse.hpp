#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct ColosseumRewardInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "xNCYRb5G"; }

	struct Data {
		std::string m_CategoryID = "";
		uint32_t m_PresentType = 0;
		std::string m_TargetID = "";
		uint32_t m_TargetCnt = 0;
		uint32_t m_RewardReceiveStatus = 0;
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
			item["VgU78CYj"] = data.m_CategoryID;
			item["30Kw4WBa"] = std::to_string(data.m_PresentType);
			item["TdDHf59J"] = data.m_TargetID;
			item["wJsB35iH"] = std::to_string(data.m_TargetCnt);
			item["rPk8gtY5"] = std::to_string(data.m_RewardReceiveStatus);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
