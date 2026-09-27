# Boss AI and Trial No. 003 (mission 2000002)

Dated 2026-09-25. Current status of the boss-AI grammar and of the first
encounter built with it. Evidence labels follow the handbook.

## Status

| Item | State | Evidence |
|---|---|---|
| AI grammar (conditions, targets, actions, budgets) | Decoded, documented in `packet-generator/assets/archive/ai.kdl` | Binary-confirmed (arm64 `libgame.so`) |
| Mission-script contract (monsters swapped in mid-battle) | `script_monsters` added to the mission archive | Binary-confirmed + original client data |
| Trial No. 003 encounter | Built: one battle, three bosses, original skills, phase AI | Server-tested; **not client-confirmed** |
| Research Lab pays first clear only | MissionEnd rule for dungeon types 2 and 8 | Gameplay source + server-tested |
| Trials unlock in order | Per-player permits; unbuilt prerequisites skipped | Gameplay source + server-tested |
| Cleared trials replay at 0 energy | **Not implemented** (see gaps) | Gameplay source |

## The AI grammar in one paragraph

A monster walks its AI rows in wire order on every action; the first row whose
conditions pass and whose percent roll succeeds runs. `skill:N` picks the
monster's Nth skill (monsters never wait on a gauge), `actbetween:N` fires on
enemy-party turns N, 2N, 3N (the counter belongs to the party and starts at 1),
`hp_pr_under:N` is strictly below N percent, `limited_act:N` and
`turn_limited_act:N` cap a row per battle and per turn, and `flg_on/flg_off`
read 50 per-monster flags that actions set. An action that does not count
toward the turn's budget is a free extra action. The three trailing action
arguments that were `unknown_*` are the counts-as-action flag and two frame
waits; their JSON keys stay as they were so builder-made `ai.json` files keep
loading. Full evidence with addresses: `ai.kdl`.

## How the original trial worked (evidence)

* **One battle, three bosses.** `F_MISSION_MST` gives mission 2000002 one
  battle. The client's own `F_MISSION_SCRIPT_MST` row 2000002 (LocalState
  `Ver288_F2Dz3QHU.dat`, identical to the decoded server dump) opens on
  Juggernaut (monster 1002), and when a monster's HP falls below 1% its command
  6 calls `MonsterParty::changeMonster`, replacing it with Demon Abaddon (1102)
  and then Creator Maxwell (54501). Dialogue plays at Abaddon 30% and Maxwell
  70/50/30/20% and on the player's defeat; every line is in the client's
  localisation. The script's second column ("1102,54501") is never read by the
  client, so it was the original server's list of extra MonsterMst rows to send.
* **Three-squad mode is client-side.** `GameUtils::isTrialMission` is
  `DungeonMst.dungeon_type == 2`, which only dungeon 2000000 carries. The
  MissionStart request has no trial fields; a plain Trial Zone MissionEnd adds
  none either.
* **Skills are the client's originals.** Matched to the wiki by hit count and
  parameters in `skill_mst`/`skill_level_mst`: Juggernaut 1902/1904/1903
  (Neoplasma 1903 at 15% paralysis; re-issue 5001051 has 50%), Abaddon
  1954/1955/1952/1953/1956, Maxwell 2052/2056/2054/2059/2055/2057/2058
  (Sacred Song 2054 = ATK +80%, DEF +200%, 2 turns, 10% revive; the story
  version 2912 and re-issue 5001072 differ).
* **Maxwell's HP and revive agree across sources.** The wiki's 1,000,000 HP and
  the guide-reported 100,000 HP revive match skill 2054's 10% Angel Idol.

Gameplay sources, read 2026-09-25 through the MediaWiki API:

