# Claude Session 14 handoff — #24 double-gate whiteout, the Farm report, Phase C4 gate scripts

Post-session QC: [QC_SESSION_14_REVIEW.md](QC_SESSION_14_REVIEW.md).
Targeted checks passed; harvest-checker uncertainty handling, withdrawal
documentation and per-part client collection were corrected. Candidate scripts
and policy are unchanged. Next directive: [CLAUDE_SESSION_15.md](CLAUDE_SESSION_15.md).

October 8, 2026. Directive: [CLAUDE_SESSION_14.md](CLAUDE_SESSION_14.md).
State: a candidate for Codex QC. **Nothing deployed, committed or pushed; no
client launched; no client cache read for writing or reset.** The live save is
byte-identical (`239f9e69…`), the player's in-tree server EXE (`6ee7d454…`)
was not relinked, `deploy/game_content/content` is unchanged (75,931 files,
newest 2026-10-02), `deploy/mst` and `deploy/system` were only read, the
player's Session 13 save `c3_effects` was only copied (backup API, read-only;
still `648032dd…`), and the Word bug list was not opened. **No C++ changed**:
the server candidate is Session 13's `90e3eff9…` (no source newer than it) and
the proxy DLL / test binary are unchanged. QA output is under
`out/qa/current/session14/` plus the suites' established folders.

| Directive part | Result |
|---|---|
| P1. #24 | **Root cause found** (binary-confirmed mechanism, both builds), reproduced in a model of the client, fixed at the script layer, delivered by a provider policy, regression added, deterministic reproduction prepared. Client acceptance pending. §1 |
| P2. Farm | **Explained, no defect**: the run's fixture had pre-rolled six empty Farm taps; credits match every entry exactly; upgrades never refill by design. New bounded fixture and wire test. §2 |
| P3. D1 | The gate-script family (Phase C4) -- the family the fix needs -- with its read and dependency contract and a new D1 constraint (script-named sounds). §3 |
| Docs | [CLIENT_SESSION_14_STEPS.md](CLIENT_SESSION_14_STEPS.md), [OFFLINE_CONTENT_PHASE_C4.md](OFFLINE_CONTENT_PHASE_C4.md), [BUG_BACKLOG_STATUS.md](BUG_BACKLOG_STATUS.md), [OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) (D1 progress), handbook pointer |

## 1. #24 double-gate whiteout

### 1.1 What ran in Session 13

- **Which gates.** The fixture had 50 gems and ended with 45 and five Omni
  units: five pulls on **Test Gate: Omni** (99008, 1 gem). Rarity 8 picks effect
  900 (gWaitRainbow, gChange(Rainbow-black), gOpenGold(Rare7)) or 901
  (gWaitBlack, gOpenGold(Rare7)) at 50/50 (`Gacha.cpp`). The broker's
  first-fetch log orders the screens: 15:36:11 `GachaGateClose6(+Add)` (black
  single gate), 15:36:25 `GachaGateClose5` then 15:36:27 `GachaGateChange5_6` --
  the **double gate** the player reported (`session14/evidence/c3_effects_server.log`).
- **Which scripts, from where.** `SummonsAnimeScene2` reads each script with
  `getLocalPath` + `getCsvList` (x86 0x874D60 -> 0x236950 / 0x235C60). The C1
  package's LocalState held no gate script before the run; it downloaded them
  at 11:36:11 / 11:36:25 from the harness server's document root
  (`deploy/game_content`) -- 8 files, **byte-identical** to the served ones
  (`gWaitBlack ff4126a5`, `gWaitRainbow 01ff5736`, `gChange(Rainbow-black)
  b056ffa6`, `gOpenGold(Rare7) db5b4bb5`, …). Every other package (player,
  ContentTest, C1 Yes) also holds the served bytes. The app package ships
  STOCK copies of four of them (`gWaitGold`, `gWaitRed`, `gOpenGold(R)`,
  `gOpenRed(SR)`), but `getLocalPath` takes the LocalState copy whenever
  `existsLocalFile` finds one, so they were not read. So the September patch
  was in use: **not a stale cache**, and no cache was reset. C3 owns the gate
  animations, not the scripts (`.txt` is outside C3).

