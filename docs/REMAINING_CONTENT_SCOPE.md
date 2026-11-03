# Remaining content scope: #12, #15, #17, #18

Maintained scope for the write-up's missing-content items. Created October 30, 2026 (fresh round 5); update it as batches land. Nothing here is authored yet.

Every count below was re-checked on October 30 against `deploy/archive/*.json`, `deploy/mst/*.json` and `deploy/game_content` (check script in `out/qa/current/fresh_round5_20261030/`). The older inventories, `out/claude_session2_2026-09-29/12_trials_inventory.md` and `18_boss_inventory.md` (September 29), list per-mission rows; their counts still hold.

Rules for all batches:

- No invented stats, rates or AI.
- Every authored number cites a wiki page revision (`revid`) or client/MST evidence, or is marked "authored" with its reason.
- "Absent" means not in the archive the server loads. Data that exists only in an MST is not playable content.

## #12 Vortex Trials ("an empty stub")

- **Identifiers.**
  - AreaMst 100003 "Vortex Trials" and its child areas: 800150 Scrolls of Urzyma, 800154, 800167 Battles of Brave Frontier 2 (9 areas), 800169 Mythological Entities, 800145 Burden of Aesys (800147/800148/800149/800150), 800148 Amduat (800155), 800149 Traversing Eternity (800157), 800156, 800164, 800165, 800171, 20001300, 20001500, 20001600, 20002200, 25010001.
  - **41 missions**, listed with energy, needs and rewards in the September inventory.
- **Current state.**
  - All 41 are still in `mission_mst` (the client can list them).
  - **0 of 41 have an archive record**, so the server has no battle to send; PermitPlace withholds missions with no archive record.
  - 15 of them have a client battle script ("client script yes" in the inventory).
  - Not part of this item: Research Lab Trials 001–003 (2000000–2000002) and The Creation God (2100004) are archived and server-tested; Trial of the Gods 8380000–8385000 are archived.
- **Missing per mission:** battle waves (monsters, HP/ATK/DEF, positions), boss AI, rewards beyond the MST's first-clear rewards, and unlock verification.
  - Mission needs come from `mission_mst` (`HSRhkf70`) and need no authoring.
  - The client-scripted missions also need their script contract checked (see `reference_mission_script_contract`).
- **Evidence sources:** BF Global wiki pages per trial (fetch exact `revid`s when authoring; none recorded yet), `mission_mst`, client battle scripts (`F_MISSION_SCRIPT_MST`), and monster rows in `deploy/mst/monster_mst.json`.
- **Dependencies:** AI grammar (`archive/ai.kdl`, `scripts/ai_model.py`); #18 for shared bosses (e.g. Herald of Yggdrasil, Osiris, Vizier of Chaos, True God-King, Savage Lion Baragan, Warmonger Zaad, Illusionist Melfel).
- **Acceptance:** per mission, the wire test (`validate_missions.py`, plus a Research-Lab-style wire suite) and a client play of first clear and replay on a copied save, including rewards and the next unlock.
- **Unknowns:** wiki completeness per trial (many pages list bosses but not wave tables); whether Global ever ran every trial in `mission_mst`.
- **Round 8 feasibility check (November 1; nothing authored).** `scripts/inventory_trials.py` confirms 41 missions, 0 archived, 18 with a client battle script. The bosses the wiki names cannot be built faithfully from what exists:
  - **Art:** none of the seven named bosses has a complete animated sprite. Herald of Yggdrasil has a sheet and part file but no idle or attack animation; Savage Lion Baragan, Vizier of Chaos and Warmonger Zaad have only the sheet (no part file, no animations); Illusionist Melfel, Osiris and the True God-King have nothing. A battle needs all of them (see the special-boss contract).
  - **Numbers:** no revision gives a story-scale HP. The wiki's figures are late-game event values (Baragan: 25.5M / 35M / 150M HP across three phases, 96 % mitigation).
  - **Mechanics:** shields, Doom, Time Stop, turn skip, column targeting and "reach < 1 % three times". The client script does carry the phase structure (8301560: boss 140002000, made insensitive, swapped for 140002001 and then 140002002), but through a trigger type 5 (`monster,3`) that is not decoded yet.
  - **So #12 needs a decision:** build stand-ins (authored static sprites as for the EU units, simplified kits from the wiki's skill lists, scaled HP, all marked AUTHORED), or leave the category closed until real art and data turn up.

