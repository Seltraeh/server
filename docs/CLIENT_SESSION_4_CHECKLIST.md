# Client test list after Session 3 / before Session 4 acceptance

October 2: Vargas SP purchase persistence after relaunch is now user-confirmed.
Use [the current remaining checklist](CLIENT_REMAINING_2026-10-02.md) for status
and priorities; the historical rows below are not all still untested.

October 1 update: the normal server installation now uses the current checkout.
See [SP purchase implementation and current tests](FE_SKILL_PURCHASE_2026-10-01.md)
for the new FeSkillGet handler; type-9 reset remains unfinished. The status text
below records the earlier Session 3 review. Three Deemo forms have since passed
the user's mission-10 battle check; this is not acceptance of every effect.

No client playthrough was performed during this QC. All cases below are
**Not tested**. Use the matching isolated executable/data and a disposable save
with an explicit configuration. The live installation has not been updated.
Record client/platform/resolution, server build, data/cache version and save
fixture. Keep first-action observations separate from post-relog behavior.

## A. Wait for implementation before acceptance testing

SP purchases and type-9 reset are unfinished in the reviewed source. The new
table/schema alone does not make them work. Run these after Session 4 builds
and passes its server tests; attempting purchase now reaches an unregistered
request and reset currently does nothing.

| Test | Steps | Expected feedback |
|---|---|---|
| First SP purchase | Eligible max-level Omni/SBB-10 fixture. Concrete server case: Vargas 10017 with 100 SP; buy skill 510000 for 10 SP. | Unspent 90, spent 10, acquired node marked, confirmation closes normally. No client exit. Other units remain in roster. |
| Prerequisites | Tap an enhancement that requires another; buy prerequisite, then dependent node. Also select a skill already owned. | Locked/enabled state and displayed cost correct; no duplicate charge; selected nodes persist. Record exact node IDs. |
| SP boundaries | Below required SP, exactly enough, near used-SP limit and at limit. | Consistent button/notice behavior, no negative balance or over-cap purchase. |
| Full roster refresh | Keep another unit selected in nearby screens, buy an enhancement, revisit overview and squad screens. | Correct unit refreshed without disappearing units, stale detail panel or lost equipment/favorites. |
| Reset | Own several nodes; note unspent/spent SP and gems. Reset once, reopen screen, then repurchase. | Charge advertised one gem, all spent SP refunded, spent SP zero, nodes cleared. Gems and roster update immediately. |
| Reset refusal/retry | No-gem fixture, already-empty skill list, cancel dialog, interrupted connection/retry if safely reproducible. | No unintended charge or free SP; clear usable feedback. Empty-reset behavior must match the documented server policy. |
| Persistence | Buy, close client/server, reopen. Then reset and repeat. | Same skills/SP/gems before and after restart; reset stays reset. |
| Actual battle effects | Buy one enhancement with an observable documented effect and enter a controlled battle; compare before/after, then after reset. | Battle behavior agrees with selected skills. Seeing a highlighted node is not proof its effect works. |
| SP growth regression | Fuse supported frogs into eligible/ineligible units before and after buying/resetting skills. | Unspent plus spent SP respects cap; preview/result/reload agree; BB/SBB behavior preserved. |

## B. Current Session 3 changes needing actual Windows checks

| Priority | Test | Steps and expected feedback |
|---|---|---|
| High | Continue/resume | Mission 10: revive once, suspend/reopen, revive again, finish. One gem per genuine revival, correct current battle resumes. Immediate retry and delayed retry after another revival need controlled network/server evidence. No old resume data should replace new data. |
| High | Stale story tile | Use a copied save with a cached locked story tile. Select it, acknowledge notice. Expected new behavior: return to usable Home, not app exit. Also test a legitimately unlocked tile. |
| High | First vs later crafted-copy lock | Craft three identical sphere 30000 copies before refresh. Lock first; separately lock only second/third. Observe immediately, after queue flush and after full list refresh/relog. Shared temporary IDs currently limit distinct lock intent; record exactly which copy stays locked rather than assuming independent behavior. |
| High | Sphere sale retry | Sell one newly crafted copy, simulate lost reply only in controlled fixture, retry. Check exact copies/Zel. Known ambiguity remains; do not accept a second sale as correct merely because the UI refreshes later. |
| High | Reward lock persistence | Claim presents, slot rewards, Merit deliveries, bond rewards; sell/fuse/evolve a disposable equipped unit. Confirm returned spheres, locks and counts immediately and after relog. |
| Medium | New Deemo roster | Light 50563/50564/50565 and Dark 60864/60865: grant via test setup, inspect overview/portrait, evolve 50563→50564, select in squad and fight, restart. Correct art/skills/rarity and no missing animation. |
| Medium | Other added units | Sample the 114 added Global archive rows across rarities/regions. Especially inspect unit 850977's reported asset gap. Being grantable does not prove artwork/battle completeness. |
| Medium | Warehouse performance | Small inventory vs supported large fixture (up to 3,200 rows), including an old save with many sold-item tombstones. Measure login, list open/sort/scroll, result refresh and repeated visits. Record cold/warm times and visible hitches. |

## C. Carry-forward acceptance of the entire mine/main diff

The detailed [full checklist](CLIENT_DIFF_CHECKLIST_2026-09-30.md) remains open
where no client evidence exists. In particular:

- Simulator 6000000: enter, all dummy elements, fight, exit/repeat; no crash,
  unintended captures/rewards or owned-item consumption.
- Mission 234 first-clear/replay and Cordelica transition: no confirmed crash fix.
- Cutscenes: Tower intermediate/final 85; Farm repeat reproduction is mission 10
  (user report October 1); replay/relog without repeats.
- Lizeria both prerequisites 666 and 20067; later compound story gates and stale-tile notice.
- Level 998→999 and edited 999: result screen completes, energy timer/count correct.
- Evolution representative chains: destination BB/SBB/eligible UBB, retained upgrades/equipment.
- Roglizer 61286/61287 quest EXP and helper combinations; Fuu 830838 actual battle drops.
- Slots 1–10: every result tile/reel renders, exact costs/grants, cold/warm multi-roll pacing.
- Normal/Super/Mega parades 100600/100601/100614: tier art, damage, battle duration, captures/EXP.
- Lukroar 51247 effects 900/901 and double-gate rapid taps: no black void or stuck white flash.
- Omni Amadream 51337 header; Galtier 860428 skill 801271 full 290-character description.
- Shop stock/expiry, Legend Stone catalog expectations, Burst Queen and Mystery Frog regressions.
- Proposed Goddess Tilith 50254 crop is not shipped. Do not mark it fixed by this review.
- Vortex Trials, large bosses and Fire Mecha God remain content investigations;
  do not infer completeness from the Research Lab or asset tests.

## Evidence to send back

```text
Case / Pass, Fail or Not tested:
Client version/platform/resolution; server build and data/cache version:
Fixture and exact unit/skill/mission/item IDs:
Starting SP/gems/items; first clear vs replay:
Steps and observed vs expected result:
Immediate result vs after relog:
Screenshot or short video, timestamp, matching server log:
Load time / visible hitch / memory or FPS observation (if relevant):
```

For performance use repeated measurements on the same device/config and label
cold/warm cache. Debug localhost timings in handoffs are not client FPS or a
validated regression comparison against mine/main.
