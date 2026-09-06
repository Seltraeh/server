#include "MissionArchiver.hpp"

#include "UnitArchiver.hpp"

#include <gimuserver/App.hpp>
#include <gimuserver/utils/JsonFile.hpp>
#include <gimuserver/utils/Random.hpp>

#include <drogon/drogon.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <format>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

std::string MissionArchiver::encodeAIConditions(const AiAction& action)
{
	// For string values, the client expects "non" to mean empty,
	// so we default to that when encoding empty strings.
	const auto nonIfEmpty = [](const std::string& value) -> std::string_view {
		return value.empty() ? std::string_view("non") : std::string_view(value);
	};

	std::ostringstream stream;

	const auto partyCnt = std::min(action.party_conditions.size(), kMaxPartyConditions);
	for (size_t i = 0; i < partyCnt; ++i)
	{
		const auto& condition = action.party_conditions[i];
		stream << std::format(
			"{}:{}:{}:{}@",
			condition.target_id,
			nonIfEmpty(condition.target_parameter),
			nonIfEmpty(condition.type),
			nonIfEmpty(condition.parameters));
	}
	for (size_t i = partyCnt; i < kMaxPartyConditions; ++i)
	{
		stream << "0:non:non:non@";
	}

	stream << '#';

	const auto selfCnt = std::min(action.self_conditions.size(), kMaxSelfConditions);
	for (size_t i = 0; i < selfCnt; ++i)
	{
		const auto& condition = action.self_conditions[i];
		stream << std::format(
			"{}:{}@",
			nonIfEmpty(condition.type),
			condition.parameter);
	}
	for (size_t i = selfCnt; i < kMaxSelfConditions; ++i)
	{
		stream << "non:0@";
	}

	return stream.str();
}

std::string MissionArchiver::encodeAIAction(const Action& action)
{
	std::ostringstream stream;
	stream << action.type << '@';
	const auto flagCnt = std::min(action.flag_changes.size(), kMaxActionFlagCount);
	for (size_t i = 0; i < flagCnt; ++i)
	{
		stream << action.flag_changes[i] << ',';
	}
	for (size_t i = flagCnt; i < kMaxActionFlagCount; ++i)
	{
		stream << "-1,";
	}
	stream << std::format(
		"@{}@{}@{}",
		action.unknown_bool ? 1 : 0,
		action.unknown_int_1,
		action.unknown_int_2);
	return stream.str();
}

uint32_t MissionArchiver::rollUnitType()
{
	// F_UNIT_TYPE_MST.appearance_rate is the game's own distribution over the
	// six personality types — 23/23/22/22/10/0, summing to exactly 100, with
	// Rex at 0 because it is never handed out by a normal drop.  Rolling
	// against it means an authored mission does not have to hardcode a type,
	// and each play of the same mission can award a different one.
	const auto& types = theServer()->cache().unitTypeMst();

	double total = 0.0;
	for (const auto& t : types)
	{
		total += parseRate(t.appearance_rate);
	}
	if (total <= 0.0)
	{
		return 1;   // no usable rates — fall back to Lord
	}

	// RandomUInt is integral, so roll in hundredths of a percent to keep the
	// 22-vs-23 split honest rather than rounding it away.
	auto ticket = static_cast<double>(RandomUInt(1, static_cast<uint32_t>(total * 100.0))) / 100.0;
	for (const auto& t : types)
	{
		const auto rate = parseRate(t.appearance_rate);
		if (rate <= 0.0)
		{
			continue;
		}
		if (ticket <= rate)
		{
			return static_cast<uint32_t>(t.unit_type_id);
		}
		ticket -= rate;
	}
	return 1;
}

double MissionArchiver::parseRate(const std::string& rate)
{
	try
	{
		return std::stod(rate);
	}
	catch (const std::exception&)
	{
		return 0.0;
	}
}

std::string MissionArchiver::encodeUnitDrop(size_t monsterIdx, const BattleMonster& monster)
{
	if (monster.unit_drop_id == 0 || RandomUInt(1, 100) > monster.unit_drop_chance)
	{
		return "";
	}

	// unit_drop_type 0 means "let the server pick", so the author does not have
	// to hardcode Lord/Anima/… and a replay can award a different variant.  The
	// roll happens here, per mission start, alongside the drop roll above.
	const auto dropType = monster.unit_drop_type == 0
		? rollUnitType()
		: monster.unit_drop_type;

	return std::format(
		"{}:0:{}:{}:{}",
		monsterIdx,
		monster.unit_drop_id,
		monster.unit_drop_level,
		dropType);
}