## #15 Missing EU / special units

- **Current state.**
  - `deploy/archive/unit.json` has 2,259 units; 114 Global units were restored in Session 3, including Deemo and the Girl (50563/50564/50565, 60864/60865), all present today.
  - `unit_mst` has **32 units with no archive record**: 1, 10695, 10696, 10783–10785, 10904, 20713–20715, 20816, 20817, 20844, 20955, 20956, 30646, 30804, 40734, 40735, 40794, 50876, 50975, 50976, 51014, 51026, 51027, 60655, 60656, 60806, 60936, 60994 and 9850167.
  - These are the 31 JP-only units left out on purpose, plus 30646 (no name in any source). Unit 1 and 9850167 are system rows.
  - 135 archive units have no `unit_anime_<id>.png` in `deploy/game_content/content/unit/img`, e.g. 60584, 60585 and the 810xxx series. That is a candidate list, not proven absence: the art may arrive through the client package or the content provider, and 850977's missing `unit_anime`/`unit_cgg` is the one confirmed gap.
- **Missing:** a definition of "EU units" (the report names only Deemo and "girl"); evolution and skill checks for restored units; art verification per candidate.
- **Evidence sources:** `unit_mst`, `sgtext_units_443.zip`, the archive, and the wiki unit pages (cite `revid`s when a unit's data is authored).
- **Acceptance:** per unit, owned in inventory, squad, Archive entry, battle (BB/SBB), and evolution where it has one. Deemo 50563→50564 evolution and battle skills are the first open check.
- **Decision for Evan:** the target roster.
  - (a) Global-released units only, i.e. today's archive less known gaps. This is the bounded option; **recommended**.
  - (b) Also the 31 JP-only units. That needs sources for 31 stat lines and is open-ended.

## #17 Fire Mecha God not dropping

- **Identifiers:** Fire Mecha God unit 10354 (archived as a unit); Sanctum of Fire Lv. 6, mission 100320 (and 100321–100325 for the other elements).
- **Current state:**
  - No archived battle fields 10354 or drops it.
  - The Lv. 6 Sanctums are archived with "Miracle Totem + <Element> Totem + <Element> Idol" finales, so there is no Mecha God boss.
  - The only route to 10354 so far was a test present (Session 6).
- **Missing:** the Mecha God encounter (stats, AI, wave), its capture rule, and the other five elements' Mecha Gods if the wiki names them for Lv. 6.
- **Evidence sources:** the wiki Sanctum / Mecha God pages (exact `revid`s to fetch) and `monster_mst` rows for the Mecha Gods.
- **Acceptance:** a capturable and a non-capturable run of 100320, with the database delta matching the result screen. Captures are one per wave (`reference_one_capture_per_wave`).
- **Unknown:** the sourced capture rate. Do not invent one; if no source exists, Evan picks an authored rate.

## #18 Placeholder big bosses

- **Current state.** `deploy/archive/ai.json` holds 12 AI records: "Attack random", the Training dummy's, and 10 Research Lab boss records. The story bosses still fight with "Attack random" (AI 1):

| Mission | Boss | Archive HP | Wiki HP (September inventory) |
|---|---|---|---|
| 85 | Juggernaut | 50,000 | — |
| 265 | Demon Abaddon | 370,000 | 370,000 |
| 365 | Creator Maxwell | 10,000 | — |
| 465 | Emperor of Amdahl | 10,000 | — |
| 565 | Demon Zebra | 10,000 | — |
| 666 | Cardes the Malevolent | 10,000 | — |
| 776 | Defense System | 180,000 | 180,000 |
| 866 | Granvalm | 200,000 | 200,000 |
| 20067 | Grantos | 10,000 | — |
| 8360505 | Xie Jing | 10,000 (placeholder) | — |

  - The 10,000-HP rows are placeholders.
  - Xie Jing has no art (img, cgg, idle and atk are all missing).
  - Juggernaut's Research Lab AI exists (records 2000002x) but is not used in mission 85.
- **Missing per boss:** sourced HP/ATK/DEF, phases and AI (skills, conditions), art scale/size checks, and results/replay checks.
- **Evidence sources:** the wiki boss pages (story missions; fetch `revid`s), client battle scripts where present, and the AI grammar.
- **Acceptance:** per boss, the AI model test (`scripts/ai_model.py`) plus a client fight on a copied save at the mission, then results and replay.

## Recommended batches (small, in order)

1. **#18-a:** reuse the existing Juggernaut AI for mission 85 and source Juggernaut's story HP. Smallest step; the AI already exists and is tested.
2. **#18-b:** Demon Abaddon (265), Defense System (776) and Granvalm (866). Their HP is already sourced; author AI from their wiki pages.
3. **#17:** Fire Mecha God at Sanctum of Fire Lv. 6, then the other elements if the wiki lists them.
4. **#12-a:** the 15 Vortex Trials with client battle scripts, starting with the three single-mission challenges that already have wiki bosses (Savage Lion 8301560/1, Warmonger 8301650/1, Illusionist 8301710/1).
5. **#18-c:** the 10,000-HP placeholders (Creator Maxwell, Emperor of Amdahl, Demon Zebra, Cardes, Grantos), source-first.
6. **#12-b:** the remaining Vortex Trials, area by area.
7. **#15:** after Evan's roster decision; Deemo evolution and skill checks can run any time.

Each batch: author through a maintained generator or archive tool (never hand-edit `deploy/archive` outside one), add a wire test, run the full regression, then play in the client on a copied save.

## Decisions for Evan

1. **#15 roster:** (a) Global-only (recommended) or (b) also the 31 JP-only units.
2. **Missing sourced numbers** (e.g. a capture rate the wiki doesn't give): allow clearly marked authored values, or leave that encounter out until a source is found. Recommended: authored and marked, as with the parades' DEF 999999.

## Independent QC correction — October 31, 2026

The earlier suggestion to reuse Research Lab Juggernaut AI for story mission 85 is **not yet approved by evidence**. Match the story variant against [Tower of Mistral revision 674494](https://bravefrontierglobal.fandom.com/wiki/Tower_of_Mistral?oldid=674494), including its existing 50,000 HP, Weakness and vulnerabilities, before selecting AI. A matching name is insufficient.

For #17, [The Mecha Gods Awaken revision 659707](https://bravefrontierglobal.fandom.com/wiki/The_Mecha_Gods_Awaken?oldid=659707) establishes Fire boss 10354 and encounter structure; its HP/capture-rate fields are blank. Recover those values before claiming faithful implementation. For #26, absence of authored per-hit items does not establish that the packet/schema supports them: trace the complete contract before classifying the gap as content-only. Do not apply item bonuses to chests.

### Round 8 evidence for this correction (October 31, 2026; researched, nothing authored)

Wiki snapshots: `out/qa/current/review_round6_20261031/`. Archive and MST counts are from `deploy/` (read only).

**#18 Juggernaut, mission 85, against Tower of Mistral rev 674494 ("The Forged Gigantor").**

| | Wiki rev 674494 | Archive (mission 85) | Research Lab Trial No. 003 (2000002) |
|---|---|---|---|
| Energy / battles / EXP | 8 / 8 / 180 | 8 / 8 / 180 | — |
| HP | 50,000 | 50,000 | 500,000 |
| Element | Thunder | 4 (Thunder) | 4 (Thunder) |
| Skills | "Causes Weakness" | none (`monster_skill_id` empty) | its own skill kit |
| AI | — | 1, "Attack random" | 20000021: three conditioned skills, then attack |
| Ailments | "Can be injured, cursed, and poisoned" | `bad_state_resists` empty (every ailment lands) | — |
| Capture | Metal Ghost 60132 (rate blank) | — | — |

- The archive already matches the wiki on every number the wiki gives.
- What is missing is the Weakness-inflicting skill and the ailment profile.
- The Lab AI cannot be reused as is. Its three skills come from the Lab row's own skill ids, which the story row lacks. It also belongs to a 10× HP trial.
- **Proposal:** author a story AI from the wiki's facts (a normal attack plus a Weakness skill) and resists for Weak/Sick/Paralysis. The skill's rate is not sourced, so it must be marked authored (Decision 2). Reading "can be injured, cursed and poisoned" as the complete list is an interpretation, and must be marked as one.

**#17 Fire Mecha God against The Mecha Gods Awaken rev 659707 ("Sanctum of Fire Lv. 6").**

- **Wiki:** energy 70, 3 battles, EXP 7,000, boss 10354.
  - Room 2 is 2× Dragon Graven plus Dalimaone.
  - Skills: *Explosion* (powerful AoE Fire) and *Volcanic Cannon* (massive AoE Fire, "used frequently").
  - HP and capture rate are blank.
  - The same page lists Dark 60344, Light 50394 and Thunder 40334, each with 3 battles and its own skill pair.
- **Archive 100320:** energy, battles and EXP match. Both rooms differ: room 2 is Fire Idol plus Fire Totem, and the finale is Miracle Totem, Fire Totem and Fire Idol, with no Mecha God.
- ~~The archive reproduces the older Lv. 6 Sanctum layout. Rev 659707 is the 2020 re-release.~~ **Corrected November 2:** all 44 revisions (2015-2020) carry the same line-up; the archive's Totem/Idol finale was `tools/gen_vortex_battles.py`'s AUTHORED guess ("No wiki line-up survives"), not an older version.
  - **Decided (Evan, November 2): end-of-service versions for every system.** Built: see the November 2 section below.
- Boss HP and capture rate cannot be recovered from client data. There is no monster MST in `deploy/mst`: `MonsterMst` arrives in each `MissionStart` from the server. They stay unknown until another source is found, or are authored and marked (Decision 2).

**#12 first encounter: 8301560 "The Savage Lion" (area and dungeon 800156).**

- **`mission_mst`:** 1 battle, energy 25, EXP and Zel 2,000, no prerequisite; first-clear reward unit 850808 (`6:850808:1:1:1`).
- **[Hard] 8301561:** needs 8301560; energy 50, EXP and Zel 6,000; reward `8:0:3:0:0`.
- It has a client battle script (September inventory). So decrypt `F_MISSION_SCRIPT_MST` before authoring waves (`reference_mission_script_contract`).
- Wiki boss: Savage Lion Baragan. No `revid` fetched yet.
- **Missing:** the wave, stats and AI.

**#15:** no new evidence. The decision stays open and explicit: (a) Global-only, recommended, or (b) also the 31 JP-only units.

**#26 per-hit items (the correction's #26 sentence):** traced in round 8, and the gap is both a missing channel and missing content.

- The client reads `BattleMonsterGroupMst.i30R8TAs` (readParam 0x13CEDA4, setter 0x13CF12C). `MonsterParty::setMaterialDrop` (arm64 0x10C2354) parses it as a comma list of `per:itemId:maxCount`.
- Live-format captures carry `7:10000:3,7:10300:3` and `7:10301:5,3:10603:1`.
- Our schema has no such field, and the archive has no per-monster item list.
- The MissionEnd award path exists and was exercised (round 8, run v4).
- Details and the bounded follow-up are in [the round-8 handoff](CLAUDE_FRESH_ROUND8_HANDOFF_2026-10-31.md) §3.

**Recommended-batch order after this evidence:**
- #18-a becomes "author the story Juggernaut's skill and ailment profile from rev 674494 (authored rate marked)", not "reuse the Lab AI".
  - **Round 8, November 1:** authored and staged (`scripts/gen_story_bosses.py`): the resists, plus skill 9600 Atom Cannon cast by AI 30000085. Monster skills crashed the Windows client until the delivery contract (`8aiBoHg5` on MissionStart) was recovered and built into server candidate `e37da19b` ([round-8 handoff](CLAUDE_FRESH_ROUND8_HANDOFF_2026-10-31.md) §3e). Boss skill kits for #18-b/c are no longer blocked, but ship only with or after that server.
- #17 waits on the version decision.
- #12-a starts at 8301560 once its client script is decrypted.

Current next work is [round 7](CLAUDE_FRESH_ROUND7_2026-10-31.md); independent evidence and acceptance limitations are in [round-6 QC](QC_FRESH_ROUND6_2026-10-31.md). Historical batches above are proposals, not completed or authorized deployments. Round 8's results and the recommended next content batch: [round-8 handoff](CLAUDE_FRESH_ROUND8_HANDOFF_2026-10-31.md).

## Round 8, November 2: Evan's decisions and what was built

Evan: "We want the EOS functionality for all systems"; #12 "is a wiki scrape / community authoring effort if nothing
exists on the wiki" (his 2026-09-18 rule: "invent the battles and the community will fine tune"); #15 yes, plus "scan
the wiki for all units to confirm every single unit is playable so I can update the client accordingly". Nothing below
is in deploy/; each is staged and tested (handoff §3g, readiness §3).

### #17 Mecha Gods (built: `scripts/gen_mecha_gods.py`, test 12/0)

- Source: The Mecha Gods Awaken rev 659707, the latest revision. Its footer says the dungeon became permanent in the
  Vortex on November 1, 2018, so this is the EOS content.
- All six Lv. 6 Sanctums (100320-100325):
  - Room 1 is the archive's own (Idol + Totem).
  - Room 2 is the wiki's line-up, rows copied from the archive.
  - The boss is the element's Mecha God (unit 10354 / 20344 / 30334 / 40334 / 50394 / 60344), alone, under the
    client dictionary's first monster id for it.
  - Each boss has its two named skills (Explosion 1054 / Volcanic Cannon 1055, ... Invasion 1854 / Ominous Shade 1855).
  - "Used every 3 turns" lands on exactly turns 3, 6, 9, ...
  - Monsters act "up to 4 times per turn" (High Elf 3), and the wiki's susceptibilities apply.
- AUTHORED (no revision gives any of it):
  - boss HP 700,000 (the Guardians of Lore Lv. 3/5 line at 7,000 EXP);
  - ATK 580 / DEF 100;
  - room-2 HP 100,000;
  - capture 40 % (the offline floor);
  - the skill percentages.
- Wire: a rolled Fire Mecha God capture settles into the box.

### #15 every unit (audit: `scripts/audit_unit_playability.py` -> [UNIT_PLAYABILITY_AUDIT.md](UNIT_PLAYABILITY_AUDIT.md))

- The wiki side: all 2,497 pages using the unit infobox (2,490 ids; `scripts/data/wiki_units.json`, revid per unit).
  Its `unreleased = true` flag marks the 251 JP-only units.
- Today, on deploy: **2,340 of 2,719 units are PLAYABLE**.
  - **205 JP-only units are not in the client at all.** They have no `unit_mst` row and no art.
  - **129 Global units have thumbnails but no battle sprite, part layout or animation scripts** (Megurine Luka
    60584/60585 and the 810xxx-860xxx series).
    - 100 of them sit in summon pools: the four Test Gates and two 2020 DBB selectors.
    - Their file list is `out/release/unit_missing_client_assets.txt`.
  - 6 SuperAnim units (Korzan, Haile) are complete.
  - The 212 EU static stand-ins count as playable.
- **Built (`scripts/jp_units.py`, test 12/0, staged in `R8/jp_root`):** the 29 JP-only units that ARE in the tables
  now have archive records, so they can be granted.
  - Names: 22 from the wiki's `altname`, 2 from its title, 6 AUTHORED.
  - Their skill text is an AUTHORED English translation.
  - The 8 name-less units get their names (7 Mystery Frogs, Wonder Mirror Idol).
  - It stages seven reissued dictionaries and F_RESOURCE_MST 3279, above #22-3's 3278.
  - Wire: a present of Demon Lord Dino arrives with its own BB/SBB/LS.
- **Still open:**
  - The 205: data needs the JP datamine (`cheahjs/bravefrontier_data` `jp/`, last updated 2021-04; not downloaded)
    and art from Evan.
  - The 129 Global units: art from Evan, or the EU-style static stand-ins.
  - 9 evolutions the wiki lists and `unit_evo_mst` lacks (e.g. Crimson Scout Reda 10716->10717, High Commander
    Rahnas 61016->61017).
  - 50876 (no leader-skill row) and 9850167 (guest unit).

### #12 Vortex Trials (dataset: `scripts/data/vortex_trials_wiki.json`; generator: `scripts/gen_vortex_trials.py`, test 11/0)

- 39 of the 41 missions match their wiki quest by name and energy / battles / EXP (latest revisions, 2020-2023). The
  other two are 8301541, which has no page, and 8301551 / 8301553, whose signature differs and is recorded per row.
- **The client already has the kits.** Every boss's named skills exist in F_SKILL_MST under the wiki's exact names,
  with their late-game effects (shields, Time Stop, turn skip) as data the client executes:
  - Odin 20000800-812, Frigg 830169001-014, Herald 830169101-109;
  - Aesys / Ptah 885003xx, God-King / Ma'at / Set 14000030xx, Osiris 140000xx.
- **Art:** the bosses fight as units with art (Herald of Yggdrasil 760006, Frigg 730287, Odin 750317, Aesys 810688,
  Ma'at 840688, Set 860688, Osiris 830688, Scorching Blade Roy 810858, Hetepheres 830358 ...). Seven have none:
  - Savage Lion Baragan, Curse Master Elhaza, Warmonger Zaad, Illusionist Melfel;
  - Vizier of Chaos, True God-King;
  - the dryads.
- **What each fight still needs:**
  - its AI rows. The wiki states every trigger in prose; the AI grammar has turn, HP and flag terms, but no "shield
    still up" or "adds alive", so those need authored approximations;
  - and, for the 18 with a client battle script, that script's monster-id contract.
- **Built: 83013400 Winter Paradise** (rev 665774): the four element Imps, three Burst Frogs, Void Tablet Emgif, and
  Imbued Tablet Elgif. Everything is capturable. One wiki group per wave.
- **Next:** the fights in dependency order, starting with the unscripted chains whose bosses have art:
  - Mythological Entities 8301690 -> 8301692;
  - Traversing Eternity 8301570 -> 8301571;
  - the Burden of Aesys paths 8301470 -> 8301492, then the Scorpion-Lord 8301500.

### November 2, later (handoff §3h): applied, and every unit accounted for

- **Applied to live (Evan: "Apply it and test"):** the story bosses, the Mecha Gods, Winter Paradise, the 29 JP units,
  #14, #22-3, the Training Ground fix, the boss icons and the material drops. The full regression passes.
- **Winter Paradise is not reachable from the Vortex.** Its dungeon sits in the "Vortex Trials" parent area, which this
  client draws as a list of child areas only. It needs its own child area, or a move, before players can enter it.
  - Its fight was client-verified as mission 10's waves (captures included).
- **The 129 "Global units without battle art" are SuperAnim units with complete files.** The audit is corrected and the
  units are client-verified. (Battles in the S19 test client sometimes crawl, SuperAnim or not; cause unknown.)
- **99 JP-only ids = Global counterparts in the game** (`scripts/data/jp_units/jp_only_roster.json`).
- **107 JP-exclusive units are in live** (installed November 2, evening; `scripts/jp_exclusive_units.py`;
  readiness 6c):
  - data from the JP datamine;
  - authored English;
  - the wiki's thumbnails and full art;
  - authored battle sprites: 60 animated from the wiki GIFs, 47 static.
  - Audit on live data: 2,613 PLAYABLE, 99 COUNTERPART, 8 INCOMPLETE.
- **Still open:**
  - 50876 (no leader-skill row);
  - 9850167 (guest form);
  - six element-guide units without comment rows;
  - the 40 Vortex Trial AIs. The JP datamine's `ai.json`, 14 MB, was not downloaded and may help.

### November 3 (handoff §3i): #12 built, staged

- All 41 Vortex Trial missions are generated (`scripts/gen_vortex_trials.py`): the wiki's line-ups, HP and skill kits,
  the client scripts' phase chains, authored AI and stand-in art.
- They are reachable through the Vortex Trials subtree permits (server candidate `ee6f920f`) and Winter Paradise's
  own child area (`scripts/vortex_trials_topology.py`).
- Client-checked: the five child areas draw; The Warmonger fights; The Savage Lion's flag chain clears and pays
  its first-clear unit.
- Not live. Everything authored is listed in [MISSING_CONTENT.md](MISSING_CONTENT.md) §2.
