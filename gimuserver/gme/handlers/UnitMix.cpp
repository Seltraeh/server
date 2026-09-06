#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/SummonerJournal.hpp>

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <cmath>
#include <deque>

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

// ---------------------------------------------------------------------------
// Special fusion materials
//
// Not every material is exp fodder.  Two families do something else entirely,
// and until now this handler ignored both — fusing a Burst Frog left the BB
// gauge at 0/10 and fusing an imp changed no stat at all.
//
//   Burst frogs  raise the recipient's BB/SBB level.  How far is DATA: the
//                fodder's own UnitMst::burst_level_boost (PXD4v2KY), which is
//                non-zero on exactly the four frogs.
//
//   Imps         permanently raise one stat, up to a cap that is per-RECIPIENT
//                (UnitMst::imp_caps, "HP:ATK:DEF:REC" — 147 distinct values,
//                and 0:0:0:0 for fodder that cannot be imped at all).
//
// The imp AMOUNTS are the one part with no column behind them; they come from
// the wiki's Units page, which states each imp's effect outright.
// ---------------------------------------------------------------------------
// The Sphere Frog (unit 20302, MST_UNIT_20302_NAME) is the third special
// material.  It grants the base unit a second sphere slot, and the game's own
// help text (MST_HELP_SUBTOPIC_100_8_DESCRIPTION, "Sphere Capacity Increase")
// states the rules outright:
//
//   "the sphere capacity of the base unit will increase.  You can only add 1
//    extra sphere per unit, so you cannot fuse 2 or more Sphere Frogs into 1
//    unit.  Those units who have increased their sphere capacity will inherit
//    this trait even when they evolve.  However, even if you fuse them with
//    other materials afterwards, their sphere capacity will not increase any
//    further."
//
// So: one bit, set once, idempotent, inherited by evolution (which updates the
// row in place), and granted by nothing else.  Extra frogs are still consumed
// as ordinary exp fodder -- the cap is on the slot, not on the fusion.
static constexpr int32_t kSphereFrogUnitId = 20302;

struct ImpBonus { int hp, atk, def, rec; };

static const ImpBonus* unitMix_impBonus(int32_t unitId)
{
    // id -> what one of them adds.  Five units, stable since launch.
    static const std::pair<int32_t, ImpBonus> kImps[] = {
        { 40432, {  50,  0,  0,  0 } },   // Vigor Imp Molin      +50 HP
        { 10452, {   0, 20,  0,  0 } },   // Power Imp Pakpak     +20 ATK
        { 20442, {   0,  0, 20,  0 } },   // Guard Imp Ganju      +20 DEF
        { 30432, {   0,  0,  0, 20 } },   // Healing Imp Fwahl    +20 REC
        { 50612, { 150, 60, 60, 60 } },   // Almighty Imp Arton   all four
    };
    for (const auto& [id, bonus] : kImps)
        if (id == unitId) return &bonus;
    return nullptr;
}

