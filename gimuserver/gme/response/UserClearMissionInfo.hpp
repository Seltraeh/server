#pragma once

#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct UserClearMissionInfo : public IResponse
{
	struct Data
	{
		int missionId = 0;

		void Serialize(Json::Value& v) const
		{
			// ⚠ Field key is a placeholder — confirm from log_res/decompilation.
			v["Xk9Rp2nW"] = missionId;
		}
	};

	const char* getGroupName() const override { return "UT1SVg59"; }

	std::vector<Data> Mst;

protected:
	size_t getRespCount() const override { return Mst.size(); }

	void SerializeFields(Json::Value& v, size_t i) const override
	{
		Mst.at(i).Serialize(v);
	}
};
RESPONSE_NS_END
