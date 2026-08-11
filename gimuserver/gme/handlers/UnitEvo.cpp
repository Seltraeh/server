#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>

// UnitEvo — evolve a unit into its next form.
//
// Request  (group 0gUSE84e, key biHf01DxcrPou5Qt):
//   "Km35HAXv": [{"edy7fq3L":"<baseId>","mnZ5K4Ii":"1"},   // base unit  (role 1)
//                {"edy7fq3L":"<matId>", "mnZ5K4Ii":"2"}, …] // evo mats   (role 2)
//   "I82p0wCL": [{"pn16CNah":"<targetMstId>"}]             // evolved-to unit MST id
//   "mCE3rUu5": [{"Rs7bCE3t":"<zelCost>"}]                 // zel cost
//
// Elem-item variant sends the base unit id under "8Z2NQrx1" instead of
// the role-1 entry inside "Km35HAXv"; both variants are handled here.
//
// Response:
//   "I82p0wCL": [EvoResultEntry]   — tells the client which MST id was evolved into
//   "qC2tJs4E": [UserUnitInfo]     — incremental unit cache update
//   "fEi17cnx": [UserTeamInfo]     — updated zel

// The request struct (UnitEvoReq + UnitEvoUnitEntry/UnitEvoTargetEntry/
// UnitEvoZelEntry/UnitEvoElemEntry) is generated from
// packet-generator/assets/net/{handlers,unit}.kdl.

// The response struct (UnitEvoResp) and its entries (EvoResultEntry under
// I82p0wCL, the shared UnitReinforceEntry under xZH6EIQ7) are generated from
// packet-generator/assets/net/{handlers,unit}.kdl.  unit_refresh rides
// UserUnitInfo under 4ceMWH6k; team_info rides UserTeamInfo under fEi17cnx.
//
// NOTE: this handler must NOT populate ope_result (1ZbHB6Im).  UnitOpeResult is
// a singleton whose only scene consumer is UnitMixPlayScene::parseMixResult —
// the evolution result screen reads UnitEvoInfo (I82p0wCL) instead.

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string unitEvo_addSuffix(int32_t mstId)
{
    return std::to_string(mstId) + "_100";
}

static std::string unitEvo_elementStr(int e)
{
    switch (e) {
        case 1: return "fire";
        case 2: return "water";
        case 3: return "earth";
        case 4: return "thunder";
        case 5: return "light";
        case 6: return "dark";
        default: return "fire";
    }
}

