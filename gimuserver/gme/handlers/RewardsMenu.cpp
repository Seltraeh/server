#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/DailySpin.hpp>
#include <gimuserver/gme/common/MysteryChest.hpp>

// The Rewards menu — the unbuilt tiles.
//
// `RewardsTopScene::loadMenuList` @0xE42440 builds the menu, and it has EIGHT
// tiles, not the seven §7.14 records.  Recovered with tools/bin/so_disasm.py;
// tag values are the ones stored at ListObject+0x48, and the actions come from
// the jump table at 0x230F1A5 read by `touchEnded` @0xE431E4:
//
//   tag 0  task_journal          -> scene 101802   (Summoner Journal — MISSING from §7.14)
//   tag 1  task_levelup          -> scene 101801   (Level Up Campaign)
//   tag 2  task_bravepts         -> scene 100000   (Brave Points — built, m7g0Ekb5)
//   tag 3  (no tile)             -> no-op
//   tag 4  task_presents         -> PresentTopScene, param 101800 (built, nhjvB52R)
//   tag 5  task_keys             -> scene 1302     (built, 1mr9UsYz)
//   tag 6  task_slots            -> scene 1330     (this file)
//   tag 7  task_mysterychest     -> MysteryChestScene   (this file)
//   tag 8  task_dailyloginspin   -> DailyLoginScene     (this file)
//
// Two tiles are gated by the feature-gating packet we send empty (§8.40), and
// their feature ids are new to that section's table — read from the `mov w1,#N`
// immediately before each `FeatureGatingHandler::shouldGateLocked` call:
//
//   feature id 3  = Keys        (@0xE428D4)
//   feature id 13 = Slots       (@0xE429F0)
//
// `shouldGateLocked` returns false when no row exists for an id, so with the
// packet empty both tiles render unlocked — which is why they are reachable at
// all today.
//
// EVERY GroupId AND KEY BELOW WAS READ OUT OF .rodata, not guessed: each
// `<Class>Request::getRequestID` / `::getEncodeKey` is a 12-byte function that
// returns a literal, and the key sits at GroupId+9 exactly as §4.2 describes.
//
// These three handlers are PROBES (§4.2): they log the body and answer `{}`.
// That is the whole point — an unregistered GroupId is rejected before the
// dispatcher can decrypt, so the client shows "Unsupported request" or dies
// with NO log written at all, and we learn nothing.  Registered, the next
// client launch prints the real body and identifies the payload.
//
// `{}` is a safe resting point for all three, for the same reason it is for
// Brave Points (§7.13) and achievements: each scene CLEARS the list it is
// about to receive, so an omitted list reads as empty rather than as stale.
// Confirmed for Mystery Chest — `MysteryChestScene::initConnect` @0x1E7A7EC
// calls `MysteryBoxList::removeAllObjects()` before it sends.
//
// DO NOT populate the response keys below until their readParam has been
// audited in full (§3.4, §6.20).  The keys and their param lists are recorded
// here so the audit has a starting point, NOT so they can be filled with
// 0/"" — §3.4 is explicit that a present-but-wrong value is worse than an
// absent key, because the client takes it at face value instead of falling
// back to its own default.

// Slots (vChFp73J / hm9X6BQj).
//
// Fired by `RandallSlotActionScene::touchBegan` @0x1A70DCC — i.e. on PULLING
// THE LEVER, not on opening the screen.  Slots is a native Cocos scene
// (setReelInit / initSlotgame / insertMedalAction / getReelSprite), NOT a web
// view; the `/bf/web/slots/html/index.php` string in brave_slots.json is
// `SlotgameInfo::setSlotHelpUrl`, a help link, and reading it as the screen
// itself would send this whole tile down the wrong path.
//
// Request (`SlotActionRequest::createBody` @0x13C95C8) — note both params are
// the INT overload of JsonNode::addParam, not the string one:
//   "kLz5ujP2":[{ "zS45RFGb": <slot_id, int>, "d04gRmkE": <draw_cnt, int> }]
//   plus the usual userInfo / signalKey / version tags.
// `zS45RFGb` cross-checks against deploy/system/brave_slots.json, which
// carries `"zS45RFGb": 1` — so slot_id 1 is "Brave Slots".
//
// Response: `SlotgameResultInfoResponse` = `s8r5M6wI` (9 params: yT3NBME0,
// CW1bko4F, 3WZ7KTp8, D20kuSLy, HgI9wEJ0, 3Vcj8eUn, NLbQm24T, h6smq0WE,
// A0P7psui) — all undecoded.
HANDLEF(SlotAction)
{
	LOG_INFO << "SlotAction: " << json;
	co_return HandleResult::success("{}");
}