std::string MissionArchiver::encodeTreasureDrop(size_t monsterIdx, const BattleMonster& monster)
{
	if (monster.treasure_drops.empty()
		|| monster.treasure_chest_chance == 0
		|| RandomUInt(1, 100) > monster.treasure_chest_chance)
	{
		return "";
	}

	uint32_t total = 0;
	std::vector<const TreasureDrop*> drops;
	drops.reserve(monster.treasure_drops.size());
	for (const auto& drop : monster.treasure_drops)
	{
		if (drop.weight == 0 || drop.amount == 0 || !validTreasureType(drop.target_type))
		{
			continue;
		}

		if (drop.target_type == 4 && drop.target_id.empty())
		{
			continue;
		}

		total += drop.weight;
		drops.push_back(&drop);
	}

	if (drops.empty())
	{
		return "";
	}

	// Go through each possible drop and return the selected one.
	auto roll = RandomUInt(1, total);
	for (const auto drop : drops)
	{
		if (roll > drop->weight)
		{
			roll -= drop->weight;
			continue;
		}

		if (drop->target_type == 4)
		{
			return std::format(
				"{}/1/{}:{}:{}:{}",
				monsterIdx,
				drop->target_type,
				drop->weight,
				drop->target_id,
				drop->amount);
		}

		return std::format(
			"{}/1/{}:{}:{}",
			monsterIdx,
			drop->target_type,
			drop->weight,
			drop->amount);
	}

	return "";
}

std::string MissionArchiver::encodeMissionDropInfo(const MissionRecord& record)
{
	std::ostringstream stream;
	for (size_t stageIdx = 0; stageIdx < record.stages.size(); ++stageIdx)
	{
		const auto& stage = record.stages[stageIdx];
		const auto stageId = static_cast<uint32_t>(stageIdx + 1);
		if (stageIdx != 0)
		{
			stream << '@';
		}

		stream << stageId << '|';

		std::string unitDrops;
		std::string treasureDrops;
		for (size_t monsterIdx = 0; monsterIdx < stage.battle_monsters.size(); ++monsterIdx)
		{
			const auto unitDrop = encodeUnitDrop(monsterIdx, stage.battle_monsters[monsterIdx]);
			if (!unitDrop.empty() && !unitDrops.empty())
			{
				unitDrops += '-';
			}
			unitDrops += unitDrop;

			const auto treasureDrop = encodeTreasureDrop(monsterIdx, stage.battle_monsters[monsterIdx]);
			if (!treasureDrop.empty() && !treasureDrops.empty())
			{
				treasureDrops += '-';
			}
			treasureDrops += treasureDrop;
		}

		stream << (unitDrops.empty() ? " " : unitDrops) << '|';
		stream << (treasureDrops.empty() ? " " : treasureDrops);
	}

	return stream.str();
}

MissionArchiver& MissionArchiver::instance()
{
	static MissionArchiver instance;
	return instance;
}

void MissionArchiver::setup(const Json::Value& serverObj)
{
	LOG_INFO << "Setting up mission archiver cache. "
		"If you see this after initialization, it is a bug.";

	const auto archiveRoot = serverObj["archive_root"].asString();
	if (archiveRoot.empty())
	{
		LOG_ERROR << "Unable to set up mission archiver cache: archive_root is empty";
		return;
	}

	std::vector<MissionRecord> missions;
	std::vector<AiRecord> ais;
	try
	{
		missions = LoadJson<std::vector<MissionRecord>>(archiveRoot, "mission.json");
		ais = LoadJson<std::vector<AiRecord>>(archiveRoot, "ai.json");
	}
	catch (const std::exception& ex)
	{
		LOG_ERROR << "Unable to set up mission archiver cache: " << ex.what();
		return;
	}

	AiRecordCache nextAiCache;
	nextAiCache.reserve(ais.size());
	for (auto& ai : ais)
	{
		nextAiCache.insert_or_assign(ai.id, std::move(ai));
	}

	aiCache_ = std::move(nextAiCache);

	MissionRecordCache nextMissionCache;
	nextMissionCache.reserve(missions.size());
	for (auto& mission : missions)
	{
		nextMissionCache.insert_or_assign(mission.id, std::move(mission));
	}

	missionCache_ = std::move(nextMissionCache);

	LOG_INFO << "Loaded " << missionCache_.size()
		<< " mission archive records and " << aiCache_.size()
		<< " AI archive records from " << archiveRoot;
}

