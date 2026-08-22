#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/BraveSlots.hpp>
#include <gimuserver/gme/common/DailySpin.hpp>

HANDLEF(Initialize)
{
	InitializeReq req = {};
	const auto& ec = glz::read_json(req, json);
	if (ec)
	{
		const auto& fmte = glz::format_error(ec, json);
		LOG_DEBUG << "Gme Initialize Error during JSON read: " << fmte;
		co_return HandleResult::error("Deserialization error", fmte);
	}

	// NOTE: A real server would verify the gumi token first...
	// TODO: Handle MSTs to answer

	// Copy the cached response and build on top of it.
	InitializeResp resp = theServer()->cache().initializeResp();

	// This is something to do with account transfer, ignore for now.
	resp.login_info.account_id = "12345678";
	// Assume we are a new user until proven otherwise. The game checks if these
	// values are empty to decide whether to enter the tutorial flow or not.
	resp.login_info.handle_name = "";
	resp.login_info.user_id = "";

	// After GuestLogin, Initialize is the first encrypted GME request the client
	// sends during normal startup. It receives the Gumi Live ID returned by the
	// account login flow and decides whether the client should resume an existing
	// game user or enter the new-user/tutorial flow.
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info, true)).data;

	// If we didn't find a user for this Gumi Live ID, we just return an empty user_id
	// and let the client enter the tutorial flow. Otherwise, we return the user info
	// stored in the database.
	if (!identity.userId.empty())
	{
		resp.login_info = std::move((co_await gme::getLoginInfo(theDb(), identity)).nonEmpty());
	}

	//resp.user_info.gumi_live_token = req.user_info.gumi_live_token;
	//resp.user_info.gumi_live_userid = req.user_info.gumi_live_userid;

	resp.signal_key.key = "C7vnXA5T";

	resp.challenge_arena_user_info.user_id = "n9ZMPC0t"; // rank name?
	resp.challenge_arena_user_info.unkstr2 = "F"; // ranking?
	resp.challenge_arena_user_info.league_id = 1;

	resp.summoner_journal.user_id = identity.userId;

	// Daily Spin (the Rewards menu's task_dailyloginspin tile).
	//
	// ⚠ `user_current_count` IS "spins ALREADY USED today", and it is what
	// decides whether the HOME SCREEN force-opens the wheel.  This is not a
	// cosmetic counter — get it wrong and the player cannot reach Home at all.
	//
	//     DailyLoginRewardsUserInfo::isDailyLoginAvailable @0x1CC8278
	//       if (this->[0x24]) return false;   // one-shot per-session latch
	//       this->[0x24] = 1;
	//       return this->[0x1c] < 1;          // user_current_count < 1
	//
	// and its ONLY two callers are HomeScene2::updateEvent @0x16F2D9C and
	// AnotherHomeScene::updateEvent @0x16E5128.  Below 1 means "hasn't spun
	// today", so Home opens the wheel over itself; the wheel then only closes
	// once `used >= limit` (DailyLoginScene::updateEvent state 1 @0xE52FDC,
	// else-branch state 7 = exit to Home).  Reporting 0 against a non-zero
	// limit with no working spin handler locked the client out of Home
	// entirely — see handbook §7.14.
	//
	// It is now read from user_daily_spin and rolls over on the UTC day, so a
	// fresh day legitimately offers the wheel again and the DailyLogin
	// (4aClzokO) handler is what closes it.
	//
	// Guarded on a resolved user: this reply also serves the new-user/tutorial
	// flow, where userId is deliberately empty, and loading there would insert
	// a spin row keyed on "".  With no user we report the day as already spun,
	// which is the one value that cannot trap anybody in the wheel.
	resp.daily_login_rewards.id = gme::dailySpinAnchor(1);
	resp.daily_login_rewards.current_day = 1;
	resp.daily_login_rewards.user_current_count = gme::kDailySpinLimit;
	resp.daily_login_rewards.user_spin_limit_count = gme::kDailySpinLimit;
	resp.daily_login_rewards.next_reward_id = gme::dailySpinAnchor(2);
	resp.daily_login_rewards.message = " day(s) more to guaranteed Gem!";

	if (!identity.userId.empty())
	{
		// The machine's only medal source on this server — Raid Battle, the
		// live one, does not exist here.  Sits beside the daily spin because
		// both roll over on the same UTC clock.
		co_await gme::grantDailyBraveMedals(theDb(), identity);

		const auto spin = co_await gme::loadDailySpin(theDb(), identity);
		// ⚠ These two are REWARD IDS, not day numbers.  The client groups the
		// wheel by the row this id belongs to, so a day number here draws a
		// different day's prizes than the spin scores against — sending 7 drew
		// day 2's wheel (200,000 Karma and all) while the spin ran on day 7.
		resp.daily_login_rewards.id = gme::dailySpinAnchor(spin.spinDay);
		resp.daily_login_rewards.current_day = spin.spinDay;
		resp.daily_login_rewards.user_current_count = spin.spinsUsed;
		resp.daily_login_rewards.next_reward_id = gme::dailySpinAnchor(spin.spinDay + 1);
		// Distance to the next guaranteed Gem — the live game gave one on the
		// first spin of days 7 / 14 / 21 / 28.  Prepended to the message by the
		// client, giving "N day(s) more to guaranteed Gem!".
		resp.daily_login_rewards.remaining_days_till_guaranteed_reward =
			(7 - (spin.spinDay % 7)) % 7;
	}

	std::string buffer{};
	const auto& ec2 = glz::write_json(resp, buffer);
	if (ec2)
	{
		const auto& glze = glz::format_error(ec2, buffer);
		LOG_DEBUG << "Gme Initialize Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}

