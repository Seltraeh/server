#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct PermitRecipeResponse : public IResponse
{
	const char* getGroupName() const override { return "51yQrDBR"; }

	struct Data {
		std::string m_RecipeID = "";
		uint32_t m_Cnt = 0;
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
			item["4HqhTf3a"] = data.m_RecipeID;
			item["H6k1LIxC"] = std::to_string(data.m_Cnt);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
