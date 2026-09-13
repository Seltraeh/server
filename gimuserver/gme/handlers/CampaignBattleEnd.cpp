#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Campaign.hpp>

// CampaignBattleEnd (pTNB6yw3) — one battle of a Grand Quest run is over.
//
// The run is NOT over: a Grand Mission is a map of spots and battles, and it
// ends with CampaignEnd (cleared, failed or abandoned).  This handler used to
// mark the mission cleared and unlock the next one after every battle — and,
// because the real request carries no mission id (see CampaignBattleEndReq in
// net/handlers.kdl), it answered "Archive error" to the client, ending the
// session after the first battle.
//
// What a battle does report is the run's archive (wVTBA6b5): the zel and karma
// picked up so far, cumulative for the run.  The latest totals are kept and a
// clearing CampaignEnd pays them out (CampaignRewardSummary).
HANDLEF(CampaignBattleEnd)
{
	LOG_INFO << "CampaignBattleEnd: " << json;

	CampaignBattleEndReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "CampaignBattleEnd: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	if (!req.mission_id.empty())
		co_await gme::openCampaignRun(theDb(), identity, req.mission_id);

	if (!req.archive.empty())
	{
		const auto& archive = req.archive.front();
		co_await gme::ensureCampaignState(theDb(), identity);
		co_await theDb()->execSqlCoro(
			"UPDATE user_campaign_state SET run_zel = $1, run_karma = $2, run_open = 1 WHERE user_id = $3;",
			static_cast<int64_t>(std::max(archive.get_zel, 0)),
			static_cast<int64_t>(std::max(archive.get_karma, 0)),
			identity.userId);
		LOG_INFO << "CampaignBattleEnd: run so far " << archive.get_zel << " zel, "
			<< archive.get_karma << " karma";
	}

	// The battle moved the party, so the new spot is banked here too — the
	// client reports it on every save point, not only on suspend.
	if (req.deck_pos)
		co_await gme::storeCampaignDeckPos(theDb(), identity, *req.deck_pos);

	CampaignBattleEndResp resp{};
	resp.team_info = std::move((co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());
	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
