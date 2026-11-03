# Fresh session, round 6: handoff (October 30, 2026, evening)

Started by Evan ("Start round 6") right after round 5. There was no separate round-6 assignment, so the targets are the round-5 handoff's §6 leftovers: #25, #26 and #22-1. This is a transient coordination note.

Nothing was staged, committed, pushed, deployed, signed or installed. There was no reboot, and no live save or deploy content was written. The S19 test client kept its original DLL `9633061d` this round. QA output: `out/qa/current/fresh_round5_20261030/` (shared with round 5; `R5/`).

## 1. #25 Omni frog SP eligibility: works as designed (client-observed)

**Fixture.** Run `s1_sp`, a copy of the completed S19 save, built with `R5/tools/prep_sp.py`:

- Bases: Intact Champion Amadream 51337 at Lv 150 (max) / SBB 10, at Lv 100 / SBB 10, and at Lv 150 / SBB 5.
- Materials: Omni Frog 750003 ×2 and Burst Frog 10312 ×2.
- Every unit has fe_sp 0, used 0 and max 100 (the migrated values). Server `c28a8db9`.

**Observed** (Fusion → base → material list; `R5/visual/run/S1/sp_materials_compare.png`):

| Base | Burst Frog | Omni Frog |
|---|---|---|
| Lv 150, SBB 10 (eligible) | "SPUP", selectable | "SPUP", selectable |
| Lv 100, SBB 10 | "BBUP", selectable | **greyed out** ("BBUP") |
| Lv 150, SBB 5 | "BBUP", selectable | **greyed out** ("BBUP") |

**Rule.** This is the client's own eligibility rule, `GameUtils::feUnitCheck` (cited in `UnitMix.cpp`):

- SP needs rarity ≥ 8, SBB exactly 10 and max level, within `fe_sp + fe_used_sp ≤ fe_max_sp`.
- Omni Frogs, which only give SP, are greyed out for any other base. Burst Frogs still level the burst.

**Disposition.**

- A max-level Omni unit with SBB 10 on a migrated save can take SP from frogs.
- The report's greyed frogs match either a base that isn't at SBB 10 and max level, or the pre-migration save (all SP fields 0) noted on October 1.
- **Not a defect.** The server's SP amounts were already wire-tested (`test_enhancement_sp_wire.py`). This round did not perform an actual fusion.

## 2. #26 Roglizer/Fuu: diagnosed (code), not client-tested

**Leader skills** (`deploy/mst/leader_skill_mst.json`):

- **Roglizer 61286/61287:** LS 10167/10168, processes `1@48@31@97`. Process **97 = Player EXP Boost** (+15 % / +20 %) is modelled server-side for the owned leader plus the resolved helper, additively, and wire-tested (`test_leader_exp_boost_wire.py` 21/0).
- **Fuu 830838:** LS 830838, processes `1@73@47@70@19`. Process **19 = drop rates**, params `40,40,5,20,30`: "40% BC, HC, 5% Item, 20% Zel, 30% Karma drop rates".

**Where each resource is decided:**

- **Zel and Karma.** `MissionEnd` pays exactly the battle amounts the client reports (`Mission.cpp:666–667`, `req.battle_result.zel/karma`) plus the mission's fixed clear reward. The rate boosts therefore apply wherever the client rolls those drops in battle, and the server passes them through unchanged. BC and HC are battle-only.
- **Items.** Treasure chests are **pre-rolled by the server** at MissionStart (`MissionArchiver::encodeTreasureDrop`: `RandomUInt(1,100) > treasure_chest_chance`). That roll ignores the squad's leader skill, so **Fuu's "+5 % Item drop rate" is not applied anywhere server-side**. This is a candidate omission. Whether the original applied it to the chest chance or the chest contents needs a source (wiki Leader Skill / process 19 page, exact revision).
- **EXP.** Modelled (process 97, see above).

**Next:**

1. Fetch the wiki revisions for Fuu, Roglizer and the drop-rate mechanic.
2. Decide how process 19's item rate maps onto the server's chest pre-roll, e.g. chance × (1 + 5 %), for the leader and the helper as process 97 does. That is a design choice only if no source states it.
3. Client check: the same mission with no leader vs Fuu leader on copies, over several runs, comparing reported Zel/Karma with stored deltas (they are paid as reported).

## 3. #22-1 Lukroar gate: not started

The targets are in round-5 §6. The prerequisite is a deterministic Lukroar 51247 pull (selector ticket or fixture gacha table on a copy).

## 4. Hashes

- `R5/tools/prep_sp.py` (new)
- `docs/BUG_ACCEPTANCE_2026-10-27.md` (#25 and #26 cells updated)

The full regression from round 5 §12 is unchanged: no runtime edits this round.

## 5. #26 sourced (Evan: "Source it, then fix"): no server change is justified

**Sources** (BF Global wiki, MediaWiki API; saved in `R5/wiki/`):

- **Drop Rate Boost** (passive id 19, the redirect target of "Item Drop Rate"), revision **645616** (2020-04-21), `r6_Item_Drop_Rate.json`: "Zel, Karma, and Item Drop Rate … each hit from an attack or skill has a chance equal to the drop rate bonus. Unlike BC and HC drop rate, the base value of Zel and Karma drop rates is respective to each enemy and the base value of Item drop rates is respective to each item that an enemy may drop, **exclusive of chest drops**."
- **Fuu**, revision **678968** (2021-02-27), `r6_Fuu.json`; **Drop Rate**, revision 622533 (2019-08-23).

**What that means here:**

- **Items.** The +5 % item rate does **not** apply to treasure chests, so the server's chest pre-roll (`MissionArchiver::encodeTreasureDrop`) is correct to ignore it. The planned change would have contradicted the source and was not made.
  - The boost acts on items an enemy drops per hit. This server's battle monsters define no per-hit item drops, only `treasure_chest_chance`/`treasure_drops` (every field checked in `deploy/archive/mission.json`). So there is nothing for it to raise: a content-authoring matter, not a missing multiplier.
- **Zel and Karma.** Each enemy's base comes from the archive (`zel_max_drop`/`zel_drop_count`, `karma_max_drop`/`karma_drop_count`). The boost is applied per hit by the client in battle, which reports the totals; `MissionEnd` pays them as reported (`Mission.cpp:666–667`).
- **BC and HC.** Battle-only, client-side.
- **EXP (Roglizer).** Process 97, modelled server-side and tested.

**Disposition.** No server defect. What remains is a client observation, not a fix: the same mission with no leader vs Fuu leader over several runs on copies, comparing the reported Zel/Karma (higher on average with Fuu) with the stored deltas.