std::optional<MissionRecord> MissionArchiver::lookup(MissionId mission_id) const
{
	const auto it = missionCache_.find(mission_id);
	if (it == missionCache_.end())
	{
		LOG_ERROR << "Unable to find mission archive record " << mission_id;
		return std::nullopt;
	}

	return it->second;
}

bool MissionArchiver::populatePacket(const MissionRecord& record, std::vector<AiMst>& msts)
{
	// Gather unique AI ids referenced by the mission record.
	std::set<AiId> ids;
	for (const auto& stage : record.stages)
	{
		for (const auto& monster : stage.battle_monsters)
		{
			ids.insert(monster.ai_id);
		}
	}
	if (ids.empty())
	{
		LOG_ERROR << "Unable to populate AI MST rows: mission " << record.id
			<< " does not reference any AI";
		return false;
	}

	msts.clear();
	// Most tutorial AI records have one action, but records can expand to
	// multiple MST rows when they contain multiple actions.
	msts.reserve(ids.size());
	for (const auto id : ids)
	{
		const auto it = instance().aiCache_.find(id);
		if (it == instance().aiCache_.end())
		{
			LOG_ERROR << "Unable to populate AI MST rows: missing AI archive record " << id;
			return false;
		}

		const auto& aiRecord = it->second;
		if (aiRecord.actions.empty())
		{
			LOG_ERROR << "Unable to populate AI MST rows: AI archive record "
				<< id << " has no actions";
			return false;
		}

		for (const auto& action : aiRecord.actions)
		{
			msts.push_back({
				.ai_id = aiRecord.id,
				.priority = action.priority,
				.name = aiRecord.name,
				.conditions = encodeAIConditions(action),
				.act_target = action.act_target,
				.search_term = action.search_term,
				.action = encodeAIAction(action.action),
				.percent = action.percent
			});
		}
	}

	return true;
}

namespace
{

// Packs BattleMonster.skills into MonsterMst.monster_skill_id.
//
// The client's own parser defines the format: BattleUnit::setUnitSkill
// @0x1120CF0 splits on '@' and then each entry on ':', taking [0] as the skill
// id and [1] (optional, 0 when absent) as the "announce the skill name" flag.
// The id resolves in UnitSkillMstList -- the ORDINARY skill table -- so a boss
// skill needs no master data this server does not already ship.
//
// A monster with no skills gets an empty string, which is what every captured
// live row carries.
std::string monsterSkillList(const BattleMonster& monster)
{
	if (!monster.skills || monster.skills->empty())
		return {};

	std::string packed;
	for (const auto& skill : *monster.skills)
	{
		if (!packed.empty())
			packed += '@';
		packed += std::to_string(skill.skill_id);
		// The flag is only worth sending when it is on: an entry of one element
		// is explicitly legal and reads as 0.
		if (skill.show_name)
			packed += ":1";
	}
	return packed;
}

// Packs BattleMonster.ailment_resists into MonsterMst.bad_state_resists.
//
// Six colon-separated percentages.  The SLOT ORDER is the unproven part and it
// is deliberately expressed only here and in the archive schema, so a future
// correction is a one-line change rather than a sweep of authored missions.
std::string monsterAilmentResists(const BattleMonster& monster)
{
	if (!monster.ailment_resists)
		return "0:0:0:0:0:0";

	const auto& r = *monster.ailment_resists;
	return std::format("{}:{}:{}:{}:{}:{}",
		r.poison, r.weak, r.sick, r.injury, r.curse, r.paralysis);
}

} // namespace

