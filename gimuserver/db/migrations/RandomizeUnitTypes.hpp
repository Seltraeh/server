#pragma once

#include "../IMigration.hpp"
#include "../DbMacro.hpp"

MIGRATION_NS_BEGIN

// Sets unit_type_id to a random value 1-6 for any existing user_units rows
// that still have the old placeholder value of 0.  New units assigned via
// the gacha INSERT already receive a random type at creation time.
struct RandomizeUnitTypes : public IMigration
{
    void execute(drogon::orm::DbClientPtr db) override
    {
        db->execSqlSync(
            "UPDATE user_units "
            "SET unit_type_id = 1 + ABS(RANDOM()) % 6 "
            "WHERE unit_type_id = 0"
        );
    }

    const char* getName() const override { return "10042026_RandomizeUnitTypes"; }
};

MIGRATION_NS_END
