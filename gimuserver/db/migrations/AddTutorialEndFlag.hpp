#pragma once

#include "../IMigration.hpp"
#include "../DbMacro.hpp"

MIGRATION_NS_BEGIN

// Adds the tutorial_end_flag column to the users table so that tutorial
// completion state can be persisted per-user rather than forced by the
// compile-time DEV_SKIP_TUTORIAL flag.
//
// DEFAULT 0 means all existing users have tutorial_end_flag=0 (incomplete).
// The tutorial-completion handler should UPDATE it to 1 after the player
// picks their starter unit.
struct AddTutorialEndFlag : public IMigration
{
    void execute(drogon::orm::DbClientPtr db) override
    {
        db->execSqlSync(
            "ALTER TABLE users ADD COLUMN tutorial_end_flag INTEGER NOT NULL DEFAULT 0;"
        );
    }

    const char* getName() const override { return "10032025_AddTutorialEndFlag"; }
};

MIGRATION_NS_END
