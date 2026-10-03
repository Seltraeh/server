# Client checks for the current mine/main diff — September 30, 2026

**Every client case below is Not tested in this QC session.** Passing server
tests are listed in `QC_DIFF_REVIEW_2026-09-30.md`; they do not establish visual
correctness, frame rate, loading performance or client crash fixes. The current
patch also has two reproduced server defects, so use a disposable test save.

## Setup and recording

Use the matching isolated Debug executable, full source data and generated
packets. It is at `out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe`;
it has not replaced the live server. Always supply an explicit isolated config
and copied save. Record build/hash, client version/platform, resolution, cache
version, fixture name and timestamp. Do not overwrite the live save to prepare
test states. Test cold cache only on a disposable client profile.

For each case record Pass / Fail / Not tested, exact IDs, steps, observed result
and screenshot/video. Record before and after counts/currencies for mutations.
Use video for freezes, flash overlays, repeated cutscenes and frame pacing.
First test immediate behavior without relogging; then restart to check persistence.

## Highest priority: changed state and reward flows

| Case | Actions | Expected visual/state result | Performance evidence |
|---|---|---|---|
| #34 immediate crafted spheres | Craft sphere 30000 with recipe 2005 once, then several. Immediately favorite, equip, transfer, unequip, sell and Merit-deliver selected copies before any relog. Repeat with an existing locked copy of the same species. | Separate copies, immediate visibility, selected locks remain on the intended copies, exact quantities/costs, no vanished or duplicated sphere. Test selecting the second/third provisional copy specifically: shared temporary IDs are a remaining ambiguity. | Film craft completion → usable list; compare first opening and repeated visits. |
| #34 regular stacks | Craft/grant a regular item into a partial stack and past cap 99. Sell part of overflow, then relog. | Correct 99+remainder display, exact sale, no unintended stack removed. | List scroll/sort responsiveness. |
| #34 return/refresh paths | Put a locked sphere on a disposable unit; sell/fuse/evolve that unit in separate copied fixtures. Claim an item present, slot prize and applicable bond reward. | Returned items retain locks/counts; favorites survive each response. Equip selection agrees with storage. | No pause or growing delay on repeated reward screens. |
| #19 new battle serials | Start mission 10, suspend/close the client, reopen and resume, revive when applicable, finish. Repeat the same mission and test abandoning one run before starting another. | Correct battle resumes with enemies/helper/state intact; cost/reward paid once; no old-run result replaces new run. **Known stale-result/Continue defects must be fixed first for approval.** | Resume/load times, no stall at completion; retain matching server log. |
| #19 owned/supplied items | Equip five Cures, use two, finish and refill. Repeat on loss/retreat and supported supplied-item modes; test raid/Grand Quest separately. | Storage/bar totals conserve items; only owned used items consumed. No free refill duplication or supplied-mode loss. | Result/refill response time and visible count transitions. |
| #25 SP fusion | Max-level Omni with SBB 10 below SP cap; test Burst Frog 10312, Emperor 10313, Queen 750004, Omni Frog 750003, Sphere Frog 20302 before/after second slot. Compare lower-level/rarity and at-cap controls. | Material enable/disable agrees with eligibility; preview, result and persisted SP agree. No duplicate consumption. Omni Emperor/Omni+ and spending are not certified by fusion-gain tests. | Fusion result animation ends normally; no sluggish material selection. |
| #26 leader EXP | Compare mission 10/12 with plain leader, Roglizer 61286 and 61287, then valid helper combinations. Try Fuu 830838 in battle for drops. | Quest EXP bonuses visible and equal stored payout. Fuu drop-rate behavior needs actual battle evidence; server merely crediting reported drops does not prove it. | Compare result animation and helper picker responsiveness. |
| #27 slots | Play 1–10 rolls, mixed/duplicate prizes, unit/sphere/item/medal outcomes, insufficient medals and full inventory. Claim/reopen inventory. | Every result tile/reel has art; no blank 2–10-roll tiles; exact accepted roll count and grants. Prize table expanded 9→54. Authored rates/consolation need explicit acceptance. | Cold versus warm artwork loads, reel FPS/frame pacing, time to multi-result list. |

## Progression, encounters and result screens

