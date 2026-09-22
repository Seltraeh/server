#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/DailyTask.hpp>

// DailyTaskUserInfo (m7g0Ekb5 / Hd8c3Y6) — the Brave Points & Rewards screen.
//
// Drives three tabs, all reached from the same request:
//   Today's Tasks     — rotating daily objectives ("Achieve 3 Victories in The
//                       Arena", "Complete 1 Mission within the Vortex",
//                       "Craft 5 Items/Spheres"), each worth 20 BP.
//   Milestone Rewards — cumulative BP thresholds paying out Brave Summon
//                       Tickets, Amber Butterfly, Omni Frog, ... at 90000,
//                       92500, 95000, 97500, 100000 BP.
//   Redeem Prizes     — a BP shop on an expiry timer (Summon Ticket 1500BP,
//                       Occult Treasure 750BP, Star of Hope 750BP), the slot
//                       the live game used for its rotating advent-calendar and
//                       event giveaways — daily currency, items, Vortex dungeon
//                       keys, whatever was being promoted that week.
//
// ── The AES key is 7 characters ──────────────────────────────────────────
// `Hd8c3Y6`.  Every other key in this server is 8 or 16 chars, and that
// assumption nearly caused this one to be discarded as a misread.  It is
// correct — CONFIRMED empirically: the request decrypts and the body is
// well-formed.  Do not "fix" it.
//
// It also does not live in the aligned handler table at 0x12AB000-0x12DD000
// (`<GroupId>\0\0\0\0<AesKey>\0\0\0\0`, 12-byte stride) where every other pair
// sits.  It is in a denser unaligned string pool at file offset 0x12DB328,
// among the DAILYTASK_* literals.  So a GroupId missing from the main table is
// not necessarily absent — check the dense pools too.
//
// ── Request shape (confirmed against a live capture) ─────────────────────
// Bare identity request, matching DailyTaskUserInfoRequest::createBody:
//     {"IKqx1Cn9":[{...}], "6FrKacq7":[{...}], "KeC10fuL":[...168 MST vers...]}
// No feature-specific group at all, so the only thing to read is the caller.
//
// ── Per-user state rides ON the task rows ────────────────────────────────
// An earlier note here said the BP totals lived in 6C0kzwM5
// UserBraveMedalInfoResponse.  They do not — that block is the Brave MEDAL
// item (medal_id / possession).  DailyTaskMst carries all three counters
// itself: `brave_points` (22rqpZTo, spendable), `brave_points_total`
// (bya9a67k, the lifetime figure the milestones are gated on) and
// `times_completed` (9cKyb15U, this task's progress).  The two totals are
// repeated on every task row; that is where the screen's headers read them.
//
// ⚠ NOTHING REPORTS PROGRESS.  The client ships exactly two daily-task
// requests -- this one and DailyTaskClaimReward -- so the server counts the
// six task codes itself from handlers that already know the event happened.
// See gme::advanceDailyTask.
HANDLEF(DailyTaskUserInfo)
{
	(void)session;

	DailyTaskUserInfoReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "DailyTaskUserInfo: parse error: " << glz::format_error(ec, json);
	}

	// Re-send the three daily-task tables.  Initialize already sends them at
	// boot, but the screen empties the lists it is about to receive, so
	// answering {} here WIPED the tiles and they never returned — see the
	// struct doc in achievement.kdl.
	const auto& init = theServer()->cache().initializeResp();

	DailyTaskUserInfoResp resp = {};
	resp.signal_key.key       = "5EdKHavF";
	resp.daily_task_bonuses   = init.daily_task_bonuses;
	resp.daily_task_prizes    = init.daily_task_prizes;
	resp.daily_tasks          = init.daily_tasks;

	// Per-user state: today's rotation with its progress, the prize catalogue
	// with this player's claim counts, and the two BP counters stamped onto
	// every task row (which is where the screen's headers read them from).
	// Falls back to the bare catalogue above if the caller cannot be resolved,
	// so the screen still renders for the tutorial/new-user flow.
	if (const auto identity = co_await gme::getUserIdentity(theDb(), req.login_info);
		!identity.data.userId.empty())
	{
		co_await gme::fillDailyTaskTables(
			theDb(), identity.data, resp.daily_tasks, resp.daily_task_prizes);
	}

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		const auto& glze = glz::format_error(ec, buffer);
		LOG_DEBUG << "Gme DailyTaskUserInfo Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}
