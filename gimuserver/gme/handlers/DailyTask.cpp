#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>

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
// No feature-specific group at all, so there is nothing to parse.
//
// ── Response: currently EMPTY, and the screen renders ────────────────────
// Returning {} leaves the client showing the MST-driven catalog with zeroed
// per-user state — BP Total 0 / Current 0, every task at [0/N], every prize
// "Short of BP".  That is a truthful rendering of an account with no BP, not a
// broken one, so an empty response is a legitimate resting point.
//
// To make it live, the per-user state goes in these:
//     6C0kzwM5  UserBraveMedalInfoResponse   3 params  (the BP totals)
// and the catalog comes from three MSTs the binary already names:
//     k23D7d43  DailyTaskMstResponse
//     a739yK18  DailyTaskPrizeMstResponse
//     p283g07d  DailyTaskBonusMstResponse
// `packet-generator/assets/mst/daily_task.kdl` already exists and carries a
// present_type field on the same 30Kw4WBa hash CampaignReceipt and the present
// box dispatch on — so BP payouts can reuse that switch rather than inventing
// a fourth reward vocabulary.  None of the three MST classes has readParam
// setter names exported yet, so audit before populating (§3.4).
HANDLEF(DailyTaskUserInfo)
{
	(void)session;

	LOG_INFO << "DailyTaskUserInfo (m7g0Ekb5): " << json;

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

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		const auto& glze = glz::format_error(ec, buffer);
		LOG_DEBUG << "Gme DailyTaskUserInfo Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}