bool MissionArchiver::populatePacket(const MissionRecord& record, std::vector<MonsterMst>& msts)
{
	msts.clear();
	msts.reserve(record.stages.size() * kMaxMonstersPerStage);
	for (size_t stageIdx = 0; stageIdx < record.stages.size(); ++stageIdx)
	{
		const auto& stage = record.stages[stageIdx];
		for (size_t monsterIdx = 0; monsterIdx < stage.battle_monsters.size(); ++monsterIdx)
		{
			const auto& monster = stage.battle_monsters[monsterIdx];

			// A special monster (a story boss) is one the client cannot resolve
			// to a playable unit: GameScene::requestMonsterFiles @0x160F648 asks
			// UnitMstList for this row's unit_id and switches asset root on the
			// answer -- /unit/ with derived filenames when it resolves,
			// /monster/ with the literal img_a / anm_cgg filenames when it does
			// not.  So the archive record carries its own visuals and we must
			// NOT look up a unit; doing so would also fail, since a boss's
			// unit_id is deliberately unresolvable.
			const MonsterVisuals* visuals = monster.visuals ? &*monster.visuals : nullptr;

			const UnitRecord* unit = nullptr;
			std::optional<UnitRecord> unitRecord;
			if (!visuals)
			{
				// Visual and movement fields come from the curated unit archive.
				unitRecord = UnitArchiver::instance().lookup(monster.unit_id);
				if (!unitRecord)
				{
					LOG_ERROR << "Unable to populate monster MST rows: missing unit archive record "
						<< monster.unit_id << " for mission " << record.id;
					return false;
				}
				unit = &*unitRecord;
			}
			else if (visuals->img_a.empty() || visuals->anm_cgg.empty())
			{
				// Half a boss renders nothing at all, so refuse it here rather
				// than let the client silently draw an empty sprite.
				LOG_ERROR << "Unable to populate monster MST rows: monster " << monster.id
					<< " in mission " << record.id
					<< " has visuals but img_a or anm_cgg is empty";
				return false;
			}

			msts.push_back({
				.monster_id = monster.id,
				// The client shows getMstText("MST_MONSTERS_" + monster_id +
				// "_NAME", base_name) -- so this is the FALLBACK used when the
				// id has no localisation entry, not the name itself.
				//
				// It used to be the id as a string, which is why an enemy whose
				// id is not a real monster id displayed as a bare number on
				// screen.  The authored archives are full of those: a unit id
				// makes a poor monster id, and 431 of them are actively WRONG
				// rather than merely missing (unit 10050 is Goblin, but
				// MST_MONSTERS_10050_NAME is "Burning Vargas").
				//
				// Feeding the archive's own name means a correct monster id
				// still wins through localisation, and everything else falls
				// back to the right English name instead of a number.
				.base_name = monster.name,
				.hp = monster.hp,
				.atk = monster.atk,
				.def = monster.def,
				.element = visuals ? visuals->element : unit->element,
				.effect_frame = visuals ? visuals->effect_frame : unit->effect_frame,
				.damage_frame = visuals ? visuals->damage_frame : unit->damage_frame,
				.drop_check_count = visuals ? visuals->drop_check_count : unit->drop_check_count,
				.img_a = visuals ? visuals->img_a : "",
				.anm_cgg = visuals ? visuals->anm_cgg : "",
				// Deliberately the AUTHORED id, not unit->id: for a special
				// monster this is the value that must fail the client's lookup.
				.unit_id = visuals ? monster.unit_id : unit->id,
				.move_speed = visuals ? visuals->move_speed : unit->move_speed,
				.attack_move_type = visuals ? visuals->attack_move_type : unit->attack_move_type,
				.back_move_type = visuals ? visuals->back_move_type : unit->back_move_type,
				.ai_id = monster.ai_id,
				.after_image = visuals ? visuals->after_image : unit->after_image,
				.max_act_count = monster.act_max,
				.min_act_count = monster.act_min,
				// The four constants below are what every captured live row carried.
				// They are seeded rather than defaulted because the KDL fields are new
				// (the wire surface is 38 keys, we modelled 24) and a zero-initialised
				// act_rate would ship "0" where the live server always sent "100".
				// They become per-monster archive data once bosses are authorable.
				.act_rate = 100.0f,
				.monster_skill_id = monsterSkillList(monster),
				.skill_move_type = visuals ? visuals->skill_move_type : unit->skill_move_type,
				.bad_state_resists = monsterAilmentResists(monster),
				.max_zel_drop = monster.zel_max_drop,
				.zel_drop_count = monster.zel_drop_count,
				.max_karma_drop = monster.karma_max_drop,
				.karma_drop_count = monster.karma_drop_count,
				.hp_disp_pos = visuals ? visuals->hp_disp_pos : "",
				.move_offset = "0,0",
				.debuff_resist = "0,0,0",
				.wait = monster.wait,
				.cursor_disp_pos = visuals ? visuals->cursor_disp_pos : unit->cursor_disp_pos
			});
		}
	}

	if (msts.empty())
	{
		LOG_ERROR << "Unable to populate monster MST rows: mission " << record.id
			<< " has no monsters";
		return false;
	}

	return true;
}

