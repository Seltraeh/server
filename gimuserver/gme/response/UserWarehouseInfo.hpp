#pragma once

#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserWarehouseInfo : public IResponse
{
	struct Data
	{
		uint32_t    userItemID  = 0;   // n6E8iMf3 — server-assigned instance ID
		std::string itemID      = "";  // kixHbe54 — master item ID string (confirmed std::string
		                               //            via UserEquipItemInfoResponse cross-ref)
		uint32_t    possession  = 0;   // wgV86x1q — stack count
		uint32_t    newFlg      = 1;   // dJNpLc81 — new-item badge flag (same key as unit inventory)
		uint32_t    unknownFlg  = 0;   // DbMVG16I — setter "IsReceipt" seen in one binary context,
		                               //            null in unit-info context; default 0

		void Serialize(Json::Value& v) const
		{
			v["n6E8iMf3"] = std::to_string(userItemID); // instance ID as string, per item/unit pattern
			v["kixHbe54"] = itemID;
			v["wgV86x1q"] = std::to_string(possession);
			v["dJNpLc81"] = newFlg;
			v["DbMVG16I"] = std::to_string(unknownFlg);
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
