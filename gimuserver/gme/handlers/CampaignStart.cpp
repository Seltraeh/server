#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

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
//   "p04iC2wr" — CampaignRewardBonusInfoResponse (empty OK)

// The response structs are generated from the KDL
// (packet-generator/assets/net/handlers.kdl): CampaignStartResp and its
// entries CampaignMissionEntry (2I9V0o6J, shared with CampaignMissionGet),
// CampaignStartDeckEntry (hT95cq8K), CampaignStartMissionDeckEntry (Yusr3Zg5),
// CampaignStartEventInfo (RseDpY04), and CampaignEmptyEntry (the always-empty
// NjZ6ds1S / 5EByfWJ4 / p04iC2wr slots).

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

    // The Grand Mission catalog (F_GRAND_MISSION_MST via ServerCache) scopes
    // this handler: user_campaign_missions doubles as the quest clear-history
    // store (MissionEnd writes quest ids there for the UT1SVg59 progression
    // list), and those quest rows must not leak into the Grand Mission select
    // screen.  Only rows whose mission_id exists in the MST are emitted.
    const auto& gmMst = theServer()->cache().grandMissionMst();
    std::set<std::string> gmIds;
    for (const auto& m : gmMst)
        gmIds.insert(std::to_string(m.mission_id));

    // Load mission progress (MST-scoped).
    bool haveGmRow = false;
    try
    {
        const auto rows = co_await theDb()->execSqlCoro(
            "SELECT mission_id, attain_percent, state"
            " FROM user_campaign_missions WHERE user_id=$1;",
            std::string(kUserId));

        resp.missions.reserve(rows.size());
        for (const auto& r : rows)
        {
            CampaignMissionEntry e{};
            e.mission_id     = r["mission_id"].as<std::string>();
            if (!gmIds.contains(e.mission_id))
                continue;
            e.attain_percent = r["attain_percent"].as<int32_t>();
            e.state          = r["state"].as<int32_t>();
            e.mission_on_flg = (e.state >= 1) ? "1" : "0";
            resp.missions.emplace_back(std::move(e));
            haveGmRow = true;
        }
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "CampaignStart: mission SELECT failed: " << ex.base().what();
    }

    // First entry: seed the lowest-id Grand Mission as available so a fresh
    // user has somewhere to start.  CampaignBattleEnd unlocks the successors
    // one clear at a time.
    if (!haveGmRow && !gmIds.empty())
    {
        const std::string firstId = *gmIds.begin();  // lexicographic == numeric: all ids are 7 digits
        try
        {
            co_await theDb()->execSqlCoro(
                "INSERT OR IGNORE INTO user_campaign_missions"
                " (user_id, mission_id, state, attain_percent)"
                " VALUES ($1, $2, 1, 0);",
                std::string(kUserId), firstId);

            CampaignMissionEntry e{};
            e.mission_id     = firstId;
            e.attain_percent = 0;
            e.state          = 1;
            e.mission_on_flg = "1";
            resp.missions.emplace_back(std::move(e));
            LOG_INFO << "CampaignStart: seeded first Grand Mission " << firstId
                     << " for user " << kUserId;
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "CampaignStart: seed INSERT failed: " << ex.base().what();
        }
    }

    // Load campaign party decks.  If user_campaign_decks is empty (not yet
    // edited), fall back to the regular user_decks so the deck panel
    // isn't blank on first launch.
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
            e.deck_num    = r["deck_num"].as<int32_t>();
            e.member_type = r["member_type"].as<int32_t>();
            resp.party_decks.emplace_back(std::move(e));
        }
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "CampaignStart: deck SELECT failed: " << ex.base().what();
    }

    // Populate mission_decks (Yusr3Zg5) — one entry per deck slot (0-9) so
    // the campaign deck-selector panel has something to render.
    for (int i = 0; i < 10; ++i)
    {
        CampaignStartMissionDeckEntry md{};
        md.deck_num      = i;
        md.now_point_num = 0;
        resp.mission_decks.emplace_back(md);
    }

    // Reward-bonus list (p04iC2wr) — the "what you can earn" panel.  Emitted
    // for the missions this user can actually see, so the payload tracks the
    // mission list above instead of shipping all 371 MST rows.
    //
    // The wire shape is CampaignRewardBonusInfoResponse, whose six setters
    // (RewardID/PresentType/TargetID/TargetCnt/TargetParam/RewardType) are the
    // client-facing subset of GrandMissionRewardMst — same six hashes, so this
    // is a straight projection with no invented fields.
    {
        std::set<int32_t> visible;
        for (const auto& m : resp.missions)
        {
            try { visible.insert(std::stoi(m.mission_id)); }
            catch (const std::exception&) {}
        }

        const auto& rewardMst = theServer()->cache().grandMissionRewardMst();
        for (const auto& rw : rewardMst)
        {
            if (!visible.contains(rw.mission_id))
                continue;

            CampaignRewardBonusEntry e{};
            e.reward_id    = std::to_string(rw.id);
            e.present_type = rw.present_type;
            e.target_id    = std::to_string(rw.target_id);
            e.target_cnt   = rw.target_cnt;
            e.target_param = rw.target_param;
            e.reward_type  = rw.reward_type;
            resp.reward_bonus.emplace_back(std::move(e));
        }

        LOG_INFO << "CampaignStart: " << resp.missions.size() << " mission(s), "
                 << resp.reward_bonus.size() << " reward-bonus row(s)";
    }

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
