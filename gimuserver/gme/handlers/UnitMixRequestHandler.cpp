#include "UnitMixRequestHandler.hpp"
#include <db/DbMacro.hpp>
#include <core/System.hpp>
#include <gme/response/ReinforcementInfoResponse.hpp>
#include <gme/response/UserTeamInfo.hpp>
#include <gme/response/UserUnitInfo.hpp>
#include <vector>
#include <memory>
#include <cmath>

// Request format (captured from client):
//   "60subGk3": [ {"81GjwoWy":1, "2vnqRIr3":2} ]   // operation type: always 1/2 (standard power fusion)
//   "mCE3rUu5": [ {"Rs7bCE3t":"<zelCost>"} ]        // zel cost
//   "Km35HAXv": [ {"edy7fq3L":"<userUnitId>", "mnZ5K4Ii":"1"}, // base (role 1)
//                 {"edy7fq3L":"<userUnitId>", "mnZ5K4Ii":"2"}, // material (role 2) x N ]
//
// "60subGk3" / "81GjwoWy" / "2vnqRIr3" are always hardcoded by the client (1 and 2);
// they distinguish standard power fusion from other mix types (skill, evo fodder, etc.)
// The server parses them for logging but does not gate behavior on them.
//
// Response: "xZH6EIQ7" (ReinforcementInfoResponse) — drives the level-up result animation
//           without replacing the client's cached unit list (unlike "4ceMWH6k" / UserUnitInfo).

// All the base-unit fields we need to carry from the SELECT into the response builder.
struct BaseUnitSnapshot {
    uint32_t id        = 0;
    std::string unitId;       // plain MST id (no _100 suffix)
    int      totalExp  = 0;
    // base stats (stay the same after fusion; note: stored as lord_hp in our schema)
    uint32_t baseHp = 0, baseAtk = 0, baseDef = 0, baseHeal = 0;
    // add stats (IMP cap from migration — preserved unchanged by fusion)
    uint32_t addHp = 0,  addAtk = 0,  addDef = 0,  addHeal = 0;
    // ext / limit-over stats (unchanged by fusion)
    uint32_t extHp = 0,  extAtk = 0,  extDef = 0,  extHeal = 0;
    uint32_t limitOverHP = 0, limitOverAtk = 0, limitOverDef = 0, limitOverHeal = 0;
    // skills & misc (unchanged by fusion)
    uint32_t skillId = 0, skillLv = 0, extraSkillId = 0, extraSkillLv = 0, leaderSkillId = 0;
    std::string element;
    uint32_t feBP = 0, feMaxUsableBP = 0, unitTypeId = 0;
    uint32_t eqipItemId = 0, eqipItemFrameId = 0, eqipItemId2 = 0, eqipItemFrameId2 = 0;
};

