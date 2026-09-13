#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Achievements.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/FrontierGate.hpp>
#include <gimuserver/gme/common/HunterOrbs.hpp>

// Frontier Gate run control — the "Continue / Pause / Retire" prompt the client
// shows between floors, which is the boss-rush loop's decision point.
//
//   Continue  uiFIMUH6 / ZiosS4cd   keep fighting; sends the run handle only
//   Pause     Ng73nFHJ / 4SdtoczN   suspend; re-uploads the party, status 4
//   Retire    cAJp7U4l / Vvpy7qZR   stop and bank rewards; status 3 — the
//                                   reply is the result screen's data
//
// The j3g5P4cq (MissionStatus) enum is DECODED from the createBody constants —
// End hardcodes addParam(..., "j3g5P4cq", 3) and Save hardcodes 4.  Continue
// sends no status at all, so "keep fighting" is implied by the endpoint.
//
// Note Save addresses the run by GATE id while Continue and End use the run
// handle; that asymmetry is the client's, not ours.
//
// Continue returns the run under Mg8K8Y1a the way Start does and Save an empty
// OK.  Retire answers with the run's result (mu0kXAlV) and rewards (QXFCkE67),
// which FrontierGateResultScene reads — see gme/common/FrontierGate.hpp.

namespace
{
// Loads the caller's active run into a FrontierGateNum, or reports that there
// is none.  Shared by all three verbs so they agree on what a run looks like.
struct StoredRun
{
    ::FrontierGateNum num{};
    std::vector<::FrontierGatePartyDeckInfo> deck;
    std::vector<::FrontierGateUserUnitInfo> units;
};

drogon::Task<std::optional<StoredRun>> loadRun(const gme::UserIdentity& identity)
{
    const auto rows = co_await db::DatabaseInterface::read(
        theDb(),
        "user_frontier_gate_run",
        {
            db::Data("frogate_id"),
            db::Data("frogate_num"),
            db::Data("mission_status"),
            db::Data("sel_support_id"),
            db::Data("party_deck_json"),
            db::Data("party_units_json"),
            db::Lookup("user_id", identity.userId),
        });

    if (rows.affected == 0)
        co_return std::nullopt;

    ::FrontierGateNum num{};
    num.frogate_id = rows.front<int32_t>("frogate_id");
    num.frogate_num = rows.front<int32_t>("frogate_num");
    num.mission_status = rows.front<int32_t>("mission_status");
    num.sel_support_id = rows.front<int32_t>("sel_support_id");
    // Neutral multipliers — the client divides these by 100, so 0 would scale
    // the party to nothing.  See FrontierGateStart.cpp for the full note.
    num.power_up_rate_hp = 100.0f;
    num.power_up_rate_atk = 100.0f;
    num.power_up_rate_def = 100.0f;

    StoredRun out{};
    out.num = num;

    // The party is stored as JSON because the client hands us the whole thing
    // and reads the whole thing back; a malformed blob is logged and treated as
    // "no party" rather than failing the request.
    const auto deckJson = rows.front<std::string>("party_deck_json");
    const auto unitsJson = rows.front<std::string>("party_units_json");
    if (!deckJson.empty())
    {
        if (const auto ec = glz::read_json(out.deck, deckJson); ec)
            LOG_WARN << "FrontierGate: stored party deck did not parse; continuing without it";
    }
    if (!unitsJson.empty())
    {
        if (const auto ec = glz::read_json(out.units, unitsJson); ec)
            LOG_WARN << "FrontierGate: stored party units did not parse; continuing without it";
    }

    co_return out;
}
}

