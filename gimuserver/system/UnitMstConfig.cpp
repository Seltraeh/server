#include "UnitMstConfig.hpp"

#include <fstream>
#include <stdexcept>
#include <json/json.h>

void UnitMstConfig::LoadFromJson(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open())
        throw std::runtime_error("unit_master.json: not found at " + path);

    Json::CharReaderBuilder rb;
    JSONCPP_STRING err;
    Json::Value root;
    if (!Json::parseFromStream(rb, ifs, &root, &err))
        throw std::runtime_error("unit_master.json: parse error – " + err);

    const Json::Value& units = root["units"];
    if (units.isNull() || !units.isObject())
        throw std::runtime_error("unit_master.json: missing top-level \"units\" object");

    m_units.reserve(units.size());
    m_orderedIds.reserve(units.size());

    for (auto it = units.begin(); it != units.end(); ++it)
    {
        const std::string& key = it.name();
        const Json::Value& u   = *it;

        UnitEntry e;
        e.unitId  = std::stoull(key);
        e.element = u["element"].asString();
        e.maxHp   = u["max_hp"].asUInt();
        e.maxAtk  = u["max_atk"].asUInt();
        e.maxDef  = u["max_def"].asUInt();
        e.maxRec  = u["max_rec"].asUInt();
        e.lsId    = u["ls_id"].asUInt();
        e.bbId    = u["bb_id"].asUInt();
        e.sbbId   = u["sbb_id"].asUInt();
        e.esId    = u["es_id"].asUInt();

        m_units.emplace(e.unitId, e);
        m_orderedIds.push_back(key);
    }
}

const UnitMstConfig::UnitEntry* UnitMstConfig::FindUnit(uint64_t id) const
{
    auto it = m_units.find(id);
    return (it != m_units.end()) ? &it->second : nullptr;
}
