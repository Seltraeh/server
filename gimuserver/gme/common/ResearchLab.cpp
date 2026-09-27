#include "ResearchLab.hpp"

#include "App.hpp"

#include <gimuserver/archive/MissionArchiver.hpp>

namespace gme
{
const std::set<int32_t>& researchLabMissions()
{
	// ServerCache is immutable once the server is up, so this is built once.
	static const std::set<int32_t> missions = [] {
		std::set<int32_t> out;
		const auto& cache = theServer()->cache();
		for (const auto& dungeon : cache.dungeonMst())
		{
			if (dungeon.dungeon_type != 2 && dungeon.dungeon_type != 8)
				continue;
			const auto it = cache.missionsByDungeon().find(dungeon.dungeon_id);
			if (it != cache.missionsByDungeon().end())
				out.insert(it->second.begin(), it->second.end());
		}
		return out;
	}();
	return missions;
}

bool isResearchLabMission(const int32_t missionId)
{
	return researchLabMissions().contains(missionId);
}

bool researchLabUnlocked(const int32_t missionId, const std::set<int32_t>& cleared)
{
	const auto& needs = theServer()->cache().missionNeeds();
	const auto it = needs.find(missionId);
	if (it == needs.end())
		return true;
	for (const auto need : it->second)
	{
		if (need <= 0 || !MissionArchiver::instance().archived(static_cast<uint32_t>(need)))
			continue;
		if (!cleared.contains(need))
			return false;
	}
	return true;
}
} // namespace gme
