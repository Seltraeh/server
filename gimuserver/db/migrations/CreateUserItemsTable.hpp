#pragma once

#include "../IMigration.hpp"
#include "../DbMacro.hpp"

MIGRATION_NS_BEGIN

struct CreateUserItemsTable : public IMigration
{
    void execute(drogon::orm::DbClientPtr db) override
    {
        db->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS user_items (
                user_id  TEXT    NOT NULL,
                item_id  INTEGER NOT NULL,
                quantity INTEGER NOT NULL DEFAULT 1,
                PRIMARY KEY (user_id, item_id)
            );
        )");
    }

    const char* getName() const override { return "11032025_CreateUserItemsTable"; }
};

MIGRATION_NS_END