| Case | Actions | Expected result / evidence |
|---|---|---|
| #30 all story gates | Four Lizeria fixtures: neither 666/20067, either alone, both. Also sample later multi-prerequisite story lands, including land 20 with all nine chapter finals versus one missing. Reopen map/relog; try stale saved tile. | Map and direct entry agree; required clears are all satisfied; earned history preserved. No accidental global unlock/lockout. Record stale-tile session behavior. |
| #23 scenarios | Tower intermediate stage, final 85, replay; Farm milestone 11, replay and relog; interrupt a scene once. | Correct milestone unlock only once, progress persists, intentional replay still available. Video scene and return to map. |
| #33 cap/energy | Natural 998→999 and edited level-999 saves, zero/large rewards, repeated clears, odd/even energy spending, tick boundary and relog. | Result screen exits; no backwards/infinite EXP loop or level 1000. Correct energy/countdown, two per 180-second tick at cap. Film until Continue is usable. |
| #40 evolution | Representative source forms 10011/10015/850637 where recipes apply. Capture upgrades, skills and equipment, evolve, inspect overview/battle, restart. | Destination skills and eligible unlocks correct; costs exact; expected retained/reset values; unrelated units preserved. Test more than one chain, including no-SBB and applicable UBB paths. |
| #11 simulator | Enter 6000000, all six dummies 10000000–10000005/elements, fight, exit and repeat. | No crash/missing artwork; dummy actions/captures/rewards absent as intended; no owned-item leak. Record entry and exit times. |
| #39 parades | Normal 100600, Super 100601 and Mega 100614; inspect every wave, battle/capture, result and replay. | Normal remains distinct; Super no Ghosts, Mega no Ghosts/Kings under current authored design. HP/DEF and minimum damage allow completion; no excessive battle duration or wrong captures. Mega currently awards 5000 base EXP, not cited wiki 7000; record version/policy acceptance. |
| Mission text cleanup | Open mission names 20002, 20003, 20022 and enemy Natalamé in Cold Cauldron 8301143. | Apostrophes/accent display without replacement characters; no font clipping. |
| #20 still unresolved | First-clear and replay mission 234, A Flash of Lightning, through Cordelica unlock. | No crash; capture full transition and logs. No fix is established by this diff. |

## Visual-only investigations and unaffected-regression checks

These have new audit evidence or remain requested backlog; do not confuse a new
test script with a new client fix.

| Issue | Client check |
|---|---|
| #22A Lukroar | Unit 51247; summon effects 900 and 901, compare 51337/51246. Film gate and reveal. Assets/wire checks pass; black-void behavior remains unplayed. |
| #24 double gate | Single/double gate, breakthrough, rapid taps, repeated and multi-summon. White flash must clear, second gate/input usable. Check cold/warm cache and frame pacing. |
| #22B Amadream | Omni 51337 overview: name, number, rarity, Light icon at actual resolution. Compare other long Omni names. No overlap/clipping. |
| #22C Galtier | 860428, leader skill 801271: whole 290-character text accessible through wrapping/scrolling. Reported cut was at character 252. Compare Durumn 840278 and other long descriptions. Data tests already show full text; layout remains unverified. |
| V1 / #8 | Guild and Merit Legend Stone separately, correct stock/expiry/payment/grant; return and relog. |
| #13 | Burst Queen BB→SBB spillover at boundaries and applicable UBB eligibility, no lost levels or duplicate spend. |
| #14 | Resolve reported four-star Seria versus Tilith 50253 before choosing portrait case. Capture unit ID and art. |
| #16 | Fixed/random Mystery Frogs: type, level-1 reset, retained upgrades/equipment, restart. |
| #17 | Identify Fire Mecha God stage and expected capture conditions; capture correct enemy and success/failure cases. Not newly fixed here. |
| #12 / #18 | Use trial/boss inventories in out/claude_session2_2026-09-29. Test authored encounters separately from placeholders; inspect art size, phases, attacks, results and replay. Ten inventoried special-art bosses remain placeholders. |
| #15 | Missing EU/special-unit inventory still needed; verify exact IDs before testing art/skills/evolution. No completion claim. |

## Performance pass

