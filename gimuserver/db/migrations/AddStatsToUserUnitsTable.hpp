#pragma once

#include "../IMigration.hpp"
#include "../DbMacro.hpp"

MIGRATION_NS_BEGIN

struct AddStatsToUserUnitsTable : public IMigration
{
    void execute(drogon::orm::DbClientPtr db) override
    {
        // Step 1: Add stat columns with safe defaults for all existing rows
        const char* addColumns[] = {
            "ALTER TABLE user_units ADD COLUMN unit_lv INTEGER DEFAULT 1",
            "ALTER TABLE user_units ADD COLUMN base_hp INTEGER DEFAULT 1000",
            "ALTER TABLE user_units ADD COLUMN add_hp INTEGER DEFAULT 100",
            "ALTER TABLE user_units ADD COLUMN ext_hp INTEGER DEFAULT 100",
            "ALTER TABLE user_units ADD COLUMN limit_over_hp INTEGER DEFAULT 200",
            "ALTER TABLE user_units ADD COLUMN base_atk INTEGER DEFAULT 1000",
            "ALTER TABLE user_units ADD COLUMN add_atk INTEGER DEFAULT 100",
            "ALTER TABLE user_units ADD COLUMN ext_atk INTEGER DEFAULT 100",
            "ALTER TABLE user_units ADD COLUMN limit_over_atk INTEGER DEFAULT 200",
            "ALTER TABLE user_units ADD COLUMN base_def INTEGER DEFAULT 1000",
            "ALTER TABLE user_units ADD COLUMN add_def INTEGER DEFAULT 100",
            "ALTER TABLE user_units ADD COLUMN ext_def INTEGER DEFAULT 100",
            "ALTER TABLE user_units ADD COLUMN limit_over_def INTEGER DEFAULT 200",
            "ALTER TABLE user_units ADD COLUMN base_heal INTEGER DEFAULT 1000",
            "ALTER TABLE user_units ADD COLUMN add_heal INTEGER DEFAULT 100",
            "ALTER TABLE user_units ADD COLUMN ext_heal INTEGER DEFAULT 100",
            "ALTER TABLE user_units ADD COLUMN limit_over_heal INTEGER DEFAULT 200",
            "ALTER TABLE user_units ADD COLUMN exp INTEGER DEFAULT 1",
            "ALTER TABLE user_units ADD COLUMN total_exp INTEGER DEFAULT 1",
            "ALTER TABLE user_units ADD COLUMN skill_id INTEGER DEFAULT 0",
            "ALTER TABLE user_units ADD COLUMN skill_lv INTEGER DEFAULT 0",
            "ALTER TABLE user_units ADD COLUMN extra_skill_id INTEGER DEFAULT 0",
            "ALTER TABLE user_units ADD COLUMN extra_skill_lv INTEGER DEFAULT 0",
            "ALTER TABLE user_units ADD COLUMN leader_skill_id INTEGER DEFAULT 0",
            "ALTER TABLE user_units ADD COLUMN element TEXT DEFAULT 'fire'",
            "ALTER TABLE user_units ADD COLUMN fe_bp INTEGER DEFAULT 100",
            "ALTER TABLE user_units ADD COLUMN fe_max_usable_bp INTEGER DEFAULT 200",
            "ALTER TABLE user_units ADD COLUMN unit_type_id INTEGER DEFAULT 1",
        };
        for (const char* sql : addColumns)
            db->execSqlSync(sql);

        // Step 2: Insert unit 51317 for the first user if not already present
        db->execSqlSync(
            "INSERT OR IGNORE INTO user_units (user_id, unit_id) "
            "SELECT id, '51317' FROM users LIMIT 1;"
        );

        // Step 3: Upsert unit 51317 with Tom's battle-ready stats
        db->execSqlSync(
            "UPDATE user_units "
            "SET unit_lv=1, "
            "    base_hp=55000, add_hp=250, ext_hp=100, limit_over_hp=200, "
            "    base_atk=5000,  add_atk=250, ext_atk=100, limit_over_atk=200, "
            "    base_def=5000,  add_def=250, ext_def=100, limit_over_def=200, "
            "    base_heal=5000, add_heal=250, ext_heal=100, limit_over_heal=200, "
            "    exp=0, total_exp=0, "
            "    skill_id=51317, skill_lv=10, "
            "    extra_skill_id=151317, extra_skill_lv=10, "
            "    leader_skill_id=0, "
            "    element='light', "
            "    fe_bp=100, fe_max_usable_bp=200, "
            "    unit_type_id=1 "
            "WHERE unit_id='51317' "
            "  AND user_id=(SELECT id FROM users LIMIT 1);"
        );
    }

    const char* getName() const override { return "13032025_AddStatsToUserUnitsTable"; }
};

MIGRATION_NS_END