// Mystery Chest — list (pAJ2Xesw / DaswA3rE).
//
// Fired by `MysteryChestScene::initConnect` @0x1E7A800, right after
// `MysteryBoxList::removeAllObjects()`.
//
// Request (`MysteryBoxListRequest::createBody` @0x1C6B094): bare identity —
// userInfoTag + signalKey and nothing else.  No version tag.
//
// Response: `MysteryBoxResponse` = `d0ajLeRi` (10 params: rEFRefr8, 6HEchexu,
// guHur3dr, peV7drec, PUgeth6p, swUtH8de, PUFr8swe, 4rUdradr, mejaw5Ca,
// s35idar9).  `rEFRefr8` is the box id — it is the one field the claim request
// echoes back, which is what identifies it.
HANDLEF(MysteryBoxList)
{
	LOG_INFO << "MysteryBoxList: " << json;

	MysteryBoxListReq req{};
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// The live game handed chests out through operator giveaways and events,
	// which an offline server has no equivalent of, so the archive's chests are
	// granted on first visit instead.  Idempotent — an opened chest is never
	// handed back.
	co_await gme::provisionMysteryChests(theDb(), identity);

	MysteryBoxListResp resp{};
	resp.boxes = co_await gme::listMysteryChests(theDb(), identity);

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		co_return HandleResult::error("Serialization error", glz::format_error(ec, buffer));
	}

	LOG_INFO << "MysteryBoxList: " << resp.boxes.size() << " chest(s) for " << identity.userId;
	co_return HandleResult::success(buffer);
}

// Mystery Chest — claim (2paswUpR / kadRadU5).
//
// Fired by `MysteryRewardScene::initConnect` @0x1D3CA6C.  Its constructor
// takes a std::string, which is why an xref on `...C1Ev` finds nothing — the
// symbol is `...C1ENSt6__ndk112basic_string...`.
//
// Request (`MysteryBoxClaimRequest::createBody` @0x1C6AFB4):
//   "d0ajLeRi":[{ "rEFRefr8": "<box id>" }]
// `rEFRefr8` uses the STRING overload of addParam (the value is read from the
// request's own std::string member with an SSO check), so it is a str, and the
// group key `d0ajLeRi` is the same key the LIST response dispatches on.
//
// Response: `MysteryBoxRewardResponse` = `CoAp2aph` (kixHbe54, 30Kw4WBa,
// TdDHf59J, wJsB35iH, qp37xTDh).  **`30Kw4WBa` is the shared present_type
// hash** — the same one CampaignReceipt, the present box and daily_task all
// dispatch on (3 = zel, 8 = gem, 6 = unit, 4/5/7 = item/material/sphere).  So
// chest rewards reuse the existing reward vocabulary; do not invent a new
// switch for them (§7.13).
HANDLEF(MysteryBoxClaim)
{
	LOG_INFO << "MysteryBoxClaim: " << json;

	MysteryBoxClaimReq req{};
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
	}
	if (req.entries.empty())
	{
		co_return HandleResult::error("MysteryBoxClaim: no chest in request");
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	MysteryBoxClaimResp resp{};
	// A refused claim — unknown, already opened or expired — still answers with
	// the current chest list and header rather than an error, so the screen
	// resynchronises instead of stranding the player on a chest that is gone.
	co_await gme::claimMysteryChest(theDb(), identity, req.entries.front().box_id, resp.rewards);

	resp.boxes = co_await gme::listMysteryChests(theDb(), identity);
	resp.team_info = std::move((co_await gme::getTeamInfo(theDb(), identity)).nonEmpty());

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		co_return HandleResult::error("Serialization error", glz::format_error(ec, buffer));
	}

	co_return HandleResult::success(buffer);
}

