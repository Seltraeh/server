#pragma once

#include "../GmeRequest.hpp"

RESPONSE_NS_BEGIN

struct BattleGroupMst : public IResponse
{
	struct Data
	{
		Data() : group_id(0), mission_id(0), battle_order(0),
		         first_atk_rate(0), battle_monster_id(0), boss_flg(false) {}

		uint32_t group_id, mission_id, battle_order, first_atk_rate,
			battle_monster_id;
		bool boss_flg;

		void Serialize(Json::Value& v) const
		{
			v["ZSf8e1MG"] = std::to_string(group_id);
			v["j28VNcUW"] = std::to_string(mission_id);
			v["VETu07N6"] = std::to_string(battle_order);
			v["5aetPz3C"] = std::to_string(first_atk_rate);
			v["etM5TCb9"] = std::to_string((int)boss_flg);
			v["Qzhp8B40"] = std::to_string(battle_monster_id);
		}
	};

	const char* getGroupName() const override { return "pj41dy9g"; }
	std::vector<Data> Mst;

protected:
	size_t getRespCount() const override { return Mst.size(); }
	void SerializeFields(Json::Value& v, size_t i) const override
	{
		Mst.at(i).Serialize(v);
	}
};

struct BattleMonsterGroupMst : public IResponse
{
	struct Data
	{
		Data() : battle_monster_id(0), monster_id(0), group_order(0) {}

		uint32_t battle_monster_id, monster_id, group_order;
		std::string position, item_drop, unit_drop, treasure_drop;

		void Serialize(Json::Value& v) const
		{
			v["Qzhp8B40"] = std::to_string(battle_monster_id);
			v["o49dYfpH"] = std::to_string(monster_id);
			v["hZtF1s8B"] = std::to_string(group_order);
			v["3g8PW6x0"] = position;
			v["i30R8TAs"] = item_drop;
			v["hw3L0uVj"] = unit_drop;
			v["5wB9SHAV"] = treasure_drop;
		}
	};

	const char* getGroupName() const override { return "75t0sx9z"; }
	std::vector<Data> Mst;

protected:
	size_t getRespCount() const override { return Mst.size(); }
	void SerializeFields(Json::Value& v, size_t i) const override
	{
		Mst.at(i).Serialize(v);
	}
};

struct MonsterMst : public IResponse
{
	struct Data
	{
		Data() : monster_id(0), hp(0), atk(0), def(0), element(0),
		         drop_check_cnt(0), max_zel_drop(0), zel_drop_cnt(0),
		         max_karma_drop(0), karma_drop_cnt(0), wait(0),
		         move_speed_type(0), atk_move_type(0), back_move_type(0),
		         skill_move_type(0), after_image(0), max_act_cnt(0),
		         min_act_cnt(0), ai_id(0), unit_id(0), act_rate(0.0f),
		         bad_state_resists("0:0:0:0:0:0"), move_offset("0,0"),
		         debuff_resist("0,0,0") {}

		uint32_t monster_id, hp, atk, def, element, drop_check_cnt,
			max_zel_drop, zel_drop_cnt, max_karma_drop, karma_drop_cnt,
			wait, move_speed_type, atk_move_type, back_move_type,
			skill_move_type, after_image, max_act_cnt, min_act_cnt,
			ai_id, unit_id;
		float act_rate;
		std::string bad_state_resists, debuff_resist, effect_frame,
			damage_frame, move_offset, unit_skills;