// ---------------------------------------------------------------------------
// Handler
// ---------------------------------------------------------------------------
HANDLEF(UnitEvo)
{
    (void)session;
    LOG_INFO << "UnitEvo: " << json;

    // Allow unknown keys — UnitEvo requests can carry extra client-side fields.
    UnitEvoReq req = {};
    {
        glz::context ctx{};
        if (const auto& ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
        {
            LOG_WARN << "UnitEvo: bad request JSON: " << glz::format_error(ec, json);
            co_return HandleResult::error("Deserialization error");
        }
    }

    // Resolve the current user from the request's login info.
    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const std::string kUserId = identity.userId;

    // Resolve base (role=1) and material (role=2) unit ids.
    int32_t baseId = 0;
    std::vector<int32_t> matIds;
    for (const auto& u : req.units)
    {
        if (u.role == "1")       baseId = u.user_unit_id;
        else if (u.user_unit_id) matIds.push_back(u.user_unit_id);
    }
    // Elem-item variant: 8Z2NQrx1 carries all units (base + mats) with role tags.
    // "inU8Q4gL" is the user_unit_id; "mnZ5K4Ii" is "1" (base) or "2" (material).
    for (const auto& e : req.elem_units)
    {
        if (e.role == "1")        baseId = e.user_unit_id;
        else if (e.user_unit_id)  matIds.push_back(e.user_unit_id);
    }

    if (baseId == 0 || req.target.empty())
    {
        LOG_WARN << "UnitEvo: missing base unit or target mst id";
        co_return HandleResult::error("UnitEvo: incomplete request");
    }

    const int32_t targetMstId = req.target[0].target_mst_id;
    const int32_t zelCost     = req.zel_cost_list.empty() ? 0 : req.zel_cost_list[0].cost;

    // Find target unit in MST cache.
    const auto& unitMst = theServer()->cache().unitMst();
    const UnitMst* targetMst = nullptr;
    for (const auto& u : unitMst) { if (u.id == targetMstId) { targetMst = &u; break; } }

    if (!targetMst)
    {
        LOG_WARN << "UnitEvo: target MST id " << targetMstId << " not found";
        co_return HandleResult::error("UnitEvo: unknown target unit");
    }

    // Step 1: SELECT current base unit (need IMP/ext/limitOver cols to preserve).
    // Also fetch unit_id so we can extract the original MST id for the animation.
    const auto baseRows = co_await theDb()->execSqlCoro(
        "SELECT user_unit_id, unit_id, add_hp, add_atk, add_def, add_rec,"
        " ext_hp, ext_atk, ext_def, ext_rec,"
        " limit_over_hp, limit_over_atk, limit_over_def, limit_over_rec,"
        " unit_type_id,"
        " eqip_item_id, eqip_item_frame_id, eqip_item_id2, eqip_item_frame_id2"
        " FROM user_units WHERE user_id=$1 AND user_unit_id=$2 LIMIT 1;",
        std::string(kUserId), baseId
    );

    if (baseRows.empty())
    {
        LOG_WARN << "UnitEvo: base unit " << baseId << " not found";
        co_return HandleResult::error("UnitEvo: base unit not found");
    }
    const auto& br = baseRows[0];

    // Extract original MST id from the unit_id column (e.g., "10011" or "10011_100" → 10011).
    // The client evo animation uses this to display the "before" unit.
    int32_t origMstId = 0;
    {
        const std::string rawUnitId = br["unit_id"].as<std::string>();
        const auto pos = rawUnitId.find('_');
        const std::string numPart = (pos != std::string::npos) ? rawUnitId.substr(0, pos) : rawUnitId;
        if (!numPart.empty()) { try { origMstId = std::stoi(numPart); } catch (...) {} }
    }

    // IMP stats are player investment — preserved through evolution.
    const int32_t keepAddHp   = br["add_hp"].as<int32_t>();
    const int32_t keepAddAtk  = br["add_atk"].as<int32_t>();
    const int32_t keepAddDef  = br["add_def"].as<int32_t>();
    const int32_t keepAddHeal = br["add_rec"].as<int32_t>();

    const std::string newElement = unitEvo_elementStr(targetMst->element);

    // Step 2: UPDATE base unit row to the evolved form.
    //   - New unit_id, reset level/exp to 1/0, reset base stats from target MST.
    //   - Preserve: add_* (IMP), ext_*, limit_over_*, equipment, FE.
    co_await theDb()->execSqlCoro(
        "UPDATE user_units SET"
        " unit_id=$1,"
        " unit_lvl=1, exp=0, total_exp=0,"
        " base_hp=$2,  base_atk=$3,  base_def=$4,  base_rec=$5, base_rec=$5,"
        " add_hp=$6,   add_atk=$7,   add_def=$8,   add_rec=$9,"
        " skill_id=$10, extra_skill_id=$11,"
        " skill_lv=1, extra_skill_lv=0,"
        " element=$13"
        " WHERE user_unit_id=$14 AND user_id=$15;",
        unitEvo_addSuffix(targetMstId),
        targetMst->min_hp,  targetMst->min_atk,  targetMst->min_def,  targetMst->min_rec,
        keepAddHp,           keepAddAtk,           keepAddDef,          keepAddHeal,
        targetMst->skill_id, targetMst->extra_skill_id,
        newElement,
        baseId, std::string(kUserId)
    );

    // Step 3: return spheres equipped on the evo materials, then DELETE them —
    // deleting without the return would destroy the equipped items.
    if (!matIds.empty())
    {
        std::string matList;
        for (size_t i = 0; i < matIds.size(); ++i)
        {
            if (i) matList += ',';
            matList += std::to_string(matIds[i]);
        }
        co_await gme::returnEquippedSpheres(theDb(), identity, matList);
        co_await theDb()->execSqlCoro(
            "DELETE FROM user_units WHERE user_id=$1 AND user_unit_id IN (" + matList + ");",
            std::string(kUserId)
        );
    }

    // Step 4: deduct zel.
    if (zelCost > 0)
    {
        co_await theDb()->execSqlCoro(
            "UPDATE user_info SET zel = MAX(0, zel - $1) WHERE id=$2;",
            zelCost, std::string(kUserId)
        );
    }

    // Build response.
    UnitEvoResp resp = {};

    {
        // UnitOpeEvoResponse::readParam builds a UnitEvoInfo from five keys and
        // commits it with UnitEvoInfoList::addObject.  All five verified against
        // the strcmp chain — do not infer these from field order.
        EvoResultEntry er = {};
        er.evolved_unit_id     = targetMstId;  // pn16CNah — setUnitID, the form evolved INTO
        er.user_unit_id        = baseId;       // edy7fq3L — setUserUnitID
        er.orig_mst_id         = origMstId;    // t9FEW2KC — setUnitIDBefore, drives the evo animation
        er.user_unit_id_before = baseId;       // u1ECvfg8 — setUserUnitIDBefore; we evolve the row
                                               //            in place, so it is the same instance
        // er.evolution_type   — dV3qji4I / setEvolutionType.  Domain undecoded;
        //                       left at 0 rather than guessed.  See EvoResultEntry.
        resp.evo_result.emplace_back(er);
    }

    // FULL-REPLACE THE UNIT CACHE (4ceMWH6k), exactly as UnitMix does.
    //
    // This replaces a hand-assembled UserUnitInfo that went out under qC2tJs4E,
    // which was wrong twice over:
    //
    //   1. qC2tJs4E is INSERT-IF-ABSENT.  readParam's tail asks
    //      UserUnitInfoList::exist() and returns WITHOUT committing when the
    //      client already owns the unit — and evolution updates the row in
    //      place, so the id is always already owned.  The entry was discarded
    //      every time; the evolved form only appeared after a trip to Home.
    //   2. Hand-assembly silently drops fields that are not plain columns.
    //      `received_order` is the known one: the schema maps it onto the
    //      user_unit_id column, so nothing in a SELECT looks missing and it
    //      goes out as 0.  Reading back through PacketInterfaceFor gives the
    //      byte-identical shape login sends, and cannot drift as fields are
    //      added to UserUnitInfo.
    //
    // The DB writes above have already landed, so this reflects the evolved
    // unit_id, the reset level/exp, the new base stats and the deleted
    // materials.
    resp.unit_refresh = std::move((co_await db::PacketInterfaceFor<::UserUnitInfo>::read(
        theDb(),
        "user_units",
        { db::Lookup("user_id", std::string(kUserId)) })).data);

    LOG_INFO << "UnitEvo: full-replace unit cache — " << resp.unit_refresh->size()
             << " unit(s) under 4ceMWH6k";

    resp.team_info = std::move(
        (co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());

    {
        UnitReinforceEntry rd = {};
        rd.handle_name    = "DecompDev";
        rd.target_lv      = 1;  // level resets to 1 after evo
        rd.unit_mst_id    = std::to_string(targetMstId);
        rd.base_hp        = targetMst->min_hp;
        rd.base_atk       = targetMst->min_atk;
        rd.base_def       = targetMst->min_def;
        rd.base_heal      = targetMst->min_rec;
        rd.add_hp         = keepAddHp;
        rd.add_atk        = keepAddAtk;
        rd.add_def        = keepAddDef;
        rd.add_heal       = keepAddHeal;
        rd.ext_hp         = br["ext_hp"].as<int32_t>();
        rd.ext_atk        = br["ext_atk"].as<int32_t>();
        rd.ext_def        = br["ext_def"].as<int32_t>();
        rd.skill_id       = std::to_string(targetMst->skill_id);
        rd.skill_lv       = 1;
        rd.extra_skill_id = std::to_string(targetMst->extra_skill_id);
        rd.extra_skill_lv = 0;
        rd.unit_type_id   = br["unit_type_id"].as<int32_t>();
        rd.mission_id     = "";
        resp.reinforce.emplace_back(std::move(rd));
    }

    std::string buffer{};
    if (const auto& ec2 = glz::write_json(resp, buffer); ec2)
    {
        LOG_ERROR << "UnitEvo: serialization error: " << glz::format_error(ec2, buffer);
        co_return HandleResult::error("Serialization error");
    }

    co_return HandleResult::success(buffer);
}