### 1.2 The mechanism (binary-confirmed; arm64 semantics, x86 the build that runs)

The white is the client's **own reveal flash**, not a script effect.
`SummonsAnimeScene2::updateEvent` (arm64 0x169D080; x86 0x875F20), after
`ScriptEngine::process`: if `SuperAnimList::getCount() == 3` (x86 `cmp ecx,3`
@0x876100) and no overlay yet, take entry 2; if its path contains `Touch.sam`
or `Hold.sam` skip; else once its frame is >= 45 (`cmp eax,0x2d` @0x8761BE)
create a full-screen white `fillRect` (0xd67840 / arm64 0x169D3B4), fade it in
by 255/15 per frame (@0x876242) and **never remove it** -- it covers everything
until the reveal scene replaces the summon scene.

`SuperAnimList` is **global**, and its count includes everything:

| Member | Evidence |
|---|---|
| a gate, then its additive blend layer as a second entry | `addAnime`/`addAnime2` 0xFE9D58 / 0xFEB2E4; id=42 -> 0xFCBD90 |
| the touch prompt (Touch.sam) | id=42 (patched) or id=21 `setTouchAnime` 0xFCD92C |
| a tap ripple (7 frames, auto-removed) on every tap | `BaseScene::startTouchEffect` 0x10012D0, `GameScene::touchBegan` |
| the header's energy-overflow glow, while energy > max | `GameScene::updateHeader` 0x1604E2C, `setHeaderControl` 0x1627F20 (a fresh one whenever the stored one is not running; the old one is not removed) |
| the download icon, while downloads are pending | `GameScene::onSceneVisible` 0x16008C8, removed by `GameScene::update` 0x16019D4 |

Removal is **deferred**: id=43 and clearScreen only disable (`setEnable`
propagates to the blend, 0xFF5BDC); `SuperAnimList::process` (from
`GameScene::update` 0x1601548 and `BaseScene::draw` 0xFFF1C0) drops disabled
entries later. And the summon screen **inherits** the previous screens'
animations: `sceneFadeOut` purges the list (`removeAllAnime`) unless
`LoadingSamUtil::isConnectPop()` (0xFFF300), which `ConnectScene` sets right
before returning to `GachaActionScene` (0x1B5DB64) -- on every summon.

`STOP id=-1` waits until every non-looping animation drawn since the last
STOP has finished (`advStopStart` 0xFCE75C / `isFinishedCmd` 0xFCE49C), so a
Change animation (frames 0–50, 24 fps) is followed by nothing until it ends;
from frame 45 (1.875 s after the tap) to its end it is the newest entry.

- **No other animation in the list:** a gate's Open has 2 entries -> the rule
  never fires: no flash at all.
- **One other** (e.g. the overflow glow): during an Open, `[glow, gate, blend]`
  -> the blend reaches frame 45 -> the designed white transition into the
  reveal. During a Change the touch prompt makes entry 2 a Touch.sam: excluded.
