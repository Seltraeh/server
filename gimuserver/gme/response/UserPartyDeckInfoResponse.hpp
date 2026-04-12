#pragma once
#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserPartyDeckInfoResponse : public IResponse
{
	const char* getGroupName() const override { return "dX7S2Lc1"; }

	struct Data {
		uint32_t m_U9ABSYEp = 0;
		uint32_t m_zsiAn9P1 = 0;
		std::string m_UserUnitID = "";
		uint32_t m_MemberType = 0;
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
			item["U9ABSYEp"] = std::to_string(data.m_U9ABSYEp);
			item["zsiAn9P1"] = std::to_string(data.m_zsiAn9P1);
			item["edy7fq3L"] = data.m_UserUnitID;
			item["gr48vsdJ"] = std::to_string(data.m_MemberType);
			arr.append(item);
		}
		
		v = arr;
	}
};
RESPONSE_NS_END
