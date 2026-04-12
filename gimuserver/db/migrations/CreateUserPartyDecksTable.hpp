#pragma once

#include "../IMigration.hpp"
#include "../DbMacro.hpp"

MIGRATION_NS_BEGIN

struct CreateUserPartyDecksTable : public IMigration
{
    void execute(drogon::orm::DbClientPtr db) override
    {
        db->execSqlSync(
            "CREATE TABLE IF NOT EXISTS user_party_decks ("
            "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  user_id      TEXT    NOT NULL,"
            "  deck_type    INTEGER NOT NULL DEFAULT 1,"
            "  deck_num     INTEGER NOT NULL DEFAULT 0,"
            "  user_unit_id INTEGER NOT NULL DEFAULT 0,"
            "  member_type  INTEGER NOT NULL DEFAULT 0,"
            "  disp_order   INTEGER NOT NULL DEFAULT 0,"
            "  UNIQUE(user_id, deck_type, deck_num, disp_order)"
            ")"
        );
    }

    const char* getName() const override { return "10042026_CreateUserPartyDecksTable"; }
};

MIGRATION_NS_END
