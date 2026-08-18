#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/archive/MissionArchiver.hpp>
#include <gimuserver/gme/common/Common.hpp>

#include <chrono>
#include <sstream>
#include <utility>
#include <vector>

namespace
{
constexpr uint32_t kFirstTutorialMission = 1;
constexpr uint32_t kSecondTutorialMission = 2;

constexpr uint8_t kFirstTutorialCheckpoint = 2;
constexpr uint8_t kSecondTutorialCheckpoint = 10;

std::vector<UserUnitInfo> parseUnitDrops(const std::string& unitDrops)
{
	std::vector<UserUnitInfo> units;
	std::istringstream drops(unitDrops);
	for (std::string drop; std::getline(drops, drop, ',');)
	{
		if (drop.empty())
		{
			continue;
		}

		std::istringstream parts(drop);
		std::string id;
		std::string level;
		std::string type;
		if (!std::getline(parts, id, ':')
			|| !std::getline(parts, level, ':')
			|| !std::getline(parts, type, ':'))
		{
			LOG_ERROR << "Invalid mission drop unit entry: " << drop;
			return units;
		}

		auto unit = gme::fromArchivedUnit(
			static_cast<uint32_t>(std::stoul(id)),
			static_cast<uint32_t>(std::stoul(type)));
		if (!unit)
		{
			LOG_ERROR << "Unable to create mission drop unit from archive: " << drop;
			return units;
		}

		unit->unit_lvl = static_cast<uint32_t>(std::stoul(level));
		units.push_back(std::move(*unit));
	}

	return units;
}

// Parses the client-reported item drops from MissionBattleResultInfo.item_rewards
// (wire key 4T0Q2Bh5).
//
// UNVERIFIED wire format: assumed to mirror unit_rewards as a comma-separated
// list of "itemId:count" pairs.  No live capture of an item drop exists yet —
// this is the first thing to confirm with a Frida / http_log capture of a
// mission that drops an item, then correct the split here if it differs.
// Returns (item_id, count) pairs; malformed entries are skipped, not fatal.
std::vector<std::pair<uint32_t, uint32_t>> parseItemDrops(const std::string& itemDrops)
{
	std::vector<std::pair<uint32_t, uint32_t>> items;
	std::istringstream drops(itemDrops);
	for (std::string drop; std::getline(drops, drop, ',');)
	{
		if (drop.empty())
		{
			continue;
		}

		std::istringstream parts(drop);
		std::string id;
		std::string count;
		std::getline(parts, id, ':');
		// count is optional; default to 1 when the entry is a bare item id.
		const bool hasCount = static_cast<bool>(std::getline(parts, count, ':'));
		try
		{
			const auto itemId = static_cast<uint32_t>(std::stoul(id));
			const auto qty = hasCount && !count.empty()
				? static_cast<uint32_t>(std::stoul(count))
				: 1u;
			if (itemId != 0)
			{
				items.emplace_back(itemId, qty);
			}
		}
		catch (const std::exception&)
		{
			LOG_ERROR << "Invalid mission drop item entry: " << drop;
		}
	}

	return items;
}

std::string encodeUnitDrops(
	const std::vector<UserUnitInfo>& unitDrops,
	const std::vector<bool>& newFlags)
{
	std::string encoded;
	for (size_t idx = 0; idx < unitDrops.size(); ++idx)
	{
		if (!encoded.empty())
		{
			encoded += ',';
		}

		const auto& drop = unitDrops[idx];

		// reward_units is formatted as:
		// unit_id:user_unit_id:play_acquired_animation:show_reward_row
		encoded += std::to_string(drop.unit_id);
		encoded += ':';
		encoded += std::to_string(drop.user_unit_id);
		encoded += ':';
		encoded += (
			idx < newFlags.size() && newFlags[idx]
				? '1'
				: '0');
		encoded += ":1";
	}

	return encoded;
}

/*!
* Applies pending EXP to the current user level.
*
* The caller should pass exp after adding the mission reward. This function
* treats UserLevelMst::exp as the per-level chunk required to reach level + 1,
* then mutates level and exp so exp remains the residual progress at the new
* level.
*
* @return True if at least one level was gained.
*/
bool levelUp(uint32_t& level, uint32_t& exp)
{
	bool leveled = false;
	while (true)
	{
		if (const auto mst = gme::getLevelMst(level + 1))
		{
			// We don't have enough to level up.
			if (exp < mst->exp)
			{
				break;
			}
		
			// Advance a level.
			level++;
			exp -= mst->exp;
			leveled = true;
			continue;
		}
		break;
	}

	return leveled;
}
}

