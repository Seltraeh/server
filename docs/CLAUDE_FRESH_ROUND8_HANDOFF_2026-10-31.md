# Fresh session, round 8: handoff (October 31, 2026)

This round follows [CLAUDE_FRESH_ROUND8_2026-11-01.md](CLAUDE_FRESH_ROUND8_2026-11-01.md) and its QC, [QC_FRESH_ROUND7_2026-11-01.md](QC_FRESH_ROUND7_2026-11-01.md). It is a transient coordination note. The dates follow the machine clock (October 31); the brief was dated November 1.

**Status.**
- **Batch 0:** both QC findings fixed and tested, with a popup regression of my own caught by the smoke and fixed.
- **New report (Training Ground status icons):** traced, fixed in data and client-verified.
- **#26:** four matched client runs; Roglizer EXP exact.
- **§3:** contract traced. Then, at Evan's request ("Per-hit item drops"), **implemented and client-verified** in server candidate `92f41576` (§3b). Then ("Yes, build them") the **content was generated** from the wiki tables and the one live capture, and client-verified on two missions (§3c). It is staged in QA archives only.
- **§3d/§3e (November 1, "Fix everything"):**
  - EU units installed into deploy.
  - Special-boss icon offsets client-verified with a giant-boss clamp, staged.
  - Fixture checker built.
  - #18-a authored (resists and the Weakness skill).
  - **Monster skills crashed the Windows client at battle start.** At Evan's choice ("Research contract") the delivery contract was recovered from the client (`8aiBoHg5` on MissionStart), built into server candidate `e37da19b` and client-verified (v16). The live server still lacks it, so the live Research Lab trials very likely crash until it is deployed (§3e).
  - EU evolution wire-tested.
- **§4:** evidence added to the scope correction.
- **Release:** checklist written ([RELEASE_READINESS_2026-10-31.md](RELEASE_READINESS_2026-10-31.md)).

Nothing was staged, committed, pushed, deployed, signed or installed. `deploy/` and the live save were only read.
- S19 is back on `9633061d…`.
- Nothing is running.
- Ports 9960, 29960 and 29961 are free.
- Crash dumps: none from round 8's §1–§3c. November 1 added three, all on QA copies: `…16736.dmp` (the EU fixture's unknown unit ids, §3d) and `…16512` / `…4636` (the monster-skill crash, §3e, copied into `R8/visual/run/V13`, `V14`).

QA root: `out/qa/current/fresh_round8_20261031/` (`R8/`).

## 0. Starting state (verified, matched the brief)

- Ports free; no client, server or Frida.
- S19 DLL `9633061d…` matched its backup.
- Server candidate `c28a8db9…`; proxy candidate `23f9a4b0…`.
- Proxy CMake cache matched `session16/baseline`; `build/bin/libcurl.dll` was `e24234c1…`.
- Proxy sources matched round 7's hashes (`cp_name_fit.cpp` `0682f2a8…`).

## 1. Batch 0: the two QC findings

**QC P2 (1): the shared code writer's result was overstated.**

*Writer* (`cp_gate_flash_internal.h`, `cp_gate_flash_logic.cpp`):
- `WriteCodeWith(ops, at, bytes, n, restoreTo)` returns a `WriteResult`: copied, protection restored, flushed, and the error of the first failed step. `WriteCode` is the same call with the OS operations.
- Every step after the copy runs, even after a failure.
- `restoreTo` puts back the protection the site had **at attach**. After a failed protection restore the page is left RWX, and a later write that "put back what it found" would leave it RWX while reporting success. The tests show this on a real page.

*Bookkeeping* (`cp_name_fit_internal.h`, `cp_name_fit_logic.cpp`):
- `PatchSite` gains `protection` and `incomplete`.
- `installed` follows the copied bytes.
- `InstallSites` rolls back every site holding our bytes, including the failed one when its bytes were copied.
- `RestoreSites` retries both installed and incomplete sites.
- Both functions report the failing write.

*Callers:*
- Gate flash: attach restores a jump that was copied but not completed; the fault path has three outcomes; detach logs.
- Name fit: one log helper per set (installed / NOT installed / PARTIAL), with the step and its error.

*Audit:*
- `cp_post_quest.cpp` `WriteOpcode` and `cp_startup.cpp` `WriteRel32` already return "bytes changed" and log a failed restore.
- `cp_hooks.cpp` `PatchSlot` (an IAT data slot) ignores its restore result.
- All three were left alone (not the shared writer; the brief says not to rewrite unrelated patch infrastructure).

**QC P2 (2): the vertical fit could leave a label half-changed.**
- The fit moved to `cp_name_fit_logic.cpp`, so the tests run the production code: `FitPopupDesc`, `CP_FitPopupDesc` and `PopupDescThunk`.
- **Checks before any change:** label vtable (RVA `0x14E8CE0`), line count (4..64), wrap width, text string, line list, a uniform scale, position, first-line top. A failure leaves the label as built.
- **After a change:** if a check fails (no usable lines after the re-wrap, an unmeasurable scaled layout, or a top that misses its target by more than 0.5), the label is restored the way the client builds it. That is the scale, the stock width, setString with a copy of the unchanged text, then setPosition at the stock x, y. The restore is **verified**: the same line count and the first line's top back within 0.5.
- **Ownership:** the client's assign makes the copy immediately before setString, which frees it.
- **A fault** is never followed by touching the label again. RestoreFailed or a fault turns the fit off for the session: the three call sites are restored to stock and later calls do nothing.
- `check_name_fit_site.py` gained 8 binary facts (§11):
  - the runtime vtable RVA;
  - vtable +30/+38/+40/+44 are cocos `getScaleX/getScaleY/setScale/getScale` (by import name);
  - `0xD67E20` builds the label as ctor → `0xDE250` → `0x38C3B0`;
  - `0x3874A0` then calls setPosition(+F4h, +F8h);
  - `0x38C3B0` itself ends with setPosition (+48h) and +158h (`0x38C9B0`).

**A regression I introduced, caught by the smoke (run v1).**
- My first restructure measured the placement target on the as-built block. Round 7 measured it after the re-wrap.
- For 5–6 line descriptions the taller as-built block reaches into the title, so SBB/UBB text sat on its titles (`R8/visual/run/V1/popup_r7_vs_r8.png`).
- **Fixed:** the target is the re-wrapped first line; the as-built top is kept only to verify a restore.
- A mutant using the as-built target fails 10 checks.
- Run v2 matches round 7's accepted frame: bright text differs by 7 / 3 / 0 pixels in BB / SBB / UBB, and the log shows the same numbers (SBB top 260.5, y 222 → 217.5).

**Tests:**
- `content_provider_tests` **1048 passed, 0 failed, 5 skipped** (was 898; the skips are the environment-gated broker suites). New: `TestCodeWriteSteps`, the rewritten `TestNameFitSites` (real pages, every position × every step), the `TestGateFlashThunk` fault variants, `TestPopupFit`.
- `check_name_fit_site.py` **52/0**.
- Mutation evidence:
  - rollback without the failed site, plus no restore after a failed re-wrap: 22 failures (`R8/mutation/`);
  - the as-built target: 10 failures.
- Not covered by tests: the client's allocator and real `CCLabelTTF` nodes (the fakes model them). The restore and fault paths have not run in the client.

**Candidates:**
- `R8/proxy_29960_r8qc/libcurl.dll` `5e8189cf…` (v1; superseded, has the regression).
- **`R8/proxy_29960_r8qc2/libcurl.dll` `f74efc2b8f0d5bd1527bba825671c9b42cc58134a2a4ae849815d2be432265b4`** (current).
- Built by `R8/tools/build_proxy_r8.sh`. The cache is restored to the saved copy and `build/bin` to `e24234c1…`.

## 2. Training Ground enemy status icons too high (Evan's report): fixed in data, client-verified

**Trace** (arm64 libgame.so, symbols):
- `MonsterUnit::initialize` @0x1155FDC passes `hp_disp_pos` (MonsterMst `3BpHN6VD`, or UnitMst's when `unit_id` resolves) to `BattleUnit::setStateIconPoint`.
- `BattleUnit::getStateIconPoint` @0x11350B4 = (base x ± x, base y − `EdgeAnime::getSize(1)`.height + y).
- `EdgeAnime::getSize` @0xFFBA00 is the union of every idle-frame cgg part rectangle × scale. That is atlas cells, padding included, not visible pixels.
- So the anchor is the top of the dummy's box plus the offset, in battle units (y-down; 600 px / 320 = 1.875 px per unit here).
- `MonsterUnit::getHpBarOffset` @0x1159274 reads the same string.
- The six dummies (10000000–10000005, `scripts/gen_battle_simulator.py`) carried the **invented `0,-60`**. 27 archived special-boss rows carry it too.
- Production UnitMst rows are mostly positive: median `0,25`; 159 of 2,291 are negative, minimum −50.

**Measured in the client** (frames under `R8/visual/run/`):

| Run | Offset | Icon centre vs the dummy's visible top | Notes |
|---|---|---|---|
| v1 | `0,-60` (before) | about 190 px above | over DAMAGE/SPARK; the top three dummies' icons off-screen (y −36…−2) |
| v2 control | ordinary enemies (mission 10, QA archive with 50M HP) | about 21 px above (Squirty −21, witch −21, Merith −20) | Klaus BB: ATK↓, DEF↓, Sick |
| v2 | `0,35` | about 10 px above (−9.3…−11.3) | — |
| **v3** | **`0,30`** | **about 18–19 px above, all six dummies** | none over the HUD |

- Sheet: `R8/visual/run/status_icons_before_after.png`.
- Covered: no icons (pre-burst frames), a single icon, and several statuses (ATK/DEF/Sick).
- Not covered:
  - Different enemy sizes: the six dummies share one cgg; the control enemies are other sizes but use UnitMst offsets.
  - Other window sizes (not run): the offset is in layer units and scales with the layer.

**Fix:**
- `scripts/gen_battle_simulator.py`: the dummies get `hp_disp_pos` **`0,30`** (with the evidence in a comment), plus `--archive DIR` so QA writes a copy.
- `scripts/test_battle_simulator_wire.py` pins `0,30`.
- The QA copy `R8/archive_r8_icons/mission.json` (`48e51e31…`) differs from deploy (`c7002b84…`) in those six lines only.
- **deploy/archive is not regenerated** (release action, readiness §3 step 4).
- **Follow-up:** the 27 special-boss rows' `0,-60` needs the same calibration per boss sprite. A shared default would be wrong because each box differs. `tools/bin/fix_special_boss_positions.py` exists (not reviewed this round).

## 3. #26 and the per-hit item contract

**Matched client runs:**
- Mission 10 "Start of Adventure", first clear in each run.
- Fixture `R8/tools/prep_r8.py`: the same five units in every run; only the leader flag moves.
- Base EXP 20, clear Zel 300, Karma 0.
- Evidence comes from `R8/tools/r26_summary.py` (relay dump plus DB).

| Run | Leader / helper | Play | Reported per-hit Zel / Karma | Reply = displayed Zel / Karma / **XP** | Stored Δ |
|---|---|---|---|---|---|
| v1 | Klaus (plain) / none | manual, one BB | 104 / 46 | 404 / 46 / **20** (screen skipped; reply) | 404 / 46 / 20 |
| v3 | **Roglizer 61287** / none | AUTO | 122 / 67 | 422 / 67 / **24** | 422 / 67 / 24 |
| v4 | **Fuu 830838** / none | AUTO | 126 / 64 (+ chest item 10603 ×1) | 426 / 64 / **20** | 426 / 64 / 20, item 10603 +1 |
| v5 | Klaus / **Roglizer helper** | AUTO | 126 / 66 | 426 / 66 / **24** | 426 / 66 / 24 |

- **Roglizer process 97 is exact** for both leader and helper: 20 × 1.20 = 24. The server logged `leader EXP boost +20% -> 20 becomes 24`.
- Fuu adds no EXP, as expected.
- The server pays reported + fixed (`Mission.cpp:666–667`), and screen = reply = DB in every run.
- **Fuu Zel/Karma:** one sample per arm, and Zel/Karma vary by hit count (Roglizer's and Fuu's stat boosts shorten fights). So 122–126 / 64–67 cannot show Fuu's +20 % / +30 %; that needs a multi-run study.
  - The client does process Fuu's LS: BC drops 1,253 vs 1,076 and HC 999 vs 214 against the Roglizer run.
  - Chests still ignore Fuu's item bonus (rev 645616).
- **Fixture artifact:** the prep copies stat columns from the save's first unit (HP 1,495 and so on). It is the same in every run, but stats are not level-accurate.

**§3 per-hit item drops: the contract is established, not implemented (bounded follow-up).**
- **Client reader** — arm64; x86 RVAs to be mapped when implementing:
  - `BattleMonsterGroupMstResponse::readParam` @0x13CEDA4 reads `i30R8TAs` → `setItemDrop` @0x13CF12C (and `5wB9SHAV` → `setTreasureDrop`).
  - `MonsterParty::entryMonsters` @0x10C1EC8 → `MonsterParty::setMaterialDrop` @0x10C2354 does `parseList(',')`, then each entry `parseList(':')` = **`per(float):itemId:maxCount(int)`** → `BattleUnitDropMaterial{setDropPer, setItemID, setDropRest}` → `BattleUnit::addDropMaterial`.
  - Per-hit checks: `DamageData::isDropItem`, `BattleUnit::isDropItem`.
- **Live-format evidence:** `deploy/log/http_log_jE6Sp0q4_20260513_184738.log` holds `"i30R8TAs":"7:10000:3,7:10300:3"` (×7) and `"7:10301:5,3:10603:1"`. The production MissionEnd item list `10300:9,10000:10,10301:2` (`MissionEndReqBattleResult` doc) names the same items.
- **Ours:**
  - `packet-generator/assets/net/mission.kdl` `BattleMonsterGroupMst` has no `i30R8TAs` (nor `5wB9SHAV`).
  - The archive `BattleMonster` has no item list.
  - So both the channel and the content are missing.
  - The **award path exists and works**: MissionEnd `parseItemDrops` → `addUserItem` stored the chest's 10603 in run v4.
- **Unresolved second source:** `BattleGroupDataMstResponse` (readParam @0x13CE5xx; keys `ZSf8e1MG 92qMeBuC o49dYfpH SnNtTh51 M6C1aXfR kixHbe54 pn16CNah EI6HtL9A→setItemDropPer f2NXTG5C→setMaxItemDrop`).
  - Its top-level dispatch key was not found, and it never appears in captures.
  - It may cap or override per-battle drops. Resolve it before claiming parity.
- **Proposed implementation** (next batch, about 1 server build):
  1. `mission.kdl`: an optional `item_drop` (`i30R8TAs`) on `BattleMonsterGroupMst`, omitted when empty, so today's wire is byte-identical.
  2. Archive `BattleMonster.item_drops: [{per, item_id, max}]` (validated at load) and the encoder.
  3. Tests: zero items (key absent), one, many (order kept), malformed rejected at load, and MissionEnd persistence of several items.
  4. One client run on a fixture with `per` 100 to see the drop icons and the reported list.
- **Content:** the wiki lists which materials a quest drops (`tools/wiki_zone_scrape.py`), not the rates. The captures give 7 %/3 % and a max of 1–5 for a few monsters. Rates are a content decision (Decision 2 in the scope document).

**SP scope against Unit Skills rev 656738:**
- `UnitMix.cpp` already maps the wiki's table figure for figure, from the client's own `getUserUnitListEfPoint`:
  - Burst Frog 1 / Lawson 2 / Burst Emperor 5 / Burst Queen 20;
  - Sphere Frog 10 (second slot); duplicate 5; Omni Frog 30;
  - Omni Emperor 5 (Omni+3 only).
- **Unported:** the ordinary-unit "chance to raise by 1" (no source odds anywhere) and Omni+ (no Omni+ levels).
- Gem refund = FeSkill reset (player-confirmed earlier).
- Client-observed: Omni Frog and the cap (round 7). Burst Queen and Sphere Frog SP are mapped but have not been client-observed.

## 3b. Per-hit item drops: implemented (Evan: "Per-hit item drops"), client-verified on an isolated copy

**Contract now complete:**
- The second source, `BattleGroupDataMstResponse` (itemDropPer / maxItemDrop), is **dead in this build**. `BattleGroupDataMstList::shared` is called only from its own readParam, and its `getItemList`, `getUnitList`, `getObject` and `getCount` have no callers. So it caps nothing.
- `BattleUnit::getDropMaterialPer` @0x111F7B0 treats `dropPer` as a **percent**:
  - less the monster's resist, times a mission bonus, plus buff/passive material-drop-up (Fuu's item rate);
  - only while `dropRest > 0` and the item has an ItemMst row;
  - `MissionBattleManager::isDropMaterial` @0x107C44C turns it off in raid, campaign and guild-raid battles.
