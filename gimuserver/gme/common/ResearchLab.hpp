#pragma once

#include <cstdint>
#include <set>

namespace gme
{
/*!
* THE SUMMONERS' RESEARCH LAB: the Trial Zone and the Strategy Zone.
*
* DungeonMst.dungeon_type names the lab: 2 is the Trial Zone (the exact value
* GameUtils::isTrialMission tests to enter the client's three-squad mode) and
* 8 the Strategy Zone, and those are the only two dungeons in the table that
* carry either value.  Trial of the Gods (type 0) is a different feature.
*
* Rules the wiki's lab page states (Summoners' Research Lab, rev 653535):
*  - "only Trial 001 will be available to you ... No other trials will be
*    available to you until the previous trial is completed";
*  - Gems, EXP and Zel come "if you managed to defeat them for the first
*    time.  Subsequent victories won't award anything."
*/

/*!
* Every mission id in the lab's two dungeons, from F_MISSION_MST.
*/
const std::set<int32_t>& researchLabMissions();

/*!
* Whether a mission belongs to the Research Lab.
*/
bool isResearchLabMission(int32_t missionId);

/*!
* Whether a lab mission is unlocked for a player.
*
* Every prerequisite in its need_mission_id must be cleared -- the lab's page
* says both "the defeat of Creator Maxwell in St. Lamia, and the completion of
* Trial No. 001 and 002" for Trial No. 003, so this is ALL-of, unlike the
* campaign's any-of policy.  A prerequisite with no archive record is
* skipped: it can never be cleared here, and honouring it would lock the
* series behind content that does not exist.
*
* @param missionId A lab mission.
* @param cleared The player's cleared mission ids.
*/
bool researchLabUnlocked(int32_t missionId, const std::set<int32_t>& cleared);
} // namespace gme