* [Trial No. 003](https://bravefrontierglobal.fandom.com/wiki/Trial_No._003), rev 661740 (2020-08-12); launch-week revs 86806/86822/88502 (2014-10) for the dialogue and sprite.
* [Template:Maxwell Turns](https://bravefrontierglobal.fandom.com/wiki/Template:Maxwell_Turns), rev 502228 — the 20-turn cycle counted from the first Juggernaut turn.
* [Summoners' Research Lab](https://bravefrontierglobal.fandom.com/wiki/Summoners%27_Research_Lab), rev 653535 — 50 energy, no Gem revives, first-clear-only rewards, free replays since 2015-02-25.
* Corroboration: [Linathan's squad spotlight](https://bravefrontierglobal.fandom.com/wiki/User_blog:Linathan/Squad_Spotlight:_Trial_003) (rev 200690), [Dera73's guide](https://bravefrontierglobal.fandom.com/wiki/User_blog:Dera73/TRIAL_3_GUIDE_vs._Maxwell) (rev 134394), the period [turn counter](https://touchandswipe.github.io/bravefrontier/maxwellcounter) (same cycle).
* Conflicts recorded: Death Gate is "120% HP" on the wiki, skill 1956 carries 150; the JP guide on [Qlay](https://qlay.jp/archives/24203) puts Abaddon's bursts at 50%/25% (Global: 50%/30%, used).

## What was built

`scripts/gen_research_lab.py` writes everything below (idempotent, `--check`);
the lab-wide rules are in [RESEARCH_LAB.md](RESEARCH_LAB.md).

| Boss | Behaviour authored | Source |
|---|---|---|
| Juggernaut 1002 | Neoplasma Blast on turns 4, 8, 12…; Magnetron/Graviton at random | Wiki |
| Demon Abaddon 1102 | Evil Hole every 4 turns; Death Gate ×2 below 50% and ×4 below 30%, once each, as a free-action burst; Banishment/Necro Curse/Black Force at random | Wiki |
| Creator Maxwell 54501 | Sacred Song + Rune on 4k, Destiny on 5k, all three on 20k, nothing else on those turns; Destiny twice below 30%; Endless once below 20%, replacing Sacred Song + Rune but after any Destiny; Meteor below 70% and Resurrection below 50% at random | Wiki + template |

Server changes: `MissionArchiver` emits MonsterMst/AI/art rows for
`script_monsters` but no battle-group row, refuses a mission whose scripted ids
repeat or collide, and sends an authored `act_rate`. `MissionEnd` pays nothing
for a repeat Research Lab clear. `F_MISSION_MST` 2000002 gains the Creator
Maxwell unit as a first-clear present, as 2100004 did.

## Authored choices (not recovered)

* HP of Juggernaut and Abaddon (500,000 each, Trial No. 002's documented
  value) and ATK 2000 / DEF 800 for all three: undocumented anywhere found.
* Action budgets: Juggernaut and Abaddon 1–2 actions (`act_rate` 50), Maxwell
  one regular action plus the free scheduled casts.
* Random-skill percentages (30/30, 30/30/30, 40, …).
* Precedence where the wiki is silent: a hit that crosses both of Abaddon's
  thresholds gives four Death Gates, not six (flag 1 retires the unused 50%
  burst); Maxwell's "< 30% HP: Genesis" is read as one cast at the threshold.
* Resistances: the wiki lists what CAN be inflicted, so every unlisted ailment
  is an immunity (100): Juggernaut takes only paralysis, Abaddon only poison
  (80, "highly resistant, but not immune"), Maxwell only weakness (80, "very
  resistant"). The slot order itself is binary-confirmed.
* The Creator Maxwell reward is delivered to Presents on the first clear; the
  original delivery path was not recovered.

## Known gaps

* **Free replays.** The client prices a trial as `MissionMst` energy × the
  dungeon's quest-bonus rate (`TrialPartyTopScene::getUseActionPoint`); no
  per-mission "cleared = free" channel was found, so replays still cost 50.
* **Multi-threshold dialogue.** The wiki says crossing several of Maxwell's
  thresholds at once makes her use Endless "after each dialogue"; the authored
  rows give one Endless. Unexplained by the data available.
* **Ailments from basic attacks** (Juggernaut injury/weakness, Maxwell
  injury/weakness) have no known MonsterMst field; only skill ailments apply.
* The other 17 Trial Zone missions and 9 Strategy Zone missions are unbuilt.

## Tests

* `python scripts/test_research_lab_ai.py` — checks on the authored rows
  against `scripts/ai_model.py`, a model of the client selector built from the
  decode: the 40-turn schedule, HP exactly at and just past 70/50/30/20%,
  several thresholds crossed in one hit, once-only rows, late arrivals on the
  shared turn counter, budgets, and a guard against free rows that never stop.
  Mutating the row order or dropping Abaddon's flag fails it.
* `python scripts/test_research_lab_wire.py out/trial003-tests-2026-09-25/gme.sqlite`
  — isolated server on port 19960: the MissionStart payload (one placed
  monster, three MonsterMst rows, every AI row against an independent encoder,
  six sprite rows, 50 energy), then a loss (nothing), a first clear
  (2,000,000 Zel, 50,000 EXP, Gem + unit present) and a replay (nothing), with
  mission 10 as a paying control.
* `python scripts/validate_missions.py` — no findings for 2000002; it now checks
  `script_monsters` (new C12) and exempts lab missions from "drops nothing".

Results, 2026-09-25 (server-tested, isolated copies of the live save): first
deployed at 19:53 on its own; re-tested and redeployed at 23:35 with Trials
001/002, the per-player unlock and the immunity reading (see RESEARCH_LAB.md
for those results). The live server loaded 1,338 missions and 7 AI records.

## In-game checklist (not yet observed)

1. The Trial Zone tile appears in the Research Lab and opens three-squad setup.
2. Noel's opening line plays; Juggernaut fights with named skills.
3. Juggernaut below 1% → dark effect, Abaddon appears with its lines.
4. Abaddon's 50%/30% Death Gate bursts; Maxwell arrives with her lines.
5. Maxwell's turn 4/5/20 pattern, the 20% Endless, the revive at 100,000 HP.
6. A wiped squad is replaced by the next; losing all three shows Noel's line.
7. First clear: result shows 2,000,000 Zel/50,000 EXP; Presents hold a Gem and
   Creator Maxwell. A second clear shows no rewards.

## Next slice

Trials No. 001 and 002 are built (see RESEARCH_LAB.md). Next: the Strategy
Zone -- The Creation God (2100004) still runs on "attack random".
