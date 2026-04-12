#pragma once

#include "../IMigration.hpp"
#include "../DbMacro.hpp"
#include <fstream>
#include <sstream>
#include <json/json.h>

MIGRATION_NS_BEGIN

struct PopulateUnitMstTable : public IMigration
{
    void execute(drogon::orm::DbClientPtr db) override
    {
        // Step A: Create unit_mst table
        db->execSqlSync(
            "CREATE TABLE IF NOT EXISTS unit_mst ("
            "  unit_id      TEXT PRIMARY KEY,"
            "  lord_hp      INTEGER DEFAULT 1000,"
            "  lord_atk     INTEGER DEFAULT 1000,"
            "  lord_def     INTEGER DEFAULT 1000,"
            "  lord_rec     INTEGER DEFAULT 1000,"
            "  add_hp       INTEGER DEFAULT 100,"
            "  add_atk      INTEGER DEFAULT 100,"
            "  add_def      INTEGER DEFAULT 100,"
            "  add_heal     INTEGER DEFAULT 100,"
            "  bb_id        INTEGER DEFAULT 0,"
            "  sbb_id       INTEGER DEFAULT 0,"
            "  ls_id        INTEGER DEFAULT 0,"
            "  element      TEXT DEFAULT 'fire',"
            "  unit_kind    INTEGER DEFAULT 1"
            ")"
        );

        // Step B: Load JSON files and populate unit_mst
        static const char* kFiles[] = {
            "./system/F_UNIT_MST_1_Ver1084.json",
            "./system/F_UNIT_MST_2_Ver1084.json",
        };

        static const char* kElementMap[] = {
            "fire",     // 1
            "water",    // 2
            "earth",    // 3
            "thunder",  // 4
            "light",    // 5
            "dark",     // 6
        };

        for (const char* path : kFiles)
        {
            std::ifstream ifs(path);
            if (!ifs.is_open())
            {
                LOG_WARN << "PopulateUnitMstTable: file not found, skipping: " << path;
                continue;
            }

            Json::CharReaderBuilder rb;
            Json::Value root;
            std::string errs;
            if (!Json::parseFromStream(rb, ifs, &root, &errs))
            {
                LOG_ERROR << "PopulateUnitMstTable: parse error in " << path << ": " << errs;
                continue;
            }

            // Build batch INSERTs of 500 rows at a time
            const int kBatchSize = 500;
            std::string sql;
            int count = 0;
            int total = 0;

            auto flush = [&]() {
                if (sql.empty()) return;
                try { db->execSqlSync(sql); }
                catch (const std::exception& e) {
                    LOG_ERROR << "PopulateUnitMstTable: batch insert error: " << e.what();
                }
                sql.clear();
                count = 0;
            };

            auto escape = [](const std::string& s) -> std::string {
                std::string out;
                out.reserve(s.size());
                for (char c : s) {
                    if (c == '\'') out += "''";
                    else out += c;
                }
                return out;
            };

            for (const auto& unit : root)
            {
                std::string unitId = unit.get("unitId", "").asString();
                if (unitId.empty()) continue;

                // Stats
                int lordHp  = std::stoi(unit.get("unitLordHp",  "1000").asString());
                int lordAtk = std::stoi(unit.get("unitLordAtk", "1000").asString());
                int lordDef = std::stoi(unit.get("unitLordDef", "1000").asString());
                int lordRec = std::stoi(unit.get("unitLordRec", "1000").asString());

                // Imp caps: "HP:ATK:DEF:REC"
                int addHp = 100, addAtk = 100, addDef = 100, addHeal = 100;
                std::string impCaps = unit.get("unitImpCaps", "").asString();
                if (!impCaps.empty())
                {
                    std::istringstream ss(impCaps);
                    std::string tok;
                    int idx = 0;
                    while (std::getline(ss, tok, ':') && idx < 4)
                    {
                        int val = tok.empty() ? 0 : std::stoi(tok);
                        if      (idx == 0) addHp   = val;
                        else if (idx == 1) addAtk  = val;
                        else if (idx == 2) addDef  = val;
                        else if (idx == 3) addHeal = val;
                        ++idx;
                    }
                }

                // Skill IDs
                int bbId  = std::stoi(unit.get("bbId",  "0").asString());
                int sbbId = std::stoi(unit.get("sbbId", "0").asString());
                int lsId  = std::stoi(unit.get("lsId",  "0").asString());

                // Element (1–6 → text)
                std::string elemStr = unit.get("element", "1").asString();
                int elemIdx = elemStr.empty() ? 0 : (std::stoi(elemStr) - 1);
                if (elemIdx < 0 || elemIdx > 5) elemIdx = 0;
                const char* element = kElementMap[elemIdx];

                // Unit kind — preserve 0 (enhancement/fodder) as-is; the client
                // uses it to distinguish normal units (1) from fodder (0) in the
                // ingredient selection screen.
                int unitKind = std::stoi(unit.get("unitKind", "1").asString());

                // Build row
                std::string row =
                    "('" + escape(unitId) + "',"
                    + std::to_string(lordHp)  + ","
                    + std::to_string(lordAtk) + ","
                    + std::to_string(lordDef) + ","
                    + std::to_string(lordRec) + ","
                    + std::to_string(addHp)   + ","
                    + std::to_string(addAtk)  + ","
                    + std::to_string(addDef)  + ","
                    + std::to_string(addHeal) + ","
                    + std::to_string(bbId)    + ","
                    + std::to_string(sbbId)   + ","
                    + std::to_string(lsId)    + ","
                    "'" + element + "',"
                    + std::to_string(unitKind)
                    + ")";

                if (count == 0)
                    sql = "INSERT OR IGNORE INTO unit_mst "
                          "(unit_id,lord_hp,lord_atk,lord_def,lord_rec,"
                          " add_hp,add_atk,add_def,add_heal,"
                          " bb_id,sbb_id,ls_id,element,unit_kind) VALUES ";
                else
                    sql += ",";

                sql += row;
                ++count;
                ++total;

                if (count >= kBatchSize)
                    flush();
            }
            flush();
            LOG_INFO << "PopulateUnitMstTable: loaded " << total << " units from " << path;
        }

        // Step C: Update existing user_units rows that still have the default base_hp=1000
        // The SUBSTR trick strips suffixes like _100 or _2_100 to get the base unit_id.
        // Unit 51317 is skipped automatically because its base_hp=55000 != 1000.
        db->execSqlSync(
            "UPDATE user_units SET "
            "  base_hp  = COALESCE((SELECT lord_hp  FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), base_hp),"
            "  base_atk = COALESCE((SELECT lord_atk FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), base_atk),"
            "  base_def = COALESCE((SELECT lord_def FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), base_def),"
            "  base_heal= COALESCE((SELECT lord_rec FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), base_heal),"
            "  add_hp   = COALESCE((SELECT add_hp   FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), add_hp),"
            "  add_atk  = COALESCE((SELECT add_atk  FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), add_atk),"
            "  add_def  = COALESCE((SELECT add_def  FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), add_def),"
            "  add_heal = COALESCE((SELECT add_heal FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), add_heal),"
            "  skill_id = COALESCE((SELECT bb_id  FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1) AND bb_id  != 0), skill_id),"
            "  extra_skill_id  = COALESCE((SELECT sbb_id FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1) AND sbb_id != 0), extra_skill_id),"
            "  leader_skill_id = COALESCE((SELECT ls_id  FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), leader_skill_id),"
            "  element       = COALESCE((SELECT element   FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), element),"
            "  unit_type_id  = COALESCE((SELECT unit_kind FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), unit_type_id),"
            "  skill_lv      = CASE WHEN COALESCE((SELECT bb_id  FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), 0) != 0 THEN 10 ELSE skill_lv END,"
            "  extra_skill_lv= CASE WHEN COALESCE((SELECT sbb_id FROM unit_mst WHERE unit_id = SUBSTR(user_units.unit_id,1,INSTR(user_units.unit_id||'_','_')-1)), 0) != 0 THEN 10 ELSE extra_skill_lv END "
            "WHERE base_hp = 1000"
        );
        LOG_INFO << "PopulateUnitMstTable: updated user_units with master stats";
    }

    const char* getName() const override { return "14032025_PopulateUnitMstTable"; }
};

MIGRATION_NS_END
