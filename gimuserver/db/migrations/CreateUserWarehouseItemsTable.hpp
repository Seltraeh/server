#pragma once

#include "../IMigration.hpp"
#include "../DbMacro.hpp"

MIGRATION_NS_BEGIN

struct CreateUserWarehouseItemsTable : public IMigration
{
    void execute(drogon::orm::DbClientPtr db) override
    {
        // One row per item type per user. The auto-increment id becomes the
        // per-user item instance ID (n6E8iMf3 / UserItemID in the response).
        // possession stores the stack count (wgV86x1q).
        // new_flg mirrors dJNpLc81 — set to 1 on insert, cleared to 0 after the
        // client acknowledges the item (handled by future item-ack handler).
        // unknown_flg is DbMVG16I; setter "IsReceipt" observed in one binary
        // context but null in another — defaulting to 0 until confirmed.
        db->execSqlSync(R"(
            CREATE TABLE IF NOT EXISTS user_warehouse_items (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id     TEXT    NOT NULL,
                item_id     TEXT    NOT NULL,
                possession  INTEGER NOT NULL DEFAULT 0,
                new_flg     INTEGER NOT NULL DEFAULT 1,
                unknown_flg INTEGER NOT NULL DEFAULT 0,
                UNIQUE(user_id, item_id)
            );
        )");
    }

    const char* getName() const override { return "08042025_CreateUserWarehouseItemsTable"; }
};

MIGRATION_NS_END
