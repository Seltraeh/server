#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct SGChatRoomMemberInfoListResponse : public IResponse
{
	const char* getGroupName() const override { return "N3d1supA"; }

	struct Data {
		std::string m_h7eY3sAK = "";
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
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
