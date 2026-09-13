#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Campaign.hpp>

// CampaignMissionGet (RSm6p2d4) — returns per-mission progress/state.
// Response is the CampaignMissionInfoResponse shape (group "2I9V0o6J"):
//   [{ "j28VNcUW": <MissionID>,
//      "HUo4T7i8": "<AttainPercent>",
//      "JcKMjH64": "<MissionOnFlg>",
//      "j0Uszek2": "<State>" }, …]
// CampaignMissionEntry + CampaignMissionGetResp are generated from the KDL
// (packet-generator/assets/net/handlers.kdl).  CampaignMissionEntry is shared
// with CampaignStart.

HANDLEF(CampaignMissionGet)
{
    LOG_INFO << "CampaignMissionGet: " << json;

    CampaignMissionGetReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "CampaignMissionGet: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    CampaignMissionGetResp resp{};

    // Grand Missions only, with the rewards already earned (get_reward) —
    // see gme::loadCampaignMissions.
    try
    {
        resp.missions = co_await gme::loadCampaignMissions(theDb(), identity);
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "CampaignMissionGet: SELECT failed: " << ex.base().what();
        co_return HandleResult::success("{}");
    }

    // The Grand Quest master tables.  They belong on THIS request rather than
    // on CampaignStart: the mission-select screen resolves its
    // CampaignMissionMst* when the quest is tapped, which is before the run
    // starts, and a null pointer there costs the field its whole map.  See
    // CampaignMissionGetResp in net/handlers.kdl.
    gme::fillCampaignMst(resp);

    LOG_INFO << "CampaignMissionGet: " << resp.missions.size() << " mission(s) and the "
             << resp.spot_mst.size() << " map spots behind them";

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