void Handler::UnitMixRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
    LOG_INFO << "UnitMixRequest: " << req.toStyledString();

    const std::string groupId    = GetGroupId();
    const std::string aesKey     = GetAesKey();
    const std::string handleName = user.info.handleName;

    // --- Parse operation type (always 1/2 from client; log only) ---------
    // 60subGk3[0].81GjwoWy = mix type   (1 = standard power fusion)
    // 60subGk3[0].2vnqRIr3 = ingredient type (2 = consume selected as materials)
    {
        const Json::Value& opType = req["60subGk3"];
        int mixType = 1, ingredientType = 2;
        if (opType.isArray() && !opType.empty())
        {
            // Values arrive as strings on the wire ("1", "2")
            try { mixType        = std::stoi(opType[0].get("81GjwoWy", "1").asString()); } catch (...) {}
            try { ingredientType = std::stoi(opType[0].get("2vnqRIr3", "2").asString()); } catch (...) {}
        }
        LOG_DEBUG << "UnitMixRequest: mixType=" << mixType << " ingredientType=" << ingredientType;
    }

    // --- Parse unit list -------------------------------------------------
    const Json::Value& unitList = req["Km35HAXv"];
    if (!unitList.isArray() || unitList.empty())
    {
        Json::Value res;
        cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
        return;
    }

    uint32_t baseUnitId = 0;
    std::vector<uint32_t> materialIds;

    for (const auto& u : unitList)
    {
        const std::string role = u.get("mnZ5K4Ii", "2").asString();
        uint32_t uid = 0;
        try { uid = (uint32_t)std::stoull(u.get("edy7fq3L", "0").asString()); } catch (...) {}
        if (role == "1")  baseUnitId = uid;
        else if (uid != 0) materialIds.push_back(uid);
    }

    if (baseUnitId == 0)
    {
        LOG_WARN << "UnitMixRequest: no base unit in request";
        Json::Value res;
        cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
        return;
    }

    // --- Zell cost -------------------------------------------------------
    int zellCost = 0;
    if (req.isMember("mCE3rUu5") && req["mCE3rUu5"].isArray() && !req["mCE3rUu5"].empty())
        try { zellCost = std::stoi(req["mCE3rUu5"][0].get("Rs7bCE3t", "0").asString()); } catch (...) {}

    // Snapshot team info with zel pre-deducted so client updates immediately
    Response::UserTeamInfo capturedTeamInfo = user.teamInfo;
    if (zellCost > 0)
        capturedTeamInfo.Zel = (capturedTeamInfo.Zel >= (uint64_t)zellCost)
            ? capturedTeamInfo.Zel - zellCost : 0;

    const std::string userId = user.info.userID;

    std::string matIdList;
    for (size_t i = 0; i < materialIds.size(); ++i)
    {
        if (i) matIdList += ',';
        matIdList += std::to_string(materialIds[i]);
    }

    // --- Step 1: Read full base-unit row ---------------------------------
    // Select everything needed so we can build the full response without a
    // second round-trip after the UPDATE.
    static const char* kBaseSelect =
        "SELECT id, unit_id, total_exp, "
        "base_hp, base_atk, base_def, base_heal, "
        "add_hp, add_atk, add_def, add_heal, "
        "ext_hp, ext_atk, ext_def, ext_heal, "
        "limit_over_hp, limit_over_atk, limit_over_def, limit_over_heal, "
        "skill_id, skill_lv, extra_skill_id, extra_skill_lv, leader_skill_id, "
        "element, fe_bp, fe_max_usable_bp, unit_type_id, "
        "eqip_item_id, eqip_item_frame_id, eqip_item_id2, eqip_item_frame_id2 "
        "FROM user_units WHERE user_id=$1 AND id=$2 LIMIT 1";

    GME_DB->execSqlAsync(
        kBaseSelect,

        [cb, groupId, aesKey, userId, baseUnitId, materialIds, zellCost, matIdList, handleName, capturedTeamInfo]
        (const drogon::orm::Result& baseRows)
        {
            if (baseRows.empty())
            {
                LOG_WARN << "UnitMixRequest: base unit " << baseUnitId << " not found";
                Json::Value res;
                cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
                return;
            }

            // Populate snapshot from the DB row
            auto snap = std::make_shared<BaseUnitSnapshot>();
            snap->id        = baseRows[0]["id"].as<uint32_t>();
            snap->totalExp  = baseRows[0]["total_exp"].as<int>();
            snap->baseHp    = baseRows[0]["base_hp"].as<uint32_t>();
            snap->baseAtk   = baseRows[0]["base_atk"].as<uint32_t>();
            snap->baseDef   = baseRows[0]["base_def"].as<uint32_t>();
            snap->baseHeal  = baseRows[0]["base_heal"].as<uint32_t>();
            snap->addHp     = baseRows[0]["add_hp"].as<uint32_t>();
            snap->addAtk    = baseRows[0]["add_atk"].as<uint32_t>();
            snap->addDef    = baseRows[0]["add_def"].as<uint32_t>();
            snap->addHeal   = baseRows[0]["add_heal"].as<uint32_t>();
            snap->extHp     = baseRows[0]["ext_hp"].as<uint32_t>();
            snap->extAtk    = baseRows[0]["ext_atk"].as<uint32_t>();
            snap->extDef    = baseRows[0]["ext_def"].as<uint32_t>();
            snap->extHeal   = baseRows[0]["ext_heal"].as<uint32_t>();
            snap->limitOverHP   = baseRows[0]["limit_over_hp"].as<uint32_t>();
            snap->limitOverAtk  = baseRows[0]["limit_over_atk"].as<uint32_t>();
            snap->limitOverDef  = baseRows[0]["limit_over_def"].as<uint32_t>();
            snap->limitOverHeal = baseRows[0]["limit_over_heal"].as<uint32_t>();
            snap->skillId       = baseRows[0]["skill_id"].as<uint32_t>();
            snap->skillLv       = baseRows[0]["skill_lv"].as<uint32_t>();
            snap->extraSkillId  = baseRows[0]["extra_skill_id"].as<uint32_t>();
            snap->extraSkillLv  = baseRows[0]["extra_skill_lv"].as<uint32_t>();
            snap->leaderSkillId = baseRows[0]["leader_skill_id"].as<uint32_t>();
            snap->element       = baseRows[0]["element"].as<std::string>();
            snap->feBP          = baseRows[0]["fe_bp"].as<uint32_t>();
            snap->feMaxUsableBP = baseRows[0]["fe_max_usable_bp"].as<uint32_t>();
            snap->unitTypeId    = baseRows[0]["unit_type_id"].as<uint32_t>();
            snap->eqipItemId      = baseRows[0]["eqip_item_id"].as<uint32_t>();
            snap->eqipItemFrameId = baseRows[0]["eqip_item_frame_id"].as<uint32_t>();
            snap->eqipItemId2     = baseRows[0]["eqip_item_id2"].as<uint32_t>();
            snap->eqipItemFrameId2= baseRows[0]["eqip_item_frame_id2"].as<uint32_t>();

            // Strip _100 suffix to get plain MST id
            std::string rawUnitId = baseRows[0]["unit_id"].as<std::string>();
            auto us = rawUnitId.find('_');
            snap->unitId = (us != std::string::npos) ? rawUnitId.substr(0, us) : rawUnitId;

            if (materialIds.empty())
            {
                Json::Value res;
                cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
                return;
            }

            // --- Step 2: Sum material exp ---------------------------------
            // Client formula (GameUtils::getMixExp): per material —
            //   totalExp/2.5 + cost*2 + adjustExp + rarityBonus[rare]
            // with 1.5x multiplier when mat.element == base.element.
            // Final sum is llroundf'd to match client float accumulation.
            const std::string matQuery =
                "SELECT unit_id, total_exp "
                "FROM user_units "
                "WHERE user_id=$1 AND id IN (" + matIdList + ")";

            GME_DB->execSqlAsync(
                matQuery,

                [cb, groupId, aesKey, userId, baseUnitId, materialIds, zellCost,
                 matIdList, snap, handleName, capturedTeamInfo]
                (const drogon::orm::Result& matRows)
                {
                    // Client formula (GameUtils::getMixExp, decompiled from APK):
                    //   for each material:
                    //     exp  = totalExp / 2.5
                    //     exp += cost * 2
                    //     exp += adjustExp        (= unitAdditionalXpBoost)
                    //     exp += rarityBonus       (100/200/500/1000/1500/3000/5000/10000 for rarity 1-8)
                    //     if base.element == mat.element: exp *= 1.5
                    //   total = llroundf(sum)
                    static const int kRarityBonus[] = { 0, 100, 200, 500, 1000, 1500, 3000, 5000, 10000 };

                    const auto& mst = System::Instance().MstConfig();
                    const auto* baseData = mst.GetUnitMstData(snap->unitId);
                    const int baseElement = baseData ? baseData->element : 0;

                    float gainedExpF = 0.0f;
                    for (const auto& row : matRows)
                    {
                        std::string rawId = row["unit_id"].as<std::string>();
                        auto us = rawId.find('_');
                        std::string matUnitId = (us != std::string::npos) ? rawId.substr(0, us) : rawId;
                        int matTotalExp = row["total_exp"].as<int>();

                        const auto* matData = mst.GetUnitMstData(matUnitId);
                        int adjustExp = matData ? matData->xpBoost  : 0;
                        int matCost   = matData ? matData->cost     : 1;
                        int matRare   = matData ? matData->rare     : 1;
                        int matElem   = matData ? matData->element  : 0;

                        float matExp = (float)matTotalExp / 2.5f;
                        matExp += (float)(matCost * 2);
                        matExp += (float)adjustExp;
                        if (matRare >= 1 && matRare <= 8)
                            matExp += (float)kRarityBonus[matRare];
                        if (baseElement != 0 && matElem == baseElement)
                            matExp *= 1.5f;

                        gainedExpF += matExp;
                    }
                    int gainedExp = (int)llroundf(gainedExpF);

                    // --- Exp / level calculation --------------------------
                    const int expPatternId = baseData ? baseData->expPatternId : 10;
                    const int maxLevel     = baseData ? baseData->maxLevel     : 100;

                    const int maxLevelExp  = mst.GetExpForLevel(expPatternId, maxLevel);
                    int newTotalExp = snap->totalExp + gainedExp;
                    if (maxLevelExp > 0 && newTotalExp > maxLevelExp)
                        newTotalExp = maxLevelExp;

                    const int newLevel          = mst.GetLevelFromTotalExp(expPatternId, maxLevel, newTotalExp);
                    const int levelThresholdExp = mst.GetExpForLevel(expPatternId, newLevel);
                    const int newExp            = newTotalExp - levelThresholdExp;

                    LOG_INFO << "UnitMixRequest: unit=" << baseUnitId
                        << " mst=" << snap->unitId
                        << " totalExp " << snap->totalExp << "+" << gainedExp
                        << "=" << newTotalExp
                        << " lv->" << newLevel << "/" << maxLevel
                        << " (pattern=" << expPatternId << ")";

                    // --- Step 3: UPDATE base unit -------------------------
                    // Only update level/exp — do NOT touch add_hp/atk/def/heal.
                    // Those columns store the IMP cap from the migration. Overwriting
                    // them with a leveling bonus would cause apparent stat decreases
                    // because GetUserInfo (and the client cache) uses them as IMP caps.
                    GME_DB->execSqlAsync(
                        "UPDATE user_units "
                        "SET unit_lv=$1, exp=$2, total_exp=$3 "
                        "WHERE id=$4 AND user_id=$5",

                        [cb, groupId, aesKey, userId, baseUnitId, materialIds,
                         matIdList, zellCost, snap, handleName,
                         newLevel, newExp, newTotalExp,
                         capturedTeamInfo]
                        (const drogon::orm::Result&)
                        {
                            // sendResponse builds:
                            //   xZH6EIQ7 — reinforcement animation (target level in 4A6LzBxr)
                            //   4ceMWH6k — updated unit cache entry (same key as GetUserInfo/Gacha)
                            //   fEi17cnx — team/zel update
                            //
                            // 4ceMWH6k (UserUnitInfo) includes edy7fq3L (userUnitID) and
                            // pn16CNah (unitID) so the client can identify which unit to
                            // update in its cache. The previous attempt with qC2tJs4E
                            // (UserUnitInfoResponse) crashed because that struct was missing
                            // those identification fields.
                            auto sendResponse = [cb, groupId, aesKey,
                                                 snap, handleName, userId,
                                                 newLevel, newExp, newTotalExp,
                                                 capturedTeamInfo]()
                            {
                                Response::ReinforcementInfoResponse::Data rd;
                                rd.m_HandleName  = handleName;
                                rd.m_4A6LzBxr   = (uint32_t)newLevel;  // target level for animation
                                rd.m_pn16CNah   = snap->unitId;
                                rd.m_e7DK0FQT   = snap->baseHp;
                                rd.m_67CApcti   = snap->baseAtk;
                                rd.m_q08xLEsy   = snap->baseDef;
                                rd.m_PWXu25cg   = snap->baseHeal;
                                rd.m_cuIWp89g   = snap->addHp;   // IMP cap — matches GetUserInfo
                                rd.m_RT4CtH5d   = snap->addAtk;
                                rd.m_GcMD0hy6   = snap->addDef;
                                rd.m_C1HZr3pb   = snap->addHeal;
                                rd.m_TokWs1B3   = snap->extHp;
                                rd.m_t4m1RH6Y   = snap->extAtk;
                                rd.m_e6mY8Z0k   = snap->extDef;
                                rd.m_e6mY8Z0k_2 = snap->extHeal;
                                rd.m_nj9Lw7mV   = std::to_string(snap->skillId);
                                rd.m_3NbeC8AB   = snap->skillLv;
                                rd.m_iEFZ6H19   = std::to_string(snap->extraSkillId);
                                rd.m_RQ5GnFE2   = snap->extraSkillLv;
                                rd.m_nBTx56W9   = snap->unitTypeId;
                                rd.m_MissionID  = "";
                                Response::ReinforcementInfoResponse reinf;
                                reinf.items.emplace_back(std::move(rd));

                                Json::Value res;
                                reinf.Serialize(res);

                                // qC2tJs4E — incremental unit cache update.
                                // Uses UserUnitInfo::Data::Serialize() to get the
                                // full field set (edy7fq3L, pn16CNah, nj9Lw7mV, etc.)
                                // under the qC2tJs4E key, which merges/updates a
                                // single entry rather than replacing the entire list.
                                // (4ceMWH6k would replace the whole unit list, wiping
                                // all other units from the client's cache.)
                                {
                                    Response::UserUnitInfo::Data ud;
                                    ud.userID        = userId;
                                    ud.userUnitID    = snap->id;
                                    ud.unitID        = (uint32_t)std::stoull(snap->unitId);
                                    ud.unitTypeID    = snap->unitTypeId;
                                    ud.unitLv        = (uint32_t)newLevel;
                                    ud.exp           = (uint32_t)newExp;
                                    ud.totalExp      = (uint32_t)newTotalExp;
                                    ud.baseHp        = snap->baseHp;
                                    ud.baseAtk       = snap->baseAtk;
                                    ud.baseDef       = snap->baseDef;
                                    ud.baseHeal      = snap->baseHeal;
                                    ud.addHp         = snap->addHp;
                                    ud.addAtk        = snap->addAtk;
                                    ud.addDef        = snap->addDef;
                                    ud.addHeal       = snap->addHeal;
                                    ud.extHp         = snap->extHp;
                                    ud.extAtk        = snap->extAtk;
                                    ud.extDef        = snap->extDef;
                                    ud.extHeal       = snap->extHeal;
                                    ud.limitOverHP   = snap->limitOverHP;
                                    ud.limitOverAtk  = snap->limitOverAtk;
                                    ud.limitOverDef  = snap->limitOverDef;
                                    ud.limitOverHeal = snap->limitOverHeal;
                                    ud.skillID       = snap->skillId;
                                    ud.skillLv       = snap->skillLv;
                                    ud.extraSkillID  = snap->extraSkillId;
                                    ud.extraSkillLv  = snap->extraSkillLv;
                                    ud.leaderSkillID = snap->leaderSkillId;
                                    ud.element       = snap->element;
                                    ud.FeBP          = snap->feBP;
                                    ud.FeMaxUsableBP = snap->feMaxUsableBP;
                                    ud.eqipItemID       = snap->eqipItemId;
                                    ud.eqipItemFrameID  = snap->eqipItemFrameId;
                                    ud.equipItemID2     = snap->eqipItemId2;
                                    ud.eqipItemFrameID2 = snap->eqipItemFrameId2;
                                    ud.newFlg        = 1;
                                    ud.receiveDate   = 100;

                                    Json::Value unitItem;
                                    ud.Serialize(unitItem);
                                    Json::Value arr(Json::arrayValue);
                                    arr.append(unitItem);
                                    res["qC2tJs4E"] = arr;
                                }

                                // fEi17cnx — updated team info so zel shows immediately
                                capturedTeamInfo.Serialize(res);
                                cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
                            }; // sendResponse

                            // Deduct Zel then delete materials, then send response.
                            auto deleteAndRespond = [sendResponse, cb, groupId, aesKey,
                                                     userId, materialIds, matIdList]()
                            {
                                if (!materialIds.empty())
                                {
                                    GME_DB->execSqlAsync(
                                        "DELETE FROM user_units WHERE user_id=$1 AND id IN (" + matIdList + ")",
                                        [sendResponse](const drogon::orm::Result&) { sendResponse(); },
                                        [sendResponse](const drogon::orm::DrogonDbException& e)
                                        {
                                            LOG_ERROR << "UnitMixRequest: delete materials failed: " << e.base().what();
                                            sendResponse();
                                        },
                                        userId
                                    );
                                }
                                else
                                {
                                    sendResponse();
                                }
                            };

                            if (zellCost > 0)
                            {
                                GME_DB->execSqlAsync(
                                    "UPDATE userinfo SET zel = MAX(0, zel - $1) WHERE id=$2",
                                    [deleteAndRespond](const drogon::orm::Result&) { deleteAndRespond(); },
                                    [deleteAndRespond](const drogon::orm::DrogonDbException& e) {
                                        LOG_WARN << "UnitMixRequest: zel deduction failed: " << e.base().what();
                                        deleteAndRespond();
                                    },
                                    zellCost, userId
                                );
                            }
                            else
                            {
                                deleteAndRespond();
                            }
                        },
                        [cb, groupId, aesKey](const drogon::orm::DrogonDbException& e)
                        {
                            LOG_ERROR << "UnitMixRequest: unit UPDATE failed: " << e.base().what();
                            Json::Value res;
                            cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
                        },
                        newLevel, newExp, newTotalExp,
                        baseUnitId, userId
                    );
                },
                [cb, groupId, aesKey](const drogon::orm::DrogonDbException& e)
                {
                    LOG_ERROR << "UnitMixRequest: material SUM query failed: " << e.base().what();
                    Json::Value res;
                    cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
                },
                userId
            );
        },
        [cb, groupId, aesKey](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "UnitMixRequest: base unit SELECT failed: " << e.base().what();
            Json::Value res;
            cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
        },
        userId, baseUnitId
    );
}
