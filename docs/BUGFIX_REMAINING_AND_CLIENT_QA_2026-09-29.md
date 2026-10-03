# Remaining work and client visual QA — September 29, 2026

The patch is not ready for release. Automated server results are recorded in
[the QC handoff](BUGFIX_HANDOFF_2026-09-29.md); no actual client acceptance run
has been completed. This list separates implementation work from visual checks.
Existing working-tree changes are uncommitted and include Claude's work plus QC
repairs. Do not reset them or update the packet-generator submodule blindly.

## Next implementation work, in priority order

1. **#34: finish the immediate crafting contract.** Persistent per-copy IDs,
   favorites, selling and equipment now pass 35 server checks. However, the
   ARM64 client locally creates `INT_MAX - itemId` inventory IDs after crafting;
   ItemMix returns `{}` and the current test reloads UserInfo before use. Trace
   the Windows client and implement a safe mapping or refresh at the correct
   scene boundary. Test craft → immediately favorite/equip/sell without relog,
   multiple copies of the same species, repeated crafts, stale requests, and
   rejection rollback. Never reinterpret all high IDs as temporary aliases.
   Audit per-copy locks in Merit delivery and other aggregate spenders, sphere
   returns from unit sale/fusion/evolution, capacity limits, and favorite
   refresh in responses other than UserInfo. Test migration of equipped and
   empty inventory saves and compatibility with any earlier split-row save.
2. **#19: extend mission settlement coverage.** The persisted mission-ID guard
   stops duplicate completion after a run closes; it cannot distinguish a late
   result from an earlier run after the same mission starts again. Determine
   the existing client run/session contract before adding a token. Cover raid,
   Grand Quest, challenge/supplied bars, suspended missions, retry, and migration
   of in-flight missions. Assert counts/payments and persisted state, not just
   response success. Ordinary owned items and simulator bars already have tests.
3. **#20: reproduce mission 234 in the client.** Capture first-clear Cordelica
   transition and replay. Claude found valid assets/prerequisites; no confirmed
   root cause or fix exists. Use the crash/log/request evidence to localize it.
4. **#23/#30/#33/#40: finish acceptance gaps.** Verify actual cutscene timing;
   audit other compound land gates; verify stale locked-tile refusal; check
   level-cap result animation and energy; check evolution chains, preserved
   upgrades/equipment, burst levels and UBB eligibility. Existing wire tests
   cover representative cases, not every chain or visual behavior.
5. **#25: frog SP eligibility.** Establish a sourced target/material matrix
   before edits. Separate SP, BB, Mystery Frog and imp behavior. Cover levels,
   rarities, caps, mixed batches, payment, consumption, rollback and reload.
6. **#26: Roglizer/Fuu leader bonuses.** Resolve variants and exact effects,
   server versus client ownership, helper stacking and rounding. Use fixed
   rewards to prove displayed and stored payouts match without double counting.
7. **#27: slots.** Audit existing implementation first. Deterministically test
   every prize category and roll counts 1–10, art IDs, duplicate rewards,
   insufficient tokens, capacity and atomic costs/grants. Client multi-roll
   tiles must all render. Do not claim rates or prize lists without evidence.
8. **#17/#39: enemy content.** Resolve Fire Mecha God stage/capture conditions;
   then verify Mega Metal Parade stages, tiers, wave counts, HP/DEF, XP and
   captures. Add deterministic success/failure and table-driven wave tests.
   Do not use invented maximum defense values or assume all parade modes match.
9. **#22A/#22B/#22C/#24: visual defects.** Trace Lukroar gate dependencies;
   Omni Amadream header fields/layout; full Omni Galtier description through
   source, served data and client cache; double-gate flash state. Establish
   exact variants and reproducible conditions before editing assets or text.
10. **#12/#18/#15: larger content work.** Inventory missing Vortex trials,
    large-boss encounters/AI/assets, and EU/special units. Existing Research
    Lab coverage is not proof of full requested content. List supported and
    missing content with exact IDs, source evidence and per-encounter tests.