		void Serialize(Json::Value& v) const
		{
			v["o49dYfpH"] = std::to_string(monster_id);
			v["e7DK0FQT"] = std::to_string(hp);
			v["67CApcti"] = std::to_string(atk);
			v["q08xLEsy"] = std::to_string(def);
			v["CEeqs63b"] = bad_state_resists;
			v["iNy0ZU5M"] = std::to_string(element);
			v["eyUo6a8c"] = effect_frame;
			v["6Aou5M9r"] = damage_frame;
			v["n9h7p02P"] = std::to_string(drop_check_cnt);
			v["Najhr8m6"] = std::to_string(max_zel_drop);
			v["9FN0GAei"] = std::to_string(zel_drop_cnt);
			v["HTVh8a65"] = std::to_string(max_karma_drop);
			v["vNk2sI4X"] = std::to_string(karma_drop_cnt);
			v["9pXSKmn1"] = std::to_string(wait);
			v["J2hPXGo5"] = std::to_string(move_speed_type);
			v["mv4o39Uz"] = std::to_string(atk_move_type);
			v["h2L1YI90"] = std::to_string(back_move_type);
			v["XE7Yi5c3"] = std::to_string(skill_move_type);
			v["D4Y5bWK7"] = std::to_string(after_image);
			v["oMGC3hW0"] = std::to_string(max_act_cnt);
			v["m4EK7Gt6"] = std::to_string(min_act_cnt);
			v["6fwL59FT"] = std::to_string(act_rate);
			v["i74vGUFa"] = std::to_string(ai_id);
			v["CYk84E3W"] = move_offset;
			v["Lkh6gYkT"] = "0,0";
			v["pn16CNah"] = std::to_string(unit_id);
			v["Y6bd4fXp"] = debuff_resist;
			v["F4bQ7r8C"] = unit_skills;
			// Fields present in captured traffic with empty/zero defaults
			v["Btf93Xs1"] = "";
			v["3evIn0zZ"] = "";
			v["hjAy9St3"] = "";
			v["2Smu5Mtq"] = "";
			v["2EF0d6ue"] = "";
			v["QqfI9mM4"] = "";
			v["3BpHN6VD"] = "";
			v["jm6JSK2D"] = "";
			v["qp37xTDh"] = "";
		}
	};

	const char* getGroupName() const override { return "U0v5IeJo"; }
	std::vector<Data> Mst;

protected:
	size_t getRespCount() const override { return Mst.size(); }
	void SerializeFields(Json::Value& v, size_t i) const override
	{
		Mst.at(i).Serialize(v);
	}
};

struct UnitSkillMst : public IResponse
{
	struct Data
	{
		Data() : skill_id(0), skill_type(0), skill_rank(0), move_flag(0),
		         atk_move_flag(0), drop_check_cnt(0), wait(0), element(0) {}

		uint32_t skill_id, skill_type, skill_rank, move_flag,
			atk_move_flag, drop_check_cnt, wait, element;
		std::string skill_name, process_id, target_type, target_area,
			disp_frame, effect_frame, start_frame, damage_frame;

		void Serialize(Json::Value& v) const
		{
			v["nj9Lw7mV"] = std::to_string(skill_id);
			v["0nxpBDz2"] = skill_name;
			v["h6UL9A1B"] = std::to_string(skill_type);
			v["AR7y0sY4"] = std::to_string(skill_rank);
			v["hjAy9St3"] = process_id;
			v["moWQ30GH"] = target_type;
			v["IZt73kLG"] = target_area;
			v["I7cmtqW8"] = std::to_string(move_flag);
			v["K2i8emcH"] = std::to_string(atk_move_flag);
			v["diGe6u21"] = disp_frame;
			v["eyUo6a8c"] = effect_frame;
			v["qYCx73y2"] = start_frame;
			v["6Aou5M9r"] = damage_frame;
			v["n9h7p02P"] = std::to_string(drop_check_cnt);
			v["9pXSKmn1"] = std::to_string(wait);
			v["iNy0ZU5M"] = std::to_string(element);
			v["qp37xTDh"] = "";
			v["ToZj4zyn"] = "0";
			v["57BsydKH"] = "0";
			v["R8uaKdbf"] = ",,";
		}
	};

	const char* getGroupName() const override { return "8aiBoHg5"; }
	std::vector<Data> Mst;

protected:
	size_t getRespCount() const override { return Mst.size(); }
	void SerializeFields(Json::Value& v, size_t i) const override
	{
		Mst.at(i).Serialize(v);
	}
};

struct AIMst : public IResponse
{
	struct Data
	{
		Data() : ai_id(0), priority(0), target(0), percent(0) {}

		uint32_t ai_id, priority, target, percent;
		std::string ai_term, atk_param, search_term;

		void Serialize(Json::Value& v) const
		{
			v["4eEVw5hL"] = std::to_string(ai_id);
			v["yu18xScw"] = std::to_string(priority);
			v["q7Nit8JW"] = ai_term;
			v["4xctV8gF"] = std::to_string(target);
			v["VBj9u0ot"] = search_term;
			v["Hhgi79M1"] = atk_param;
			v["ug9xV4Fz"] = std::to_string(percent);
		}
	};

	const char* getGroupName() const override { return "89ausgc4"; }
	std::vector<Data> Mst;

protected:
	size_t getRespCount() const override { return Mst.size(); }
	void SerializeFields(Json::Value& v, size_t i) const override
	{
		Mst.at(i).Serialize(v);
	}
};

RESPONSE_NS_END
