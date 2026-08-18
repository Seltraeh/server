#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Common.hpp>

// Frontier Gate run control — the "Continue / Pause / Retire" prompt the client
// shows between floors, which is the boss-rush loop's decision point.
//
//   Continue  uiFIMUH6 / ZiosS4cd   keep fighting; sends the run handle only
//   Pause     Ng73nFHJ / 4SdtoczN   suspend; re-uploads the party, status 4
//   Retire    cAJp7U4l / Vvpy7qZR   stop and bank rewards; status 3
//
// The j3g5P4cq (MissionStatus) enum is DECODED from the createBody constants —
// End hardcodes addParam(..., "j3g5P4cq", 3) and Save hardcodes 4.  Continue
// sends no status at all, so "keep fighting" is implied by the endpoint.
//
// Note Save addresses the run by GATE id while Continue and End use the run
// handle; that asymmetry is the client's, not ours.
//
// None of the three has a dedicated response class in the binary.  Continue
// returns the run under Mg8K8Y1a the way Start does; Save and Retire return an
// empty OK, which is what the legacy fork did for every one of these GroupIds.

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

    try
    {
        // Retiring closes the run.  The row's EXISTENCE is what marks a run in
        // progress (see the migration note), so ending one deletes it — that is
        // also what makes FrontierGateSuspendedInfo report "nothing suspended".
        (co_await db::DatabaseInterface::remove(
            theDb(),
            "user_frontier_gate_run",
            { db::Lookup("user_id", identity.userId) }));

        LOG_INFO << "FrontierGateEnd: run " << req.run.frogate_num
                 << " retired with status " << req.run.mission_status;
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_ERROR << "FrontierGateEnd: delete failed: " << ex.base().what();
    }

    // // NOT IMPLEMENTED: the result payload.  FrontierGateEndInfo exposes
    // getResScore / getResZel / getResKarma / getResGradeID / getResAchieveP /
    // getResBonusRate / getPrestigePoints, so a real retire screen expects
    // score and rewards.  None of those has a decoded response key yet, and
    // handbook §3.4 is explicit that shipping invented reward values is worse
    // than omitting them — the client falls back to its own defaults.  Decode
    // the End response before paying anything out.
    co_return HandleResult::success("{}");
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

        // // NOT IMPLEMENTED: the Hunter Orb cost.  The prompt says a retry
        // costs one orb, and orbs are ChallengeHeaderInfo::Aube (handbook
        // §6.16) — which the server does not own a counter for yet.  ChallengeBase
        // still sends probe values, so debiting here would be debiting a field we
        // have not finished identifying.  Wire the orb column first, then charge.
        LOG_INFO << "FrontierGateRetry: run " << run->num.frogate_num
                 << " retried with " << resp.party_deck.size() << " deck member(s), "
                 << resp.units.size() << " unit(s); orb NOT debited (see comment)";
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