- The x86 2.19.6.0 EXE reads the same key: a reference at 0x4CEA55, beside `hw3L0uVj` at 0x4CEAD5.

**Change:**
- **Wire schema:** `packet-generator/assets/net/mission.kdl` gains `BattleMonsterGroupMst.item_drop` (`i30R8TAs`, optional) between `3g8PW6x0` and `hw3L0uVj`, the captured order.
- **Archive schema:** `packet-generator/assets/archive/mission.kdl` gains `json MaterialDrop {per (f32, percent), item_id, max_count}` and an optional `BattleMonster.material_drops`.
- **Encoder:** `gimuserver/archive/MissionArchiver.cpp`:
  - `encodeMaterialDrops` writes `per:itemId:maxCount` joined by `,` in authored order, and omits the key when there are none;
  - `setup()` checks every stage, scripted and Mimic monster (and the shared Mimic policy) once. A list with any entry outside per (0, 100], item_id non-zero, max_count 1..99 is refused whole and logged, and the mission still loads.
- **Content:** no production content was added. `deploy/archive` has no `material_drops`, so production wire output is unchanged (tested).
- **Server candidate `92f415766fa2027b06ff5f90f1d21981effbbfa8ba4ae74053474b6c173bc5fa`** (`R8/bin/gimuserverw.exe`): `c28a8db9`'s source plus this change. No other server or schema source had changed since `c28a8db9` (mtime check).
  - Built with `R8/tools/build_server_r8.sh`. The temporary runtime-output override was removed, and the in-tree `f72310df` and the earlier candidates are unchanged.
  - The generated headers `all.hpp` (`e31d0173…`) and `archive.hpp` (`141966b0…`) carry the new fields.

**Release caveat:** the old server's strict loader rejects an archive that carries `material_drops`; the baseline could not start mission 10 at all. So material-drop content must ship **with or after** this server, never before.

**Tests:** `scripts/test_mission_material_drops_wire.py`, registered in the runner. **19 passed, 0 failed** on `92f41576`.
- Load: five invalid lists are refused and logged; two kept.
- Wire: one, many (including a 2.5 % fraction), absent and empty lists; the client-model parse; captured key order; rows without drops keep their five keys; production missions 11, 12, 100600 and 100601 send no `i30R8TAs`.
- Award: zero, one and many items credited exactly; a repeat credits nothing; `10603`, `10603:2,` and `999999:1` are refused with nothing credited.
- **Negative control** on the baseline `c28a8db9`: 4 passed, 9 failed, cleanly.

**Client run v6:**
- Setup: S19 on its baseline DLL `9633061d`; server `92f41576`; QA archive `R8/archive_r8_items` (`deb47542…`). Mission 10 wave 1:
  - Thunder Eye 10603 at 100 %, max 2;
  - Green Grass 10000 at 100 %, max 3, and Green Drop 10300 at 50 %, max 3;
  - Blue Drop 10301 at 7 %, max 5.
- The server logged "3 monster(s) with per-hit material drops", and MissionStart sent exactly those three `i30R8TAs` strings.
- In battle, the dropped materials (Green Grass leaves, a Green Drop) lie on the field beside the crystals (`R8/visual/run/V6/w1b_field.png`).
- MissionEnd reported `10603:3,10000:3,10300:3,10301:5`. Thunder Eye is 2 from material drops (max respected) plus 1 from the boss chest, which rolled `0/1/4:10:10603:1`.
- The **Materials** result screen shows Thunder Eye ×3, Green Grass ×3, Green Drop ×3 and Blue Drop ×5, with art (`R8/visual/run/V6/r05.png`). The database holds the same counts.
- Not done: a relog check of the item box (the database is the record), and a Fuu-leader run on this fixture to see the boost on material drops.

**Still open for this feature:** content. Done in §3c.

## 3c. Per-hit drop content: generated (Evan: "Yes, build them", using the live-server rates), client-verified on copies

**Sources** (extracted once into the committed `scripts/data/material_drop_sources.json`, `b95b04af…`):
- **The one live capture.** `deploy/log/http_log_jE6Sp0q4_20260513_184738.log` is mission 10. Its 27 sibling logs replay the same body, so it is one capture, not many.
  - Every regular monster: `7:10000:3,7:10300:3` (Green Grass, Green Drop).
  - The boss: `7:10301:5,3:10603:1` (Blue Drop, Thunder Eye).
  - A search of the whole BF folder found no other source of these lists.
- **Wiki dungeon pages** (`tools/wiki_zones` cache): the footer's `drops` (the dungeon's pool), `dropsrare`, and each quest's own `drops`.
  - Mission 10 matches the capture on all three: quest drop Thunder Eye = the boss's 3 % entry; Green Grass, Green Drop and Blue Drop are in the pool.
- **Wiki material pages** (`tools/wiki_items` cache), joined to our ids by dungeon + mission name through the client's own strings (540 of 1,742 entries placed).
  - They name missions and sometimes the dropping enemy ("Sword of Fire - Lava", "Douglas (up to 2)").
  - Rewards, CBP chests, exchanges and the Raid/Event Bazaar regions are skipped. A story mission called "Dragon Disciple Raid" is kept: a word filter dropped it, so the region decides.
- **Area tables:** an area's dungeon pools plus its material-page items that are not boss items. These are Evan's "what drops at random in each area".

