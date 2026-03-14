#pragma once

#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN
struct PermitPlace : public IResponse
{
	struct Data
	{
		enum PlaceType { NONE, AREA, GATE, DUNGEON, MISSION, LAND };

		PlaceType place = NONE;
		std::string areaID, landID, gateID, dungeonID, missionID;

		void setAreaID(const std::string& id)
		{
			if (place != NONE) throw std::runtime_error("PermitPlaceID already set");
			areaID = id; place = AREA;
		}
		void setLandID(const std::string& id)
		{
			if (place != NONE) throw std::runtime_error("PermitPlaceID already set");
			landID = id; place = LAND;
		}
		void setGateID(const std::string& id)
		{
			if (place != NONE) throw std::runtime_error("PermitPlaceID already set");
			gateID = id; place = GATE;
		}
		void setDungeonID(const std::string& id)
		{
			if (place != NONE) throw std::runtime_error("PermitPlaceID already set");
			dungeonID = id; place = DUNGEON;
		}
		void setMissionID(const std::string& id)
		{
			if (place != NONE) throw std::runtime_error("PermitPlaceID already set");
			missionID = id; place = MISSION;
		}

		void Serialize(Json::Value& v) const
		{
			switch (place)
			{
			case AREA:    v["VjCY7rX4"] = areaID;    break;
			case LAND:    v["9C64Qwe0"] = landID;    break;
			case GATE:    v["0Cq2AlXW"] = gateID;    break;
			case DUNGEON: v["MHx05sXt"] = dungeonID; break;
			case MISSION: v["j28VNcUW"] = missionID; break;
			default: break;
			}
		}
	};

	const char* getGroupName() const override { return "yXNM8kL3"; }

	std::vector<Data> Mst;

protected:
	size_t getRespCount() const override { return Mst.size(); }

	void SerializeFields(Json::Value& v, size_t i) const override
	{
		Mst.at(i).Serialize(v);
	}
};
RESPONSE_NS_END
