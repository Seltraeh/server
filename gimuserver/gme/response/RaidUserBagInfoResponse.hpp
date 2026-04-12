#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct RaidUserBagInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "UsDq1E6o"; }

	struct Data {
		uint32_t m_FrameID = 0;
		std::string m_UserID = "";
		uint32_t m_ItemNum = 0;
		uint32_t m_DiffType = 0;
		uint32_t m_NewFlg = 0;
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
			item["n6E8iMf3"] = std::to_string(data.m_FrameID);
			item["h7eY3sAK"] = data.m_UserID;
			item["wgV86x1q"] = std::to_string(data.m_ItemNum);
			item["DbMVG16I"] = std::to_string(data.m_DiffType);
			item["dJNpLc81"] = std::to_string(data.m_NewFlg);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
