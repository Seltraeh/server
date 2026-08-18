#include "UnitArchiver.hpp"

#include <gimuserver/utils/JsonFile.hpp>

#include <drogon/drogon.h>

#include <exception>
#include <string>
#include <utility>
#include <vector>

UnitArchiver& UnitArchiver::instance()
{
	static UnitArchiver instance;
	return instance;
}

void UnitArchiver::setup(const Json::Value& serverObj)
{
	LOG_INFO << "Setting up unit archiver cache. "
		"If you see this after initialization, it is a bug.";

	const auto archiveRoot = serverObj["archive_root"].asString();
	if (archiveRoot.empty())
	{
		LOG_ERROR << "Unable to set up unit archiver cache: archive_root is empty";
		return;
	}

	std::vector<UnitRecord> units;
	try
	{
		units = LoadJson<std::vector<UnitRecord>>(archiveRoot, "unit.json");
	}
	catch (const std::exception& ex)
	{
		LOG_ERROR << "Unable to set up unit archiver cache: " << ex.what();
		return;
	}

	cache_.clear();
	cache_.reserve(units.size());
	for (auto& unit : units)
	{
		cache_.insert_or_assign(unit.id, std::move(unit));
	}

	LOG_INFO << "Loaded " << cache_.size()
		<< " unit archive records from " << archiveRoot << "/unit.json";
}

std::optional<UnitRecord> UnitArchiver::lookup(UnitId unit_id) const
{
	const auto it = cache_.find(unit_id);
	if (it == cache_.end())
	{
		LOG_ERROR << "Unable to find unit archive record " << unit_id;
		return std::nullopt;
	}

	return it->second;
}

bool UnitArchiver::populatePacket(
	const UnitRecord& unitRecord,
	UnitType unit_type_id,
	UserUnitInfo& unit)
{
	// Grab the base stats for the given unit type.
	const auto& stats = unitRecord.stats[0];

	// TODO: Accept a unit level here and compute
	// stats from both the unit type and level.

	unit.unit_id = unitRecord.id;
	unit.unit_lvl = 1;
	unit.unit_type_id = unit_type_id;

	// Brave Burst.  bb_id is unit_mst's skill_id (nj9Lw7mV) and every value in
	// the archive is checked to exist in skill_mst — a unit whose id is not a
	// real skill gets an empty string rather than a dangling reference, because
	// the client resolves this against its own skill table.  Materials and
	// enhancers (Ghosts, Frogs, Imps, Metals) legitimately have none.
	//
	// sbb_id is honoured rather than forced empty.  It used to be hardcoded to
	// "" here, and correctly so at the time: every archived value was fabricated
	// (110011-style ids that exist nowhere in skill_mst's 33381 rows).  Those
	// have been cleared, so the field can be trusted again.  It is still empty
	// for everything currently archived — unit_mst carries exactly one skill per
	// unit, and Super Brave Burst is a 5-star-and-up mechanic, so no 3-star
	// summon pool has one.  The plumbing is here for when higher-rarity units
	// are archived with a verified SBB.
	unit.bb_id = unitRecord.bb_id;
	unit.bb_lvl = unitRecord.bb_id.empty() ? 0 : 1;
	unit.sbb_id = unitRecord.sbb_id;
	unit.sbb_lvl = unitRecord.sbb_id.empty() ? 0 : 1;
	unit.base_hp = stats.hp;
	unit.base_atk = stats.atk;
	unit.base_def = stats.def;
	unit.base_rec = stats.rec;
	unit.ext_hp = 0;
	unit.ext_atk = 0;
	unit.ext_def = 0;
	unit.ext_rec = 0;

	return true;
}

