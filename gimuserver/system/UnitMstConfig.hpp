#pragma once

// UnitMstConfig — loads unit_master.json produced by tools/gen_master_json.py.
//
// Provides two things at runtime:
//  1. FindUnit(id)        — look up lord-type max stats and skill IDs for a
//                           given unit ID. Used by PopulateUnitData in
//                           GetUserInfoRequestHandler to replace placeholder stats.
//  2. GetAllUnitIds()     — ordered list of every unit ID in the master data,
//                           with Maxwell (51147) always at index 0 so the seeding
//                           loop inserts him first and gives him the lowest
//                           AUTOINCREMENT id (≥ 10001), making him the deck leader.

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

class UnitMstConfig
{
public:
    // Per-unit data extracted from the data-mined info.json.
    // All skill IDs default to 0 when the field was absent in the source data.
    // Note: BF calls the heal stat "rec" (recovery); the server response uses "heal".
    struct UnitEntry
    {
        uint64_t    unitId  = 0;
        std::string element = "fire";
        uint32_t    maxHp   = 0;
        uint32_t    maxAtk  = 0;
        uint32_t    maxDef  = 0;
        uint32_t    maxRec  = 0;   // BF "rec" → server baseHeal
        uint32_t    lsId    = 0;   // leader skill ID
        uint32_t    bbId    = 0;   // brave burst ID  → server skillID
        uint32_t    sbbId   = 0;   // super brave burst ID (informational, not sent yet)
        uint32_t    esId    = 0;   // extra skill ID  → server extraSkillID
    };

    // Load from unit_master.json.  Throws std::runtime_error on failure.
    void LoadFromJson(const std::string& path);

    // Returns a pointer to the entry for the given base unit ID, or nullptr if
    // the unit is not in the master data.  The pointer is stable for the lifetime
    // of this object.
    const UnitEntry* FindUnit(uint64_t id) const;

    // Insertion-ordered list of all unit IDs as strings (matching the format
    // stored in user_units.unit_id).  Maxwell is always first.
    const std::vector<std::string>& GetAllUnitIds() const { return m_orderedIds; }

private:
    std::unordered_map<uint64_t, UnitEntry> m_units;
    std::vector<std::string>                m_orderedIds;
};
