#include "App.hpp"
#include "Handlers.hpp"

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
	co_return HandleResult::success("{}");
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
	co_return HandleResult::success("{}");
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
// The screen itself is driven entirely by `DailyLoginRewardsUserInfo::shared()`
// — `getUserCurrentCount()` / `getUserLimitCount()` decide whether the spin
// button is enabled (`DAILY_SPIN_LIMIT_LABEL` is the exhausted-state text).
// That state arrives on `DailyLoginRewardsUserInfoResponse` = `Drudr2w5`
// (XIvaD6Jp, 35JXN4Ay, 5xStG99s, ad6i23pO, u8iD6ka7, ZC0msu2L, outas79f), with
// the catalogue on `DailyLoginRewardsMstResponse`.  Neither is sent today, so
// both counters read 0 — meaning the wheel will render but the button state is
// whatever 0/0 produces.  Populating `Drudr2w5` is the next step for this tile,
// and per §7.12.4 it can ride on UserInfo rather than needing this handler at
// all, since getResponseObject is a global key->class registry.
HANDLEF(DailyLogin)
{
	LOG_INFO << "DailyLogin: " << json;
	co_return HandleResult::success("{}");
}
