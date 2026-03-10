#pragma once

#include "../IMigration.hpp"
#include "../DbMacro.hpp"

MIGRATION_NS_BEGIN

// Creates two tables for the daily task system:
//
//  user_daily_task_claims
//    Tracks how many times a user has claimed a specific daily prize (keyed by
//    the task_id from dailytask.json / DailyTaskPrizeMst). currentClaimCount
//    in the DailyTaskPrizeMst response should be populated from this table.
//
//  user_daily_task_progress
//    Tracks a user's progress toward each daily task type (e.g., "AV" = Arena
//    Victories, "VV" = Vortex runs, "CM" = Crafts). reset_date stores the Unix
//    timestamp of the day boundary when progress was last reset so progress can
//    be zeroed on a new day without deleting rows.
struct CreateDailyTaskTables : public IMigration
{
    void execute(drogon::orm::DbClientPtr db) override
    {
        // Prize claim counts per user
        db->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS user_daily_task_claims (
                user_id      TEXT    NOT NULL,
                task_id      INTEGER NOT NULL,
                claim_count  INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (user_id, task_id)
            );
        )");

        // Per-day progress counters per user
        db->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS user_daily_task_progress (
                user_id       TEXT    NOT NULL,
                task_type_key TEXT    NOT NULL,
                progress      INTEGER NOT NULL DEFAULT 0,
                reset_date    INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (user_id, task_type_key)
            );
        )");
    }

    const char* getName() const override { return "10032025_CreateDailyTaskTables"; }
};

MIGRATION_NS_END