11. **V1 and regressions #8/#13/#14/#16.** Independently confirm Legend Stone
    in both Guild and Merit exchanges. Resolve source-list Seria versus existing
    notes' Tilith 50253 portrait discrepancy. Recheck shop expiry, Burst Queen
    spillover and Mystery Frog behavior in-client after integration.

After code changes: rebuild the isolated executable, rerun affected QC tests
and the existing-suite runner, run startup/handler/data checks where affected,
and update the handoff with actual results. Keep the 3,117 existing mission
validator findings visible; unchanged counts do not mean the validator is clean.

## Client test setup and evidence

Use a disposable copy of the save and a test-only server configuration. The
isolated executable is
`out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe`.
It has **not** replaced the live executable. Do not launch it with a default
configuration that could select the live save. The new migrations need the
matching executable, source data and generated packets; a cached old client
table can invalidate a visual test. Record the client/platform version,
server build, data/cache version and fixture name before testing.

For each row below, record **Pass / Fail / Not tested**, the exact unit/stage
ID selected, starting state, and whether it was first clear or replay. Capture
a short video for animation/crash cases and before/after screenshots for
inventory, descriptions and headers. For a failure include the timestamp,
server log, last action, and whether a restart changes it. Do not use a relog
to hide an immediate-refresh failure. No untested row is a pass.

## Client acceptance checklist