- **Two others:** during a Change, `[x, y, change]` -> **white over the second
  gate ~1.9 s after the first tap**, the reported symptom ("the first click is
  treated as a unit being revealed"); a single gate's Open has 4 entries -> no
  flash, which nobody notices. Three or more: nothing again.

In the Session 13 run the fixture's energy was 100 over a level-22 maximum of 27,
so the header's overflow glow was in the list. **What is not proven** is which
member made the second (a second glow -- the GachaActionScene header's,
carried over by the connect-pop path, beside the summon screen's own -- the
download icon, or another member); the fix does not depend on it, and the
optional Part C of the client steps tests the glow's share.

### 1.3 Single vs double, normal vs fast taps, frame rate (modeled)

`scripts/gate_overlay_model.py` simulates the scene frame by frame with those
rules (every rule cited to its address) and the real section tables of every
animation involved (`parse_sam_sections` walks each .sam to EOF; the
regression compares). `python scripts/gate_overlay_model.py --baseline overflow,overflow`
plays all 15 effects. Over 6 baselines x normal/fast/late taps x 60/30 fps x
ripple on/off (premature = white before the Open part):

| Scripts | 0 others | 1 other | 2 others | 3-4 others |
|---|---|---|---|---|
| current (Session 9 patch) | no white | flash during every Open (designed); at 30 fps the black single gate's blend whitens its Wait | **every double gate whitens during its Change**; at 30 fps singles whiten during the Wait too | no white |
| reconstructed stock | same as current (fault predates the patch) | | | |
| **guarded (candidate)** | no white | flash during every Open, at the same instant as now | **no premature white** | no white |

Fast taps keep a ripple in the list, which shifts counts again (no flash and no
whiteout in the model for either script set). The stock scripts were rebuilt by
inverting `tools/patch_gacha_scripts.py`; the four stock files the app package
ships (`gWaitGold`, `gWaitRed`, `gOpenGold(R)`, `gOpenRed(SR)`) equal the
reconstruction command for command.

### 1.4 Not a C3 regression

- The rule and the list are client code; C3 only changes where SuperAnim and
  plist bytes come from, and Session 13 proved those reads byte-exact.
- The October 2 recurrence was on the **player's main package, which has no
  provider** (its installed `libcurl.dll` is `9ae05b17…`, dated May 2026, before
  the provider existed), while `GachaGateChange5_6.sam` was being downloaded on demand
  (LocalState mtime 21:56:05, GachaAction 21:56:03) -- a pending download adds
  the download icon.
- The stock scripts fail the same way (§1.3), and the original report predates
  the provider.
- If anything C3 lowers the chance: owned gate animations are no longer
  downloaded during a summon, so the icon appears less. The model's
  "overflow glow + download icon" case is that pre-C3 situation (an on-demand
  gate download pending) and whitens every double gate with today's scripts
  (regression section B). A client A/B under the prior policy (C2) would not
  be a clean control for the same reason -- C2 downloads the gate animations
  during the first summon, which itself adds the icon -- so none was added to
  the player's steps; the provider-less October 2 occurrence is the client
  evidence.

### 1.5 The fix (script layer)

`scripts/gen_gate_scripts.py` rewrites the 9 Wait/Change scripts from the
current ones (idempotent; refuses `deploy/` and client packages):

```
*,type=PARAM,id=42,param=door_change:160:240:GachaGateChange5_6.sam:none,#
*,type=PARAM,id=42,param=gate_guard:-1000:-1000:Touch.sam:none,#     <- new, right after the gate
…
*,type=PARAM,id=43,param=touch_prompt,#
*,type=PARAM,id=43,param=gate_guard,#                                 <- new, removed with the gate
```

plus a leading `id=43 gate_guard` after the existing leftover cleanup (§D of
the summon suite still holds: the first five ops are unchanged; everything
drawn is removed). While a gate (or its blend) is up, an off-screen Touch.sam
follows it in the list, so the gate can be entry 2 of three only when the
guard makes four, and the rule never fires on a Touch.sam itself. The 5 Open
scripts are unchanged, so the designed flash happens exactly when it does now.
Nothing is hidden or bypassed: no white effect is suppressed, every gate
transition still plays, and the guard is invisible. Rejected: hooking the
client's rule (a DLL change for a script-layer fault), showing the TOUCH prompt
earlier (a visible change), padding the list (the number of other animations
is not knowable from a script).

### 1.6 Delivery: Phase C4

The client never re-checks cached scripts, so the guarded scripts reach it
through the provider: policy C4 owns the 14 scripts `gacha_effect_mst` names,
served from a candidate root (hard links to `game_content` + the 9 guarded
files); rollback is the C3 policy. Contract, dependencies (all SuperAnims and
plists already owned; sounds never) and verification:
[OFFLINE_CONTENT_PHASE_C4.md](OFFLINE_CONTENT_PHASE_C4.md).

### 1.7 Deterministic reproduction

- `scripts/make_gate_test_mst.py --out out/qa/current/session14/mst_gate_test`:
  a hard-link copy of `deploy/mst` whose `gacha_effect_mst.json` silences 5
  rows (weights only; the client never reads weights): Omni -> always 900
  (the double gate reported), 7 Star -> always 229 (single black), 5/6 Star ->
  their doubles only, rarity 4 -> its double. The harness gained
  `server --mst-root DIR` (passed to `bf_testkit.Fixture.create`).
- Fixture `s14_gate_town_kit.sqlite`: Session 13's validated kit (Selena, 50
  gems, energy 100 over 27 -- the overflow glow on) plus known Town lists
  (§2), built by `make_debug_fixture.py` (new `--sql`) and validated through
  the server: UserInfo carries energy 100 / 27 and the four exact lists.
  `s14_gate_flat_kit.sqlite`: the same with energy 27 of 27 (Part C).

