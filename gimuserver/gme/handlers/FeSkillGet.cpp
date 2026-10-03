#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/FeSkills.hpp>
#include <gimuserver/gme/common/Transactions.hpp>

// Binary evidence and wire contract: docs/FE_SKILL_PURCHASE_2026-10-01.md.
// One purchase per request. The full roster is the client's only state update.
HANDLEF(FeSkillGet)
{
    ::FeSkillGetReq req{};
    if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json); ec)
        co_return HandleResult::refuseToHome("Invalid enhancement request.");

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const auto& entry = req.fe_skill_get;
    const auto ownedId = gme::FeSkillSet::id(entry.user_unit_id);
    const auto unitId = gme::FeSkillSet::id(entry.unit_id);
    const auto skillId = gme::FeSkillSet::id(entry.fe_skill_id);
    if (!ownedId || *ownedId <= 0 || !unitId || *unitId <= 0 || !skillId || *skillId <= 0)
        co_return HandleResult::refuseToHome("Invalid enhancement selection.");

    const auto& cache = theServer()->cache();
    const UnitFeSkillMst* node = nullptr;
    for (const auto& row : cache.unitFeSkillMst())
        if (row.unit_id == *unitId && row.skill_id == *skillId) { node = &row; break; }
    const FeSkillMst* skill = nullptr;
    for (const auto& row : cache.feSkillMst())
        if (row.id == *skillId) { skill = &row; break; }
    const UnitMst* unit = nullptr;
    for (const auto& row : cache.unitMst())
        if (row.id == *unitId) { unit = &row; break; }
    if (!node || !skill || skill->need_bp <= 0 || !unit)
        co_return HandleResult::refuseToHome("This enhancement is not available for that unit.");

    std::string body;
    auto tx = co_await theDb()->newTransactionCoro();
    try
    {
        const auto rows = co_await tx->execSqlCoro(
            "SELECT unit_id,unit_lvl,sbb_lvl,fe_sp,fe_used_sp,fe_max_sp,fe_skill_info"
            " FROM user_units WHERE user_id=$1 AND user_unit_id=$2;", identity.userId, *ownedId);
        if (rows.empty() || gme::FeSkillSet::id(rows[0]["unit_id"].as<std::string>()) != unitId)
            co_return HandleResult::refuseToHome("The selected unit is no longer available.");
        const auto& row = rows[0];
        if (unit->rarity < 8 || row["unit_lvl"].as<int32_t>() != unit->max_lv || row["sbb_lvl"].as<int32_t>() != 10)
            co_return HandleResult::refuseToHome("Unlock this unit's enhancements before purchasing a skill.");

        const auto prior = row["fe_skill_info"].as<std::string>();
        auto acquired = gme::FeSkillSet::parse(prior);
        const auto canonicalSkill = std::to_string(*skillId);
        // A lost reply followed by a retry must refresh, not spend again.
        if (!acquired.has(canonicalSkill))
        {
            if (!node->term_skill.empty() && node->term_skill != "0")
                for (const auto& term : gme::FeSkillSet::split(node->term_skill, '/'))
                {
                    const auto fields = gme::FeSkillSet::split(term, '@');
                    if (fields.size() != 2 || fields[0] != "1" || !acquired.has(fields[1]))
                        co_return HandleResult::refuseToHome("Acquire the required enhancement first.");
                }
            const auto available = row["fe_sp"].as<int64_t>();
            const auto spent = row["fe_used_sp"].as<int64_t>();
            const auto limit = row["fe_max_sp"].as<int64_t>();
            const int64_t cost = skill->need_bp;
            if (available < cost || spent < 0 || limit < 0 || spent + cost > limit)
                co_return HandleResult::refuseToHome("Not enough usable SP for this enhancement.");
            acquired.add(std::to_string(node->category_id), canonicalSkill);
            co_await tx->execSqlCoro(
                "UPDATE user_units SET fe_sp=$1,fe_used_sp=$2,fe_skill_info=$3"
                " WHERE user_id=$4 AND user_unit_id=$5;",
                available - cost, spent + cost, acquired.str(), identity.userId, *ownedId);
        }

        ::FeSkillGetResp resp{};
        resp.signal_key = req.signal_key;
        resp.unit_refresh = std::move((co_await db::PacketInterfaceFor<::UserUnitInfo>::read(
            tx, "user_units", {db::Lookup("user_id", identity.userId)})).data);
        if (const auto ec = glz::write_json(resp, body); ec)
            throw std::runtime_error(glz::format_error(ec, body));
    }
    catch (const std::exception& ex)
    {
        tx->rollback();
        co_return HandleResult::refuseToHome("Enhancement purchase failed. Please try again.", ex.what());
    }
    // Drogon commits on destruction: the SP is spent only once that commit
    // reports success, not when the reply is ready.
    if (!(co_await gme::CommitTransaction(std::move(tx))))
        co_return HandleResult::refuseToHome("Enhancement purchase could not be saved. Please reconnect.");
    co_return HandleResult::success(body);
}