| Issue | Setup and actions | Expected result / evidence |
|---|---|---|
| #34 — highest priority | Have multiple sphere 30000 copies. Favorite one; sell another. Craft one, then several (server fixture uses recipe 2005). Before relog, favorite/equip/sell a newly crafted copy. Transfer between units and unequip. Finally restart. Repeat with an already-favorited copy and a regular stackable item. | Separate sphere rows, one lock per selected copy, immediate craft visibility, exact costs/counts, equipped copy cannot be sold, no other copy disappears. Stable locks/equipment after restart. Record every immediate-action failure; this is still an open implementation gap. |
| #11 | Enter Battle Simulator mission 6000000, select each element, fight all applicable dummies 10000000–10000005, exit and repeat. | No crash/missing enemy; correct dummy artwork; no capture/drop/reward leak or owned-item consumption. Video entry and first battle. |
| #19 | Put five Cures in the battle bar, record storage/bar totals, use two in an ordinary mission, finish, refill and replay. Also quit/reconnect a battle and exercise supplied-item modes separately. | Used quantity is spent once, transfers conserve inventory, grants occur once, no refill duplication. Screenshots before battle, after result, after refill and after restart. Network-retry cases need controlled server tests as well. |
| #20 | On a fresh eligible fixture, first-clear mission 234, A Flash of Lightning, then replay it. | Result and Cordelica unlock transition complete without crash. Record from before completion until map is usable, plus the matching log. If it crashes, retain the exact first-clear state. |
| #23 | Fresh progression: complete an intermediate Tower of Mistral stage, then the qualifying final stage 85; replay both. Separately reach Farm milestone mission 11, replay and relog. | Morgan/Farm unlock scenes appear at the correct milestone once; intermediate/repeated clears do not retrigger them. Record interrupted scene recovery separately. |
| #30 | Four copied progression states: neither 666 nor 20067 cleared, only 666, only 20067, both. Open world map and Lizeria tiles; relog. | Land 4 and playable tiles require both. Other lands behave normally. Previously earned Lizeria progress remains saved. Capture all four maps. |
| #33 | Use a natural 998→999 transition fixture and a separately edited level-999 fixture. Complete zero/ordinary/high-XP missions; spend odd and even energy amounts and observe tick boundaries. | Result screen reaches usable Continue/exit, no backwards/endless EXP animation or level 1000. Correct energy with two per 180-second tick at 999; cap and timer phase remain sensible after relog. Video results and timer. |
| #40 | Evolve representative source forms 10011, 10015 and 850637 where applicable to fixture recipes; record source upgrades, burst levels and equipment first. View destination skills, enter battle, then relog. | New-form BB/SBB identities match destination data; unlock availability follows progression, not rarity alone. Costs/materials exact, unrelated units intact, expected upgrades/equipment preserved. Capture overview and battle skill selection. |
| V1 / #8 | Open Guild and Merit exchanges independently; inspect Legend Stone, permanent stock/expiry, buy one eligible item and reopen. | Correct item in each intended shop, readable stock/expiry, one charge/grant, persistent remaining stock. Existing wire tests do not establish both Legend Stone displays. |
| #13 | Fuse Burst Queen at BB/SBB boundaries and already-maxed states; inspect applicable UBB availability. | Expected BB→SBB spillover and eligible unlock; no levels lost, duplicate consumption or inappropriate unlock. |
| #14 | Identify the reported four-star Seria variant and compare the Tilith 50253 note before choosing the case. Open collection and unit portrait. | Correct character/form portrait, no broken art. Capture ID and portrait; naming discrepancy must be resolved before closure. |
| #16 | Apply fixed/random Mystery Frogs to representative eligible units with upgrades/equipment; relog. | Intended type, level-1 reset, protected upgrades and proper costs; special-case restrictions retained. |
| #22A | Resolve Lukroar's exact variant. Perform relevant normal/special gate reveals and a known-good comparison summon. | Gate and reveal render, no black void, input advances normally. Video entire sequence. |
| #22B | Open Omni Amadream overview at the supported client resolution; compare another long-name Omni. | Unit number, rarity, element and full name are legible without overlap/clipping. Screenshot whole header. |
| #22C | Open Omni Galtier leader skill with refreshed data/cache; compare source full text and another long description. | Complete effect and conditions accessible via supported wrapping/scrolling, no truncation at “damage reductio”. Screenshot/scroll video. |
| #24 | Repeat single and double summon gates, breakthrough path, rapid taps and applicable multi-summon. | White flash clears; second gate and final reveal visible; input never stuck. Video including taps/transition timing. |
| #25 | Max-level Omni below SP cap, at cap, and lower-level/rarity controls; select each relevant material class. | Enabled/disabled state agrees with researched eligibility matrix; legitimate SP appears and persists. Greyed-out alone is not proof of a bug. |
| #26 | Same fixed mission with no qualifying leader, Roglizer, Fuu and applicable helper combinations; record exact variants. | Displayed drops/EXP/resources and persisted amounts agree with researched bonus rules. Screenshot leaders and result totals. |
| #27 | Execute 1 through 10 slot rolls with deterministic representative rewards, including mixed/duplicate prizes. | Every result tile/reel has valid art, costs exact and prizes actually in inventory after relog. Video 2–10 rolls and capture before/after currency. |
| #17 | Resolve reported Fire Mecha God stage first; run controlled capturable and non-capturable cases. | Correct enemy art and capture rules; captured unit appears once and persists. Record stage/encounter ID. |
| #39 | Run each resolved Mega Metal Parade difficulty and compare ordinary parade variants. | Evidence-backed enemy tiers/waves/stats, battle completes, correct rewards/captures; no substituted Ghost/King waves in stages requiring Gods/Crystals. |
| #12 / #18 | Use a written inventory of supported Vortex trials and large bosses; enter and finish each, including later phases and repeat entry. | Correct boss art/size/animation, attacks/phases, result and reward; no placeholder completeness claim from three Research Lab trials. |
| #15 | From the missing-content inventory, inspect each added EU/special unit in summon/collection, overview and battle. | Correct name/art/skills/evolution path and no missing assets. Unit IDs must be recorded before testing. |

## Feedback template

```text
Issue / test case:
Result: Pass / Fail / Not tested
Client version/platform/resolution:
Server build and data/cache version:
Fixture; unit / mission / item IDs:
Starting state; first clear or replay:
Exact steps:
Expected:
Observed:
Screenshot/video and timestamp:
Matching server log:
After relog/restart:
```

A visual pass closes only the tested case. Keep unresolved coding gaps and
unplayed modes open until their own evidence exists.