HANDLEF(MissionEnd)
{
	MissionEndReq req = {};
	const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json);
	if (ec)
	{
		const auto error = glz::format_error(ec, json);
		LOG_ERROR << "MissionEndReq deserialization failed:\n" << error;
		co_return HandleResult::error("Deserialization error", error);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).data;

	// Same battle-content fallback as MissionStart — without it a mission that
	// STARTS on template content would fail on completion instead, which is a
	// worse failure: the player fights the battle and then loses the result.
	// See the block in MissionStart for why only content is substituted.
	auto missionRecord = MissionArchiver::instance().lookup(req.mission_num.serial_id);
	if (!missionRecord)
	{
		missionRecord = MissionArchiver::instance().lookup(10);
		if (!missionRecord)
		{
			co_return HandleResult::error("Archive error", "Unable to find mission record");
		}
		// Relabelled for the same reason as MissionStart — the result screen
		// reads the mission back out of the response.
		missionRecord->id = req.mission_num.serial_id;

		LOG_WARN << "MissionEnd: mission " << req.mission_num.serial_id
		         << " has no authored battle content; using the template record "
		            "relabelled as this mission";
	}

	MissionEndResp resp{};

	// This needs to be wrapped as a transaction to prevent cases where we issue
	// partial rewards upon mission completion.
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			auto userInfo = co_await db::DatabaseInterface::read(
				transaction,
				"user_info",
				{
					db::Data("level"),
					db::Data("exp"),
					db::Data("zel"),
					db::Data("karma"),
					db::Data("brave_coin"),
					db::Lookup("gumi_user_id", identity.gumiUserId),
					db::Lookup("id", identity.userId),
			});

			// Fetch the current user state.
			const auto currentZel = userInfo.front<uint64_t>("zel");
			const auto currentKarma = userInfo.front<uint64_t>("karma");
			const auto currentLevel = userInfo.front<uint32_t>("level");
			const auto currentExp = userInfo.front<uint32_t>("exp");
			const auto currentBraveCoin = userInfo.front<int32_t>("brave_coin");

			// Fetch mission rewards from archive.
			const auto rewardZel = req.battle_result.zel + missionRecord->zel;
			const auto rewardKarma = req.battle_result.karma + missionRecord->karma;
			const auto rewardExp = missionRecord->exp;
	
			// See if we leveled up.
			auto newLevel = currentLevel;
			auto newExp = currentExp + rewardExp;
			const auto leveledUp = levelUp(newLevel, newExp);

			// The client expects the post-mission total, including the amount earned
			// during this mission.
			(co_await db::DatabaseInterface::update(
				transaction,
				"user_info",
				{
					db::Data("level", newLevel),
					db::Data("exp", newExp),
					db::Data("zel", currentZel + rewardZel),
					db::Data("karma", currentKarma + rewardKarma),
					db::Lookup("gumi_user_id", identity.gumiUserId),
					db::Lookup("id", identity.userId)
				})).nonEmpty();

			if (leveledUp)
			{
				(co_await gme::UserEnergy::refresh(transaction, identity, newLevel, true)).nonEmpty();
			}

			// Persist tutorial checkpoints so leaving and returning mid-tutorial does not
			// replay completed steps.
			if (req.mission_num.serial_id == kFirstTutorialMission)
			{
				(co_await db::DatabaseInterface::update(
					transaction,
					"user_info",
					{
						db::Data("tutorial_status", kFirstTutorialCheckpoint),
						db::Lookup("gumi_user_id", identity.gumiUserId),
						db::Lookup("id", identity.userId),
					})).nonEmpty();
			}
			else if (req.mission_num.serial_id == kSecondTutorialMission)
			{
				(co_await db::DatabaseInterface::update(
					transaction,
					"user_info",
					{
						db::Data("tutorial_status", kSecondTutorialCheckpoint),
						db::Lookup("gumi_user_id", identity.gumiUserId),
						db::Lookup("id", identity.userId),
					})).nonEmpty();
			}

			// Record the clear in the mission clear-history
			// (user_campaign_missions, state=2).  UserInfo reports this set as
			// UT1SVg59 (UserClearMissionInfo) — the list the client evaluates
			// feature unlocks against (F_FUNCTION_RELEASE_MST conditions and
			// the hardcoded town/early-feature gates), so every victorious
			// MissionEnd must land here.
			{
				const auto clearedAt = static_cast<int64_t>(
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now().time_since_epoch()).count());
				co_await transaction->execSqlCoro(
					"INSERT INTO user_campaign_missions"
					" (user_id, mission_id, state, attain_percent, clear_count, last_cleared_at)"
					" VALUES ($1, $2, 2, 100, 1, $3)"
					" ON CONFLICT(user_id, mission_id) DO UPDATE SET"
					" state=2, attain_percent=100,"
					" clear_count=clear_count+1, last_cleared_at=$3;",
					identity.userId,
					std::to_string(req.mission_num.serial_id),
					clearedAt);
			}

			auto loginInfo = std::move((co_await gme::getLoginInfo(transaction, identity)).nonEmpty());
			auto teamInfo = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());

			auto droppedUnits = parseUnitDrops(req.battle_result.unit_rewards);

			// We need to track which units are already seen by the player.
			std::vector<bool> newFlags;
			newFlags.reserve(droppedUnits.size());
			for (auto& dropped : droppedUnits)
			{
				// Read from unit dictionary to see if the player has already seen this unit.
				newFlags.push_back((co_await db::PacketInterfaceFor<UserUnitDictionary>::read(
					transaction,
					"user_unit_dictionary",
					{
						db::Lookup("user_id", identity.userId),
						db::Lookup("unit_id", dropped.unit_id),
					})).affected == 0);

				// Add the unit and persist it to the dictionary.
				dropped = std::move(
					(co_await gme::addUserUnit(
						transaction,
						identity,
						dropped)).nonEmpty());
				resp.unit_dictionary.push_back(std::move(
					(co_await db::PacketInterfaceFor<UserUnitDictionary>::read(
						transaction,
						"user_unit_dictionary",
						{
							db::Lookup("user_id", identity.userId),
							db::Lookup("unit_id", dropped.unit_id),
						})).nonEmpty().front()));
			}

			// Credit item/sphere drops reported by the client to the player's
			// warehouse.  Spheres travel the same path as any other item — they
			// just carry stat params in ItemMst.  The client already renders
			// the obtained-item cards from its own battle-result state, so we
			// only need to persist ownership here.
			for (const auto& [itemId, qty] : parseItemDrops(req.battle_result.item_rewards))
			{
				(co_await gme::addUserItem(transaction, identity, itemId, qty));
			}

			resp.reward_info.clear_mission_id = req.mission_num.serial_id;
			resp.reward_info.zel = rewardZel;
			resp.reward_info.karma = rewardKarma;
			resp.reward_info.before_level = currentLevel;
			resp.reward_info.lvlup_flag = leveledUp ? 1 : 0;
			resp.reward_info.inc_exp = rewardExp;
			resp.reward_info.reward_units = encodeUnitDrops(droppedUnits, newFlags);
			resp.login_info = std::move(loginInfo);
			resp.team_info = std::move(teamInfo);
			resp.unit_info = std::move(droppedUnits);
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	std::string buffer;
	const auto& writeError = glz::write_json(resp, buffer);
	if (writeError)
	{
		const auto& glze = glz::format_error(writeError, buffer);
		LOG_ERROR << "MissionEnd response serialization failed: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}

HANDLEF(MissionStart)
{
	MissionStartReq req = {};
	const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json);
	if (ec)
	{
		const auto error = glz::format_error(ec, json);
		LOG_ERROR << "MissionStartReq deserialization failed:\n" << error;
		co_return HandleResult::error("Deserialization error", error);
	}

	// BATTLE-CONTENT FALLBACK.
	//
	// deploy/archive/mission.json is the authored battle content — waves, AIs,
	// monsters, battle groups — and it currently holds THREE missions (1, 2,
	// 10).  Every other mission in the game has metadata in mission_mst.json
	// but no authored fight, so starting one used to fail outright with
	// "Archive error" (Frontier Gate hit this on mission 9010001 "Panache",
	// 2026-08-07).
	//
	// Rather than block every unauthored mission, serve a template mission's
	// content so the flow is exercisable.  This is the existing known
	// limitation — "all missions use mission 10's enemy data" — made explicit
	// and applied where it was previously an error.
	//
	// CRITICAL: only the CONTENT comes from the template.  resp.start_info is
	// echoed from the request below, so the response still names the mission the
	// client asked for.  Handbook §3.1 is the story of what happens otherwise:
	// answering mission 11 with "mission 10" crashed the client outright.
	//
	// Replace this with real authored content per mission — that is what the
	// mission editor exists for.  When the archive covers a mission, nothing
	// here runs.
	constexpr MissionArchiver::MissionId kTemplateMissionId = 10;

	auto missionRecord = MissionArchiver::instance().lookup(req.start_info.mission_id);
	const bool usingTemplate = !missionRecord;
	if (usingTemplate)
	{
		missionRecord = MissionArchiver::instance().lookup(kTemplateMissionId);
		if (!missionRecord)
		{
			// The template itself is missing, so the archive is unusable.
			co_return HandleResult::error("Archive error", "Unable to find mission record");
		}
		// RELABEL THE TEMPLATE AS THE REQUESTED MISSION.
		//
		// populatePacket stamps record.id into BattleGroupMst::mission_id and
		// MissionNumInfo::serial_id.  Left alone, the response would carry
		// start_info for mission 9010001 while every battle group and the
		// mission_num claimed mission 10 — the client resolves the mission it
		// asked for, finds battle data belonging to a different one, and dies
		// during load.  That is handbook §3.1's mismatch crash exactly, and it
		// is what echoing start_info alone did NOT fix (verified 2026-08-07: FG
		// downloaded its assets and then crashed with a clean server log).
		//
		// Safe because the archive contract is explicit that the SERVER owns
		// these ids: "Client battle group ids and encoded AI/drop wire strings
		// are generated while formatting a mission-start response"
		// (MissionArchiver.hpp).  The client does not require the group ids its
		// own MST lists — only that the response is internally consistent about
		// which mission it describes.
		missionRecord->id = req.start_info.mission_id;

		LOG_WARN << "MissionStart: mission " << req.start_info.mission_id
		         << " has no authored battle content; serving mission "
		         << kTemplateMissionId << "'s waves relabelled as this mission";
	}

	// Energy is only consumed for a mission we actually have data for.  The
	// template's cost belongs to mission 10, not to whatever was requested, and
	// charging a made-up amount is the §3.4 anti-pattern; the real per-mission
	// cost lives in the archive (handbook §6.15 rule 3), which by definition
	// does not have this one.  Frontier Gate in particular spends Hunter Orbs
	// (Aube) client-side rather than energy — see §6.16.
	if (!usingTemplate && missionRecord->energy_cost > 0)
	{
		const auto identity = (co_await gme::getUserIdentity(
			theDb(),
			req.login_info)).nonEmpty();
		(co_await gme::UserEnergy::consume(
			theDb(),
			identity,
			missionRecord->energy_cost)).nonEmpty();
	}

	MissionStartResp resp{};
	resp.signal_key = req.signal_key;
	resp.start_info = req.start_info;

	if (!MissionArchiver::populatePacket(*missionRecord, resp.mission_num)
		|| !MissionArchiver::populatePacket(*missionRecord, resp.ais)
		|| !MissionArchiver::populatePacket(*missionRecord, resp.monsters)
		|| !MissionArchiver::populatePacket(*missionRecord, resp.battle_monster_groups)
		|| !MissionArchiver::populatePacket(*missionRecord, resp.battle_groups))
	{
		co_return HandleResult::error("Archive error", "Unable to populate mission start response");
	}

	std::string buffer;
	const auto& writeError = glz::write_json(resp, buffer);
	if (writeError)
	{
		const auto& glze = glz::format_error(writeError, buffer);
		LOG_ERROR << "MissionStart response serialization failed: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}
