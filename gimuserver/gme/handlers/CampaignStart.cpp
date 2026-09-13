#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Campaign.hpp>

// CampaignStart (6Y0gaPQN) — fired when the player opens the Campaign menu.
// Returns the mission catalog (state, progress), party deck list, item slots,
// and misc event flags so the UI can render the mission-select screen.
//
// Response keys (from typed_responses/*.hpp):
//   "2I9V0o6J" — CampaignMissionInfoResponse  (mission state list)
//   "hT95cq8K" — CampaignPartyDeckListResponse (party decks for select screen)
//   "NjZ6ds1S" — CampaignEqpItemInfoResponse   (equipped items — empty OK)
//   "5EByfWJ4" — CampaignRsvItemInfoResponse    (reserve items — empty OK)
//   "Yusr3Zg5" — CampaignMissionDeckInfoResponse
//   "RseDpY04" — CampaignMissionEventInfoResponse (single-object flags)
//
// p04iC2wr (the earned-bonus list) is NOT sent here — see CampaignStartResp in
// net/handlers.kdl: only the result screens read it, after CampaignEnd.

// The response structs are generated from the KDL
// (packet-generator/assets/net/handlers.kdl): CampaignStartResp and its
// entries CampaignMissionEntry (2I9V0o6J, shared with CampaignMissionGet),
// CampaignStartDeckEntry (hT95cq8K), CampaignStartMissionDeckEntry (Yusr3Zg5),
// CampaignStartEventInfo (RseDpY04), and CampaignItemEntry (the NjZ6ds1S /
// 5EByfWJ4 item loadout).

// ---------------------------------------------------------------------------
HANDLEF(CampaignStart)
{
    LOG_INFO << "CampaignStart: " << json;

    CampaignStartReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "CampaignStart: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const std::string kUserId = identity.userId;

    CampaignStartResp resp{};

    // Mission progress (Grand Missions only — see gme::loadCampaignMissions).
    try
    {
        resp.missions = co_await gme::loadCampaignMissions(theDb(), identity);
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "CampaignStart: mission SELECT failed: " << ex.base().what();
    }

    // There used to be a "seed the lowest-id Grand Mission as available" block
    // here, which wrote a state=1 row so a fresh user had somewhere to start.
    // loadCampaignMissions now returns the whole F_GRAND_MISSION_MST catalogue
    // (the client drops any quest tile whose mission has no info row), so the
    // seed row is neither needed nor wanted — it claimed a mission had been
    // started before the player touched it.

    // Campaign party decks.  If user_campaign_decks is empty (not yet
    // edited), fall back to the regular user_decks so the deck panel
    // isn't blank on first launch.  The unit id and slot go out too:
    // CampaignPartyDeckListResponse::readParam @0x1425E58 reads them, and
    // without them every party slot came back empty.
    try
    {
        auto rows = co_await theDb()->execSqlCoro(
            "SELECT deck_num, member_type, user_unit_id, disporder"
            " FROM user_campaign_decks WHERE user_id=$1 ORDER BY deck_num, disporder;",
            std::string(kUserId));

        if (rows.empty())
        {
            rows = co_await theDb()->execSqlCoro(
                "SELECT deck_num, member_type, user_unit_id, disp_order AS disporder"
                " FROM user_decks WHERE user_id=$1 ORDER BY deck_num, disp_order;",
                std::string(kUserId));
        }

        for (const auto& r : rows)
        {
            CampaignStartDeckEntry e{};
            e.deck_num     = r["deck_num"].as<int32_t>();
            e.member_type  = r["member_type"].as<int32_t>();
            e.user_unit_id = r["user_unit_id"].as<std::string>();
            e.disp_order   = r["disporder"].as<int32_t>();
            resp.party_decks.emplace_back(std::move(e));
        }
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "CampaignStart: deck SELECT failed: " << ex.base().what();
    }

    // Where each deck stands on the map (Yusr3Zg5).  This used to be ten rows
    // of zeros, which put the party marker on no spot at all; it is now what
    // the client last reported through createBodySaveDataPos, so a resumed run
    // comes back where it stopped.  See gme::loadCampaignDeckPos.
    try
    {
        resp.mission_decks = co_await gme::loadCampaignDeckPos(theDb(), identity);
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "CampaignStart: deck position SELECT failed: " << ex.base().what();
    }

    // The Grand Quest master tables again.  CampaignMissionGet is the send that
    // MATTERS (the mission pointer is resolved before a run starts, so tables
    // arriving here would be too late); this one covers a path that reaches a
    // run without the menu, and costs nothing because every block is a REPLACE.
    gme::fillCampaignMst(resp);

    // The Grand Quest item loadout (set by CampaignItemEdit, updated by
    // CampaignSave / CampaignEnd), and the run opens: a CampaignEnd pays out
    // once per opened run.
    try
    {
        co_await gme::ensureCampaignState(theDb(), identity);
        co_await theDb()->execSqlCoro(
            "UPDATE user_campaign_state SET run_open = 1 WHERE user_id = $1;", identity.userId);
        const auto state = co_await theDb()->execSqlCoro(
            "SELECT eqp_items, rsv_items FROM user_campaign_state WHERE user_id = $1;",
            identity.userId);
        if (!state.empty())
        {
            resp.eqp_items = gme::parseCampaignItems(state[0]["eqp_items"].as<std::string>());
            resp.rsv_items = gme::parseCampaignItems(state[0]["rsv_items"].as<std::string>());
        }
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "CampaignStart: state read failed: " << ex.base().what();
    }

    LOG_INFO << "CampaignStart: " << resp.missions.size() << " mission(s), "
             << resp.eqp_items.size() << "+" << resp.rsv_items.size() << " item slot(s), "
             << resp.spot_mst.size() << " map spot(s) over " << resp.map_mst.size() << " map(s)";

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
