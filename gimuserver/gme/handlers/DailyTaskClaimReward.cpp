#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/DailyTask.hpp>

// DailyTaskClaimReward (oP3bn47e / ut0j9h3K) — the Claim button on the Brave
// Points screen's Milestone Rewards and Redeem Prizes tabs.
//
// ⚠ THIS WAS UNREGISTERED UNTIL 2026-09-20 while both tabs were reachable, so
// tapping Claim closed the session: an unregistered GroupId and a handler
// error produce the same GmeErrorCommand::Close, and neither writes an
// http_log line, which is exactly the "looks like a crash but is not one"
// signature in the unsupported-requests note.
//
// The pair did not come from the aligned handler table at 0x12AB000 either.
// `DailyTaskClaimRewardRequest::getRequestID` @0xF85F70 and `getEncodeKey`
// @0xF85F7C return them directly, and `createBody` @0xF85F88 shows the body is
// a single param: {"a739yK18":[{"d83aQ39U": <prize id>}]} — keyed on the PRIZE
// TABLE's own hash rather than a request-specific one.
//
// ⚠ ONE SETTER MEANS THE SERVER DECIDES EVERYTHING ELSE.  `setPrizeId` is the
// request's only input, so whether this is a milestone (gated on the lifetime
// BP total, claimed once, spending nothing) or a shop purchase (spending the
// available balance, repeatable to max_claim_count) is read off the MST row —
// see gme::claimDailyTaskPrize.
HANDLEF(DailyTaskClaimReward)
{
	(void)session;

	DailyTaskClaimRewardReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
		{
			const auto error = glz::format_error(ec, json);
			LOG_ERROR << "DailyTaskClaimReward deserialization failed: " << error;
			co_return HandleResult::error("Deserialization error", error);
		}
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	DailyTaskClaimRewardResp resp{};
	resp.signal_key = req.signal_key;
	resp.daily_task_bonuses = theServer()->cache().initializeResp().daily_task_bonuses;

	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// The debit, the claim tick and the present must land together or a
			// failure part-way leaves a paid-for prize that never arrives.
			const auto result = co_await gme::claimDailyTaskPrize(
				transaction, identity, req.claim.prize_id);

			switch (result)
			{
			case gme::DailyTaskClaimResult::Ok:
				break;
			case gme::DailyTaskClaimResult::UnknownPrize:
				transaction->rollback();
				co_return HandleResult::error("Invalid claim", "no such prize");
			case gme::DailyTaskClaimResult::NotEnoughPoints:
				transaction->rollback();
				co_return HandleResult::error("Invalid claim", "not enough Brave Points");
			case gme::DailyTaskClaimResult::AlreadyClaimed:
				transaction->rollback();
				co_return HandleResult::error("Invalid claim", "that prize is already claimed");
			}

			// Re-send the catalogue in the SAME transaction that changed it, so
			// the counters the screen redraws are the ones just written.
			co_await gme::fillDailyTaskTables(
				transaction, identity, resp.daily_tasks, resp.daily_task_prizes);
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	std::string buffer{};
	if (const auto ec = glz::write_json(resp, buffer); ec)
	{
		const auto error = glz::format_error(ec, buffer);
		LOG_ERROR << "DailyTaskClaimReward serialization failed: " << error;
		co_return HandleResult::error("Serialization error", error);
	}

	co_return HandleResult::success(buffer);
}
