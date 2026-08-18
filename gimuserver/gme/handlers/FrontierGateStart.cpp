#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>

// FrontierGateStart (l3lkDBSc) — opens a Frontier Gate run.
//
// Request  : group Mg8K8Y1a carries the gate id + chosen support id.  The same
//            body also carries the deck (MxmCpDRC) and unit (52xDBRGr)
//            snapshots, which belong to tier 3 and are ignored for now — the
//            lenient read drops them.
// Response : key Mg8K8Y1a (FrontierGateNumResponse), a single OBJECT.
//
// Every setter in FrontierGateNumResponse::readParam @0x142FF48 is a real named
// call, so this struct carries no guessed field names — unlike dPM7oJDl.
//
// Evidence: tools/ida/audits/dPM7oJDl_audit.txt (§6c/§6d),
// tools/FRONTIER_GATE_STATE_MODEL.md.

namespace
{
// The client parses the three power-up rates with CommonUtils::StrToFloat and
// then divides by 100 before storing, so the wire unit is a PERCENTAGE and 100
// is the identity multiplier.  Sending 0 would scale the party's HP/ATK/DEF by
// zero, so the neutral value is 100 — not the zero-initialised default.
//
// This is not a guessed value in the §3.4 sense: the semantic is fully decoded,
// only the *source* of a non-neutral bonus is not.  When that source is found
// (gate MST battle params or the support effect), replace this constant.
constexpr float kNeutralPowerUpRate = 100.0f;
}

HANDLEF(FrontierGateStart)
{
    LOG_INFO << "FrontierGateStart: " << json;

    // `::` qualification throughout — the handler is GmeHandlers::FrontierGateStart
    // and the generated types sit in the global namespace (cf. BadgeInfo.cpp).
    ::FrontierGateStartReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "FrontierGateStart: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    const int32_t gateId = req.frogate.frogate_id;

    // SANITISE THE SUPPORT ID BEFORE IT IS STORED OR ECHOED.
    //
    // Picking "None" on the support-select screen makes the client send
    // UNINITIALISED MEMORY here: a live capture (2026-08-07) carried
    // NungTq5g = -858993460, which is 0xCCCCCCCC, MSVC's debug fill pattern.
    //
    // Storing that was bad; echoing it back in the Mg8K8Y1a response was worse.
    // The client takes SelSupportID and looks it up in FrontierGateSupportMst
    // (61 rows, ids 100..202003), gets nothing back, and dereferences the miss
    // — the mission loaded its assets and then crashed.  Same class of failure
    // as the null UnitCgsMstList deref in handbook §3.2.
    //
    // Anything that is not a real support id becomes 0, which is the "no
    // support chosen" value the rest of this subsystem already understands
    // (FrontierGateInfo omits 0 from its comma list rather than emitting it).
    const auto& supportMst = theServer()->cache().frontierGateSupportMst();
    const bool supportKnown = std::any_of(
        supportMst.begin(), supportMst.end(),
        [&](const ::FrontierGateSupportMst& s) { return s.id == req.frogate.support_id; });

    const int32_t supportId = supportKnown ? req.frogate.support_id : 0;
    if (!supportKnown && req.frogate.support_id != 0)
    {
        LOG_WARN << "FrontierGateStart: support id " << req.frogate.support_id
                 << " is not in FrontierGateSupportMst"
                 << (req.frogate.support_id == -858993460
                         ? " (0xCCCCCCCC — client sent uninitialised memory, likely \"None\" selected)"
                         : "")
                 << "; storing 0 instead";
    }

    // Reject a gate the catalog does not know: the client would then hold a run
    // handle for a gate it cannot render.  Empty OK rather than a closed
    // session (handbook §5) so the UI simply does not enter.
    const auto& gateMst = theServer()->cache().frontierGateMst();
    const bool gateKnown = std::any_of(
        gateMst.begin(), gateMst.end(),
        [gateId](const ::FrontierGateMst& gate) { return gate.id == gateId; });
    if (!gateKnown)
    {
        LOG_WARN << "FrontierGateStart: unknown frogate_id " << gateId << ", refusing to open a run";
        co_return HandleResult::success("{}");
    }

    ::FrontierGateStartResp resp{};

    try
    {
        // frogate_num only has to be stable for the life of the run and unique
        // enough that a resumed run is not confused with the previous one, so
        // it advances by one each time this user starts a gate.
        const auto existing = co_await db::DatabaseInterface::read(
            theDb(),
            "user_frontier_gate_run",
            {
                db::Data("frogate_num"),
                db::Lookup("user_id", identity.userId),
            });

        const int32_t runNum =
            existing.affected > 0 ? existing.front<int32_t>("frogate_num") + 1 : 1;

        // One run per user: starting a gate replaces whatever was in progress.
        co_await db::DatabaseInterface::upsert(
            theDb(),
            "user_frontier_gate_run",
            {
                db::Data("user_id", identity.userId),
                db::Data("frogate_id", gateId),
                db::Data("frogate_num", runNum),
                db::Data("mission_status", 0),
                db::Data("sel_support_id", supportId),
                // Persist the party so Continue and Retry -- which do not
                // resend the units -- can hand it back.  Without this the next
                // floor came up with an empty FrontierGateUserUnitInfoList and
                // the party vanished.
                db::Data("party_deck_json", glz::write_json(req.party_deck).value_or("[]")),
                db::Data("party_units_json", glz::write_json(req.units).value_or("[]")),
            },
            /*conflict*/ {"user_id"});

        resp.num.frogate_num = runNum;
        resp.num.frogate_id = gateId;
        resp.num.sel_support_id = supportId;
        // // UNVERIFIED: the j3g5P4cq status enum is not decoded, so a freshly
        // opened run reports 0.  If the FG screen misreads the run's phase,
        // this is the first value to question.
        resp.num.mission_status = 0;
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_ERROR << "FrontierGateStart: run upsert failed: " << ex.base().what();
        co_return HandleResult::success("{}");
    }

    resp.num.power_up_rate_hp = kNeutralPowerUpRate;
    resp.num.power_up_rate_atk = kNeutralPowerUpRate;
    resp.num.power_up_rate_def = kNeutralPowerUpRate;

    // ECHO THE PARTY BACK — this is what makes the battle loadable.
    //
    // MissionStartScene::downloadFiles branches on GameUtils::isFrontierGate
    // and, in Frontier Gate mode, builds its asset download set from
    // FrontierGateUserUnitInfoList (52xDBRGr) rather than from the player's
    // normal unit list.  With that list empty the client requests no unit
    // files, and the battle then runs with units whose assets were never
    // fetched — the mission downloaded its shared assets and crashed.
    //
    // The client hands us both groups in this request, so echoing them is both
    // the fix and the resume mechanism the suspended-run flow needs later.
    // Nothing is persisted yet: tier 3 adds the deck rows, and until then a
    // resumed run rebuilds from whatever the client last sent.
    resp.party_deck = std::move(req.party_deck);
    resp.units = std::move(req.units);

    LOG_INFO << "FrontierGateStart: echoing " << resp.party_deck.size()
             << " deck member(s) and " << resp.units.size() << " unit(s)";

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
