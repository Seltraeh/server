#pragma once

#include <gimuserver/db/Types.h>
#include <drogon/drogon.h>
#include <string>
#include <string_view>
#include <set>

namespace gme
{
struct UserIdentity;

// Lizeria's land gate (land 4 needs both 666 and 20067).  Since story lists are
// read all-of (see buildPermitPlace) its rows already say the same; kept as the
// land-level statement of it.
bool storyLandUnlocked(int32_t landId, const std::set<int32_t>& cleared);
// Whether a story mission may start: the same all-of rules the map applies to
// its dungeon, area and own prerequisites.  Special-mode ids always pass.
bool storyMissionUnlocked(int32_t missionId, const std::set<int32_t>& cleared);
// Whether clearing `missionId` is what opened a story area: an area whose own
// need list names the mission and is now satisfied under the map's all-of rule.
// `cleared` must already include the mission.  MissionEnd reports exactly these
// clears as an area clear (NgPQbA46).
bool storyAreaOpenedBy(int32_t missionId, const std::set<int32_t>& cleared);

// Full normal PermitPlace slice from current progress, weekday, and key windows.
// Pass the held transaction when called during a mutation (single SQLite connection).
drogon::Task<std::string> buildPermitPlace(db::Database database, UserIdentity identity);

// Replace the generated empty-array placeholder. Throws if its shape is wrong,
// so an accidental empty replacement cannot silently lock the player's map.
void injectPermitPlace(std::string& body, std::string_view permit);
}