// UnitMst::param_max is already decoded as a colon-separated int list
// (HP:ATK:DEF:REC).  A short or absent list means "no imps allowed", which is
// what the 0:0:0:0 rows say anyway.
static ImpBonus unitMix_impCaps(const std::deque<int32_t>& paramMax)
{
    ImpBonus caps{ 0, 0, 0, 0 };
    int* out[] = { &caps.hp, &caps.atk, &caps.def, &caps.rec };
    for (size_t i = 0; i < 4 && i < paramMax.size(); ++i)
        *out[i] = paramMax[i];
    return caps;
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
        "SELECT user_unit_id, unit_id, total_exp, bb_id, bb_lvl, sbb_id, sbb_lvl,"
        " base_hp, base_atk, base_def, base_rec,"
        " add_hp, add_atk, add_def, add_rec,"
        " ext_hp, ext_atk, ext_def, ext_rec,"
        " limit_over_hp, limit_over_atk, limit_over_def, limit_over_rec,"
        " skill_id, skill_lv, extra_skill_id, extra_skill_lv,"
        " element, unit_type_id,"
        " eqip_item_id, eqip_item_frame_id, eqip_item_id2, eqip_item_frame_id2,"
        " sphere_ext"
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

    // Step 2: SELECT material stats to compute exp gain -- and the two special
    // effects that are not exp at all (see unitMix_impBonus above).
    float    gainedExpF     = 0.0f;
    int      burstLevelGain = 0;
    int      sphereFrogs    = 0;
    ImpBonus impGain{ 0, 0, 0, 0 };
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

            // A frog and an imp still hand over their exp; what they ALSO do
            // is the part that was missing.
            if (matData && matData->burst_level_boost > 0)
                burstLevelGain += matData->burst_level_boost;
            if (matMstIdInt == kSphereFrogUnitId)
                ++sphereFrogs;
            if (const ImpBonus* imp = unitMix_impBonus(matMstIdInt))
            {
                impGain.hp  += imp->hp;
                impGain.atk += imp->atk;
                impGain.def += imp->def;
                impGain.rec += imp->rec;
            }
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

    // Imps raise the stat up to a cap that belongs to the RECIPIENT, so clamp
    // against what this unit is allowed rather than a global constant.  Fodder
    // carries 0:0:0:0 and therefore cannot be imped at all — which is correct,
    // and also why the clamp has to happen after the base MST lookup.
    // ⚠ IMPS LIVE IN ext_*, NOT add_*.  CONFIRMED IN-CLIENT 2026-09-05 by
    // probing both buckets with distinct values on one unit: the card drew
    //     HP 5365  [111]   from base 5243 + add 11 + ext 111
    // i.e. the big stat is base+add+ext, and the SMALL ORANGE NUMBER beside it
    // -- the imp total the player actually looks at -- is ext_* alone.
    //
    // Writing imps to add_* (what this did before) still moved the big number,
    // which is why it looked like it worked, but the orange imp figure stayed
    // absent and the cap it represents was invisible.  net/user.kdl called
    // TokWs1B3 the "Imp/stat-up HP bucket" all along.
    //
    // add_* is a separate bonus that folds silently into the total; nothing in
    // this handler writes it, and nothing should until we know what it is.
    const ImpBonus impCaps = baseMstData ? unitMix_impCaps(baseMstData->param_max)
                                         : ImpBonus{ 0, 0, 0, 0 };
    const int oldImpHp  = br["ext_hp"].as<int32_t>();
    const int oldImpAtk = br["ext_atk"].as<int32_t>();
    const int oldImpDef = br["ext_def"].as<int32_t>();
    const int oldImpRec = br["ext_rec"].as<int32_t>();
    const auto clampAdd = [](int have, int gain, int cap) {
        return cap <= 0 ? have : std::min(have + gain, cap);
    };
    const int newImpHp  = clampAdd(oldImpHp,  impGain.hp,  impCaps.hp);
    const int newImpAtk = clampAdd(oldImpAtk, impGain.atk, impCaps.atk);
    const int newImpDef = clampAdd(oldImpDef, impGain.def, impCaps.def);
    const int newImpRec = clampAdd(oldImpRec, impGain.rec, impCaps.rec);

    // BB and SBB level share the frog: one Burst Frog raises whichever the
    // unit has.  10 is the ceiling the UI shows.
    //
    // ⚠ GATE ON THE SKILL ID, NOT THE LEVEL.  A unit with no Brave Burst at all
    // must not gain a BB level -- 134 of unit_mst's 2291 rows carry skill_id 0
    // (every frog, emperor, queen, ghost and metal, plus a few oddities like
    // Thunder Mecha God 40334), and the archive correctly leaves their bb_id
    // empty.  The previous test was `oldBbLvl > 0 || burstLevelGain > 0` with a
    // `max(oldBbLvl, 1)` floor, which fed a frog into a BB-less unit and moved
    // it from 0 straight to 2 -- inventing a Brave Burst the unit does not have
    // and cannot use, and skipping level 1 on the way.
    //
    // A BB-less unit still eats the frog for exp, which is what the real game
    // does; it simply gains no burst level.
    static constexpr int kMaxBurstLevel = 10;
    const bool hasBb    = !br["bb_id"].isNull()  && !br["bb_id"].as<std::string>().empty()
                          && br["bb_id"].as<std::string>() != "0";
    const bool hasSbb   = !br["sbb_id"].isNull() && !br["sbb_id"].as<std::string>().empty()
                          && br["sbb_id"].as<std::string>() != "0";
    const int oldBbLvl  = br["bb_lvl"].as<int32_t>();
    const int oldSbbLvl = br["sbb_lvl"].as<int32_t>();
    const int newBbLvl  = hasBb
        ? std::min(std::max(oldBbLvl, 1) + burstLevelGain, kMaxBurstLevel) : oldBbLvl;
    const int newSbbLvl = hasSbb
        ? std::min(std::max(oldSbbLvl, 1) + burstLevelGain, kMaxBurstLevel) : oldSbbLvl;

    if (burstLevelGain > 0 && !hasBb)
    {
        LOG_INFO << "UnitMix: unit " << baseId << " has no Brave Burst (bb_id empty) — "
                 << "burst material spent as exp only";
    }

    // One slot, ever.  A second frog on a unit that already has the slot is not
    // an error -- it is the case the help text says the client should have
    // stopped -- so it is logged and the frog is spent as plain fodder.
    const int oldSphereExt = br["sphere_ext"].as<int32_t>();
    const int newSphereExt = (sphereFrogs > 0) ? 1 : oldSphereExt;

    // THE SLOT ITSELF lives in eqip_item_frame_id2, not in sphere_ext: -1 hides
    // it, 0 shows it empty, 1..14 shows it holding that sphere type.  See
    // gme::kNoSecondSphereSlot.  max(0, old) is what opens the slot -- it lifts
    // -1 to "empty" while leaving an already-equipped frame untouched, so a
    // second frog on a unit that has the slot cannot wipe its sphere.
    const int oldEqpFrame2 = br["eqip_item_frame_id2"].as<int32_t>();
    const int newEqpFrame2 = newSphereExt ? std::max(0, oldEqpFrame2)
                                          : gme::kNoSecondSphereSlot;

    if (sphereFrogs > 0)
    {
        if (oldSphereExt)
            LOG_WARN << "UnitMix: unit " << baseId << " already has the extra sphere slot — "
                     << sphereFrogs << " Sphere Frog(s) spent as exp fodder only";
        else
            LOG_INFO << "UnitMix: unit " << baseId << " gains the second sphere slot"
                     << (sphereFrogs > 1 ? " (only one of the frogs could grant it)" : "");
    }

    if (burstLevelGain || impGain.hp || impGain.atk || impGain.def || impGain.rec)
    {
        LOG_INFO << "UnitMix: special materials — burst +" << burstLevelGain
                 << " (bb " << oldBbLvl << "->" << newBbLvl
                 << ", sbb " << oldSbbLvl << "->" << newSbbLvl << ")"
                 << ", imps +" << impGain.hp << "/" << impGain.atk << "/"
                 << impGain.def << "/" << impGain.rec
                 << " capped to " << newImpHp << "/" << newImpAtk << "/"
                 << newImpDef << "/" << newImpRec
                 << " (caps " << impCaps.hp << ":" << impCaps.atk << ":"
                 << impCaps.def << ":" << impCaps.rec << ")";
    }

    // ⚠ BASE STATS ARE LEVEL-SCALED AND MUST BE REWRITTEN HERE.
    //
    // user_units.base_* holds the stats AT THE UNIT'S CURRENT LEVEL -- the
    // client displays them verbatim and does no scaling of its own (the unit
    // card showed 3039 HP on a Lv80/80 Nemia whose max_hp is 4854, which is
    // exactly the stored level-1 value).  Levelling a unit without rewriting
    // them left every levelled unit fighting with its level-1 statline.
    //
    // Scale from the MST's min/max, NOT from the stored base_*: the stored
    // value is already scaled to the OLD level, so feeding it back as the
    // minimum compounds the error on every fusion.
    const auto scaleStat = [&](int minV, int maxV) {
        return unitMix_statAtLevel(minV, maxV, newLevel, maxLevel);
    };
    const int newBaseHp  = baseMstData ? scaleStat(baseMstData->min_hp,  baseMstData->max_hp)
                                       : br["base_hp"].as<int32_t>();
    const int newBaseAtk = baseMstData ? scaleStat(baseMstData->min_atk, baseMstData->max_atk)
                                       : br["base_atk"].as<int32_t>();
    const int newBaseDef = baseMstData ? scaleStat(baseMstData->min_def, baseMstData->max_def)
                                       : br["base_def"].as<int32_t>();
    const int newBaseRec = baseMstData ? scaleStat(baseMstData->min_rec, baseMstData->max_rec)
                                       : br["base_rec"].as<int32_t>();

    if (newLevel != oldLevel)
    {
        LOG_INFO << "UnitMix: base stats rescaled for lv " << oldLevel << "->" << newLevel
                 << " — hp " << br["base_hp"].as<int32_t>() << "->" << newBaseHp
                 << ", atk " << br["base_atk"].as<int32_t>() << "->" << newBaseAtk
                 << ", def " << br["base_def"].as<int32_t>() << "->" << newBaseDef
                 << ", rec " << br["base_rec"].as<int32_t>() << "->" << newBaseRec;
    }

    // Step 3: UPDATE base unit level/exp, the rescaled base stats, plus the imp
    // and burst-level columns the special materials just changed.
    co_await theDb()->execSqlCoro(
        "UPDATE user_units SET unit_lvl=$1, exp=$2, total_exp=$3,"
        " ext_hp=$4, ext_atk=$5, ext_def=$6, ext_rec=$7, bb_lvl=$8, sbb_lvl=$9,"
        " sphere_ext=$10, eqip_item_frame_id2=$11,"
        " base_hp=$12, base_atk=$13, base_def=$14, base_rec=$15"
        " WHERE user_unit_id=$16 AND user_id=$17;",
        newLevel, newExp, newTotalExp,
        newImpHp, newImpAtk, newImpDef, newImpRec, newBbLvl, newSbbLvl,
        newSphereExt, newEqpFrame2,
        newBaseHp, newBaseAtk, newBaseDef, newBaseRec,
        baseId, std::string(kUserId)
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
        rd.base_hp        = newBaseHp;
        rd.base_atk       = newBaseAtk;
        rd.base_def       = newBaseDef;
        rd.base_heal      = newBaseRec;
        // add_* passes through untouched -- fusion does not write it.  ext_* is
        // the IMP bucket and carries the POST-fusion clamped values, because an
        // imp that just landed has to show on the result screen.
        rd.add_hp         = br["add_hp"].as<int32_t>();
        rd.add_atk        = br["add_atk"].as<int32_t>();
        rd.add_def        = br["add_def"].as<int32_t>();
        rd.add_heal       = br["add_rec"].as<int32_t>();
        rd.ext_hp         = newImpHp;
        rd.ext_atk        = newImpAtk;
        rd.ext_def        = newImpDef;
        // ⚠ THESE ARE THE BRAVE BURST FIELDS, DESPITE THE NAMES.
        // UnitReinforceEntry.skill_id carries hash nj9Lw7mV and skill_lv
        // carries 3NbeC8AB -- the same two hashes UserUnitInfo uses for bb_id
        // and bb_lvl.  They were being fed from user_units.skill_id/skill_lv,
        // a different (and entirely unused, 0 on all 77 rows) pair of columns,
        // which is why the fusion RESULT screen read "BB Lv. 0/10" while the
        // unit card -- which reads UserUnitInfo -- correctly showed Lv.10.
        // Post-fusion values, like add_* above, so the screen shows the gain.
        rd.skill_id       = br["bb_id"].as<std::string>();
        rd.skill_lv       = newBbLvl;
        rd.extra_skill_id = br["sbb_id"].as<std::string>();
        rd.extra_skill_lv = newSbbLvl;
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

        // ⚠ before/after_skill_lv IS THE BRAVE BURST LEVEL, and fusion DOES
        // change it.  This block (UnitOpeResult, 1ZbHB6Im) is what the fusion
        // RESULT screen draws its "BB Lv. x/10" from -- not the reinforce entry
        // and not the unit cache, both of which carry the right value and are
        // ignored for this field.  Confirmed 2026-09-05 against a live capture:
        // Eliza fused 1->2, the response carried bb_lvl 2 in BOTH xZH6EIQ7 and
        // 4ceMWH6k, and the screen still read 0/10 because these two were 0.
        //
        // They were being read from user_units.skill_lv / extra_skill_lv, an
        // unused pair of columns that is 0 on every row -- the same wrong-column
        // mistake as the reinforce entry, in a second place.
        //
        // Slot 1's frame genuinely is unchanged by fusion, so it keeps the
        // before == after treatment (a 0/0 pair would read as "dropped to
        // nothing" on any stat the UI chooses to flourish).
        //
        // SLOT 2's FRAME IS NOT INERT: a Sphere Frog moves it from -1 to 0,
        // which is the whole visible effect of that fusion.  Sending the old
        // value on both sides is what made the frog look like it did nothing
        // on the result screen.
        const auto eqpFrame     = br["eqip_item_frame_id"].as<int32_t>();

        r.before_skill_lv         = oldBbLvl;
        r.after_skill_lv          = newBbLvl;
        r.before_extra_skill_lv   = oldSbbLvl;
        r.after_extra_skill_lv    = newSbbLvl;
        r.before_eqp_frame_id     = eqpFrame;
        r.after_eqp_frame_id      = eqpFrame;
        r.before_eqp_frame_id2    = oldEqpFrame2;
        r.after_eqp_frame_id2     = newEqpFrame2;

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
            // Scaled from the MST's MIN, not from the stored base_*: the
            // stored value is already scaled to the unit's level, so using it
            // as the floor made every row of the animation drift upward.
            // ⚠ THE TWO STRINGS MUST DIFFER OR NOTHING IS SHOWN.
            //
            // parseMixResult builds a MixResultStatus table from lvup_status
            // (one "lv:hp:atk:def:heal" chunk per level) and then looks
            // param_up_state's level up IN that table -- the stat-gain flourish
            // is the DIFFERENCE between the two.  Building both from the same
            // post-fusion numbers makes that difference zero, which is why an
            // imp fusion moved the totals and animated nothing.
            //
            // So: the table carries the statline WITHOUT this fusion's imps
            // (levelling alone), and param_up_state carries it WITH them.  For a
            // pure level-up the imp terms are equal and the flourish is the
            // level gain, exactly as before.
            const auto statRow = [&](int lv, int impHp, int impAtk, int impDef, int impRec) {
                const int hp = unitMix_statAtLevel(
                                     baseMstData ? baseMstData->min_hp : br["base_hp"].as<int32_t>(),
                                     baseMstData ? baseMstData->max_hp : br["base_hp"].as<int32_t>(),
                                     lv, maxLevel)
                                 + br["add_hp"].as<int32_t>() + impHp
                                 + br["limit_over_hp"].as<int32_t>();
                const int atk = unitMix_statAtLevel(
                                     baseMstData ? baseMstData->min_atk : br["base_atk"].as<int32_t>(),
                                     baseMstData ? baseMstData->max_atk : br["base_atk"].as<int32_t>(),
                                     lv, maxLevel)
                                 + br["add_atk"].as<int32_t>() + impAtk
                                 + br["limit_over_atk"].as<int32_t>();
                const int def = unitMix_statAtLevel(
                                     baseMstData ? baseMstData->min_def : br["base_def"].as<int32_t>(),
                                     baseMstData ? baseMstData->max_def : br["base_def"].as<int32_t>(),
                                     lv, maxLevel)
                                 + br["add_def"].as<int32_t>() + impDef
                                 + br["limit_over_def"].as<int32_t>();
                const int rec = unitMix_statAtLevel(
                                     baseMstData ? baseMstData->min_rec : br["base_rec"].as<int32_t>(),
                                     baseMstData ? baseMstData->max_rec : br["base_rec"].as<int32_t>(),
                                     lv, maxLevel)
                                 + br["add_rec"].as<int32_t>() + impRec
                                 + br["limit_over_rec"].as<int32_t>();
                return std::to_string(lv) + ':' + std::to_string(hp) + ':'
                     + std::to_string(atk) + ':' + std::to_string(def) + ':'
                     + std::to_string(rec);
            };

            std::string table;
            for (int lv = oldLevel; lv <= newLevel; ++lv)
            {
                if (!table.empty()) table += ',';
                table += statRow(lv, oldImpHp, oldImpAtk, oldImpDef, oldImpRec);
            }
            r.lvup_status    = table;
            // Same level as the table's last row, so the lookup resolves -- a
            // level absent from the table null-derefs in parseMixResult.
            r.param_up_state = statRow(newLevel, newImpHp, newImpAtk, newImpDef, newImpRec);
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

    // Journal: "10 Fusions performed".  Counts the units consumed, which is
    // what a player means by a fusion - feeding five fodder in one action is
    // five fusions, not one.
    co_await gme::addJournalProgress(
        theDb(), identity, gme::kJournalTaskUnitFusion,
        static_cast<int32_t>(std::max<size_t>(matIds.size(), 1)));

    // Trophies 100220 合成回数 / 100230 合成ユニット素材使用数 / 100040 総合ゼル使用額.
    // matIds is the material set the handler actually consumed, and zelCost is
    // what it charged -- both taken after validation, not from the raw request.
    co_await gme::bumpArchiveCounters(theDb(), identity, {
        { "unit_mix_cnt",      1 },
        { "unit_mix_elem_cnt", static_cast<int64_t>(matIds.size()) },
        { "zel_use",           static_cast<int64_t>(zelCost) },
    });

    co_return HandleResult::success(buffer);
}
