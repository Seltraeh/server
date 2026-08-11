#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <cmath>

// UnitMix (Power Fusion) — fuse material units into a base unit, gaining exp.
//
// Request  (group Mw08CIg2, key JnegC7RrN3FoW8dQ):
//   "60subGk3": [{"81GjwoWy":"1","2vnqRIr3":"2"}]  — mix/ingredient type (log only)
//   "mCE3rUu5": [{"Rs7bCE3t":"<zelCost>"}]          — zel cost (string)
//   "Km35HAXv": [{"edy7fq3L":"<id>","mnZ5K4Ii":"1"}, // base unit  (role 1)
//                {"edy7fq3L":"<id>","mnZ5K4Ii":"2"}, // material   (role 2) x N]
//
// Response:
//   "xZH6EIQ7": [UnitReinforceEntry] — drives the level-up animation
//   "qC2tJs4E": [UserUnitInfo]       — incremental unit cache update
//   "fEi17cnx": [UserTeamInfo]       — updated zel

// The request struct (UnitMixReq + UnitMixUnitEntry/UnitMixZelEntry) is
// generated from packet-generator/assets/net/{handlers,unit}.kdl.

// The response struct (UnitMixResp + the shared UnitReinforceEntry under
// xZH6EIQ7) is generated from packet-generator/assets/net/{handlers,unit}.kdl.
// unit_update rides UserUnitInfo under qC2tJs4E; team_info rides UserTeamInfo
// under fEi17cnx.

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string unitMix_stripSuffix(const std::string& raw)
{
    auto pos = raw.find('_');
    return (pos != std::string::npos) ? raw.substr(0, pos) : raw;
}

// Cumulative exp needed to REACH `level` from level 1.
// UnitExpPatternMst::need_exp is the incremental cost per level transition.
static int unitMix_expForLevel(const std::vector<UnitExpPatternMst>& pat,
                                int patternId, int level)
{
    int acc = 0;
    for (const auto& e : pat)
    {
        if (e.id != patternId) continue;
        if (e.lv <= 1)         continue;
        if (e.lv > level)      break;
        acc += e.need_exp;
    }
    return acc;
}

// A unit's base stat at a given level.  BF scales linearly from the MST's
// min (level 1) to its max (max_lv); user_units stores only the min, so the
// level scaling has to be recomputed here.
//
// // UNVERIFIED: the rounding.  Linear interpolation is the established BF
// formula, but whether the client floors or rounds at each step has not been
// checked against a capture — an off-by-one in a displayed stat is cosmetic,
// and the structure of the payload is what matters (see buildLvupStatus).
static int unitMix_statAtLevel(int minV, int maxV, int level, int maxLv)
{
    if (maxLv <= 1 || level <= 1) return minV;
    if (level >= maxLv)           return maxV;
    return minV + (int)((double)(maxV - minV) * (double)(level - 1) / (double)(maxLv - 1));
}

// Highest level reachable with totalExp under maxLevel cap.
static int unitMix_levelFromExp(const std::vector<UnitExpPatternMst>& pat,
                                 int patternId, int maxLevel, int totalExp)
{
    int acc = 0, level = 1;
    for (const auto& e : pat)
    {
        if (e.id != patternId)   continue;
        if (e.lv <= 1)           continue;
        if (e.lv > maxLevel)     break;
        if (acc + e.need_exp <= totalExp) { acc += e.need_exp; level = e.lv; }
        else break;
    }
    return level;
}

