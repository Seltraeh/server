#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct MysteryBoxResponse : public IResponse
{
	const char* getGroupName() const override { return "d0ajLeRi"; }

	struct Data {
		std::string m_MysteryBoxId = "";
		uint32_t m_MysteryBoxRewardDate = 0;
		uint32_t m_MysteryBoxExpiryDate = 0;
		std::string m_MysteryBoxRewardDateStr = "";
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
			item["rEFRefr8"] = data.m_MysteryBoxId;
			item["6HEchexu"] = std::to_string(data.m_MysteryBoxRewardDate);
			item["guHur3dr"] = std::to_string(data.m_MysteryBoxExpiryDate);
			item["peV7drec"] = data.m_MysteryBoxRewardDateStr;
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