// Daily Spin (4aClzokO / stI81haQ).
//
// NOT sent when the tile opens — `DailyLoginScene::initConnect` @0xE50F4C is
// 16 bytes, i.e. empty.  It is fired from `DailyLoginScene::updateEvent`
// @0xE5307C, immediately before `wheelSpin()`, so this is the SPIN action.
//
// Request (`DailyLoginRequest::createBody` @0x1CC9880): bare identity —
// userInfoTag + signalKey, nothing feature-specific.
//
// The screen is driven entirely by `DailyLoginRewardsUserInfo::shared()` —
// `getUserCurrentCount()` / `getUserLimitCount()` decide whether the spin
// button is enabled (`DAILY_SPIN_LIMIT_LABEL` is the exhausted-state text),
// and `isDailyLoginAvailable()` decides whether HOME force-opens the wheel at
// all.  That state is `Drudr2w5`, seeded by Initialize and replaced here.
//
// ⚠ The reward CATALOGUE is not ours to send.  `DailyLoginRewardsMstResponse`
// is absent from `getResponseObject` entirely, and its only constructor caller
// is `DataMstManager::loadDailyLoginRewardMst` @0x1209684, which loads
// `F_SG_DAILYLOGIN_REWARDS_MST` — the client-loaded MST path (§11.2's
// DataMstManager rule).  The client already has the 29-day table; the server's
// only job is to say WHICH row was won.
HANDLEF(DailyLogin)
{
	LOG_INFO << "DailyLogin: " << json;

	DailyLoginReq req{};
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
	auto state = co_await gme::loadDailySpin(theDb(), identity);

	// The reward the wheel lands on.  ⚠ THIS IS A PROBE, NOT THE FINAL RULE.
	//
	// XIvaD6Jp is not a record id — it is the PRIZE SELECTOR.  wheelSpin
	// @0xE536E0 opens by looking getId() up in a std::map, and
	// setupSpinWheelRewards @0xE52740 additionally resolves it through
	// DailyLoginRewardsMstList::getObject(id) -> getGroupType() ->
	// getRewardGroupViaGroupType(), so the id picks BOTH which day's six
	// prizes are displayed AND which of them is won.
	//
	// We cannot derive that numbering: F_SG_DAILYLOGIN_REWARDS_MST is loaded
	// through DataMstManager (the client-loaded MST path — its response class
	// is absent from getResponseObject entirely), and the table is in none of
	// the 147 decoded MSTs.  The client has it; we do not.
	//
	// So the id is the cycle day for now, which makes the mapping observable:
	// launch, read the six prizes the wheel shows, compare against the wiki's
	// day table, then step `spin_day` in deploy/gme.sqlite and relaunch.  A
	// handful of samples pins the formula (§6.16's probe technique, and §6.19
	// on why that edit belongs in the database rather than a migration).
	//
	// Until it is pinned, this handler deliberately GRANTS NOTHING.  Awarding
	// from the wiki table against an unverified id mapping would show one
	// prize and pay a different one, which is worse than paying nothing.
	//
	// Award BEFORE consuming: awardDailySpin reads spinsUsed to decide whether
	// this is the day's first spin (which is what the 7/14/21/28 guaranteed Gem
	// hangs off), and consumeDailySpin advances spinDay past the row we are
	// drawing from.  A refused spin awards nothing and keeps the stored reward
	// id, so it cannot invent a prize for a spin that never happened.
	// The day being played.  consumeDailySpin advances spinDay, so anything that
	// has to describe THIS spin has to be captured first.
	const int32_t spunDay = state.spinDay;

	std::string awarded = "nothing (no spins left)";
	if (state.spinsUsed < gme::kDailySpinLimit)
	{
		awarded = co_await gme::awardDailySpin(theDb(), identity, state);
	}

	const bool spun = co_await gme::consumeDailySpin(theDb(), identity, state);
	if (!spun)
	{
		LOG_INFO << "DailyLogin: user " << identity.userId
			<< " has no spins left today (" << state.spinsUsed
			<< "/" << gme::kDailySpinLimit << ")";
	}

	DailyLoginResp resp{};
	// ⚠ id and next_reward_id are REWARD IDS, not day numbers — the client
	// groups the wheel by the row the id belongs to, so a day number draws the
	// wrong day's prizes (§7.14).  current_day IS a day: it is the "Days: N"
	// label, and it reports the day just played rather than tomorrow's, so the
	// header does not tick over while the result animation is still running.
	resp.daily_login_rewards.id = state.lastRewardId;
	resp.daily_login_rewards.current_day = spunDay;
	resp.daily_login_rewards.user_current_count = state.spinsUsed;
	resp.daily_login_rewards.user_spin_limit_count = gme::kDailySpinLimit;
	resp.daily_login_rewards.next_reward_id = gme::dailySpinAnchor(spunDay + 1);

	// u8iD6ka7 is prepended to the message by the client (setDay, a STRING
	// setter at +0x30 — the KDL types it i32::str, which happens to serialise
	// compatibly).  The live game guaranteed a Gem on the first spin of days
	// 7 / 14 / 21 / 28, so this is the distance to the next such day and the
	// label reads "N day(s) more to guaranteed Gem!".
	resp.daily_login_rewards.remaining_days_till_guaranteed_reward =
		(7 - (spunDay % 7)) % 7;
	resp.daily_login_rewards.message = " day(s) more to guaranteed Gem!";

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		co_return HandleResult::error("Serialization error", glz::format_error(ec, buffer));
	}

	LOG_INFO << "DailyLogin: user " << identity.userId
		<< " reward id " << state.lastRewardId
		<< ", awarded " << awarded
		<< ", used " << state.spinsUsed << "/" << gme::kDailySpinLimit
		<< ", next day " << state.spinDay;
	co_return HandleResult::success(buffer);
}
