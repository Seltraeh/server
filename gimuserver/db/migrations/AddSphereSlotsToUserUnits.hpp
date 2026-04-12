#pragma once

#include "../IMigration.hpp"
#include "../DbMacro.hpp"

MIGRATION_NS_BEGIN

// Adds two sphere equipment slots to user_units.
// eqip_item_id / eqip_item_id2   — warehouse item ID of the equipped sphere (0 = empty)
// eqip_item_frame_id / _frame_id2 — itemSphereType of the equipped sphere (cosmetic frame,
//                                    0 = empty). Maps to UserUnitInfo keys:
//                                      slot 1: Ge8Yo32T (itemId) / 0R3qTPK9 (frameId)
//                                      slot 2: mZA7fH2v (itemId) / RXfC31FA (frameId)
struct AddSphereSlotsToUserUnits : public IMigration
{
    void execute(drogon::orm::DbClientPtr db) override
    {
        db->execSqlSync("ALTER TABLE user_units ADD COLUMN eqip_item_id      INTEGER NOT NULL DEFAULT 0");
        db->execSqlSync("ALTER TABLE user_units ADD COLUMN eqip_item_frame_id INTEGER NOT NULL DEFAULT 0");
        db->execSqlSync("ALTER TABLE user_units ADD COLUMN eqip_item_id2      INTEGER NOT NULL DEFAULT 0");
        db->execSqlSync("ALTER TABLE user_units ADD COLUMN eqip_item_frame_id2 INTEGER NOT NULL DEFAULT 0");
    }

    const char* getName() const override { return "09042026_AddSphereSlotsToUserUnits"; }
};

MIGRATION_NS_END
