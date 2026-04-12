#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct MinigameInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "Ajp4SmMG"; }

	struct Data {
		std::string m_h7eY3sAK = "";
		std::string m_B5JQyV8j = "";
		uint32_t m_2Fh3J7ng = 0;
		std::string m_pn16CNah = "";
		uint32_t m_4A6LzBxr = 0;
		uint32_t m_NH65Wj0f = 0;
		uint32_t m_2pAyFjmZ = 0;
		uint32_t m_2rqxaZ6K = 0;
		uint32_t m_TPR79fyI = 0;
		uint32_t m_Ep3Flg = 0;
		uint32_t m_Sex = 0;
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
			item["h7eY3sAK"] = data.m_h7eY3sAK;
			item["B5JQyV8j"] = data.m_B5JQyV8j;
			item["2Fh3J7ng"] = std::to_string(data.m_2Fh3J7ng);
			item["pn16CNah"] = data.m_pn16CNah;
			item["4A6LzBxr"] = std::to_string(data.m_4A6LzBxr);
			item["NH65Wj0f"] = std::to_string(data.m_NH65Wj0f);
			item["2pAyFjmZ"] = std::to_string(data.m_2pAyFjmZ);
			item["2rqxaZ6K"] = std::to_string(data.m_2rqxaZ6K);
			item["TPR79fyI"] = std::to_string(data.m_TPR79fyI);
			item["jkldTrhL"] = std::to_string(data.m_Ep3Flg);
			item["9i2xhMaJ"] = std::to_string(data.m_Sex);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