On a representative supported large save, measure login→home, warehouse open,
scroll/sort, craft→list, fusion/evolution result, slots 10-roll result,
mission start/end and resume. Repeat each five times, labeling cold/warm cache.
Record median/worst elapsed time, visible hitches and memory before/after 20
repetitions. Compare mine/main only with the same client/data/fixture and a
separate compatible database copy; never run the old server against the new
migrated save as a benchmark shortcut.

Server-only diagnostic: UserInfo warm calls were about 62–64 ms at 100 sphere
copies, 155–163 ms at 1,000 and 490–512 ms at synthetic 5,000. These include
Python transport/decode overhead on localhost Debug and are **not client FPS or
a measured regression versus main**. New per-copy lists, persistent tombstones
and mission history warrant long-lived-save testing. Do not invent a frame-rate
pass threshold; record baseline and observed regressions on the actual device.

## Session 3 additions (September 30, Claude) — all Not tested

Server changes since this checklist was written are in
[CLAUDE_SESSION_3_HANDOFF.md](CLAUDE_SESSION_3_HANDOFF.md).  The two
reproduced mission defects above are fixed server-side (automated tests only);
the #19 row still needs play.

| Case | Actions | Expected result / evidence |
|---|---|---|
| #19 continue rules | Start mission 10, wipe, Continue (1 gem), wipe again later, Continue again; then finish. Separately: Continue, force-close the client, reopen and resume, wipe, Continue. | Each revival charges exactly one gem; the resume screen offers the right battle; the result settles once. Record gem counts before/after each step and the server log. |
| #19 stale result | Start mission 10, abandon it (back out), start mission 10 again, finish the second run. | Rewards paid once for the second run; nothing odd on the result screen. |
| #30 stale tile → Home | Open the quest map, then (in the copied save only) remove a prerequisite clear with the server stopped, restart it, and tap the tile the client still shows. | A notice "This quest is still locked. Clear the quests it requires first." and OK returns to **Home** (it used to exit the app). No energy spent. Screenshot the notice and the Home screen. |
| #34 lock the first new copy | Craft sphere 30000 three times (no relog), lock the FIRST new copy, go Home and back. | All three new copies show locked (client behaviour); after a relog/refresh exactly those three are locked. None can be sold while locked. |
| #34 lock a later new copy | Craft three, lock the SECOND or THIRD new copy only, go Home and back. | Known client limitation: the lock is not sent and shows removed after the next reply. After a relog, locking that copy (now with its own id) sticks. Record what the client shows at each step. |
| #15 Deemo and the Girl | In a copied save, grant 50563 (present or debug CLI `addunit 50563`), open overview, collection, squad; evolve to 50564 when materials allow; take it into a quest. | Portrait, name, rarity (4★→5★), Light icon, BB/SBB/LS names, battle sprite animate; evolution keeps it; no missing art. Repeat for Dark 60864. |
| #15 other restored units | Spot-check a few of the 114 (e.g. 10573 Stahn 4★, 10985 Rain 6★, 810045 Titan Wing Blaze) as above. | Same. 850977 lacks `unit_anime`/`unit_cgg` on disk: expect a missing battle sprite; record it. |
| #14 Goddess Tilith 50254 | Set 50254 as the Home unit. | Currently expected **wrong**: face off the right edge (same defect 50253 had). A proposed crop exists but is not deployed. Screenshot. |
| #14 Swordswoman Seria 10233 | Set 10233 as the Home unit. | Expected framed correctly (data check). Screenshot to close the Seria/Tilith question. |
| #25 SP spending | On a max-level Omni with SP, open Enhancements and acquire one skill. | Known **crash path**: `FeSkillGet` is not implemented and the client closes. Do not use a live save. Record only whether the behaviour matches. |
| V1 Guild Legend Stone | Open the Guild exchange. | No Legend Stone offer is expected (the client's own guild catalogue has none); the Merit shop has it at 4,000. |

## Feedback form

```text
Case / issue; Pass / Fail / Not tested:
Client version, platform, resolution:
Server build, data/cache version, fixture:
Exact mission/unit/item IDs and initial state:
Steps (first clear/replay; immediate action/relog):
Expected / observed:
Before/after currency, inventory or SP:
Cold/warm elapsed times; hitch/FPS/memory observations:
Screenshot/video; timestamp and matching server log:
After restart:
```
