#pragma once

#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserWarehouseInfo : public IResponse
{
	struct Data
	{
		std::string userID;
		int itemId   = 0;
		int quantity = 0;

		void Serialize(Json::Value& v) const
		{
			// ⚠ Field keys are placeholders — confirm from log_res captures.
			// "h7eY3sAK" is the user_id key shared with UserUnitInfo.
			v["h7eY3sAK"] = userID;
			v["JE2nFs9R"] = itemId;
			v["yH6kTq0A"] = quantity;
		}
	};

	const char* getGroupName() const override { return "9wjrh74P"; }

	std::vector<Data> Mst;

protected:
	size_t getRespCount() const override { return Mst.size(); }

	void SerializeFields(Json::Value& v, size_t i) const override
	{
		Mst.at(i).Serialize(v);
	}
};
RESPONSE_NS_END
