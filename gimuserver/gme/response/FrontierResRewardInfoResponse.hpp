#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct FrontierResRewardInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "QXFCkE67"; }

	struct Data {
		uint32_t m_TarType = 0;
		std::string m_TarID = "";
		uint32_t m_TarCnt = 0;
		std::string m_TarParam = "";
		uint32_t m_RewardType = 0;
		uint32_t m_RewardParam = 0;
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
			item["30Kw4WBa"] = std::to_string(data.m_TarType);
			item["TdDHf59J"] = data.m_TarID;
			item["wJsB35iH"] = std::to_string(data.m_TarCnt);
			item["37moriMq"] = data.m_TarParam;
			item["IkmC8gG2"] = std::to_string(data.m_RewardType);
			item["empaR60j"] = std::to_string(data.m_RewardParam);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
