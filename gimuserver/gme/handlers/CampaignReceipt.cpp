#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>

// CampaignReceipt (5Imq3wC0) — claims the rewards for a cleared Grand Mission.
// Rewards come from F_GRAND_MISSION_REWARD_MST (ServerCache
// grandMissionRewardMst()), replacing the old fixed 1000-zel/200-karma stub.
//
// Reward-row selection (evidence: deploy/mst/grand_mission_reward_mst.json,
// semantics pinned via the labelled daily_task_prize_mst rows that share the
// present_type hash 30Kw4WBa — see mst/grand_mission.kdl):
//   reward_type=1 "N% Completion Bonus"    — conditions = percent threshold;
//                                            granted when attain_percent >= it
//   reward_type=2 "Acquired Treasure" etc. — conditions = flag id; belongs to
//                                            the in-mission flag/treasure flow
//                                            (CampaignSave — not wired), so the
//                                            receipt never grants these
//   reward_type=4 "First Quest Clear Bonus" — granted on the first claim
//
// present_type dispatch:
//   3 = zel (unused under rt 1/4 today), 8 = gem, 6 = unit (addUserUnit),
//   4/5/7 = item/material/sphere (all resolve in item_mst -> addUserItem).
//   Anything else is logged and skipped.
//
// Claims are one-shot: rewards are granted only when the mission row flips
// reward_claimed 0 -> 1.  Repeat receipts return team_info with no grants.
//
// Response:
//   "fEi17cnx" — [UserTeamInfo]          — refreshes HUD zel/karma/exp
//   "4MCxgS5p" — { "pCIRMw04": "" }      — receipt payload (stub)

// Client currency display caps (handbook §10) — exceeding them overflows the
// HUD counter to zero.
static constexpr int64_t kMaxZelKarma = 99'999'999LL;
static constexpr int64_t kMaxGems     = 9'999LL;

// CampaignReceiptReq (login_info + mission_id) is generated from the KDL
// (packet-generator/assets/net/handlers.kdl).

// CampaignReceiptResp (fEi17cnx team_info + 4MCxgS5p receipt stub) is generated
// from the KDL (packet-generator/assets/net/handlers.kdl) and shared with
// CampaignBattleEnd.

HANDLEF(CampaignReceipt)
{
    LOG_INFO << "CampaignReceipt: " << json;

    CampaignReceiptReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "CampaignReceipt: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const std::string kUserId = identity.userId;

    int32_t missionId = -1;
    try { missionId = std::stoi(req.mission_id); } catch (const std::exception&) {}

    // Claim gate: the mission must be cleared and not yet claimed.  The flip
    // to reward_claimed=1 happens before granting so a mid-grant failure can
    // not be replayed into duplicates.
    bool grantRewards = false;
    int32_t attainPercent = 0;
    if (missionId >= 0)
    {
        try
        {
            const auto rows = co_await theDb()->execSqlCoro(
                "SELECT state, attain_percent, reward_claimed"
                " FROM user_campaign_missions WHERE user_id=$1 AND mission_id=$2;",
                std::string(kUserId), req.mission_id);
            if (!rows.empty()
                && rows[0]["state"].as<int32_t>() >= 2
                && rows[0]["reward_claimed"].as<int32_t>() == 0)
            {
                attainPercent = rows[0]["attain_percent"].as<int32_t>();
                grantRewards  = true;
            }
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "CampaignReceipt: mission SELECT failed: " << ex.base().what();
        }
    }

    if (grantRewards)
    {
        try
        {
            co_await theDb()->execSqlCoro(
                "UPDATE user_campaign_missions SET reward_claimed=1"
                " WHERE user_id=$1 AND mission_id=$2;",
                std::string(kUserId), req.mission_id);
        }
        catch (const drogon::orm::DrogonDbException& ex)
        {
            LOG_WARN << "CampaignReceipt: claim UPDATE failed: " << ex.base().what();
            grantRewards = false;
        }
    }

    if (grantRewards)
    {
        const auto& rewardMst = theServer()->cache().grandMissionRewardMst();
        const auto& unitMst   = theServer()->cache().unitMst();

        int64_t zel = 0, gems = 0;
        for (const auto& rw : rewardMst)
        {
            if (rw.mission_id != missionId)
                continue;

            bool due = false;
            if (rw.reward_type == 4)
            {
                due = true;  // first-clear bonus
            }
            else if (rw.reward_type == 1)
            {
                // conditions = percent threshold (plain int; empty or
                // unparsable is treated as not due).
                try { due = attainPercent >= std::stoi(rw.conditions); }
                catch (const std::exception&) {}
            }
            if (!due)
                continue;

            switch (rw.present_type)
            {
            case 3:
                zel += rw.target_cnt;
                break;
            case 8:
                gems += rw.target_cnt;
                break;
            case 6:  // unit
            {
                const auto unit = std::find_if(unitMst.begin(), unitMst.end(),
                    [&rw](const auto& u) { return u.id == rw.target_id; });
                if (unit == unitMst.end())
                {
                    LOG_WARN << "CampaignReceipt: reward unit " << rw.target_id
                             << " not in unit_mst — skipped";
                    break;
                }
                for (int32_t i = 0; i < rw.target_cnt; ++i)
                    co_await gme::addUserUnit(theDb(), identity, *unit);
                LOG_INFO << "CampaignReceipt: granted unit " << rw.target_id
                         << " x" << rw.target_cnt << " (" << rw.name << ")";
                break;
            }
            case 4:
            case 5:
            case 7:  // item / material / sphere — all item_mst domains
                co_await gme::addUserItem(theDb(), identity,
                    static_cast<uint32_t>(rw.target_id),
                    static_cast<uint32_t>(rw.target_cnt));
                LOG_INFO << "CampaignReceipt: granted item " << rw.target_id
                         << " x" << rw.target_cnt << " (" << rw.name << ")";
                break;
            default:
                LOG_WARN << "CampaignReceipt: present_type " << rw.present_type
                         << " (target " << rw.target_id << " x" << rw.target_cnt
                         << ") not supported — skipped";
                break;
            }
        }

        if (zel > 0 || gems > 0)
        {
            try
            {
                // $N in strict first-appearance order (sqlite named-param
                // gotcha — see CampaignBattleEnd).
                co_await theDb()->execSqlCoro(
                    "UPDATE user_info SET"
                    " zel  = MIN(zel  + $1, $2),"
                    " gems = MIN(gems + $3, $4)"
                    " WHERE id=$5;",
                    zel, kMaxZelKarma, gems, kMaxGems, std::string(kUserId));
                LOG_INFO << "CampaignReceipt: credited zel +" << zel
                         << ", gems +" << gems;
            }
            catch (const drogon::orm::DrogonDbException& ex)
            {
                LOG_WARN << "CampaignReceipt: currency UPDATE failed: " << ex.base().what();
            }
        }
    }
    else
    {
        LOG_INFO << "CampaignReceipt: no grants for mission '" << req.mission_id
                 << "' (not cleared, already claimed, or unknown id)";
    }

    // Fetch fresh user_info so the HUD updates (team_info under fEi17cnx) and
    // return it alongside the receipt stub (4MCxgS5p) in one serialization.
    CampaignReceiptResp resp{};
    resp.team_info = std::move(
        (co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
