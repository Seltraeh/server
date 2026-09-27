# Summoners' Research Lab (Trial Zone and Strategy Zone)

Dated 2026-09-25. Lab-wide rules and Trials No. 001 and 002. Trial No. 003 has
its own note: [TRIAL_003.md](TRIAL_003.md), and so does the Strategy Zone's
The Creation God (2100004, rebuilt 2026-09-26): [CREATION_GOD.md](CREATION_GOD.md).

## Lab rules

Source: [Summoners' Research Lab](https://bravefrontierglobal.fandom.com/wiki/Summoners%27_Research_Lab),
rev 653535.

| Rule | Implementation | Evidence |
|---|---|---|
| The lab is its two dungeons | `DungeonMst.dungeon_type` 2 (Trial Zone, the value `GameUtils::isTrialMission` tests) and 8 (Strategy Zone); `gme/common/ResearchLab.hpp` | Binary-confirmed + table data |
| Only Trial 001 at first; each trial waits for the previous one | PermitPlace adds lab missions per player when every prerequisite in `need_mission_id` with an archive record is cleared (all-of; Trial 003 also needs St. Lamia's mission 365) | Gameplay source + server-tested |
| Rewards only on the first clear | MissionEnd pays no archive Zel/Karma/EXP for a repeat lab clear; `SiYs27Cj` presents are already once-only | Gameplay source + server-tested |
| Three squads, shared items, no Gem revive | Client-side (`isTrialMission`); no server fields | Binary-confirmed |
| 50 energy; free replays since 2015-02-25 | 50 is charged; free replays **not implemented** — the client prices energy from `MissionMst` × the dungeon's quest bonus, with no per-mission channel | Gameplay source; gap |

A prerequisite with no archive record is skipped, because it can never be
cleared here. That keeps unbuilt trials from locking the series, and it means
The Creation God (2100004), whose two prerequisites are unbuilt, stays open.

## Trial No. 001 (mission 2000000)

Source: [Trial No. 001](https://bravefrontierglobal.fandom.com/wiki/Trial_No._001), rev 661563.
Client script (`F_MISSION_SCRIPT_MST` 2000000): Brave Knight Karl (monster
22651) is set "insensitive" at the start, so his HP cannot reach 0; below 50%
Karl speaks, three effects play, and command 6 swaps in Ice Warrior Karl
(22701) where he stands.

| | Brave Knight Karl 22651 | Ice Warrior Karl 22701 |
|---|---|---|
| Unit art | 20233 | 20234 |
| HP | 400,000 (wiki) | 500,000 (wiki) |
| Skills | Frozen Axe 6140, Flood Offensive 6143, Verdant Axe 6141, Lightning Axe 6142 | Verdant Axe 6141, Lightning Axe 6142, Flashing Axe Combo 6151, Blue Execution 6154, Cursed Blue Slash 6150, Tidal Offensive 6152 |
| Behaviour | Frozen Axe every 5 turns; below 75% Flood Offensive once, axes at random | Axes at random; below 75% Flashing Axe; below 50% Blue Execution once and Cursed Blue Slash; below 15% Tidal Offensive once |
| Ailments | Poison only (wiki) | Paralysis and injury only (wiki) |

Skill matches: Blue Execution 6154 carries the wiki's 500% modifier, Flood
Offensive 6143 its ATK +30% / DEF −50% for 3 turns, Tidal Offensive 6152 its
DEF +80% and 5,000–10,000 regeneration for 3 turns.

Reward: 1 Gem, 500,000 Zel, 20,000 EXP (MST) and Brave Knight Karl (unit
20233, added to `SiYs27Cj` as a first-clear present).

## Trial No. 002 (mission 2000001)

Source: [Trial No. 002](https://bravefrontierglobal.fandom.com/wiki/Trial_No._002), rev 661687.
Client script: Grahdens (63051) alone; lines at the start, below 40%, below 1%
and on defeat. He is not insensitive, so a large hit can skip the 1% phase —
original client behaviour.

Grahdens: unit art 60324, 500,000 HP (wiki), one action a turn. Skills
7410/7320/7430/7420 (the four elemental sweeps), Soul Rejection 7310 (110% HP,
matching the wiki), "I'm tired. Time for a break." 7440 (ATK −30% for 999
turns), Sacred Change 7380, Life Shower 7400, Holy Light 7340, Darkness Change
7381, Sharp Gaze 7350, Light Attack 7450, Vanishing Wave 7360, Apocalypse 7370
(6,666), Instant Barrage 7330, Steel Fortification 7390, Apocalypse Zero 7371
(9,999).

| Trigger | Authored behaviour |
|---|---|
| Turn 1 | Soul Rejection |
| Turn 6 while HP ≥ 70% | "I'm tired" once; afterwards Holy Light, Sacred Change and Life Shower join his pool |
| < 80% / < 70% / < 40% | Darkness Change / Soul Rejection / Apocalypse, once each |
| < 30% / < 20% / < 5% | Instant Barrage / Life Shower + Steel Fortification / Life Shower, once each, free |
| < 15% | Sacred Change once |
| < 40% | Sharp Gaze, Light Attack, Vanishing Wave join his pool |
| < 1% | Apocalypse Zero, then two idle turns, then normal |

Reward: 1 Gem, 1,000,000 Zel, 30,000 EXP and Grahdens (unit 60324).

## Authored choices (Trials 001 and 002)

* ATK 2000 / DEF 800 throughout; Karl's forms act 1–2 times (`act_rate` 50).
* Once-only readings of threshold buffs and nukes; random percentages.
* Grahdens' thresholds, when several are crossed by one hit, play one per turn
  in threshold order (buffs and heals are free and ride along).
* "After 5 turns" is read as turn 6, and the break is skipped if Grahdens is
  already below 70% then.
* Grahdens' ailment resistances are left at 0: the wiki says nothing.

## Known gaps

* Grahdens' "Break's over!" at 70% removes his ATK debuff on the wiki; no skill
  in his band does that, so the debuff stays.
* The wiki's unnamed "gains an ATK & DEF buff" at 40% and 15% and "DEF buff"
  at 5% are not mapped to a skill.
* Ice Warrior Karl's "leeches HP with every attack" below 15% has no mapping.
* Karl's second Frozen Axe (6153, weaker) is not used; the wiki gives Ice
  Warrior Karl none.

## Tests

`scripts/test_research_lab_ai.py` (model) and `scripts/test_research_lab_wire.py`
(isolated server) cover all three trials: thresholds exactly at and just past
each boundary, the Grahdens cascade, the 1% sequence, the unlock order (nothing
cleared → Trial 001 only; after 001 → 002; 003 needs 002 and 365), and loss /
first clear / replay pay for each trial with its own unit reward.

Results, 2026-09-25 23:35 build (server-tested, fresh copies of the live save):
`test_research_lab_ai.py` 0 failures; `test_research_lab_wire.py` 30/30,
including the unlock order and each trial's loss / first clear / replay. The
same binary passed the parallel session's feature-visit (4 groups + restart
persistence) and synthesis (5 groups) suites and `audit_handlers.py` (0
structural errors). Deployed live at 23:35 with those changes included; the
live server applied the `25092026_CreateUserEnteredFeatures` migration.
**Not yet played in the client.**
