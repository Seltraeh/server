# The Creation God (Strategy Zone, mission 2100004)

Dated 2026-09-26. Rebuilt as the original transforming, script-driven fight.
Server- and model-tested; **not yet played in the client**. Lab-wide rules
(unlock order, first-clear-only pay) are in [RESEARCH_LAB.md](RESEARCH_LAB.md).

## Sources

* [The Creation God](https://bravefrontierglobal.fandom.com/wiki/The_Creation_God),
  rev 663846 (2020-08-12), read 2026-09-26 through the MediaWiki API.
* [Template:MaxwellSZTurns](https://bravefrontierglobal.fandom.com/wiki/Template:MaxwellSZTurns),
  rev 535212 — both Maxwells' 20-turn tables.
* The client's own `F_MISSION_SCRIPT_MST` row 2100004 (LocalState
  `Ver288_F2Dz3QHU.dat`), monsters column `1104,5000851`.
* `deploy/mst/skill_mst.json` / `skill_level_mst.json` — the re-issue skill
  band 5001050–5001094; client string table (`tools/sgtext/monsters.tsv`) for
  the monster names.
* `libgame.so` for the AI terms used here (see `archive/ai.kdl`).

## What was wrong

Built 2026-09-20 from the wiki alone: four monsters with invented ids
(21000041–44), two per wave, all on AI 1 (attack at random) with no skills. The
archive could not express "transforms into".

## How the original fight works (evidence)

The mission script runs the transformations on the client
(see the mission-script contract in `archive/mission.kdl`):

| Event | Battle | Trigger | Commands |
|---|---|---|---|
| 1 | 1 | start | dialogue 615; Juggernaut **1004** insensitive (HP floors at 1) |
| 2 | 1 | 1004 below 1% | effect 600550; **swap to Demon Abaddon 1104** at (70,187); dialogue 616 |
| 3 | 2 | start | dialogue 617; Creator Maxwell **54508** insensitive |
| 4 | 2 | 54508 below 70% | dialogue 618 |
| 5 | 2 | 54508 below 50% | dialogue 619; effect; **swap in place to Inception God Maxwell 5000851**; BGM `bf100_shinshiren.mp3`; dialogue 620 |
| 6–8 | 2 | 5000851 below 90 / 50 / 15% | dialogue 621–623 |
| 9 | 1 | game over | dialogue 624 |

`MissionMst` gives two battles, which is exactly this: Juggernaut opens battle
1 and Creator Maxwell opens battle 2. Abaddon and Inception God Maxwell are
`script_monsters` (MonsterMst + AI, no battle-group row), and every id matches
the script because `changeMonster` finds monsters by id alone. The names come
from the client's own string table. A swap keeps the enemy party's turn count,
which is what the wiki means by Abaddon's "count starts at the beginning of the
fight" and "turn counting from the Creator Maxwell still applies".

Every skill the page names is in the re-issue band, with matching parameters:
Magnetism's −70% BB efficacy, Necro Curse's 100% poison/curse/sick/paralysis,
Death Gate's 700%, Sacred Song's ATK +30% / DEF +200% with Angel Idol,
Meteor's 50,000 and Code Meteor's 55,000 fixed damage, Destiny's 250% and
Destiny Rune's 130%, and Sacred Anthem's Angel Idol, whose 6% revive is the
wiki's 1,320,000 HP.

## The encounter as built

`scripts/gen_research_lab.py` (`creation_god`); AI ids 20000031–34. HP is the
wiki's; ATK 2000 / DEF 800 are authored, as for every lab boss.

| Form | Skills (index order) | Behaviour |
|---|---|---|
| Juggernaut 1004, 4,000,000 HP, 1–2 actions | Magnetron 5001050, Graviton 5001052, Magnetism 5001053, Neoplasma Blast 5001051 | Neoplasma every 4th turn; the other three at random |
| Demon Abaddon 1104, 6,000,000 HP, 1–2 actions | Fool's Chains 5001060, Black Hole 5001061, Black Force 5001062, Evil Hole 5001063, Necro Curse 5001065, Banishment 5001064, Death Gate 5001066 | Banishment every 3rd turn of the fight; four Death Gates in one turn below 66%, four more below 30%; the rest at random |
| Creator Maxwell 54508 (unit 50525), 13,000,000 HP, 1 action | Genesis 5001070, Rune 5001074, Sacred Song 5001072, Destiny 5001077, Meteor 5001073, Resurrection 5001075, Endless 5001076 | Sacred Song + Rune every 4th turn; Destiny every 5th and twice every 10th; those turns are only the schedule. Resurrection once below 75%, Endless once below 70%; Genesis/Meteor at random |
| Inception God Maxwell 5000851 (unit 51147), 22,200,000 HP, 1 action + Primal Genesis | Primal Genesis 5001080, Code Meteor 5001085, Verse Genesis 5001083, Sacred Anthem 5001084, Akashic Rune 5001086, Destiny Rune 5001087, Overdrive 5001094, Existence 5001082, Return to the Origin 5001090, Rebirth's Beginnings 5001091, Destruction and Creation 5001092, Ultimate Creation 5001093 | Primal Genesis every turn, then one action (Verse Genesis below 50%, Code Meteor, attack). Sacred Anthem + Akashic Rune every 4th; Destiny Rune every 5th on the highest-HP unit and a second every 10th on the lowest. Below 15%: Overdrive alone, then Existence alone the next turn. After a UBB: Return to the Origin, plus the 1st / 2nd / 3rd buff |

The UBB answer uses the client's `ubb_use` term as a party condition on the
player's side. It reads each unit's latest action, which the client keeps
until that unit acts again (`BattleUnit::exitAction`; `initDead` clears it),
so it describes the player turn just played. The Existence turn is already
spent, which gives the wiki's "other than on the turn Overdrive is used".

## Authored choices

* ATK/DEF 2000/800 (not on the wiki); random-skill percentages.
* Death Gate: four per threshold in a single turn, as free actions. Crossing
  66% and 30% with one hit gives one burst of four, not eight (Trial 003's
  precedence).
* Endless and Resurrection wait for Creator Maxwell's next unscheduled turn.
* The two Destiny Runes on 10th turns hit the highest and the lowest HP unit;
  the page's other pairing (highest and second highest) is not used.
* Ailments: Juggernaut's "Inflicts Injury, Paralysis and Weakness" cannot
  describe its skills (they only paralyse), so it is read as susceptibility
  and the other three are immunities. Abaddon ("all") and both Maxwells (no
  statement) keep 0.
* Juggernaut 1–2 and Abaddon 1–2 actions per turn (`act_rate` 50), as in
  Trial 003.

## Known gaps

* **Eyes of Ends and Beginning / Destiny Record** (after 9 BB/SBB; twice below
  60%). No decoded term counts Brave Bursts across turns (`s_skill_*` is the
  SBB gauge, not a count), so neither is cast. The Red Target search term
  (`target_mark`) exists but is unused.
* **Heals 440,000 HP on killing a unit** — no mapping.
* Ecphoria of Inception is correctly unused ("not used during the entire battle").
* "Ignore def buff active" on the turn after each Rune is Rune's own effect;
  turn 1's is not reproduced.
* Free replays (lab-wide, see RESEARCH_LAB.md).

## Tests

* `scripts/test_research_lab_ai.py` — structure and ids against the script,
  HP, skill bands, unit art; every schedule over 40 turns including arrival
  mid-fight; thresholds exactly at and just past 75/70/66/50/30/15%; the
  one-hit 70→29.9% crossing; Overdrive→Existence cancelling a turn-20
  schedule; the UBB answers 1st/2nd/3rd/4th, none without a UBB, none on the
  Existence turn, none for an SBB. `scripts/ai_model.py` now models
  opposing-party last-action terms.
* `scripts/test_research_lab_wire.py` — MissionStart payload (two battles,
  only 1004 and 54508 placed, MonsterMst/AI rows for all four), offered from
  the start, loss / first clear (3,000,000 Zel, a Gem, Inception God Maxwell
  51147) / replay.
* `scripts/validate_missions.py` — no CRITICAL or HIGH finding for 2100004.

Client checklist: battle 1 swaps Juggernaut for Abaddon below 1% with
dialogue; Banishment on turns 3/6/9; Death Gate bursts at 66% and 30%; battle
2 opens on the unit-art Creator Maxwell; the 50% swap to Inception God Maxwell
with the BGM change; the turn-table casts; a UBB answered by Return to the
Origin; Overdrive then Existence below 15%; the clear pays unit 51147.