HANDLEF(FrontierGateContinue)
{
    LOG_INFO << "FrontierGateContinue: " << json;

    ::FrontierGateContinueReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "FrontierGateContinue: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    ::FrontierGateRunStateResp resp{};
    try
    {
        auto run = co_await loadRun(identity);
        if (!run)
        {
            // Nothing to continue.  Empty OK rather than a closed session so the
            // client returns to the gate list instead of retry-looping.
            LOG_WARN << "FrontierGateContinue: no active run for " << identity.userId;
            co_return HandleResult::success("{}");
        }

        if (run->num.frogate_num != req.run.frogate_num)
        {
            // The client is continuing a run we do not think is current.  Trust
            // the stored row rather than the request — inventing a run from a
            // handle we never issued would put the client in a state the server
            // cannot end or save.
            LOG_WARN << "FrontierGateContinue: client sent run handle "
                     << req.run.frogate_num << " but the stored run is "
                     << run->num.frogate_num << "; continuing the stored one";
        }

        // Hand the party back.  Continue carries none, so without the stored
        // snapshot the client rebuilds an empty FrontierGateUserUnitInfoList
        // and the next floor loads with no units at all.
        resp.num = std::move(run->num);
        resp.party_deck = std::move(run->deck);
        resp.units = std::move(run->units);
        LOG_INFO << "FrontierGateContinue: restored " << resp.party_deck.size()
                 << " deck member(s), " << resp.units.size() << " unit(s)";
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_ERROR << "FrontierGateContinue: read failed: " << ex.base().what();
        co_return HandleResult::success("{}");
    }

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

HANDLEF(FrontierGateSave)
{
    LOG_INFO << "FrontierGateSave: " << json;

    ::FrontierGateSaveReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "FrontierGateSave: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    try
    {
        // Record the suspension AND the party it was suspended with — Save is
        // the only verb besides Start that carries the units, so this is the
        // snapshot a later Continue restores from.
        (co_await db::DatabaseInterface::update(
            theDb(),
            "user_frontier_gate_run",
            {
                db::Data("mission_status", req.run.mission_status),
                db::Data("party_deck_json", glz::write_json(req.party_deck).value_or("[]")),
                db::Data("party_units_json", glz::write_json(req.units).value_or("[]")),
                db::Lookup("user_id", identity.userId),
            }));

        LOG_INFO << "FrontierGateSave: gate " << req.run.frogate_id
                 << " suspended with status " << req.run.mission_status
                 << " (" << req.party_deck.size() << " deck member(s), "
                 << req.units.size() << " unit(s) stored)";
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_ERROR << "FrontierGateSave: update failed: " << ex.base().what();
    }

    co_return HandleResult::success("{}");
}

HANDLEF(FrontierGateEnd)
{
    LOG_INFO << "FrontierGateEnd: " << json;

    ::FrontierGateEndReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "FrontierGateEnd: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    // Retire.  The interval scene changes to the result scene when this reply
    // arrives, and the result scene reads the run's result and reward list from
    // it (FrontierGateEndResp in net/handlers.kdl).  The request names only the
    // run handle, so the run is paid from what its battles reported
    // (gme::finishFrontierRun), which also closes it: the row's EXISTENCE is what
    // marks a run in progress, and FrontierGateSuspendedInfo then reports
    // nothing suspended.
    ::FrontierGateEndResp resp{};
    auto transaction = co_await theDb()->newTransactionCoro();
    try
    {
        if (const auto run = co_await gme::loadFrontierRun(transaction, identity))
        {
            auto result = co_await gme::finishFrontierRun(transaction, identity, *run);
            resp.frontier_end.push_back(std::move(result.end));
            resp.frontier_rewards = std::move(result.rewards);
            // The gate's own currencies are credited, not gifted, so the result
            // screen's reply carries the balances it changed.
            if (result.tokensChanged)
                resp.event_token_info = co_await gme::loadEventTokens(transaction, identity);
            if (result.ticketsChanged)
                resp.summon_ticket_v2_user = co_await gme::loadSummonTicketsV2(transaction, identity);
        }
        else
        {
            // Nothing to pay, but the result scene still needs a result.
            LOG_WARN << "FrontierGateEnd: no open run for " << identity.userId;
            ::FrontierEndInfo end{};
            end.grade_id = gme::frontierGrade(0);
            end.bonus_rate = 1.0f;
            resp.frontier_end.push_back(std::move(end));
        }
        resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());

        // Merit Points.  Retiring is the one thing in this fork that moves the
        // balance, and the number the result screen prints is its OWN singleton
        // (gme/common/Achievements.hpp) -- user_info does not carry it.  Sent
        // unconditionally: even a run worth 0 points re-states the total, and a
        // run with no open row above still leaves the client consistent.
        resp.achievement_info = co_await gme::loadAchievementInfo(transaction, identity);

        LOG_INFO << "FrontierGateEnd: run " << req.run.frogate_num
                 << " retired with status " << req.run.mission_status
                 << "; merit points now " << resp.achievement_info.id;
    }
    catch (...)
    {
        transaction->rollback();
        throw;
    }

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

HANDLEF(FrontierGateRetry)
{
    LOG_INFO << "FrontierGateRetry: " << json;

    ::FrontierGateRetryReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "FrontierGateRetry: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    ::FrontierGateRunStateResp resp{};
    try
    {
        auto run = co_await loadRun(identity);
        if (!run)
        {
            LOG_WARN << "FrontierGateRetry: no active run for " << identity.userId;
            co_return HandleResult::success("{}");
        }

        // Retry re-uploads the DECK but not the units, so the fresh deck is
        // stored over the old one while the stored unit snapshot is kept.
        // That is what lets the party survive a retry.
        if (!req.party_deck.empty())
        {
            co_await db::DatabaseInterface::update(
                theDb(),
                "user_frontier_gate_run",
                {
                    db::Data("party_deck_json", glz::write_json(req.party_deck).value_or("[]")),
                    db::Lookup("user_id", identity.userId),
                });
            run->deck = req.party_deck;
        }

        resp.num = std::move(run->num);
        resp.party_deck = std::move(run->deck);
        resp.units = std::move(run->units);

        // THE HUNTER ORB.  The prompt says a retry costs one, and the orb
        // column now exists (gme/common/HunterOrbs.hpp), so this charges.
        //
        // Charged AFTER the run has been rebuilt: a retry that could not find
        // its run has nothing to sell.  A player with none is let through
        // rather than stranded mid-run — the entry was already paid for at
        // FrontierGateStart, the client has its own count and does its own
        // prompting, and refusing here would drop them out of a run they are
        // standing in.  Refusing at the DOOR is safe; refusing inside is not.
        const auto orbPaid = co_await gme::spendHunterOrb(theDb(), identity);
        LOG_INFO << "FrontierGateRetry: run " << run->num.frogate_num
                 << " retried with " << resp.party_deck.size() << " deck member(s), "
                 << resp.units.size() << " unit(s); orb "
                 << (orbPaid ? "debited" : "NOT debited (none held; retry allowed anyway)");
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_ERROR << "FrontierGateRetry: failed: " << ex.base().what();
        co_return HandleResult::success("{}");
    }

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

HANDLEF(FrontierGateRestart)
{
    LOG_INFO << "FrontierGateRestart: " << json;

    ::FrontierGateRestartReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "FrontierGateRestart: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    try
    {
        // Restart abandons the run: drop it and let FrontierGateStart open a
        // fresh one, which is also where the party gets re-uploaded.
        (co_await db::DatabaseInterface::remove(
            theDb(),
            "user_frontier_gate_run",
            { db::Lookup("user_id", identity.userId) }));

        LOG_INFO << "FrontierGateRestart: gate " << req.run.frogate_id
                 << " run discarded; awaiting a fresh Start";
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_ERROR << "FrontierGateRestart: delete failed: " << ex.base().what();
    }

    co_return HandleResult::success("{}");
}