// ---------------------------------------------------------------------------
// Handler
// ---------------------------------------------------------------------------
HANDLEF(UnitMix)
{
    (void)session;
    LOG_INFO << "UnitMix: " << json;

    // Parse request.  Use error_on_unknown_keys=false so the extra "60subGk3"
    // operation-type group sent by the client doesn't abort parsing.
    UnitMixReq req = {};
    {
        glz::context ctx{};
        if (const auto& ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
        {
            LOG_WARN << "UnitMix: bad request JSON: " << glz::format_error(ec, json);
            co_return HandleResult::error("Deserialization error");
        }
    }

    // Resolve the current user from the request's login info.
    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const std::string kUserId = identity.userId;

    // Split base vs material units.
    int32_t baseId = 0;
    std::vector<int32_t> matIds;
    for (const auto& u : req.units)
    {
        if (u.role == "1")       baseId = u.user_unit_id;
        else if (u.user_unit_id) matIds.push_back(u.user_unit_id);
    }

    if (baseId == 0)
    {
        LOG_WARN << "UnitMix: no base unit in request";
        co_return HandleResult::error("UnitMix: no base unit");
    }

    const int32_t zelCost = req.zel_cost_list.empty() ? 0 : req.zel_cost_list[0].cost;

    // Build material IN-clause.
    std::string matList;
    for (size_t i = 0; i < matIds.size(); ++i)
    {
        if (i) matList += ',';
        matList += std::to_string(matIds[i]);
    }

    // Step 1: SELECT base unit full stats.
    const auto baseRows = co_await theDb()->execSqlCoro(
        "SELECT user_unit_id, unit_id, total_exp,"
        " base_hp, base_atk, base_def, base_rec,"
        " add_hp, add_atk, add_def, add_rec,"
        " ext_hp, ext_atk, ext_def, ext_rec,"
        " limit_over_hp, limit_over_atk, limit_over_def, limit_over_rec,"
        " skill_id, skill_lv, extra_skill_id, extra_skill_lv,"
        " element, unit_type_id,"
        " eqip_item_id, eqip_item_frame_id, eqip_item_id2, eqip_item_frame_id2"
        " FROM user_units WHERE user_id=$1 AND user_unit_id=$2 LIMIT 1;",
        std::string(kUserId), baseId
    );

    if (baseRows.empty())
    {
        LOG_WARN << "UnitMix: base unit " << baseId << " not found";
        co_return HandleResult::error("UnitMix: base unit not found");
    }
    const auto& br = baseRows[0];

    const std::string rawBaseUnitId = br["unit_id"].as<std::string>();
    const std::string baseMstId     = unitMix_stripSuffix(rawBaseUnitId);
    const int32_t     baseMstIdInt  = std::stoi(baseMstId);
    const int         baseTotalExp  = br["total_exp"].as<int32_t>();

    // Lookup base unit MST data.
    const auto& unitMst = theServer()->cache().unitMst();
    const UnitMst* baseMstData = nullptr;
    for (const auto& u : unitMst) { if (u.id == baseMstIdInt) { baseMstData = &u; break; } }

    const int baseElement   = baseMstData ? baseMstData->element       : 0;
    const int expPatternId  = baseMstData ? baseMstData->exp_pattern_id: 10;
    const int maxLevel      = baseMstData ? baseMstData->max_lv        : 100;

    // Step 2: SELECT material stats to compute exp gain.
    float gainedExpF = 0.0f;
    if (!matIds.empty())
    {
        static const int kRarityBonus[] = { 0, 100, 200, 500, 1000, 1500, 3000, 5000, 10000 };

        const auto matRows = co_await theDb()->execSqlCoro(
            "SELECT unit_id, total_exp FROM user_units"
            " WHERE user_id=$1 AND user_unit_id IN (" + matList + ");",
            std::string(kUserId)
        );

        for (const auto& row : matRows)
        {
            const std::string matMstId    = unitMix_stripSuffix(row["unit_id"].as<std::string>());
            const int32_t     matMstIdInt = std::stoi(matMstId);
            const int         matTotalExp = row["total_exp"].as<int32_t>();

            const UnitMst* matData = nullptr;
            for (const auto& u : unitMst) { if (u.id == matMstIdInt) { matData = &u; break; } }

            const int matAdjust = matData ? matData->adjust_exp : 0;
            const int matCost   = matData ? matData->cost       : 1;
            const int matRare   = matData ? matData->rarity     : 1;
            const int matElem   = matData ? matData->element    : 0;

            float matExp = (float)matTotalExp / 2.5f;
            matExp += (float)(matCost * 2);
            matExp += (float)matAdjust;
            if (matRare >= 1 && matRare <= 8)
                matExp += (float)kRarityBonus[matRare];
            if (baseElement != 0 && matElem == baseElement)
                matExp *= 1.5f;

            gainedExpF += matExp;
        }
    }

    const int gainedExp = (int)llroundf(gainedExpF);

    // Compute new level / exp.
    const auto& expPat      = theServer()->cache().initializeResp().exp_pattern;
    const int   maxTotalExp = unitMix_expForLevel(expPat, expPatternId, maxLevel);
    int         newTotalExp = baseTotalExp + gainedExp;
    if (maxTotalExp > 0 && newTotalExp > maxTotalExp) newTotalExp = maxTotalExp;

    const int newLevel    = unitMix_levelFromExp(expPat, expPatternId, maxLevel, newTotalExp);
    const int levelExpFlr = unitMix_expForLevel(expPat, expPatternId, newLevel);
    const int newExp      = newTotalExp - levelExpFlr;

    // The BEFORE side of the result screen.  Derived the same way as the after
    // side so the two are guaranteed consistent: the row stores total_exp, not
    // a level, so the pre-fusion level is whatever that total maps to.
    const int oldLevel    = unitMix_levelFromExp(expPat, expPatternId, maxLevel, baseTotalExp);
    const int oldExp      = baseTotalExp - unitMix_expForLevel(expPat, expPatternId, oldLevel);

    LOG_INFO << "UnitMix: unit=" << baseId << " mst=" << baseMstId
             << " totalExp " << baseTotalExp << "+" << gainedExp << "=" << newTotalExp
             << " lv->" << newLevel << "/" << maxLevel;

    // Step 3: UPDATE base unit level/exp (preserve IMP add_* cols).
    co_await theDb()->execSqlCoro(
        "UPDATE user_units SET unit_lvl=$1, exp=$2, total_exp=$3"
        " WHERE user_unit_id=$4 AND user_id=$5;",
        newLevel, newExp, newTotalExp, baseId, std::string(kUserId)
    );

    // Step 4: return spheres equipped on the fodder, then DELETE the material
    // units — deleting without the return would destroy the equipped items.
    if (!matIds.empty())
    {
        co_await gme::returnEquippedSpheres(theDb(), identity, matList);
        co_await theDb()->execSqlCoro(
            "DELETE FROM user_units WHERE user_id=$1 AND user_unit_id IN (" + matList + ");",
            std::string(kUserId)
        );
    }

    // Step 5: deduct zel.
    if (zelCost > 0)
    {
        co_await theDb()->execSqlCoro(
            "UPDATE user_info SET zel = MAX(0, zel - $1) WHERE id=$2;",
            zelCost, std::string(kUserId)
        );
    }


    // Build response.
    UnitMixResp resp = {};

    // Reinforcement animation entry.
    {
        UnitReinforceEntry rd = {};
        rd.handle_name    = "DecompDev";
        rd.target_lv      = newLevel;
        rd.unit_mst_id    = baseMstId;
        rd.base_hp        = br["base_hp"].as<int32_t>();
        rd.base_atk       = br["base_atk"].as<int32_t>();
        rd.base_def       = br["base_def"].as<int32_t>();
        rd.base_heal      = br["base_rec"].as<int32_t>();
        rd.add_hp         = br["add_hp"].as<int32_t>();
        rd.add_atk        = br["add_atk"].as<int32_t>();
        rd.add_def        = br["add_def"].as<int32_t>();
        rd.add_heal       = br["add_rec"].as<int32_t>();
        rd.ext_hp         = br["ext_hp"].as<int32_t>();
        rd.ext_atk        = br["ext_atk"].as<int32_t>();
        rd.ext_def        = br["ext_def"].as<int32_t>();
        rd.skill_id       = std::to_string(br["skill_id"].as<int32_t>());
        rd.skill_lv       = br["skill_lv"].as<int32_t>();
        rd.extra_skill_id = std::to_string(br["extra_skill_id"].as<int32_t>());
        rd.extra_skill_lv = br["extra_skill_lv"].as<int32_t>();
        rd.unit_type_id   = br["unit_type_id"].as<int32_t>();
        rd.mission_id     = "";
        resp.reinforce.emplace_back(std::move(rd));
    }

    // Incremental unit cache update.
    //
    // Read back through PacketInterfaceFor<UserUnitInfo> rather than
    // hand-assembling the entry.  This is the SAME read UserInfo uses at login,
    // and login is the one path the client provably accepts — so building the
    // entry any other way is guessing at a shape we already have.
    //
    // Hand-assembly is how the level stopped showing after a fusion: the entry
    // was 47 fields and complete-looking, but `received_order` (Bvkx8s6M) was
    // never assigned and so went out as 0, where login sends 1000.  It is not
    // a column — the schema maps it onto `user_unit_id` — so there was nothing
    // in the SELECT to notice was missing.  Any field added to UserUnitInfo in
    // future would have silently defaulted the same way.
    //
    // The DB writes above have already landed, so this read reflects the new
    // level, exp and total_exp.
    //
    // FULL-REPLACE THE UNIT CACHE (4ceMWH6k).  Confirmed working in-client
    // 2026-08-10: the fusion menu shows the new level and the fodder are gone,
    // with no trip to Home.
    //
    // We deliberately do NOT echo the base unit under qC2tJs4E.  That key is
    // INSERT-IF-ABSENT: readParam's tail asks UserUnitInfoList::exist() and
    // returns without committing when the client already owns the unit, so for
    // a fusion base it is a guaranteed no-op.
    //
    // 4ceMWH6k is the only key that can change an owned unit, because its
    // readParam calls removeAllObjects() first — which is also why it must
    // carry the WHOLE roster, not just the fused unit.
    //
    // It is needed because the client has no local apply path of its own: it
    // never writes the fused unit (no UserUnitInfo mutator is reachable from a
    // fusion scene) and never drops the fodder (removeObject's only real callers
    // are the FrontierGate/FGPlus friend lists; removeObjectWithUserUnitID has
    // none), and it issues no request after the fusion.
    //
    // The historical objection was that removeAllObjects release()s every
    // CCObject in the list, dangling any raw UserUnitInfo* a live scene holds —
    // it soft-locked the result screen once.  That build also sent
    // lvup_status "1", which crashes parseMixResult by itself; with the ritual
    // payload correct, this key is fine here.  If a future scene does start
    // soft-locking on it, that is the mechanism to suspect.
    resp.unit_refresh = std::move((co_await db::PacketInterfaceFor<::UserUnitInfo>::read(
        theDb(),
        "user_units",
        { db::Lookup("user_id", std::string(kUserId)) })).data);

    LOG_INFO << "UnitMix: full-replace unit cache — " << resp.unit_refresh->size()
             << " unit(s) under 4ceMWH6k";

    // THE RESULT SCREEN (1ZbHB6Im).  This is what actually drives the xp-bar
    // sweep and the level-up flourish, and the server had never sent it — the
    // screen rendered the unit and then sat there with nothing to animate.
    // Every field here is a before/after pair for exactly that reason.
    {
        UnitOpeResult r = {};
        r.unit_id       = baseMstId;
        r.user_unit_id  = std::to_string(baseId);
        r.zel           = zelCost;
        r.exp           = gainedExp;
        r.success_type  = 0;                    // normal result; see the KDL note
        r.before_lv     = oldLevel;
        r.after_lv      = newLevel;
        r.before_exp    = oldExp;
        r.after_exp     = newExp;

        // Fusion does not change any of these, but the screen reads before and
        // after for each, so send them equal rather than leaving them at zero —
        // a 0/0 pair would read as "dropped to nothing" on any stat the UI
        // chooses to flourish.
        const auto skillLv      = br["skill_lv"].as<int32_t>();
        const auto extraSkillLv = br["extra_skill_lv"].as<int32_t>();
        const auto eqpFrame     = br["eqip_item_frame_id"].as<int32_t>();
        const auto eqpFrame2    = br["eqip_item_frame_id2"].as<int32_t>();

        r.before_skill_lv         = skillLv;
        r.after_skill_lv          = skillLv;
        r.before_extra_skill_lv   = extraSkillLv;
        r.after_extra_skill_lv    = extraSkillLv;
        r.before_eqp_frame_id     = eqpFrame;
        r.after_eqp_frame_id      = eqpFrame;
        r.before_eqp_frame_id2    = eqpFrame2;
        r.after_eqp_frame_id2     = eqpFrame2;

        // FE and DBB are not implemented; equal zeroes are the honest "no
        // change" for a feature that does not exist yet.
        r.before_fe_bp = r.after_fe_bp = 0;
        r.before_max_fe_bp = r.after_max_fe_bp = 0;
        r.before_dbb_skill_level = r.after_dbb_skill_level = 0;

        // THE STAT TABLE.  lvup_status and param_up_state are a matched pair
        // and must be built together — decoded from parseMixResult:
        //
        //   lvup_status    strtok(s, ",")  -> one MixResultStatus per chunk,
        //                  each chunk "lv:hp:atk:def:heal"
        //   param_up_state parseList(':')  -> a single "lv:hp:atk:def:heal";
        //                  its lv is looked up AGAINST that table
        //
        // Sent empty, the client falls back to building ONE row from the unit
        // it is displaying — at the OLD level — which is why the bar moved but
        // no stats or level-up ever appeared.  And param_up_state alone would
        // look up a level absent from that one-row table, leaving `v119 = 0`
        // and dereferencing it (`MixResultStatus::getHp(v119)`) — the crash.
        //
        // So: emit a row for every level from before to after, then point
        // param_up_state at the final one.
        {
            const auto statRow = [&](int lv) {
                const int hp   = unitMix_statAtLevel(br["base_hp"].as<int32_t>(),
                                     baseMstData ? baseMstData->max_hp : br["base_hp"].as<int32_t>(),
                                     lv, maxLevel)
                                 + br["add_hp"].as<int32_t>() + br["ext_hp"].as<int32_t>()
                                 + br["limit_over_hp"].as<int32_t>();
                const int atk  = unitMix_statAtLevel(br["base_atk"].as<int32_t>(),
                                     baseMstData ? baseMstData->max_atk : br["base_atk"].as<int32_t>(),
                                     lv, maxLevel)
                                 + br["add_atk"].as<int32_t>() + br["ext_atk"].as<int32_t>()
                                 + br["limit_over_atk"].as<int32_t>();
                const int def  = unitMix_statAtLevel(br["base_def"].as<int32_t>(),
                                     baseMstData ? baseMstData->max_def : br["base_def"].as<int32_t>(),
                                     lv, maxLevel)
                                 + br["add_def"].as<int32_t>() + br["ext_def"].as<int32_t>()
                                 + br["limit_over_def"].as<int32_t>();
                const int rec  = unitMix_statAtLevel(br["base_rec"].as<int32_t>(),
                                     baseMstData ? baseMstData->max_rec : br["base_rec"].as<int32_t>(),
                                     lv, maxLevel)
                                 + br["add_rec"].as<int32_t>() + br["ext_rec"].as<int32_t>()
                                 + br["limit_over_rec"].as<int32_t>();
                return std::to_string(lv) + ':' + std::to_string(hp) + ':'
                     + std::to_string(atk) + ':' + std::to_string(def) + ':'
                     + std::to_string(rec);
            };

            std::string table;
            for (int lv = oldLevel; lv <= newLevel; ++lv)
            {
                if (!table.empty()) table += ',';
                table += statRow(lv);
            }
            r.lvup_status    = table;
            r.param_up_state = statRow(newLevel);
        }

        // FE is not implemented; empty is the honest "no allocation".  These
        // are plain string setters with no list parsing behind them.
        r.before_fe_info = "";
        r.after_fe_info  = "";

        resp.ope_result.push_back(std::move(r));

        LOG_INFO << "UnitMix: result lv " << oldLevel << "->" << newLevel
                 << " exp " << oldExp << "->" << newExp
                 << " (+" << gainedExp << "), lvup=" << r.lvup_status;
    }

    resp.team_info = std::move(
        (co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());

    std::string buffer{};
    if (const auto& ec2 = glz::write_json(resp, buffer); ec2)
    {
        LOG_ERROR << "UnitMix: serialization error: " << glz::format_error(ec2, buffer);
        co_return HandleResult::error("Serialization error");
    }

    co_return HandleResult::success(buffer);
}