bool MissionArchiver::populatePacket(
	const MissionRecord& record,
	std::optional<std::vector<MonsterCgsMst>>& msts)
{
	// One row per populated animation slot, keyed on the MONSTER id -- not the
	// unit id the sibling UnitCgsMst table uses (GameUtils::getMonsterCgs
	// @0x1EAFD74 reads a different list on each side of the branch).
	static constexpr std::pair<uint32_t, std::string MonsterVisuals::*> kSlots[] = {
		{ 1, &MonsterVisuals::cgs_idle },
		{ 2, &MonsterVisuals::cgs_move },
		{ 3, &MonsterVisuals::cgs_atk },
		{ 4, &MonsterVisuals::cgs_skill }
	};

	std::vector<MonsterCgsMst> rows;
	for (const auto& stage : record.stages)
	{
		for (const auto& monster : stage.battle_monsters)
		{
			if (!monster.visuals)
			{
				continue;
			}

			for (const auto& [cgsType, member] : kSlots)
			{
				const auto& file = (*monster.visuals).*member;
				if (file.empty())
				{
					// Most special monsters ship no move animation at all; an
					// empty row would point the client at a file that is not
					// there, so skip the slot instead.
					continue;
				}

				rows.push_back({
					.monster_id = monster.id,
					.cgs_type = cgsType,
					.anm_cgs = file
				});
			}
		}
	}

	// Leave the block ABSENT rather than empty when the mission has no special
	// monsters.  MonsterCgsMstResponse::readParam @0x13E4E20 opens with
	// MonsterCgsMstList::removeAllObjects, so an empty array would wipe the
	// client's cached list on every ordinary mission start.
	msts = rows.empty() ? std::nullopt : std::optional{ std::move(rows) };
	return true;
}

bool MissionArchiver::populatePacket(
	const MissionRecord& record,
	std::vector<BattleMonsterGroupMst>& msts)
{
	msts.clear();
	msts.reserve(record.stages.size() * kMaxMonstersPerStage);
	for (size_t stageIdx = 0; stageIdx < record.stages.size(); ++stageIdx)
	{
		const auto& stage = record.stages[stageIdx];
		const auto stageId = static_cast<uint32_t>(stageIdx + 1);
		for (size_t monsterIdx = 0; monsterIdx < stage.battle_monsters.size(); ++monsterIdx)
		{
			const auto& monster = stage.battle_monsters[monsterIdx];

			msts.push_back({
				.battle_monster_group_id = stageId,
				.monster_id = monster.id,
				.group_order = static_cast<uint32_t>(monsterIdx),
				.position = monster.position,
				// The client does not use this field to resolve unit drops; it uses the
				// encoded drop info in MissionNumInfo instead. This value only tells the
				// client which unit assets may be needed, so keep the other fields as zero.
				.unit_drop = (monster.unit_drop_id == 0 || monster.unit_drop_chance == 0)
					? "0:0:0:0"
					: std::format("0:{}:0:0", monster.unit_drop_id)
			});
		}
	}

	if (msts.empty())
	{
		LOG_ERROR << "Unable to populate battle monster group MST rows: mission "
			<< record.id << " has no monsters";
		return false;
	}

	return true;
}

bool MissionArchiver::populatePacket(const MissionRecord& record, std::vector<BattleGroupMst>& msts)
{
	if (record.stages.empty())
	{
		LOG_ERROR << "Unable to populate battle group MST rows: mission "
			<< record.id << " has no stages";
		return false;
	}

	msts.clear();
	msts.reserve(record.stages.size());

	for (size_t stageIdx = 0; stageIdx < record.stages.size(); ++stageIdx)
	{
		const auto& stage = record.stages[stageIdx];
		const auto stageId = static_cast<uint32_t>(stageIdx + 1);

		msts.push_back({
			.battle_group_id = stageId,
			.battle_monster_group_id = stageId,
			.mission_id = record.id,
			.battle_order = stageId,
			.first_attack_rate = stage.first_attack_rate,
			.boss_flag = stage.is_boss
		});
	}

	return true;
}

bool MissionArchiver::populatePacket(const MissionRecord& record, MissionNumInfo& mst)
{
	mst.serial_id = record.id;
	mst.drop_info = encodeMissionDropInfo(record);
	return true;
}