**Policy** (the wiki names items, never rates; the rates are the capture's):
- **Regular monsters:** two pool items at 7 %, up to 3 each. All regulars of a mission share the pair, as in the capture; the pair rotates with the mission's place in its dungeon.
- **The boss** (highest-HP monster of the boss wave; escorts are regulars):
  - the mission's material-page items, else the next pool item, at 7 %, up to 5;
  - each quest special drop and one rotating dungeon rare at 3 %, up to 1.
- **Boss items** (any enemy or chest entry on the wiki) go on the named enemy when the mission fields it, else on the boss, at 3 %, up to 1 or the page's "up to N".
- **Excluded:**
  - anything that is not an ItemMst material: Divine Light and other consumables, gems, tokens;
  - Mimics, script monsters, Mimic chests and the Battle Simulator.
- Mission 10 is served the capture verbatim. Treasure chests are untouched.

**Result on today's archive:** 990 of 1,339 missions, 20,919 monster lists. Every story region is covered except two where the wiki lists no material: Menon (only a gem) and Mildran.
- Parade Garden, Trials and most one-off events get nothing.
- 50 named-enemy entries name a monster our mission does not field. Those go to the boss.

**Generator:** `scripts/gen_material_drops.py` (`9808ff73…`).
- `--check [--archive DIR]` and `--out PATH`. **There is no default write**, so it cannot touch deploy by accident.
- `extract` rebuilds the sources file from `tools/`.
- It changes only `material_drops` and is idempotent.

**Validator:** `scripts/validate_missions.py` gains:
- **C13** (material item not in item_mst: the clear would end in an error);
- **H16** (a list out of bounds: the server refuses it);
- `--archive DIR`.

Deploy and the candidate both match the recorded baseline exactly, so the baseline survives release.

**Tests:**
- **New `scripts/test_gen_material_drops.py`** (registered): **30 passed, 0 failed** on `92f41576`.
  - Mission 10 = the capture, and the sources file = the log.
  - Only `material_drops` changes, and the generator is idempotent.
  - All 20,919 lists are in bounds, materials only, at the capture's rates only.
  - Nothing for the Simulator, Mimics, script monsters or uncovered missions.
  - Pool rotation, the boss list, the dungeon rare and named-enemy placement.
  - `--check`, and no write without `--out`.
  - Then an isolated server loads the generated archive ("20919 … 0 invalid list(s) refused"), and MissionStart sends each sampled monster exactly its list (missions 10, 11, 12, 220, 364 and 1).
- **Negative control** on `c28a8db9`: 23 passed, 7 failed. The archive cannot load, and every MissionStart answers "Archive error". That is the release-order rule, demonstrated.
- `test_mission_material_drops_wire.py` now strips deploy's lists before authoring its fixture, so it tests the channel whatever content deploy carries.
  - 19/0 on `92f41576`.
  - Also 19/0 when run as if deploy already carried the generated content (a hard-linked fake root).

**Client run v7** (S19 on `9633061d`, server `92f41576`, archive `R8/archive_r8_v7` = round-8 icons + generated drops, `3f51875f…`; save from the S19 fixture, materials 0):
- The server logged "20919 monster(s) with per-hit material drops; 0 invalid list(s) refused".
- **Mission 10 (the capture's real rates):**
  - MissionStart sent 12 lists (regulars `7:10000:3,7:10300:3`, King Sparky `7:10301:5,3:10603:1`; Mimics none).
  - MissionEnd reported `10300:25,10000:28,10301:5`.
  - The Materials screen shows Green Drop ×25, Green Grass ×28 and Blue Drop ×5 (`R8/visual/run/V7/r10_sheet.png`); the save holds the same.
  - Thunder Eye's 3 % did not hit.
- **Mission 11 (policy-generated):**
  - MissionStart sent 14 lists (regulars `7:10400:3,7:10200:3` = Green Claw, Don Nut; King Glowy `7:10301:5,3:10604:1`).
  - MissionEnd reported `10400:35,10200:33,10301:5,10604:1`.
  - The Materials screen shows Green Claw ×35, Don Nut ×33, Blue Drop ×5 and Light Eye ×1 (`r11_sheet.png`); the save holds the same.
  - The boss wave rolled no chest, so the Light Eye is the 3 % per-hit entry.
- **The yield is the max counts, not the 7 %.** A level-capped squad hits so often that nearly every regular fills its caps: about 3 × 2 materials per regular monster, plus up to 6 from the boss.
- **Decided (Evan, after v7): "Match whatever the client dictates as its limit. That is the truth."** The client trace (arm64) shows it dictates no limit of its own: the cap is the `maxCount` the server sends, and nothing else.
  - Each hit picks **one** entry at random (`getBattleRandom(0, count - 1)`, `DamageData::update` @0x103CBC0) and rolls only that entry's percent. So each of n entries gets about per/n per hit.
  - `setDropRest` @0x114A784 stores the parsed int unclamped. `decDropRest` @0x114A81C floors it at 0, and `getDropMaterialPer` @0x111F7B0 returns 0 once the rest is used up. The field, the pickup and the reward list (`BattleItemList::setMaterial`, `BattleItemMaterial::exitItemMove`, `BattleRewardList::addItem`) add no cap. `BattleGroupDataMst.maxItemDrop` has no reader.
  - Storage: `GameUtils::incWarehouseItem` @0x118A0A8 fills a stack to `ItemMst::getMaxNum` (999) and opens a new stack for the rest. `gme/common/Warehouse.cpp` already does the same.
  - A full item box only blocks **starting** a quest (`GameScene::isOverItemBox` @0x163B490 compares slots in use with `getMaxWarehouseCnt`). It never stops a drop.
  - On x86, run v7 agrees: Blue Drop stopped at exactly 5 and Light Eye at 1 (the caps sent), and every regular total stayed under 3 × the regulars.
  - So the truth is what the live server sent, and the capture is the only sample: **3 / 5 / 1, kept**. The generator's docstring now says so (`c8e186d7…`), and its output is unchanged: both staged archives still pass `--check`, and its test is still 30/0.
- **Not a defect:** the save's Cures went 4 → 0 while the squad bar still shows ×4. The client moves equipped items from storage onto the bar before each quest (`gme/common/BattleItems.hpp`), and MissionEnd sent no use log.
- Not done: a weak-squad run (where the 7 % matters), a Fuu-leader run, and any mission outside Adventurer's Prairie in the client.

**Release:** readiness §3 step 7. Generate into deploy only once `92f41576` is installed and before it starts. Rerun after any wholesale `mission.json` replacement: the Mission Builder's `dist/` does not carry the lists.

## 3d. EU-only units (#15): built, client-smoked, INSTALLED INTO deploy (Evan, 2026-11-01)

**Ask:** "Include these new units… get all units working in one go. We can test them later." Evan's folder `C:\Users\Evan\BF\EU Units` holds 242 units' full art, thumbnails, sprite sheets and cgs files. 18 of them were already in the game; **224 are new**. Answers to the questions:
- datamine download: yes;
- part files: "I don't have them… if not, we will have to author them";
- destination: "Install into deploy/".

**Sources:**
- **EU datamine:** `cheahjs/bravefrontier_data` `eu/` (Nov 2018, extracted from the EU client by Deathmax's bfdb), kept in git-ignored `tools/eu_datamine/raw/`. It holds every stat, BB/SBB/UBB level, leader skill, extra skill and evolution of all 242 units.
- **Wiki:** the "(EU)" unit pages (`Template:UnitEU`, 288 pages, 241 of the folder's ids) for the lore, summon, fusion and evolution text.

**Encoding:** the datamine stores skills PARSED.
- `tools/eu_datamine/encode.py` maps every effect type back to the client's raw parameters: 73 BB procs and 63 leader/extra-skill passives, including triggered buffs and passive 66.
- The specs were learned from the 1,682 units both games share and validated there: **58,214 of 61,947 shared samples (94 %) re-encode exactly** on derived positions. The rest are genuine EU/Global balance differences, or harmless ailment/element list ordering.
- One value is not in the datamine: the trailing parameter of most buff procs. It takes Global's usual value for that proc and is listed per unit in `scripts/data/eu_units/report.json`.
- **14,027 of 14,028 EU effects encode.** The one exception is 750257's extra skill (a triggered passive 55 with no buff id), so that unit plays without its extra skill. This is reported.

**Built** (`tools/eu_datamine/build_eu_units.py` → committed `scripts/data/eu_units/`):
- 224 unit rows, 433 skills plus their level rows, 192 leader skills, 91 extra skills, 135 evolutions, 4 EU-only evolution items, 224 comment rows, 224 archive records, and 10 dictionaries of names and descriptions.
- Values the datamine never carried come from rules (max level and sell price by rarity, evolution group = id − id % 10, new unit numbers after Global's highest) or from the Global template unit of the same element and rarity with the nearest-shaped full art (AI id, overdrive stats, hit-effect ids).
- **Home crop:** found by locating the thumbnail inside the full art (numpy normalised cross-correlation); 4 units fall back to the figure box.
- **Screen placements:** the template's, moved onto the art.

**Battle sprites (part files):** none exist for any EU unit in the folder, the 2022 asset pack, the decoded MSTs, the datamine, the wiki or `gifs.zip`.
- **12 units** have a sheet pixel-identical to a Global unit's and use that unit's part and cgs files, so they animate as the original.
- **The other 212 have AUTHORED stand-ins:** a static sprite cut from the full art (fitted to 150 px), and a part file of identical one-part frames, as many as the folder's cgs files address. Those cgs files install unchanged.
- The folder's own sheets are kept as `deploy/game_content/eu_parts/unit_anime_<id>.eu_parts.png`, so a real part file can replace an authored one alone.
- **Battle portraits** (`unit_ills_battle_<id>.png`, absent from the folder) are authored from the thumbnail inside its frame. Home demands them: their absence was a login-time download error in run v8.

**Installer:** `scripts/eu_units.py install|check --root ROOT`.
- **Tables:** it merges rows into ROOT's MST/archive files (each file's layout byte for byte; EU rows replace older copies of themselves), and stages the client tables over the download channel: F_UNIT_MST **1089**, F_SKILL_MST 1537, F_SKILL_LEVEL_MST 1488, F_LEADER_SKILL_MST 752, F_EXTRA_PASSIVE_SKILL_MST 772, F_UNIT_EVO_MST 598, F_UNIT_CGS_MST 812, F_ITEM_MST 1029, F_UNIT_COMMENT_MST 707.
- **Text:** it reissues the ten dictionaries under new names with **F_RESOURCE_MST 3277**, and copies or authors the art.
- **Safety:** writes are temp + rename; it is idempotent; `deploy/` needs `--backup-dir`; a staged version with different rows is refused (a version bump is required).
- **Not folded in:** the #14 crop (unit_mst_corrections) and the #22-3 text (sgtext_corrections). Staging either later must build on these versions.

**Tests:** `scripts/test_eu_units.py` (registered) **15/0**, on both candidate `92f41576` and the live build `f72310df`.
- An install into a hard-link mirror: every skill and evolution reference resolves; every unit has its sprite, portrait, thumbnail, full art, part file and cgs files; no cgs file addresses a missing frame.
- Every staged table equals the server table, at a version above deploy's. F_RESOURCE_MST lists each reissued dictionary, and each holds every key.
- A second install writes nothing, and deploy stays byte-identical.
- An isolated server boots, Initialize declares all ten versions, and UserInfo lists an owned EU unit.

**Client smoke, run v8** (S19 on `9633061d`, server `92f41576`, QA mirror):
- **Login:** the client downloaded the new tables and dictionaries, and Home showed both EU squad members with the new crops.
- **Unit list** and **detail** for Jack's Army: name, No. 10004, 7★, lord stats 6666/2500/2200/2200, and the leader skill and BB "Forgotten Flames" with their text.
- **Mission 10** cleared with 710016 (authored static sprite) and 750247 (animated twin sheet) in the squad. 750247 cast its BB with the full cut-in "Tyrant Obelion Slash". MissionEnd settled, and the server log has no errors.
- **Two crashes on the way, both fixture errors, not data:** the prep put three non-existent unit ids in the box, and cloned Klaus's stored skill columns.

**Installed into deploy** (Evan's instruction):
- 2,056 files; the 18 files they replaced, plus `deploy_before.sha256` and `written_files.txt`, are in `out/backups/2026-11-01_eu_units/`. `check` is complete.
- **For Cloudflare `assets.zip`:** `out/release/eu_units_assets_for_zip.txt` lists the 1,834 new content files (211 MiB).
- The live server picks the units up at its next restart. **Nothing was restarted and nothing was committed.**

**Regression after the deploy install** (`92f41576`): 70 entries. The first run had 11 failures: the 6 known ones, plus 5 the install caused, all fixed.
- **`test_unit_roster_wire`:** archive records wrote `'0'` for "no BB/SBB" where Global uses `''`, and the test knew no EU names. 18/0 after the fix.
- **`test_mst_bootstrap`:** its recorded inventory changed by design (four tables up a version, new dictionary names). Reviewed and re-recorded (pre-change copy in `R8/bfwd_before/`). 49/0.
- **`test_eu_units`:** expected deploy below its versions. 15/0.
- **`test_unit_mst_corrections`:** 16/0. Its real-data check now SKIPS because deploy's unit table is no longer the pristine round-3 source. The #14 crop must be rebased (readiness §2b).

**One install incident, repaired:**
- A rebuild run AFTER the install read deploy (which now holds the EU rows) as its "Global" baseline. The installer then refused its changed F_UNIT_MST, but only after it had already rewritten `deploy/mst/unit_mst.json` (unit numbers from 10225).
- **Fixes:**
  - the builder now refuses a baseline holding EU units, and takes `BF_GLOBAL_MST`/`BF_GLOBAL_ARCHIVE`, set to `out/backups/2026-11-01_eu_units/`;
  - the installer refuses before writing anything;
  - a re-install restored the server table, which now equals staged F_UNIT_MST 1089 row for row.

**Not done / authored, honestly:**
- 212 battle sprites are static stand-ins until real part files are found.
- Hit-effect visuals and attack trailing parameters come from templates.
- 750257 has no extra skill.
- 49 extra skills need EU-only spheres that are not in the game, so they never trigger.
- Not client-tested: evolution, a summon pool, and fusion with EU units. No banner or exchange offers them; they reach players only by grant or by an evolution from an owned unit.

## 3e. "Fix everything", continued (November 1): boss icons, fixture checker, #18-a, EU evolution

Every client run below: S19 on `9633061d…`, server `92f41576`, isolated ports 29960/29961, frames under `R8/visual/run/V9`…`V15`. Each archive is a QA copy; deploy/archive is unchanged (`mission.json` `c7002b84…`, `ai.json` `0b493118…`).

### Special-boss status-icon offsets: generated, client-verified on copies, staged

**The model alone hides a giant's icons.**
- `scripts/gen_boss_icon_offsets.py` first gave every measurable boss the ordinary-enemy rule (y = H + visible top + 3.5).
- **v9/v10:** no icon ever showed on Emperor or Maxwell. In v9 the bosses died before Klaus's burst landed; in v10 (200,000 HP) the burst did land and still no icon showed.
- The rule puts the icon about 20 px above the sprite's visible top. Emperor's visible top is about 230 units above its feet, i.e. above the battlefield.

**Calibration (v11).** I set y_off = H, which places the icon at the feet, and measured the icon centres:
- Juggernaut at 160:240: y 287.5; Emperor at 180:302: 350.5; Maxwell at 180:302: 357 (window px).
- Fit, with the verified 1.875 px per unit: the centre sits `pos_y/2 + K − (H − y_off)` battle units below the field top (window y 33), with K = 15.7 / 18.3 / 21.8.
- v11 also proved Klaus's debuffs land on bosses: DEF/ATK icons at their feet.

**Rule.** `y_off = max(model, H − pos_y/2 + 20)`. This keeps the icon centre at least about 38 units down (window y ≈ 105): below the zel bar (≤ 68) and the SPARK/CRITICAL line (≤ 90).

**Confirmation (v12), at the real story positions and generated values:**

| Boss | Position | Offset | Icon centre (window y) | Predicted |
|---|---|---|---|---|
| Juggernaut | 180:340 | 0,26 (model) | 130 | 130 |
| Emperor of Amdahl | 180:358 | 0,81 (clamp) | 104 | 106 |
| Creator Maxwell | 180:302 | 0,93 (clamp) | 112 | 111 |

- Each icon sits on the head or upper body, none over the HUD. Icons were located by NCC template matching on DEF/ATK crops from v11.
- **Not client-checked:** Cardes (0,109) and Defense System (0,125). They were waves 4–5 of v12, but my second capture started after they had been fought.

**Output.**
- 15 rows changed. Kept at `0,-60`: Cursed Estia Lv1–10 (no cgg), Xie Jing (no art) and the Battle Simulator (its own generator).
- QA archive `R8/archive_r8_icons3/mission.json` `1cadfba8…`. **deploy/archive is NOT regenerated** (release step).
- `scripts/gen_research_lab.py` copies story visuals into the trials. It now re-derives a measured offset for the trial's own position, so the Lab Abaddon gets 0,57 rather than the story's 0,93, and the two generators' `--check`s agree. Its `--check` against deploy is still "up to date".
- `scripts/test_gen_boss_icon_offsets.py`: 15/0, registered.
  - It pins the three client-verified values and checks that the Lab generator agrees.
  - Mutations caught: CLAMP = 0, and the Lab re-derivation switched off.

### Fixture checker (handoff §9 item 8, done)

`scripts/qa/check_fixture_units.py DB [--repair]`:
- **ERROR** (report only): unknown unit id (the View Units crash), or a squad slot naming a missing row.
- **CLONE**: a non-empty bb / sbb / leader skill / element that is not the unit's own; `extra_skill_id` other than the unit's SBB (UnitEvo keeps it as the legacy SBB mirror); or a stat outside 0.5× min to 1.6× max.
- **EXP**: `total_exp` short of the stored level.
- `--repair` works on files under `out/` only.
- Clean on the 10-24 save backup (109 rows), the S19 fixture and organic rows. Empty and 0 values are what organic rows carry, and are left alone.

What it found:
- **Every round-8 prep_r8 squad (v1–v12)** carried the cloned base row's stats (1150/529/377/474); in battle every member showed 1,495 HP. No round-8 conclusion depends on unit stats (icon placement, EXP, drops).
- **v8's EU rows** had the extra-skill id in the SBB-mirror column (my one-off repair). Nothing sends that column.
- `R8/tools/prep_r8.py` now writes each unit's own scaled stats. `prep_eu.py` runs the repair. Both leave 0 findings (v13–v15 fixtures).
- `scripts/test_check_fixture_units.py`: 13/0, registered.
- **Slip:** I ran `--repair` in place on the finished v12 run's save (only the five squad units' stat and mirror columns, after the run). The v12 frames are unaffected.

### #18-a story Juggernaut: resists and Weakness skill authored, client-verified on a new server candidate

`scripts/gen_story_bosses.py` (new; `--check` / `--out` / `--archive`, nothing written without `--out`) gives mission 85's Juggernaut its kit from Tower of Mistral rev 674494. The archive already matched every number the wiki gives (50,000 HP, Thunder, 8/8/180).
- **Resists:** immune to Weak, Sick and Paralysis; Poison, Injury and Curse land. This is an INTERPRETATION: it reads "can be injured, cursed, and poisoned" as the complete list.
- **"Causes Weakness":** skill 9600 Atom Cannon (AUTHORED choice), cast by AI 30000085 on 30 % of turns (AUTHORED rate). Juggernaut's own band 1900–1904 only ever inflicts Paralysis 15 %. 9600 is the monster-range Thunder AoE with Weak 45 % that draws the band's 400150 effect.
- QA archive `R8/archive_r8_story`: `mission.json` `d522af0b…` (icons3 + the kit), `ai.json` `4eecdb03…`.
- **It ships only with a server that sends `8aiBoHg5`** (next subsection). On today's servers the skill crashes the client.

### The monster-skill contract (Evan: "Research contract"): recovered, built, client-verified

**The crash.** Any monster skill crashed the Windows client at battle start: v13 (9600) and v14 (the Lab's own 1902), at +0xDE25E, read 0x54. Dumps: `R8/visual/run/V13/…16512.dmp`, `V14/…4636.dmp`.

**Why, from the x86 client:**
- `UnitSkillMstResponse::readParam` (x86 0x520080) files a row whose `ToZj4zyn` is 0 into `UnitSkillMstList`'s **temporary** dictionary (+0x18); otherwise into the master (+0x14).
  - `ToZj4zyn` is 1 on 3,799 of the 3,900 unit BB/SBBs.
  - It is 0 on 21,073 skills (only 101 of them a unit's burst), most of the monster range included; 8,841 skills no unit owns are 1 (Emperor of Amdahl's and Granvalm's among them) and stay in the master list.
- `GameUtils::battleTemporaryMstRemove` empties the temporary lists on entering Home (`AnotherHomeScene`/`HomeScene2` → `initAllContensFlg`), at the mission result and in raid scenes. So by the time a quest starts, every monster skill is gone.
- The mission preload (x86 `0x8268B0`, a GameScene→MissionScene cast, the twin of `DLCManager::requestMonsterAttackEffectFiles`) looks the skill up in both dictionaries, finds nothing, and reads through null.
- The level rows are fine: the downloaded F_SKILL_LEVEL_MST has no `ToZj4zyn`, so every level row is permanent (x86 0x51F5D0/0x51F7C0).

**The contract.** MissionStart re-sends the battle's skill rows under **`8aiBoHg5`**.
- getResponseObject builds `UnitSkillMstResponse(true)` for it (x86 factory 0x495C59 sets +0x18 = 1). That skips the master wipe and re-files each `ToZj4zyn`-0 row as temporary for this battle.
- **Never `5hafnym7`**: it builds the `(false)` variant, whose first row wipes the master list, i.e. every player burst.
- **Never `3ktU0D2e`**: it wipes the permanent level list.

**Built in server candidate `R8/bin_skills/gimuserverw.exe` `e37da19b…`** (92f41576 + this; build script `R8/tools/build_server_skills.sh`; the in-tree EXE and every earlier candidate were checked unchanged):
- `packet-generator/assets/net/handlers.kdl`: `MissionStartResp.skills` (`[SkillMst]`, key `8aiBoHg5`, optional).
- `gimuserver/archive/MissionArchiver.{hpp,cpp}`: a `populatePacket` overload that collects every wave and scripted monster's skills in first-use order, takes their rows from the server's own skill table, omits the block when there are none, and refuses an unknown skill id.
- `gimuserver/gme/handlers/Mission.cpp`: one line, calling it.

**Tests:**
- `scripts/test_mission_monster_skills_wire.py` (new, registered): 10/0 on `e37da19b`.
  - It checks: the rows equal the table key for key, first-use order, `ToZj4zyn` kept, no wiping key, no block on an ordinary mission, an unknown skill refused with nothing spent, and Lab trial 2000002's 15 skills including the scripted monsters'.
  - On `92f41576` the same test fails exactly the contract checks (5 of 10); the old server even starts a mission with an unknown skill and spends its energy.
- `scripts/test_gen_story_bosses.py`: 20/0 on `e37da19b`, including the 9600 row under `8aiBoHg5`.

**Client run v16** (S19 on `9633061d…`, server `e37da19b`): mission 10 wave 1 was the story Juggernaut with the full kit at 2,000,000 HP.
- **No crash.** The battle loaded, and Juggernaut cast **Atom Cannon** with its red name banner, hitting the whole party (frames 38–40 and 57).
- The quest cleared through all five waves (MissionEnd 200), and no crash dump was written.
- Klaus's ATK-down icon sat on Juggernaut's head (window y ≈ 128), matching v12.
- **Not confirmed:** whether Weak (45 % per unit) actually landed on the party; no WEAK icon was clearly visible.

**Consequence for deploy:** the live server (`f72310df`) does not send `8aiBoHg5`. Deploy's Research Lab trials (2000000–2000002 and 2100004) give monsters skills, so on Windows they very likely crash today. Deploying a server built from this source fixes them, with no archive change. Not run on the Lab missions themselves.

### EU evolution (handoff §3d "not client-tested: evolution")

- `scripts/test_eu_units.py` now runs UnitEvo on three EU recipes (first, middle and last of 135): 710013→710014, 730284→730285, 760284→760285.
- Each check covers: success, the new form's BB/SBB/leader skill, level 1, BB 9→4 and the materials consumed. **24/0.**
- The in-client evolution screen was not run.

## 3f. "Do it all" (Evan, November 1, late): rebases, eight story bosses, release proxy, EU evolution

Server candidate now **`R8/bin_skills/gimuserverw.exe` `4ab8af5b…`**, rebuilt from the final source. It is `e37da19b` plus comment-only corrections in `handlers.kdl`, which move line numbers in the debug binary; `e37da19b` is kept beside it as `gimuserverw_e37da19b.exe.keep`. The in-tree EXE and every earlier candidate were checked unchanged.

### #14 and #22-3 rebased above the EU install (staged, client-verified)

- **#22-3:** `scripts/sgtext_corrections.py` now builds on the F_RESOURCE_MST that deploy stages (3277, which lists the EU reissue `sgtext_leaderSkillDescription_499`) instead of the shipped 3275/497.
  - It writes `_500` (`7c4308c6…`) and F_RESOURCE_MST **3278** (parts `4fdc8364…`, `5257d165…`).
  - Every EU dictionary stays listed.
  - It refuses any dictionary not in `SOURCE_ZIP_SHA`.
  - Test: 14/0.
- **#14:** `scripts/unit_mst_corrections.py propose` on the EU-installed table (2,515 rows) changes exactly 50254 `1W9CxaFK` (proposal `3e8a4034…`). It is staged as F_UNIT_MST **1090** (parts `44337b57…`, `e6de035d…`).
  - `test_unit_mst_corrections.py` gained the rebased case: 19/0.
- Staged together in `R8/content_rebase` and `R8/mst_rebase` (a hard-link mirror of deploy with the proposal in place).
- **Client run v17** downloaded `Ver1090` and `Ver3278` and extracted `sgtext_leaderSkillDescription_500.csv`; Galtier's line reads ". 25% boost…" with no `<br>`. The Tilith Home crop itself was not re-photographed in v17.

### #18-b/c: the other eight story bosses (staged, client-checked)

`scripts/gen_story_bosses.py` now authors all nine story bosses. QA archive `R8/archive_r8_story`: `mission.json` `32d470c1…`, `ai.json` `ccfd3dda…`.
- **Sources:**
  - each dungeon page's full wiki revision history, about 620 revisions, scanned by `R8/tools/wiki_boss_hp.py` with the raw JSON in `R8/wiki_boss_hp/`;
  - the cached boss notes in `tools/wiki_bosses/boss_catalog.json`.
- **Skills:** every skill is the boss's own named burst (Black Force, Evil Hole, Necro Curse, Genesis, Endless, Blood Cyclone, Amdahl Vision, Chaos Rage, Gravity Nova, Hidden Dimension, Foreign Light, Destruct Drive, Heavy Barrage, System Repair, Genocide Order, Tail Swing, Crash Nail, Crunch Rack, Fatal Blow, Grand Destroyer), found by name in the monster range.
- **HP:**
  - Sourced: Defense System 180,000 and Granvalm 200,000.
  - Demon Abaddon's archived 370,000 is the Trial 03 boss, not the Tower of Morgan story fight.
  - AUTHORED from the sourced HP-to-mission-EXP line: Abaddon 70,000, Maxwell and Emperor of Amdahl 115,000, Zebra and Grantos 130,000, Cardes 150,000.
- **Turn patterns:** the ones the wiki states are exact (every 3rd/4th/5th turn; Cardes' Endless on turns 1, 6, 11 and his heal on 5, 10; Defense System's repair once below 50 %). Every percentage is AUTHORED. Resists follow the wiki's susceptibility lines (INTERPRETATION).
- **Not modelled:** ailments the wiki puts on ordinary attacks. Emperor's bursts carry no Paralysis/Poison in the data, nor Maxwell's any Weakness.
- **Correction to §3e:** `ToZj4zyn` is 0 on most of the monster range, but 8,841 skills no unit owns are 1. Emperor's 5480/5481 and Granvalm's 9310–9312 stay in the client's master list; the server re-sends them anyway, which is harmless.
- `scripts/test_gen_story_bosses.py`: **23/0** on `4ab8af5b`. It covers the names, HP line, resists and every AI over a whole fight in `ai_model.py`, plus MissionStart for all nine missions with the skill rows under `8aiBoHg5`.
- **Client run v17** (mission 10 waves: Juggernaut, Defense System, Cardes, Granvalm, Emperor of Amdahl at 800,000 HP; the rebased content and MST roots):
  - All five fought and the quest cleared, with no crash dump.
  - Juggernaut cast Atom Cannon twice; Cardes' turn-1 Endless played its script-like dialogue.
  - **Not confirmed:**
    - Weak landing on the party: no WEAK icon was seen; the party icons were DEF/ATK-related.
    - The other bosses' skill banners: the banner detector found none, and the boss waves were short (one to four turns each).

### Release proxy twin (9960) built; #24 regression on the release source

- `R8/tools/build_proxy_9960.sh`: the `f74efc2b` source built for the live port 9960, in `R8/proxy_9960_release/libcurl.dll` **`44dedefd…`**.
- It differs from the tested 29960 build in 17 bytes, all of them the port. The proxy's `build/bin` was restored to `e24234c1…`, and its cache equals the saved copy.
- **#24 (run v18):** `f74efc2b` swapped into S19 (from the `9633061d` backup), round 2's `g1_overflow` save and the Test Gate: Omni double gate, with round 2's tap timing (`R8/visual/run/V18/A1_omni/`, sheets `A1_sheet1/2.png`).
  - The rainbow gate's change plays its shard flash.
  - The black gate is then fully visible and tappable.
  - There is exactly one white after the final opening, then the Omni reveal.
  - This is the same as round 2's accepted A1.
  - S19 was restored to `9633061d…` straight after.
- Not built or done: the live client repack and signing (Evan).

### EU evolution in the client (run v19)

- `R8/tools/prep_evo.py` adds Wandering Jack 710013 at Lv 60 plus its four unit materials. The fixture checker reports 0 findings.
- Unit → Evolve Unit showed the EU recipe (Fire Totem, Dark Totem, 2 Bat Mimic; 200,000 Zel; "Can be Evolved").
- The evolution played the MEGA RARE reveal with the EU art and comment line. The unit screen showed **Wandering Lancer Jack 5★, Lv 1/80, 3200/900/900/1000, cost 13, "Blaze of Purgatory", "Ominous Blaze"**.
- Zel 600,000 → 400,000. In the database, row 1015 is 710014 and the four material rows are gone.

### #12 Vortex Trials: feasibility checked, not built

- 41 missions, 0 archived, 18 with a client script.
- None of the seven named bosses has a complete animated sprite, no revision gives a story-scale HP, and the mechanics are late-game (shields, Doom, Time Stop). The script phases use an undecoded trigger type 5.
- Recorded in [REMAINING_CONTENT_SCOPE.md](REMAINING_CONTENT_SCOPE.md) as a decision for Evan: stand-ins (authored and marked) or leave closed.

### Not done this turn

- **#22-1** (Lukroar gate negative control) and **#19** (items in Raid, Grand Quest and campaign battles): not run.
- **#20**, **#23** and **#30** need a natural-progression save, which does not exist.
- **#17** and **#15** wait on Evan's decisions.

## 3g. November 2: live server deployed; #17, #15 and #12 per Evan's decisions

Evan's answers:
- 1: "Yes, deploy the new server to the live setup".
- 2: #12 "is a wiki scrape / community authoring effort if nothing exists on the wiki".
- 3: "We want the EOS functionality for all systems".
- 4: #15 yes, and "scan the wiki for all units to confirm every single unit is playable so I can update the client accordingly".

### Live server: `4ab8af5b` deployed (Evan's approval, item 1)

- The candidate's exe/ilk/pdb were copied over the in-tree `out/build/debug-win64/standalone_frontend/Debug/` files, so the live exe is byte-identical to the tested `4ab8af5b…`.
  - Its 11 DLLs are identical.
  - It was started with `bf_ctl.ps1` at 11:47 UTC and is listening on 9960.
- **No migrations:** the schema still ends at `02102026_MissionContinueReceipts`. The log is clean except the two known SummonerJournal warnings.
- **Backups** are in `out/backups/2026-11-02_r8_server_deploy/`:
  - the save `239f9e69`, unchanged since 10-24, as a raw copy plus a backup-API copy;
  - `gimuserverw_f72310df.exe/.ilk/.pdb`.
  - Rollback = stop the server, move those three back, start it.
- **Only the binary shipped.** The staged content of readiness §3 (steps 2–6, the material drops and everything below) is NOT in deploy/.

### #17 Mecha Gods: built (EOS version), wire-tested

- Generator: `scripts/gen_mecha_gods.py`.
- Source: The Mecha Gods Awaken rev 659707.
  - All 44 revisions, 2015–2020, carry the same line-up, so my November 1 "2020 re-release" reading was wrong.
  - The archive's Totem finale was `gen_vortex_battles`' authored guess.
  - The footer: permanent in the Vortex since November 1, 2018, so this is the EOS content.
- Six Sanctums:
  - room 2 is the wiki's line-up, as copied rows;
  - the boss is the element's Mecha God, unit-backed, with the client dictionary's monster id (15850, 25800, 35950, 46850, 55900, 66000);
  - each boss has its two named skills (the 1054/1055 … 1854/1855 pairs);
  - "every 3 turns" lands on exactly turns 3, 6, 9;
  - 1–4 actions a turn ("up to 4 times per turn"; High Elf 3);
  - the wiki's susceptibilities apply.
- AUTHORED: HP 700,000 (the Guardians of Lore line), ATK 580 / DEF 100, room-2 HP 100,000, capture 40 %, and the percentages.
- `scripts/test_gen_mecha_gods.py`: **12/0**, including a rolled Fire Mecha God capture settling into the box.
- `validate_missions` gains no finding: H15 −6, M4 −15 together with the story bosses.

### #15 every unit: audited; the 29 JP-only units in the tables fixed (staged)

- `scripts/audit_unit_playability.py`, read-only, writes:
  - [UNIT_PLAYABILITY_AUDIT.md](UNIT_PLAYABILITY_AUDIT.md);
  - [unit_playability.csv](unit_playability.csv);
  - `out/release/unit_missing_client_assets.txt`: 2,023 client files to source.
- It reads the wiki's whole roster: 2,497 infobox pages, 2,490 ids, compacted to `scripts/data/wiki_units.json`.
- A unit is PLAYABLE when it has:
  - `unit_mst` and archive rows, its dictionary name and comment row, and resolving skill ids;
  - thumbnail / battle / full art, sprite sheet, part layout, and every animation script `F_UNIT_CGS_MST` names (SuperAnim counted).
- **Deploy today: 2,340 / 2,719 PLAYABLE.**

  | Gap | Units | Notes |
  |---|---|---|
  | JP-only, not in the client at all | 205 | `unreleased = true` on the wiki |
  | Global, no battle sprite, cgg or cgs | 129 | 100 of them pullable from the Test Gates / 2020 DBB selectors |
  | JP-only, in the tables but no archive record and no name | 29 | +2 with other gaps |
  | Name text missing | 8 | the Mystery Frogs and Wonder Mirror Idol |
  | Comment rows missing | 6 | |
  | Evolutions on the wiki but not in `unit_evo_mst` | 9 | |

- **Built: `scripts/jp_units.py`**, staged in `R8/jp_root`, a hard-link mirror on top of the #14 and #22-3 staging.
  - It adds archive records for the 29 using the column map `tools/gen_unit_archive.py` proved; it reproduces all 1,579 comparable existing records.
  - It adds English names: 22 the wiki's `altname`, 2 its title, 6 AUTHORED.
  - It adds AUTHORED translations of every skill, leader-skill and extra-skill text.
  - It adds names for the 8.
  - It stages seven reissued dictionaries (units 445, skillName 487, SkillDescription 640, leaderSkillName 413, leaderSkillDescription 501 carrying #22-3's fix, extraPassive 439/533) and **F_RESOURCE_MST 3279**.
  - `eu_units.reissue_dictionaries` is the EU install's text step, lifted into a function; `test_eu_units` is still 24/0.
  - `scripts/test_jp_units.py`: **12/0**. A present of 60936 Demon Lord Dino arrives with its own BB/SBB/LS, and the audit on the mirror counts all 37 PLAYABLE.

### #12 Vortex Trials: scraped; Winter Paradise built

- `scripts/data/vortex_trials_wiki.json` covers 39 of the 41 missions, matched to their wiki quest (latest revisions; the Burden of Aesys paths are on `Aesys:*` subpages). Per mission it records:
  - the revid and the signature check;
  - every boss with its phase HP and the unit whose art it can fight with;
  - every named skill with its F_SKILL_MST ids, found by exact name.
- **Finding:** the client already carries every boss's kit with the late-game effects as data (shields, Time Stop, turn skip, etc.), e.g. Odin 20000800-812, Osiris 140000xx. The original AI is not recoverable: the datamine's Global `ai.json` stops in May 2017. So each fight needs its AI authored from the wiki's prose triggers, approximating "shield up" / "adds alive", which the AI grammar cannot test.
- Seven bosses have no art (Baragan, Elhaza, Zaad, Melfel, Vizier of Chaos, True God-King, the dryads), and 18 missions need their client script contract decoded.
- **Built:** `scripts/gen_vortex_trials.py`, the maintained per-trial generator, starting with **83013400 Winter Paradise** (rev 665774: Imps, Burst Frogs, Void Tablet Emgif, Imbued Tablet Elgif; all capturable).
  - `scripts/test_gen_vortex_trials.py`: **11/0**. MissionStart is reachable, and MissionEnd settles.
- **QA content archive:** `R8/archive_r8_content` (story bosses + Mecha Gods + Winter Paradise). `mission.json` `adc18563…`, `ai.json` `ff9714dc…`. All three `--check` report "up to date".

### Client run v20 (Mecha Gods + JP text)

- **Setup:**
  - server `4ab8af5b`;
  - archive `R8/archive_r8_v20` = `archive_r8_content` + `jp_root`'s `unit.json`, with mission 10's five waves set to the Water / Fire / Earth / Thunder / Dark Mecha Gods (Water 1.5M HP, the rest 800k, capture forced to 100 %);
  - content root `R8/jp_root/game_content` (F_RESOURCE_MST 3279);
  - the v17 save with Demon Lord Dino 60936 and MEIKO 10783 added (fixture checker: 0 findings);
  - S19 on `9633061d`.
- **JP text:** the client fetched 3279. Dino's unit screen reads "Demon Lord Dino", 7★, with leader skill "Wandering Divine Soul" and Brave Burst "Drei Scourge" and their translated descriptions (`visual/run/V20/s06.png`).
- **Mecha Gods:**
  - every boss renders as its unit sprite with its name;
  - each acts several times a turn;
  - the Water Mecha God cast **Diamond Dust** with its banner on turns 1 and 2.
  - Every wave dropped its Mecha God ("MEGA RARE!!", x05), and the quest cleared.
  - The capture screen shows Water Mecha God No. 808, 5★, 5555 / 1200 / 1200 / 1200, "Use to evolve other Units!".
  - The save holds 10354, 20344, 30334, 40334 and 60344 at Lv 1.
  - No crash dump.
  - **Not seen:** the turn-3 Freezing Breeze banner. The frames around turn 3 were mis-ordered in my capture, and the Water god fell during turn 3.

### Regression (November 2, on `4ab8af5b`)

- `run_bugfix_regressions.py`: **76 entries, 2,909 passed, 6 failed**. Log: `R8/regression_20261102.log`.
- The 6 failures are the known Training Ground staged-data checks (`test_battle_simulator_wire` and `gen_battle_simulator --check`). They pass once deploy's `mission.json` is regenerated (readiness §3 step 4).
- New suites:
  - `test_gen_mecha_gods` 12/0;
  - `test_jp_units` 12/0;
  - `test_gen_vortex_trials` 11/0.
- `test_eu_units` is 24/0 after the refactor.
- **End state:**
  - the live server is `4ab8af5b` on 9960;
  - no QA processes are left;
  - S19 is on `9633061d`;
  - `deploy/archive/mission.json` `c7002b84` and `unit.json` `53198608` are unchanged;
  - no new crash dumps.

### Not done this turn

- Winter Paradise is wire-tested only (no client run).
- The 205 JP-only units need the JP datamine download (`cheahjs/bravefrontier_data` `jp/`: info 50.5 MB, bbs 49.9 MB, es / ls / evo / feskills / items about 5.6 MB). They also need their art.
- The 129 Global units need art.
- The other 40 trials need authored AI.

## 3h. November 2, later: staged content applied to live; every unit accounted for

Evan's answers:
- 1: "Apply it and test".
- 2: JP datamine "Yes, but see if you can't find their art from our files/ data scraping. Some of those Japanese units should already be implemented in our server so make sure first".
- 3: "Make placeholders but try your best to discover the content either online or in the files we have for all missing assets".

### Applied to live (Evan's item 1)

- **Order (readiness §3):**
  1. #22-3 (`_500`, F_RESOURCE_MST 3278), copied from `R8/content_rebase`;
  2. #14 (`unit_mst.json` `3e8a4034`, F_UNIT_MST 1090);
  3. Training Ground icons, then #39 Super boss (diff: 6000000 and 100601 only);
  4. boss icon offsets (13 missions);
  5. story bosses, then Mecha Gods, then Winter Paradise (`ai.json` = `ff9714dc`, the client-tested QA archive's);
  6. JP units (`jp_units.py install`, F_RESOURCE_MST 3279);
  7. material drops, last: 990 missions, 20,919 lists.
- Every generator's `--check` reads "up to date".
  - `game_content` and `mst` are byte-identical to `R8/jp_root`.
  - `validate_missions`: 3,117 → 3,097 findings (H15 −6, M4 −14), no new kind, C13/H16 clean.
- **Live now:**

  | File | Hash or version |
  |---|---|
  | `mission.json` | `5ce525d6…` |
  | `ai.json` | `ff9714dc…` |
  | `unit.json` | `f955a881…` |
  | `unit_mst.json` | `3e8a4034…` |
  | F_UNIT_MST | 1090 |
  | F_RESOURCE_MST | 3279 |
  | Server | `4ab8af5b`, restarted on 9960 |
  | Save | `239f9e69` (untouched) |

  - Boot log: 2,512 unit records, 1,340 missions, 27 AIs, "20919 monster(s) with per-hit material drops; 0 invalid list(s) refused"; no errors; no migrations.
- **Backups:** `out/backups/2026-11-02_r8_content_apply/`:
  - the archive, `unit_mst.json`, the two manifests and the save (backup API);
  - `jp_units/` holds the files `jp_units.py` replaced;
  - rollback = stop, copy them back, start.
- ⚠ **Deploy's files were hard-linked into QA mirrors** (up to 21 links each). The first in-place `cp` of `unit_mst.json` and two manifests wrote through into `eu_units_root`, `jp_units_root` and `mst_gate`.
  - The mirrors were restored from the backup, and every deploy file about to change was relinked (copy + rename) first.
  - The installers' `put` was always temp + rename. Hand copies are not.
- **Tests made apply-aware:**
  - The generator tests diff against `bf_testkit.PRE_RELEASE` (the backup) once deploy carries their output.
  - `test_jp_units` starts its mirror from the backup's pre-JP files.
  - `test_eu_units` accepts a later release staged above its versions.
  - `test_sgtext_corrections` verifies the released fix (re-staging it is refused by design).
  - `test_unit_roster_wire` counts the JP names (30645 → 30646 no longer leaves the archive).
  - `test_mst_bootstrap` and the `validate_missions` baseline were re-recorded; the diffs are exactly the applied versions.
- **Full regression on the live exe:** every entry passes. The two Training Ground entries that failed before the apply now pass.

### Client run v21 (isolated copy of the released archive; S19 `9633061d`)

- **Winter Paradise cannot be reached from the Vortex.** Its dungeon 80013400 sits directly in area 100003, and this end-of-service client turned 100003 into the "Vortex Trials" parent tile, which lists only its four child areas. The server permits it (dungeon and mission). The 2018 event predates the parent.
- The fight was run as mission 10's waves (`R8/archive_r8_v21b`, Burst Frog wave doubled to keep five battles):
  - the Imps, Burst Frogs, Void Tablet Emgif and Imbued Tablet Elgif all rendered and acted;
  - the quest cleared;
  - Guard Imp Ganju, Burst Frog and Void Tablet Emgif (60812, drawn and named by its unit entry) were captured into the box.
- **SuperAnim units work:** Megurine Luka 60584, Red Hood Ciara 810104 and Snow Queen Eva 850116 were drawn from `unit_sam/`, and their Brave Bursts played with cut-ins ("Zealous Sweep", "Double Lariat").
  - ⚠ The battle crawled: one CPU core pegged, roughly one turn a minute. **Corrected (run v23): not SuperAnim.**
    v23 crawled the same way with no SuperAnim unit (Sanctum of Light; Galtier, Tilith, Harley, Klaus, Durumn),
    even with the window in front, while v22's battle ran at normal speed. The cause is not established (the S19 test
    client renders on one core; software rendering is a guess); it is not a data defect.

### Every unit, accounted for (items 2 and 3)

- **Our files first.** Every BF folder, client package, Downloads/Desktop/Documents, and `C:\Users\Evan\BF\21900\assets.zip` / `gifs.zip` (listings) were searched: **no file exists for any of the 205 JP-only ids.**
- **The 129 "Global units without battle art" were an audit error.** All 129 are SuperAnim units.
  - Their complete `unit_sam/unit_<id>/` sets are in deploy, and F_RESOURCE_MST lists them.
  - The client queues every resource whose path holds `/unit_sam/unit_<id>` (x86 `0x8265B0`) and caches by file name.
  - The audit only looked for `unit/cgs/unit_anime_<id>.sam`, the extra copy Korzan and Haile happen to ship.
  - Fixed, and client-verified (v21).
- **99 of the 205 JP-only ids are already in the game** under their Global counterpart. Each JP page says "Not to be confused with Global counterpart, [[...]]". Examples:
  - 赤頭巾シアラ 10653-6 = Red Hood Ciara 810104-7;
  - the 鏡音 Rin/Len lines = 840204-6 / 850204-6;
  - 巡音ルカ 160583-5 = 60583-5;
  - the 戦姫 maidens = 8x0003-6.
  - They are recorded in `scripts/data/jp_units/jp_only_roster.json`, and the audit's new verdict COUNTERPART lists them.
- **The other 106, plus Nazil 5★ 60614** (its wiki page carries 60613's id, so the extract missed it), are **built and staged, not installed**:
  - **Data:** the JP datamine (`cheahjs/bravefrontier_data` `jp/` at `2617690e`, 2020-07-22; seven files, 103.6 MB in git-ignored `tools/jp_datamine/raw`).
    - Built by `tools/jp_datamine/build_jp_units.py` on the EU builder's encoders.
    - Five specs added: procs 68/75/130 and passives 49/100, each reproducing a Global raw string.
    - Four evolutions end in the Global counterpart (11175 → Fallen Morgana 710168, 41055 → Mordred 840607, 51355 → Avalon Arthur 750166, 61294 → Wizard Merlin 860607).
  - **Text:** `scripts/data/jp_units/exclusive/text_en.json`, AUTHORED English for 661 lines.
    - Collab units take their wiki titles; seven use the wiki's English altname.
    - The mirror-world series reverse the Global boss names their kana reverse (ルェウスクマ = Llewxam).
    - 167 skills, 92 leader skills and 17 extra skills were in the client's tables with no text at all.
  - **Art:** the wiki's own files (381 files, 28.8 MB, sha1-checked; `&format=original`, since the CDN serves WebP otherwise). They are in git-ignored `tools/jp_art/raw`.
    - Thumbnails and full art are as published. The nine DC uploads (about 200x300) are scaled up 2-3x, and their Home crops fall back to the figure box (their thumbnails are not cut from the full art).
    - **Battle sprites are AUTHORED stand-ins** (`scripts/unit_sprite_stand_ins.py`):
      - 60 are animated from the wiki's idle/attack/move GIFs, attack registered onto the idle pose, timing scaled to the datamine's animation lengths;
      - 47 are static (no GIF on the wiki).
  - **Install:** `scripts/jp_exclusive_units.py`, at F_UNIT_MST 1092 … F_RESOURCE_MST 3281 (1091/3280 were burnt by the v22 attempt).
    - `eu_units.install_tables` / `install_archive` are the EU install's steps, lifted out; `test_eu_units` passes.
  - **Tests:** `test_jp_exclusive_units.py` 16/0 (with the wire grant of Harley Quinn); `test_unit_sprite_stand_ins.py` 10/0.
  - **Client run v22** (mirror `R8/jpx_root`):
    - the Home panels show Harley Quinn, Flash and Otherworld Rift God Sedrac correctly framed;
    - the battle panels show their English names;
    - Harley and Flash animate, run and attack; Sedrac's static figure advances and strikes;
    - Harley's Brave Burst cut-in reads "Mental Beatdown".
- **Audit, deploy today:** 2,506 PLAYABLE, 99 COUNTERPART, 106 NOT IN CLIENT (the staged ones), 8 INCOMPLETE.
  - The 8: 50876 has no leader-skill row; 9850167 is a guest form; six element-guide units have no comment row.
  - With the staging installed, all 107 are PLAYABLE.
  - `out/release/unit_missing_client_assets.txt` is now 848 files: the 106 staged units' set (the install provides them).

### Not done

- The 107 JP-exclusive units are not in live (an Evan decision).
- Winter Paradise needs a Vortex entry before players can reach it.
- The test client's slow battle rendering (v21, v23) is not explained.
- 50876, 9850167 and the six comment rows remain.
- The other 40 Vortex Trials still need AI. The JP datamine also has a 14 MB `jp/ai.json` (not downloaded) that may hold some of the trial bosses' original AI.

### Installed into live (Evan: "Install them", November 2, evening)

- Save backed up (backup API), server stopped, `python scripts/jp_exclusive_units.py install --root deploy
  --backup-dir out/backups/2026-11-02_r8_jp_exclusive`: 900 files written, 20 replaced files backed up; `check`
  complete; a second install writes nothing. Deploy's mst, archive, sgtext, F_RESOURCE_MST, F_UNIT_MST and unit art
  are byte-identical to the tested mirror.
- Live now: `unit.json` `8c079147…`, `unit_mst.json` `a1ea5a5b…`, F_UNIT_MST 1092, F_SKILL_MST 1539,
  F_SKILL_LEVEL_MST 1490, F_LEADER_SKILL_MST 754, F_EXTRA_PASSIVE_SKILL_MST 774, F_UNIT_EVO_MST 600,
  F_UNIT_CGS_MST 814, F_UNIT_COMMENT_MST 709, F_RESOURCE_MST 3281 (dictionaries units_446, skillName_488,
  SkillDescription_641, leaderSkillName_414, leaderSkillDescription_502, extraPassiveSkill_Name_440 / _DESC_534).
  Server restarted on 9960: "Loaded 2619 unit archive records", no errors; save `239f9e69` untouched.
- Audit on live data: **2,613 PLAYABLE, 99 COUNTERPART, 8 INCOMPLETE** (the old oddities); the missing-client-assets
  list is empty.
- Tests made install-aware: `test_jp_exclusive_units` (writes nothing on an installed mirror; versions equal
  deploy's; the text's "before" is the backup's F_RESOURCE_MST), `test_unit_roster_wire` (the exclusive units'
  English names); `test_mst_bootstrap` re-recorded (diff = exactly the install). `bf_testkit.PRE_JP_EXCLUSIVE`.
- Rollback: stop; copy `out/backups/2026-11-02_r8_jp_exclusive/` back over deploy (the replaced files); the new
  files (Ver1092…, the seven new zips, the units' art) are inert without the old manifests.
- Players get the new tables and text at their next login; the per-unit art downloads on demand. The Cloudflare
  `assets.zip` is not updated.

### Client run v23: visual pass on live data (Evan: "Run visual tests")

Isolated copy (`R8/live_root`, a hard-link mirror of deploy after the JP install; save = v22's; squad Galtier,
Tilith, Harley Quinn, Klaus, Durumn at max level); S19 on `9633061d`; frames in `R8/visual/run/V23/`.
- **Login and Home:** no download errors. Tilith's Home panel (#14) shows her face and figure; Harley's panel is framed.
- **JP unit detail:** Harley Quinn, Unit No. 10235, 6★, Lv 100/100. Leader skill "Big Trouble" and BB "Mental Beatdown",
  both with English descriptions.
- **Mecha Gods by the real route:** Vortex Gate → Enhancement → The Mecha Gods Awaken lists all six Lv. 6 Sanctums with
  their Mecha God and owned count.
  - Sanctum of Light: Light Idol and Totem, then room 2, then a "WARNING" intro and the **Light Mecha God**.
  - The boss acted and cast its named skill (banner "Southern Cross", turn 5).
  - The squad's BB cut-ins played (Flashfire Shockwave, Inundation, Mental Beatdown, Black Harvest).
  - Cleared on turn 6: Zel 5,074, Karma 40, XP 7,000, 70 energy.
  - Materials screen: Angel Tear (per-hit drops). Captures: Light Idol, Mimic. The Mecha God was not caught (a 40 % roll).
- **#22-3:** Galtier's leader skill "March of the Wild Hunt" reads "50% boost to all parameters, boosts Atk and Def
  when BB gauge is over 50%, probable damage reduc…" with no `<br>` in the part shown. The rest of the ticker was too slow to
  read; the dictionary row itself is checked by `test_sgtext_corrections`.
- **Not run:** Training Ground icons. That live data is byte-identical to the round-8 copies that passed.
- **Rendering:** the client again ran slowly (one core pegged) with no SuperAnim unit. See the correction under v21.
- No crash dump; no QA process left; the live server and save were untouched.

### Client run v24: exclusive units only (Evan: Harley not centred on Home, not visible in battle)

Isolated copy of live (`R8/live_root2`), save with 50 exclusive units in ten squads (`R8/tools/prep_v24.py`), each squad
run into a battle and every unit tapped to attack (`R8/tools/v24_squads2.ps1`); frames in `R8/visual/run/V24/`.
- **Home crop fix (authored, staged, NOT live):**
  - 20 JP units have wiki thumbnails that are not crops of their full art. Their Home strip fell back to the figure box,
    which centres on the picture, not the character (Harley showed the edge of her face).
  - `scripts/data/jp_units/exclusive/home_crops.json` now gives 22 strips (those 20 plus the two Armored Knights)
    by eye; `build_jp_units.py` applies them.
  - Staged as F_UNIT_MST **1093**. The difference from live is `mst/unit_mst.json` plus the 1093 parts and manifest.
  - Client-verified: Harley centred on Home (squad 1); the static squad 7 framed on each character.
- **Battle, 50 units:**
  - The 60 animated JP stand-ins render, idle, move and attack correctly in formation (squads 1-6, 10), BB cut-ins included.
  - Some wiki attack GIFs bake an additive glow as an opaque grey or brown disc (e.g. Gandalf's spell). The GIF cannot
    store additive blending, so this is a limit of the source.
  - The EU units on real Global sheets (Mai Shiranui, Kula Diamond) look native.
  - **Static stand-ins** (47 JP, 212 EU) read as busy illustrations: a squad of five overlaps into a wall of art (squads
    7-8). Measured, they are not oversized: 8-12k visible px, against about 29k for a real 7★ sprite.
    - A smaller, character-centred cut was built, tested and **dropped**: it read as a boxed cut-out.
    - The real fix is real sprite sheets (the EU folder's `eu_parts` sheets lack part layouts; none exist for the JP ones).
- Harley in battle is small next to Global 6-7★ sprites: the DC collab's original sprites were chibi (48x61 frames).
- Corrected the earlier claim: sprite height per rarity from the cgg extents (68-116 px) understated real sprites. Rendered,
  the visible heights run 100 px (4★) to 212 px (Omni).

### Home crops applied to live (Evan: "Yes apply", November 3)

- Live client absent; save backed up (backup API, integrity ok) and hashes recorded in
  `out/backups/2026-11-03_r8_home_crops/`; server stopped with `bf_ctl.ps1`.
- `python scripts/jp_exclusive_units.py install --root deploy --backup-dir out/backups/2026-11-03_r8_home_crops`:
  4 files written (`mst/unit_mst.json` `8d1d3cc4…`, F_UNIT_MST `Ver1093` parts `268f4ccf…` / `fe0052d1…`, manifest
  `4851d5fd…`), the replaced `unit_mst.json` and manifest backed up. `check` complete; a second install writes nothing.
  All four are byte-identical to the client-tested mirror `R8/live_root2`.
- Restarted on 9960: "Loaded 2619 unit archive records", no errors (the two known SummonerJournal warnings only); save
  `239f9e69` untouched. Players fetch F_UNIT_MST 1093 at their next login.
- Rollback: stop; copy the backup's `mst/unit_mst.json` and `game_content/content/F_UNIT_MST/manifest.json` back over
  deploy (the `Ver1093` parts are inert behind the 1092 manifest).
- Full regression on the live exe `4ab8af5b` afterwards: **79 entries, 2,950 checks passed, 0 failed**; `validate_missions.py`
  matched its baseline. `test_mst_bootstrap` failed once on its recorded inventory (F_UNIT_MST 1092) and was re-recorded:
  the diff is exactly F_UNIT_MST 1092 -> 1093 (same 2,622 rows); rerun 49/0. Full summary
  `out/qa/SUMMARY_2026-11-03_home_crops_full.md`.

## 3i. November 3: release package, EU sprites, evolutions, the Vortex Trials (staged, not live)

Evan: "Build it, then lets look into the content still missing to author some of it back into existence ...
take note of it in a missing content document" ([MISSING_CONTENT.md](MISSING_CONTENT.md)).

### Player release package

- `out/releases/bf-server-20261103-091258-757653`: `server.zip` and four `game-content-NNN.zip` (3.2 GB). Built from
  the live content.
- The extracted-package smoke test passed (`packaging/Test-Release.ps1`): 169 MST versions and 182 parts verified,
  save persisted across a restart.
- Fixes needed to get there:
  - `AchievementDeliver.cpp` and `UnitMix.cpp` join the C4737 `/Od` list (`C4737_SOURCES` in
    `gimuserver/CMakeLists.txt`; a `ninja -k 0` pass found no other);
  - `Test-Release.ps1` counts only the announced version's parts (folders keep superseded versions).
- `VCPKG_ROOT` in the environment points at a missing `C:\BF\vcpkg`: pass `-VcpkgRoot C:\Users\Evan\BF\vcpkg`.
- `out/release/assets_zip_delta_since_20260922.txt` lists the 2,728 new and 8 changed content files (330 MiB) for the
  Cloudflare `assets.zip`.

### Evan's EU client dump (`Downloads\bf\bf\estratto`)

- It holds the EU client's own part layouts (cgg). Sheets are pixel-identical to the EU folder's.
- **187 of the 212 static EU stand-ins get their real sprite:** `eu_units.real_sprite()`, sets staged in
  git-ignored `tools/eu_sprites/` with `SOURCE.txt`.
- Your live client has cached none of those units' sprites, so the real ones reach it on first view.
- No JP-exclusive unit is in the dump.

### Units

- **9 missing evolutions** (`build_jp_units.py`: wiki-named, datamine recipe, target in game), staged as
  F_UNIT_EVO_MST 601.
- 4 of the audit's 13 "wiki evolves into" were false: the JP target is routed to its Global counterpart.

### Vortex Trials (#12)

- **All 41 missions built** by `gen_vortex_trials.py` v2.
- **Root cause found:** the trial areas were unreachable because the server never permitted area ids above
  200000. Fixed by the subtree permits: candidate server `out/qa/current/r9_content/bin/gimuserverw.exe`
  `ee6f920f…`, built with the round-8 override recipe; the live exe was checked unchanged.
- **Winter Paradise** moved into its own area: `vortex_trials_topology.py`, F_DUNGEON_MST 1004.
- **Trigger 5 decoded** (BattleScriptList::getAiFlgEvent): the boss AI's flag N drives the script.
- **Tests:**
  - `test_gen_vortex_trials` 19/0 on the live exe (wire: MissionStart and MissionEnd for all 40);
  - AI model: no stalls; 405 of 471 skill slots fire in a modelled 60-turn fight.
  - `test_vortex_trials_topology.py` 8/0 on the candidate. It is run by hand at deploy time, not registered: on
    the live exe its Winter Paradise checks fail by design.
  - Full regression on the candidate `ee6f920f` (live data): **79 entries, 2,959 checks, 0 failed**;
    `validate_missions` matched its baseline (summary `out/qa/SUMMARY_2026-11-03_candidate_ee6f920f.md`).
- **Client run v25** (S19 `9633061d`, a copy of the live save, the candidate):
  - the Vortex Trials tile opens onto its five child areas, Winter Paradise and Mythological Entities included;
  - BF2 lists its trials with banners;
  - The Warmonger fought 4 turns: stand-in art, name banner, drops counted, no crash, no dump.
- **Client run v25b** (the same, plus a QA-only archive `r9_content/archive_lowhp` with the Savage Lion's and
  Warmonger's phases at 3,000-4,000 HP):
  - The Savage Lion's late-game fire kit executed on x86 and wiped the party; one continue was used.
  - Phase 1 sat at 0 % (the script's insensitive floor). The **AI-flag chain then advanced**: phase 2, phase 3,
    "AREA CLEARED" on turn 5. Phase 1 cannot die, so only the swaps can clear the fight.
  - Server: "MissionEnd: first clear of 8301560 -> 1 reward(s) queued (6:850808:1:1:1)", the wiki's Opal Goddess
    Tilith.
  - No new crash dump.

### The hard-link incident

Writing the generator's output into the hard-linked mirror rewrote **deploy's** `mission.json` / `ai.json`.
- The live server was unaffected: it reads the archive at boot only.
- Restored minutes later from the release staging copy (`5ce525d6` / `ff9714dc`) via temp+rename.
- A scan showed no other deploy file written.
- `gen_vortex_trials.py` and `gen_material_drops.py` now write temp+rename.

### Deployed (Evan: "Deploy it", November 3)

- **Backups:** `out/backups/2026-11-03_r9_deploy/`:
  - the save (backup API, integrity ok);
  - `bin/gimuserverw_4ab8af5b.*`;
  - `archive/mission.json` / `ai.json`;
  - every file the installers replaced;
  - `before_hashes.txt`.
- **Install, server stopped:**
  - candidate `ee6f920f` exe/ilk/pdb copied in-tree (DLLs already identical);
  - `eu_units.py install` (383 files);
  - `jp_exclusive_units.py install` (3);
  - `vortex_trials_topology.py install` (4);
  - `gen_vortex_trials.py --out deploy/archive`;
  - `gen_material_drops.py` (already current).
- **Checks:**
  - every installer's `check` and every generator's `--check` clean;
  - 24,465 files byte-identical to the client-tested mirror. The one difference: the mirror's `ai.json` carried 38
    orphan AI records from an earlier generator version.
- **Boot:**
  - "Vortex Trials subtree — 17 area(s)";
  - 1,379 mission and 110 AI records;
  - 20,919 drop lists, 0 refused;
  - no errors.
- **Rollback:** stop; restore `bin/gimuserverw_4ab8af5b.*` and the backup's archive, mst and game_content files over
  deploy; start. The new `Ver601` / `Ver1004` parts and the EU sprite files are inert or harmless behind the old
  manifests.
- **Regression on the deployed `ee6f920f`:**
  - 77 entries passed; the only failures were the two expected baseline mismatches.
  - Re-recorded after review:
    - `validate_missions` +11 M2: the trial normal/Hard pairs share a display order in the client's own
      mission_mst;
    - `validate_missions` +4 M4: the Skeleton-wave missions have no skills;
    - `test_mst_bootstrap`: exactly F_DUNGEON_MST 1003 -> 1004 and F_UNIT_EVO_MST 600 -> 601.
  - `test_vortex_trials_topology.py` is now registered (8/0).
  - All 80 entries pass. Live archive `mission.json` `6ba80131`, `ai.json` `8c693248`; save `239f9e69` untouched.
- **Player package rebuilt** from the deployed content: `out/releases/bf-server-20261103-133810-87d212` (Release exe
  `bf3dd5ba`). The smoke test passed; the 09:12 package is superseded.
  `out/release/assets_zip_delta_since_20260922.txt` is regenerated: 2,730 new and 9 changed files, 337 MiB.

### Not done

- The 50876 unit (buildable from the JP datamine).
- Omni+ Boost (needs one client tap), guild raids, Journal progress sources: see MISSING_CONTENT §3.
- Nothing above is live. Deploying it needs Evan's word: the server, the topology, the generator output and a
  material-drop rerun, together.

## 4. Content proposals

Recorded in [REMAINING_CONTENT_SCOPE.md](REMAINING_CONTENT_SCOPE.md) under the October 31 correction ("Round 8 evidence"). History is preserved; the pre-edit copy (rebuilt by removing only my insertions) is at `R8/REMAINING_CONTENT_SCOPE_before_round8.md`.

- **#18:** the story Juggernaut matches rev 674494 on every number the wiki gives. The Lab AI is not reusable: different skill ids and a 10× HP trial. Proposed: author a Weakness skill plus resists from the wiki, marked as authored.
- **#17:** the archive is the older Lv. 6 layout. Rev 659707 is the Mecha God re-release, with the room-2 roster and skill names; HP and capture are blank and not in client data. The version is Evan's decision.
- **#12:** the first encounter is 8301560 "The Savage Lion" (1 battle, energy 25, reward unit 850808, client script yes).
- **#15:** decision unchanged.

## 5. Regression (isolated fixtures)

`python scripts/run_bugfix_regressions.py out/qa/current/fresh_round4_20261030/bin/gimuserverw.exe` (server `c28a8db9…`, a fixture copied from the live save, read only).

**Result: 67 entries, 2,754 checks passed, 6 failed (runner exit 1).**
- The failed entries are exactly the two expected ones:
  - `test_battle_simulator_wire.py`: 93 passed, 6 failed. The six failures are the new `hp_disp_pos "0,30"` checks, run against the deploy archive, which still has `0,-60`.
  - `gen_battle_simulator.py --check`: "stale: mission.json".
- Both pass once deploy/archive is regenerated (readiness §3 step 4).
- Against the round-8 archive copy, `--check --archive R8/archive_r8_icons` prints "up to date".
- Every other suite passed. `validate_missions.py` matched its baseline.

For comparison, round 5 had 67 entries, 2,755 passed and 0 failed. The difference is the `--check` marker entry (now stale) plus the 6 new checks.

Log: `R8/regressions_r8.log`. Summary: `out/qa/current/SUMMARY.md` (also copied to `R8/`).

**After §3b, on the new candidate `92f41576`:** **68 entries, 2,772 passed, 6 failed.**
- `test_mission_material_drops_wire.py` is new: 19/0.
- The failed entries are the same two expected ones (deploy/archive not regenerated).
- `validate_missions.py` matched its baseline.
- Log: `R8/regressions_r8_items.log`. Summary: `R8/SUMMARY_items.md`.

**After §3c (generated drop content), same server:** **69 entries, 2,806 passed, 6 failed.**
- `test_gen_material_drops.py` 30/0 (new).
- `test_mission_material_drops_wire.py` 19/0.
- `validate_missions.py` matched its baseline, now with C13/H16.
- The failed entries are the same two expected ones.
- `test_brave_slots_wire.py` (249 → 251) and `test_reward_lock_snapshots_wire.py` (20 → 22) have data-dependent check counts, both at 0 failed.
- Log: `R8/regressions_r8_drops.log`. Summary: `R8/SUMMARY_drops.md` = the current `out/qa/current/SUMMARY.md`.

**After §3d/§3e, same server `92f41576`:** **73 entries, 2,862 passed, 6 failed.**
- The failed entries are the same two expected ones (deploy/archive not regenerated).
- New and changed suites: `test_gen_boss_icon_offsets` 14/0 (15/0 after the Lab-generator check was added), `test_check_fixture_units` 13/0, `test_gen_story_bosses` 20/0, `test_eu_units` 24/0.
- Log: `R8/regressions_r8_final.log`. Summary: `R8/SUMMARY_final.md`.

**On the skills candidate `e37da19b` (handoff §3e, monster-skill contract):** **74 entries, 2,865 passed, 6 failed**: the same two staged-data entries. New: `test_mission_monster_skills_wire` 10/0; `test_gen_story_bosses` 20/0 with the skill. `validate_missions.py` matched its baseline. Log: `R8/regressions_r8_skills.log`, summary `R8/SUMMARY_skills.md`.

**Final, on `4ab8af5b` (handoff §3f):** **74 entries, 2,879 passed, 6 failed**: the same two staged-data entries. `test_gen_story_bosses` 23/0 (nine bosses), `test_sgtext_corrections` 14/0 and `test_unit_mst_corrections` 19/0 (rebased), `test_mission_monster_skills_wire` 10/0, `test_gen_boss_icon_offsets` 15/0, `test_eu_units` 24/0. Log: `R8/regressions_r8_final2.log`, summary `R8/SUMMARY_final2.md` (= the current `out/qa/current/SUMMARY.md`).

## 6. Files and hashes (SHA-256 prefixes)

**offline-proxy** (pre-edit copies in `R8/proxy_before/`):

| File | Hash |
|---|---|
| `OfflineMod.win32/content/cp_gate_flash.cpp` | `f6a68ca630cba1e0` |
| `OfflineMod.win32/content/cp_gate_flash_internal.h` | `fa320ea697683efd` |
| `OfflineMod.win32/content/cp_gate_flash_logic.cpp` | `99e65aec33f89787` |
| `OfflineMod.win32/content/cp_name_fit.cpp` | `4e4cee22920921dd` |
| `OfflineMod.win32/content/cp_name_fit_internal.h` | `a6441084638616c2` |
| `OfflineMod.win32/content/cp_name_fit_logic.cpp` | `d6d9c5b640a1cb34` |
| `tests/test_main.cpp` | `c5d4d4572863240e` |
| `tests/fixtures/check_name_fit_site.py` | `65ea323d95d5eaad` |
| unchanged: `cp_signatures.h` `32af44bb…`, `tests/CMakeLists.txt` `cdff5e5c…`, `OfflineMod.win32/CMakeLists.txt` `e58ae3bc…` | |

**BF-WorkingDirRust:**
- `scripts/gen_battle_simulator.py` `6c9c29a1c205b3b4` (pre-edit = HEAD, copy in `R8/bfwd_before/`)
- `scripts/test_battle_simulator_wire.py` `b9273fa4d38f3b04` (pre-edit copy in `R8/bfwd_before/`)
- `docs/BUG_ACCEPTANCE_2026-10-27.md`: Round 8 section appended (pre-edit `R8/BUG_ACCEPTANCE_before_round8.md`)
- `docs/REMAINING_CONTENT_SCOPE.md`: round-8 evidence appended
- `docs/RELEASE_READINESS_2026-10-31.md` (new) and this handoff (new)

**Per-hit item drops (§3b)** (pre-edit copies in `R8/items_before/`; the rebuilt net schema hashes to round 4's `b524a34c…`):

| File | Hash |
|---|---|
| `packet-generator/assets/net/mission.kdl` | `4e292437c607b46b` |
| `packet-generator/assets/archive/mission.kdl` | `a0a4231ad5136c2c` (pre-edit = HEAD) |
| `gimuserver/archive/MissionArchiver.cpp` | `08a3cfe9cf74bde9` |
| `scripts/test_mission_material_drops_wire.py` (new) | `10f9f322e09b5550` |
| `scripts/run_bugfix_regressions.py` (one suite registered) | `ae08f4579c8b7b0a` |
| generated, git-ignored: `gimuserver/packets/all.hpp` | `e31d0173c0690ba9` |
| generated, git-ignored: `gimuserver/archive/archive.hpp` | `141966b0c14c4dbd` |

**Per-hit drop content (§3c)** (pre-edit copy of the validator in `R8/bfwd_before/validate_missions.py.pre_drops`):

| File | Hash |
|---|---|
| `scripts/gen_material_drops.py` (new; docstring records the client trace) | `c8e186d70a5dc9de` |
| `scripts/data/material_drop_sources.json` (new) | `b95b04af639cd07e` |
| `scripts/test_gen_material_drops.py` (new, registered) | `c49e7d8ace72e4b0` |
| `scripts/test_mission_material_drops_wire.py` (strips deploy's lists) | `6405532b261adab8` |
| `scripts/validate_missions.py` (C13, H16, `--archive`) | `90adc9ef65e48e59` |
| `scripts/run_bugfix_regressions.py` (one more suite) | `57f92ab03300e01e` |
| QA only: `R8/archive_r8_drops/mission.json` (deploy + drops) | `30aac9468f88bb8c` |
| QA only: `R8/archive_r8_v7/mission.json` (icons + drops) | `3f51875fec688f60` |

**packet-generator:** the two `.kdl` files above.

**QA artifacts** (not release files):
- `R8/tools/prep_r8.py` `fbc1790f…`
- `R8/tools/r26_summary.py` `1fe77a94…`
- `R8/tools/build_proxy_r8.sh` `61191d92…`
- `R8/archive_r8_icons/`, `R8/archive_r8_control/` (QA only: mission 10 wave 1 at 50M HP)
- `R8/mutation/`

## 7. Visual routes (for the next round)

- **Run tool:** `scripts/qa/prep_run.py --archive-root` takes a copied archive. `run_ctl.py start ... --relay-dump MissionStart MissionEnd`.
- **Login route** as in the brief. The tap at (422,708) is the Daily Spin.
- **Popup:** Unit (272,885) → View Units (282,368) → unit tile (from a screenshot) → BB bar (420,863). Back is (165,228).
- **Mission 10:** Home → Quest banner (690,720) → Adventurer's Prairie (596,832) → Start of Adventure (420,330) → No Helper (663,228) or a helper row → Begin (420,850) → **AUTO (598,407)**.
  - The Quest Clear Reward screen comes about 15 s after the last wave. Do not tap during battle: the taps skip the result screens.
  - Afterwards: 4 capture screens (tap centre, then Back), then a first-clear story scene. Taps at (420,700) on Home open the Vortex banner.
- **Mission 11 (run v7):** on the S19 fixture save it unlocks only after mission 10. Same route: Monsters of the Prairie is then the top row (420,330).
  - The login shows the Daily Spin (422,708), then Today's Tasks (Close 309,784), then the login campaign (Claim 421,784) before Home.
  - Result screens: tap (420,700). The second is the Materials screen.
  - The relay dump (`--relay-dump MissionStart MissionEnd`) holds `i30R8TAs` per row, `4T0Q2Bh5` (items reported) and `LE6JkUp7` (the per-wave capture/chest rolls).
- **Training Ground:** Home castle (250,548) → Survey Office (522,255) → drag (420,820→200) → Battle Simulator (420,790) → about 18 dialogue taps (they pick a friend; irrelevant here) → Begin Training (420,850).
  - The gauges were already full (the client-side settings persisted).
  - Klaus's flick: (270,525) → (270,375).
- **Give Up:** let every unit act first (Menu is disabled mid-turn) → Menu (650,880) → Give Up (618,604) → Yes (296,812).

**Incidental observations** (whole-scene checks at each checkpoint):
- Defects found: the popup regression (§1, fixed) and the status icons (§2, fixed).
- Odd-looking but not game defects:
  - The low unit stats (HP 1,150 / Atk 529 at Lv 150) are a fixture artifact (§3).
  - The friend auto-picked during the Training Ground intro is known from round 7.
- Not checked: audio, and transient frames between captures (about 1.1–1.5 s apart).

## 8. End state

- No processes; ports 9960/29960/29961 free.
- S19 DLL `9633061d…` (backup `R8/dll_backup/libcurl_9633061d.dll`).
- Proxy cache matches the saved copy; `build/bin` `e24234c1…`.
- Candidates on disk, installed nowhere: server **`92f41576…`** (`c28a8db9` plus per-hit material drops; `R8/bin/`); proxy 29960 `f74efc2b…`. `out/build/debug-win64` holds no runtime-output override.
- After §3c: `deploy/archive/mission.json` is still `c7002b84…` (the generated content is only in `R8/archive_r8_drops` and `R8/archive_r8_v7`). Run v7 is stopped, and no crash dump is newer than October 23.
- The cancelled heartbeat was not touched.
- After §3e (November 1):
  - Runs v9–v15 are stopped, with no processes left. S19 is still on `9633061d…`.
  - deploy/archive `mission.json` `c7002b84…` and `ai.json` `0b493118…` are unchanged.
  - deploy holds the EU install only (§3d).
  - After the contract work: run v16 is stopped; candidates on disk are `92f41576` (`R8/bin`) and `e37da19b` (`R8/bin_skills`), installed nowhere. `gimuserver/packets/all.hpp` was regenerated by the build (git-ignored).

## 9. Still open (honest)

1. **Release:**
   - the 9960 twin, the live-client repack and sign (Evan);
   - regenerating deploy/archive (icons + Super boss);
   - #14 / #22-3 staging;
   - server deploy;
   - #24 on the release DLL;
   - every step in readiness §3.
2. **Per-hit item drops:** the channel (§3b) and the generated content (§3c) are done and client-verified on copies. What remains:
   - The yield is decided: the caps stay the live server's 3 / 5 / 1. The client enforces exactly what it is sent and nothing else (§3c).
   - Ship the content only with or after `92f41576` (readiness §3 step 7).
   - Optional client checks: a weak squad, a Fuu leader, and a mission outside Adventurer's Prairie.
   - The wiki gives no materials for Menon or Mildran, and nothing for Parade Garden, the Trials or most one-off events. Those missions drop no materials.
0. **Most urgent: deploy a server that sends `8aiBoHg5` (§3e).**
   - The live server crashes every monster skill, including deploy's Research Lab trials (very likely; not run on the Lab missions themselves).
   - Candidate `e37da19b` (= `92f41576` + the contract) is client-verified (v16) and passes its suites.
   - Release order: the server first, then any archive with skills (`gen_story_bosses.py`).
   - Not confirmed: whether the Weak ailment lands on the party.
3. **Content:**
   - #12;
   - #15 (decision);
   - #17 (version decision);
   - #18: #18-a is staged (`R8/archive_r8_story`); #18-b/c are not started, and the skill contract no longer blocks them.
4. **Special-boss status-icon offsets: done in data, staged** (§3e). Ship by regenerating deploy/archive, in this order: `gen_battle_simulator.py`, `gen_boss_icon_offsets.py`, then `gen_story_bosses.py`. Cardes and Defense System are not client-checked.
5. **The popup fit's restore and fault paths** are proven only against fakes; other descData popups are not patched.
6. **#26 Fuu Zel/Karma:** a multi-sample study (client behaviour; the server pays what is reported).
7. **Optional, still not done:**
   - the #22-1 missing-art negative control;
   - #40 in a natural-gauge story battle;
   - #20 on a real-progression save.
8. ~~**Fixture hygiene.**~~ Done (§3e): `scripts/qa/check_fixture_units.py`. Round-7 prep scripts still clone stat columns, so run the checker after them.
9. **EU units:** evolution is wire-tested (§3e). The in-client evolution, summon and fusion flows are not run.

**Recommended next content batch:**
- ~~First: the material-drop content.~~ Done in §3c, and the caps are decided.
- ~~#18-a~~ and ~~the monster-skill contract~~: done (§3e).
- **Next:** #18-b (Demon Abaddon, Defense System, Granvalm: HP already sourced; author AI and skills from their wiki pages), now that boss skills work.

Stop here for Evan/Codex review: no deployment and no automatic next session.