## 2. The Farm report

**Data.** `town_location_mst`: 1 Mountain (needs mission 12), 2 River, **3
Farm** (needs 11), 4 Forest. Farm lv 1 (`town_location_lv_mst`): items Don Nut
10200 and Buru Nut 10202 at rate 10 each (80 % no item), Zel 20 % (110–180),
Karma 20 % (15–30), 5–7 taps.

**What happened** (`scripts/verify_town_harvest.py <fixture> <final save>`, and
section E of the new wire test, from the run's own fixture -- the pre-claim
balances -- and the copied final save):

- Session 13's fixture `c3_effects_kit` had rolled the Farm period at 13:34 UTC
  as **six `0:0:0` entries**; the player's TownUpdate `1:3,2:8,3:6,4:10` spent
  them all. The client draws an icon only for an item that resolves, Zel >= 1
  and Karma >= 1 (`MyTownTopScene::collectItem` @0x18F244C) -- an empty entry
  draws nothing but still uses the tap, and the sparkle leaves after the last.
- Credits match every entry exactly: items (Green Pelt, Paladin Stone, 7 Green
  Drop, 9 Green Grass; nothing from the Farm), Zel +684 = 304 from the plots +
  380 from the quest, Karma -335 = 122 + 43 from the quest - 500 for the Farm
  upgrade, harvest counter +27 (the Farm's 6 included).
- The upgrade (15:46:15) came after the Farm was exhausted; an upgrade
  re-rolls only remaining taps and never refills (`Town::locationState`,
  periods of three hours). No sparkle afterwards is the designed result.
- Under the authored lv-1 rates, an all-empty six-tap period has a 0.512^6 =
  1.8 % chance. The rates are authored policy since August and were **not
  changed** (no evidence for a different rule).

**Server-tested** (`scripts/test_town_harvest_wire.py`, new, 24/0): a known
Farm list (item; Zel; Karma; all three; nothing; item) paid entry by entry over
two flushes; the player's exact flush on known lists for all four plots; taps
beyond the list and a repeated flush never pay more than the list; Farm and
Mountain upgrades persist, the exhausted Farm is not refilled and keeps its
period start, the partly harvested Mountain keeps its 2 taps re-rolled at
lv 2; a period older than three hours (controlled time on the copy) re-rolls
at login at the new level. Client check with known lists: Part B.

## 3. Priority 3: D1, the gate-script family

Advanced with the family the fix needs (§1.6): proven read path (shared with
the client-confirmed unit CSVs), enqueue drop, assets named, and one new D1
constraint -- the downloader parses a *downloaded* script's PARAM lines for
missing dependencies (`processSamAndCutSceneFile` arm64 0x1CD9BC0), so an
owned script no longer pulls sounds only it names; the three unpackaged gate
sounds would be silent on a package that never cached them (both test
packages and the player's hold all five). Remaining D1 families are unchanged
([plan §4](OFFLINE_STARTUP_TRANSITION_PLAN.md)). No startup change; audio not
owned; FILE_CHECK removal remains open.

## 4. Tests

Candidate server `90e3eff9…`, proxy test binary `ad5cbc67…`, unless named.

| Check | Command (from `BF-WorkingDirRust`) | Result |
|---|---|---|
| Overlay model regression (modeled; new, in the runner) | `python scripts/test_gate_overlay_model.py --scripts out/qa/current/session14/gate_scripts` | **56/0**: section tables = the .sam files; current and reconstructed-stock scripts reproduce the double-gate whiteout with two other animations (negative controls); guarded scripts never whiten before the Open in 360 conditions, keep the flash at the same instant; 3 mutations caught; generator idempotent and refusing deploy/ and packages (`test_gate_overlay_model.log`) |
| Town harvest (new, in the runner) | `python scripts/test_town_harvest_wire.py <exe>` | **24/0** (`test_town_harvest_wire.log`) |
| Session 13 credits | `python scripts/verify_town_harvest.py out/qa/current/session13/fixtures/c3_effects_kit.sqlite out/qa/current/session14/evidence/c3_effects_copy.sqlite` | 4 OK; per tap: the Farm's six taps NOTHING |
| C4 policy check | `python scripts/check_content_policy.py <exe> out/qa/current/session14/c4_content --policy out/qa/current/session14/family_policy_c4.json` | **8/0** (`check_policy_c4.log`) |
| C4 live family (real broker, real hooks) | `python scripts/test_content_family_live.py <exe> ../offline-proxy/build/tests/content_provider_tests.exe out/qa/current/session14/family_policy_c4.json out/qa/current/session11/first_visit_downloads.json --effect-report out/qa/current/session13/family_report_c3.json --script-bundle gate_scripts` | **driver 11/0, proxy 5,973/0** (2 unrelated live modes skipped): 36,335 game reads byte-exact incl. the 14 scripts text-mode `"r"`, 9 differing from game_content (`family_live_c4.log`) |
| Harness smoke (player's commands, no client) | `python out/qa/current/session14/harness_s14_smoke.py <exe> <C3> <C4> <fixture> out/qa/current/session14/mst_gate_test` | **23/0** (`harness_s14_smoke.log`) |
| Harness pre-flight | `python scripts/content_provider_client_test.py --identity c1 check --exe <exe> --policy <C4>` | 0 item(s) outstanding |
| Full runner (54 entries) | `python scripts/run_bugfix_regressions.py out/qa/current/session13/bin/gimuserverw.exe` | **54 entries, 2,090 checks passed, 0 failed**, no failed entry; validator at its 3,117-finding baseline; 12.5 min (`session14/SUMMARY_runner_90e3eff9_s14.md`, `runner_90e3eff9_s14.log`). Against Session 13's run of the same EXE (52 entries, 2,013): the two new entries (model 50 checks without `--scripts`, Town 24) and two unchanged suites whose check COUNT varies with random draws (Brave Slots 249 -> 250, reward-lock snapshots 20 -> 22; the live save is byte-identical to Session 13's) |

Not repeated (no change touches them): C1 prompt/persistence, spheres, Vargas,
crafting, the Session 12/13 swap cases, the broker suite beyond the runner.

## 5. Candidates and artifacts

| Artifact | SHA-256 |
|---|---|
| Server candidate (unchanged) `out/qa/current/session13/bin/gimuserverw.exe` | `90e3eff97739b0d5d5713cae983db9fc4cadaa7e105b8bdf009a81b1444b5ebc` |
| Proxy tests (unchanged) / C1 DLL in the test package (unchanged) | `ad5cbc67…` / `523c77ed…` |
| **C4 policy** `session14/family_policy_c4.json` / report | `8bdcd08ec614cf53ced6b1383238ac8c70c9d6476b2d35d299260e5b8aa1562b` / `dab8ed9e…` |
| C4 root `session14/c4_content/content` | 53,771 hard links + 9 files (the guarded scripts below) |
| Guarded scripts `session14/gate_scripts/` | gWaitBlack `7de82005…`, gWaitGold `91a00f09…`, gWaitRainbow `e91c237b…`, gWaitRed `779880a1…`, gWaitStone `90ff222d…`, gChange(Gold-Rainbow) `16443626…`, gChange(Gold-Red) `f67ecb54…`, gChange(Rainbow-black) `359af228…`, gChange(Red-Rainbow) `6ff1b97c…`; Open scripts identical to game_content |
| Test MST `session14/mst_gate_test/gacha_effect_mst.json` | `87c2113b5dcdad5de49a8f24cfc3413cfe7f5c2620c8beb0d35cf90ea08cf2d7` (other 185 tables hard-linked) |
| **Fixture** `session14/fixtures/s14_gate_town_kit.sqlite` / manifest | `287e9b91f6f1163487e06ae23a67e4993a2e6f62ee6f00488fdcd3cade606de1` / `90f8ed1a…` (from `c3_effects_kit` `e2244ed2…`, unchanged) |
| Fixture `s14_gate_flat_kit.sqlite` / manifest | `554a80b7b531d2760393387791d44ff6a593d7f85db9c412832ba6f2758dd8d3` / `8af74a11…` |
| Evidence copies `session14/evidence/` | `c3_effects_copy.sqlite` `d41971ff…` (of `648032dd…`), `c3_effects_server.log` `a9cd056d…` |
| Reversing notes | `session14/re/*.txt` (disassembly excerpts), `re/sam_frames.py` (prototype) |

## 6. Changed files (this session's hunks; earlier uncommitted work preserved)

| File | Change | SHA-256 now |
|---|---|---|
| `scripts/gate_overlay_model.py` (new) | the overlay model + `.sam` section walker | `83378f80…` |
| `scripts/test_gate_overlay_model.py` (new) | model regression | `0f90c5ad…` |
| `scripts/gen_gate_scripts.py` (new) | guarded script generator | `e4745f04…` |
| `scripts/gen_gate_script_policy.py` (new) | C4 policy + candidate root | `10ce5eff…` |
| `scripts/make_gate_test_mst.py` (new) | deterministic gate MST copy | `9df9686a…` |
| `scripts/test_town_harvest_wire.py` (new) | Town harvest wire test | `3ea427af…` |
| `scripts/verify_town_harvest.py` (new) | read-only run checker | `5aeeb3f6…` |
| `scripts/content_provider_client_test.py` | `server --mst-root` | `e3cab95b…` |
| `scripts/make_debug_fixture.py` | `--sql`; manifest records energy and Town rows | `8125cc98…` |
| `scripts/test_content_family_live.py` | policy `root`; `--script-bundle` (scripts read "r") | `4c99eb32…` |
| `scripts/run_bugfix_regressions.py` | the two new suites | `9867083a…` |
| docs: this handoff, `CLIENT_SESSION_14_STEPS.md`, `OFFLINE_CONTENT_PHASE_C4.md` (new); `BUG_BACKLOG_STATUS.md`, `OFFLINE_STARTUP_TRANSITION_PLAN.md`, handbook (edited) | | |

Diffs for the four edited scripts: `session14/diffs/*.session14.diff`, each
against a "before" copy rebuilt by reversing exactly these edits; three of the
rebuilt copies hash to the values Session 13 recorded (`868c7294…`,
`86ab1186…`, `882b5ddf…`), the fourth was not recorded then. No KDL,
migration, MST, archive, `deploy/` file, C++ or proxy source changed.

## 7. Limitations and remaining work

- **Client acceptance is the open check** for both the reproduction (Part A)
  and the fix (Part B). The model encodes the client's rules but is not the
  client; whether two other animations are present in a given state is
  inferred, not observed.
- Shipping the fix is a deployment decision: update
  `deploy/game_content/content/gacha` with the generator's output (so fresh
  downloads are right too) and/or ship C4; neither was done.
- C4 and sounds on uncached packages (§3). Three Open scripts no effect row
  names stay legacy (harmless).
- The Farm's authored rates allow empty periods (1.8 % for a six-tap lv-1
  period); changing that needs evidence about the original rule.
- Town credits do not raise `zel_get`/`karma_get` (achievement counters);
  noticed while accounting, not changed.
- Session 12's Archive replacement check and the explicit empty-pane checks
  remain owed.
