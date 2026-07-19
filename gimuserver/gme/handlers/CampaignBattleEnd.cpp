#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/archive/MissionArchiver.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <algorithm>
#include <chrono>
#include <optional>

// CampaignBattleEnd (pTNB6yw3) — post-battle result handler.
// Marks the mission cleared, credits the cleared mission's zel + karma from the
// mission archive, and returns a fresh UserTeamInfo so the client HUD updates.
//
// Response keys:
//   "fEi17cnx" — [UserTeamInfo]    — refreshes zel / energy in HUD
//   "4MCxgS5p" — CampaignReceiptResponse (stub payload — enough to unblock
//                the receipt screen; real reward logic is future work)
//
// DB writes:
//   1. UPDATE user_campaign_missions SET state=2, clear_count+=1,
//             attain_percent=100, last_cleared_at=<epoch>
//      WHERE user_id=$1 AND mission_id=<active_mission_id>
//   2. UPDATE user_info SET zel = zel + <reward>
//   3. UPDATE user_campaign_state SET active_mission_id=''

// Client overflows zel/karma above this and resets to 0, so cap every credit.
static constexpr int64_t kMaxZelKarma = 99'999'999LL;

// CampaignBattleEndReq (login_info + mission_id) is generated from the KDL
// (packet-generator/assets/net/handlers.kdl).

// Response: CampaignReceiptResp (team_info under fEi17cnx + receipt stub under
// 4MCxgS5p) is generated from the KDL
// (packet-generator/assets/net/handlers.kdl) and shared with CampaignReceipt.

HANDLEF(CampaignBattleEnd)
{
    LOG_INFO << "CampaignBattleEnd: " << json;

    // Parse — lenient so extra envelope keys don't abort.
    CampaignBattleEndReq req{};
    glz::context ctx{};
    if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
    {
        LOG_WARN << "CampaignBattleEnd: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const std::string kUserId = identity.userId;

    // Read active_mission_id from state table if not in the request body.
    std::string missionId = req.mission_id;
    if (missionId.empty())
    {
        try
        {
            const auto sr = co_await theDb()->execSqlCoro(
                "SELECT active_mission_id FROM user_campaign_state WHERE user_id=$1;",
                std::string(kUserId));
            if (!sr.empty())
                missionId = sr[0]["active_mission_id"].as<std::string>();
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "CampaignBattleEnd: state SELECT failed: " << ex.base().what();
        }
    }

    // Is this a Grand Mission?  (F_GRAND_MISSION_MST via ServerCache.)  The
    // handler also used to see quest-shaped ids while the campaign flow was
    // being brought up, so behaviour is keyed off MST membership.
    const auto& gmMst = theServer()->cache().grandMissionMst();
    int32_t curId = -1;
    try { curId = std::stoi(missionId); } catch (const std::exception&) {}
    const bool isGrandMission = std::any_of(gmMst.begin(), gmMst.end(),
        [curId](const auto& m) { return m.mission_id == curId; });

    // Per-run zel/karma come from the mission archive when a record exists.
    // Grand Missions have no archive records (their meaningful rewards are
    // claimed through CampaignReceipt from F_GRAND_MISSION_REWARD_MST), so a
    // missing record is only an error for non-Grand missions.
    std::optional<MissionRecord> missionRecord;
    try
    {
        missionRecord = MissionArchiver::instance().lookup(
            static_cast<uint32_t>(std::stoul(missionId)));
    }
    catch (const std::exception&)
    {
        // std::stoul throws on an empty / non-numeric mission id.
    }
    if (!missionRecord && !isGrandMission)
    {
        co_return HandleResult::error("Archive error",
            "CampaignBattleEnd: no mission archive record for mission '" + missionId + "'");
    }

    // Step 1: mark mission cleared.
    if (!missionId.empty())
    {
        try
        {
            const int64_t now = static_cast<int64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

            // NOTE: sqlite indexes $N params by order of FIRST APPEARANCE and
            // drogon binds positionally — $N must appear in 1,2,3… order or
            // the wrong values bind silently (this exact UPDATE previously
            // used $3 first and matched zero rows).
            co_await theDb()->execSqlCoro(
                "UPDATE user_campaign_missions"
                " SET state=2, attain_percent=100,"
                "     clear_count = clear_count + 1,"
                "     last_cleared_at = $1"
                " WHERE user_id=$2 AND mission_id=$3;",
                now, std::string(kUserId), missionId);
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "CampaignBattleEnd: mission UPDATE failed: " << ex.base().what();
        }

        // Step 1b: unlock the next Grand Mission, if any.  This keeps the
        // player on a one-mission-at-a-time progression: only the missions
        // they've earned (cleared one earlier) become available.
        //
        // The successor comes from F_GRAND_MISSION_MST, NOT curId+1 — the real
        // id sequence has gaps (…5000016 → 5008001 → … → 5008003 → 5008008 →
        // … → 5200000 → 5200002), so arithmetic succession both inserts
        // phantom rows and strands the player at every gap.
        //
        // INSERT OR IGNORE means we never downgrade a mission that's already
        // available or cleared; we only add brand-new rows.
        if (isGrandMission)
        {
            int32_t nextId = -1;
            for (const auto& m : gmMst)
                if (m.mission_id > curId && (nextId < 0 || m.mission_id < nextId))
                    nextId = m.mission_id;

            if (nextId >= 0)
            {
                try
                {
                    co_await theDb()->execSqlCoro(
                        "INSERT OR IGNORE INTO user_campaign_missions"
                        " (user_id, mission_id, state, attain_percent)"
                        " VALUES ($1, $2, 1, 0);",
                        std::string(kUserId), std::to_string(nextId));

                    LOG_INFO << "CampaignBattleEnd: cleared mission " << missionId
                             << " — unlocked next mission " << nextId;
                }
                catch (const drogon::orm::DrogonDbException& ex)
                {
                    LOG_WARN << "CampaignBattleEnd: next-mission unlock failed: "
                             << ex.base().what();
                }
            }
            else
            {
                LOG_INFO << "CampaignBattleEnd: cleared final Grand Mission " << missionId;
            }
        }
        else
        {
            LOG_WARN << "CampaignBattleEnd: mission " << missionId
                     << " not in F_GRAND_MISSION_MST — successor unlock skipped";
        }
    }

    // Step 2: credit the cleared mission's zel + karma from the archive record.
    // Grand Missions without an archive record credit nothing here — their
    // rewards come from the receipt claim.
    if (missionRecord)
    {
        try
        {
            // $N in strict first-appearance order (sqlite named-param gotcha).
            co_await theDb()->execSqlCoro(
                "UPDATE user_info"
                " SET zel = MIN(zel + $1, $2),"
                "     karma = MIN(karma + $3, $4)"
                " WHERE id=$5;",
                static_cast<int64_t>(missionRecord->zel), kMaxZelKarma,
                static_cast<int64_t>(missionRecord->karma), kMaxZelKarma,
                std::string(kUserId));
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "CampaignBattleEnd: reward UPDATE failed: " << ex.base().what();
        }
    }

    // Step 3: clear active mission state.
    try
    {
        co_await theDb()->execSqlCoro(
            "UPDATE user_campaign_state SET active_mission_id='', active_battle_seed=0"
            " WHERE user_id=$1;",
            std::string(kUserId));
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "CampaignBattleEnd: state clear failed: " << ex.base().what();
    }

    // Refreshed team_info (fEi17cnx) + receipt stub (4MCxgS5p) in one pass.
    CampaignReceiptResp resp{};
    resp.team_info = std::move(
        (co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
