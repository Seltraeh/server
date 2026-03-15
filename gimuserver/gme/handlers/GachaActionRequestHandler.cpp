#include "GachaActionRequestHandler.hpp"
#include "gme/response/SignalKey.hpp"
#include "gme/response/UserUnitInfo.hpp"
#include "core/System.hpp"
#include <db/DbMacro.hpp>

void Handler::GachaActionRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
    // TODO: randomize unit_id from gacha rates; 10017 is hardcoded for now.
    static const char* kSummonUnitId = "10017";

    // Insert the summoned unit into user_units with stats from unit_mst.
    // INSERT OR IGNORE keeps the operation safe if the unit already exists.
    GME_DB->execSqlAsync(
        "INSERT OR IGNORE INTO user_units "
        "  (user_id, unit_id, base_hp, base_atk, base_def, base_heal,"
        "   add_hp, add_atk, add_def, add_heal,"
        "   skill_id, skill_lv, extra_skill_id, extra_skill_lv,"
        "   leader_skill_id, element, unit_type_id, fe_bp, fe_max_usable_bp) "
        "SELECT $1, $2,"
        "  COALESCE(lord_hp,  1000), COALESCE(lord_atk, 1000),"
        "  COALESCE(lord_def, 1000), COALESCE(lord_rec, 1000),"
        "  COALESCE(add_hp,  100),  COALESCE(add_atk,  100),"
        "  COALESCE(add_def, 100),  COALESCE(add_heal, 100),"
        "  CASE WHEN bb_id  != 0 THEN bb_id  ELSE 0 END, CASE WHEN bb_id  != 0 THEN 10 ELSE 0 END,"
        "  CASE WHEN sbb_id != 0 THEN sbb_id ELSE 0 END, CASE WHEN sbb_id != 0 THEN 10 ELSE 0 END,"
        "  COALESCE(ls_id, 0), COALESCE(element, 'fire'), COALESCE(unit_kind, 1),"
        "  100, 200 "
        "FROM unit_mst WHERE unit_id = $2",
        [this, userId = user.info.userID, cb](const drogon::orm::Result&)
        {
            // Fetch the newly inserted (or existing) row back so we return real DB data.
            GME_DB->execSqlAsync(
                "SELECT id, unit_id, unit_lv, "
                "base_hp, add_hp, ext_hp, limit_over_hp, "
                "base_atk, add_atk, ext_atk, limit_over_atk, "
                "base_def, add_def, ext_def, limit_over_def, "
                "base_heal, add_heal, ext_heal, limit_over_heal, "
                "exp, total_exp, skill_id, skill_lv, "
                "extra_skill_id, extra_skill_lv, leader_skill_id, "
                "element, fe_bp, fe_max_usable_bp, unit_type_id "
                "FROM user_units WHERE user_id = $1 AND unit_id = $2 LIMIT 1",
                [this, userId, cb](const drogon::orm::Result& rows)
                {
                    Json::Value res;

                    Response::UserUnitInfo unitInfo;
                    if (!rows.empty())
                    {
                        const auto& row = rows[0];
                        Response::UserUnitInfo::Data d;
                        d.userID     = userId;
                        d.userUnitID = row["id"].as<uint32_t>();

                        std::string rawId = row["unit_id"].as<std::string>();
                        auto us = rawId.find('_');
                        d.unitID = (uint32_t)std::stoull(
                            us == std::string::npos ? rawId : rawId.substr(0, us));

                        d.unitLv        = row["unit_lv"].as<uint32_t>();
                        d.baseHp        = row["base_hp"].as<uint32_t>();
                        d.addHp         = row["add_hp"].as<uint32_t>();
                        d.extHp         = row["ext_hp"].as<uint32_t>();
                        d.limitOverHP   = row["limit_over_hp"].as<uint32_t>();
                        d.baseAtk       = row["base_atk"].as<uint32_t>();
                        d.addAtk        = row["add_atk"].as<uint32_t>();
                        d.extAtk        = row["ext_atk"].as<uint32_t>();
                        d.limitOverAtk  = row["limit_over_atk"].as<uint32_t>();
                        d.baseDef       = row["base_def"].as<uint32_t>();
                        d.addDef        = row["add_def"].as<uint32_t>();
                        d.extDef        = row["ext_def"].as<uint32_t>();
                        d.limitOverDef  = row["limit_over_def"].as<uint32_t>();
                        d.baseHeal      = row["base_heal"].as<uint32_t>();
                        d.addHeal       = row["add_heal"].as<uint32_t>();
                        d.extHeal       = row["ext_heal"].as<uint32_t>();
                        d.limitOverHeal = row["limit_over_heal"].as<uint32_t>();
                        d.exp           = row["exp"].as<uint32_t>();
                        d.totalExp      = row["total_exp"].as<uint32_t>();
                        d.skillID       = row["skill_id"].as<uint32_t>();
                        d.skillLv       = row["skill_lv"].as<uint32_t>();
                        d.extraSkillID  = row["extra_skill_id"].as<uint32_t>();
                        d.extraSkillLv  = row["extra_skill_lv"].as<uint32_t>();
                        d.leaderSkillID = row["leader_skill_id"].as<uint32_t>();
                        d.element       = row["element"].as<std::string>();
                        d.FeBP          = row["fe_bp"].as<uint32_t>();
                        d.FeMaxUsableBP = row["fe_max_usable_bp"].as<uint32_t>();
                        d.unitTypeID    = row["unit_type_id"].as<uint32_t>();
                        d.newFlg        = 1;
                        d.receiveDate   = 100;
                        unitInfo.Mst.emplace_back(d);
                    }
                    unitInfo.Serialize(res);

                    uint32_t userUnitId = unitInfo.Mst.empty() ? 9999 : unitInfo.Mst[0].userUnitID;

                    {
                        Json::Value d;
                        d["edy7fq3L"] = std::to_string(userUnitId);
                        d["u0vkt9yH"] = 13762; // gate animation flag
                        res["Km35HAXv"] = d;
                    }

                    cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
                },
                [this, cb](const drogon::orm::DrogonDbException& e) { OnError(e, cb); },
                userId, kSummonUnitId
            );
        },
        [this, cb](const drogon::orm::DrogonDbException& e) { OnError(e, cb); },
        user.info.userID, kSummonUnitId
    );
}
