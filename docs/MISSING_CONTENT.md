# Missing content: what no source has, and the stand-ins that keep each system working

Started November 3, 2026 (Evan: "If the wiki and our files truly don't have the data required, take note of it in a
missing content document and do your best to make the system functional similar to how the units are still images
when they lack their sprite sheet").

**Sources searched for every item below:**
- the Global wiki (latest revisions, the end-of-service versions);
- the JP datamine (`cheahjs/bravefrontier_data` `jp/`, 2020-07);
- the EU datamine and Evan's `EU Units` folder;
- Evan's EU client app-data dump (`Downloads\bf\bf\estratto`, 2026-11-03: 41,727 files);
- the Global client's own tables and caches.

"AUTHORED" means a value or asset made here because no source has it. Each one is marked where it is built, so the
community can replace it.

Status words:
- **Built**: authored from sources, playable.
- **Stand-in**: works, but some part is AUTHORED (listed).
- **Missing**: not built; why.

## 1. Units

| What | Status | Detail |
|---|---|---|
| EU-only units' battle sprites | **187 of 212 real** (2026-11-03) | The EU client's own part layouts (cgg) from Evan's dump, beside sheets pixel-identical to the EU folder's (`scripts/eu_units.py` `real_sprite()`, staged in `tools/eu_sprites/`). |
| 25 EU units | Stand-in (still image) | No complete set anywhere. 3 dump sets have parts outside their sheet (710145, 730215, 750157). The other 22 have no dump set (710013, 710014, 710157, 720104, 720137, 720147, 720186, 720187, 730137, 730146, 730165, 730186, 740196, 750114, 750176, 750177, 750186, 760145, 760176, 760187, 760216, 760267). |
| 47 JP-exclusive units | Stand-in (still image) | No sprite sheet in any source (the EU client never had them; the wiki has no GIFs for these). |
| 60 JP-exclusive units | Stand-in (animated) | Animated from the wiki's idle/attack/move GIFs (`scripts/unit_sprite_stand_ins.py`). Some attack GIFs bake an additive glow as an opaque grey disc (Gandalf's spell): a GIF cannot store additive blending. |
| 9 evolutions the wiki names | **Built** (2026-11-03) | The JP datamine's recipes (Crimson Scout Reda 10716→10717, High Commander Rahnas 61016→61017, MEIKO, KAITO, Samatha, Rogi, Ramu), F_UNIT_EVO_MST 601. |
| 50876 聖輝の蒼勇神ハルト | Missing → buildable | In the client's unit table. The JP datamine has the unit and its leader skill 10700, which the client's table lacks. Not built yet. |
| 9850167 Empyrean Juno-Seto G | Missing (by design) | A guest form for a story battle; no source gives it player data. |
| 6 element-guide units (100884, 200212, 300021, 400023, 500242, 600092) | Missing (cosmetic) | Not on the wiki; no comment text anywhere. |

## 2. Vortex Trials (#12) -- `scripts/gen_vortex_trials.py`, `scripts/vortex_trials_topology.py`

**41 missions, all built** (2026-11-03). Sources:
- the wiki dataset: boss line-ups, phase HP and skill names, matched to F_SKILL_MST;
- the wiki's per-phase skill notes (`scripts/data/vortex_trials_skills.json`);
- the client's battle scripts: monster ids and phase order. Trigger 5 is the boss AI's flag (decoded 2026-11-03).

**Reachability:**
- The trial areas are permitted through the Vortex Trials subtree. This needs server code: `ee6f920f`, live since November 3.
- Winter Paradise moves into its own child area (80013400), staged as F_DUNGEON_MST 1004.
- An area shows only when a fight under it exists.

**What is AUTHORED (community tuning welcome):**

| Item | Where | Why |
|---|---|---|
| Every boss AI | `ai_rows()` | The wiki states triggers in prose. Mapped:<br>- "turn N" → that turn, once<br>- "every N turns" → every N<br>- "every turn" → 50 %<br>- "HP < N%" → once below N<br>Everything else is a 20 % pool. Instant-defeat skills only at a stated turn.<br>Not expressible in the client's AI grammar: shields "still up", adds alive, BB/SBB counters, marked units, Doom counters. |
| Later phases' timing | `ai_rows(relative)` | The AI's turn counter runs from the battle's start, and no term counts a monster's own turns. So a phase's "Turn 1" becomes its first chance, and later fixed turns join the pool. |
| ATK / DEF | `TIERS` | No source gives any: 2000/800 (10 energy) up to 6000/3000 (100 energy). |
| Zel / Karma drops | `boss_row()` | The archive's richest Vortex boss drops. |
| HP left blank by the wiki | `BLANK_HP` | 8301551 Fiora 8M, 8301570 Ma'at and Set 45M each, 8301471 60M, 8301481 60M, 8301482 90M, 8301491 120M, 8301500 Osiris 180M; adds sized by the fight. |
| Third phase the wiki omits | `build()` | The normal Savage Lion and both Curse Masters have a third scripted phase; it takes the previous phase's HP. |
| Merged / split HP | `MERGED_HP` | Chilling Beauties (each beauty's two phases as one bar) and 8301472 (one 170M split between Malvan and Hetepheres). |
| Fights not run through their script | `MISSIONS` | Chilling Beauties ×2 (phase 1 needs both beauties under 1 %, a state the server cannot see) and the True God-King ("the battle ends upon the body being defeated"): fresh monster ids, so the script does nothing. |
| Boss art | `STAND_INS` | No sprite exists in any source for these, so each fights as a unit of its element and kind:<br>- Savage Lion Baragan → Ember Hellion Adel<br>- Curse Master / Elhaza → Accursed Calamity Velma<br>- Warmonger Zaad → Beast God Exvehl<br>- Illusionist Melfel → Steel Magus Dessit<br>- Vizier of Chaos → Wicked Sorcerer Gregor<br>- True God-King Body / Head → God Atro / God Knight Will<br>- Chilling Beauty Rene → Ice Queen Selena<br>- the dryads → Dryad<br>- Hugin / Munin → Crow Tengu<br>- the auras → Imbued Tablet<br>- Tainted Rivermaw → Leviathan<br>- Corrupted Fleshsnapper → Crocodile Lord Kawab<br>- the necromancers → Necromancer Lemia / Lilly Matah<br>- Undead Entity → Lich<br>- Wild Hoof → Horseman Sodis<br>- Claws of Osiris → Skeleton King |
| 8301541 "Return of the Goddess" (area Weaver of Friendships) | `TRIALS` | The wiki page does not exist. Its waves are the 10-energy openers' Skeleton line-up. |
| The 10-energy Burden of Aesys openers (8301470/80/90) | `SKELETON_PATHS` | The wiki names only Skeletons; their HP is authored. |

**Client-checked** (runs v25/v25b on a copy of the live save, S19 package):
- the Vortex Trials tile opens onto its five child areas;
- The Warmonger fights with its stand-in;
- The Savage Lion's AI-flag phase chain runs to "AREA CLEARED" and pays the first-clear unit;
- no crash.

Not played: the other 38. Their kits are the client's own skill rows; one late-game kit has now run on x86.

**Not modelled:**
- "Can only be accessed once a day" (no per-mission daily limit);
- clear rewards beyond the client's first-clear rewards (Exalted Tokens: no such item in item_mst);
- the anniversary bonuses.

## 3. Systems

| System | Status | What is missing, and where it would come from |
|---|---|---|
| Omni+ Boost (`UnitOmniEvo.cpp`) | Missing (probe) | The request's node that names the boosted unit is unknown, and a guess could spend 4M Zel on the wrong unit. One tap in the client with the probe answers it. Nothing fills `omni_level` yet. |
| Guild raids, boards, research | Missing (stubs) | 43 of 45 guild requests answer `{}`. Raids have no data model: no seasons, rounds, rooms, bosses or rewards in any source we hold (the live server sent them; no capture exists). |
| Arena | Locked by decision | To become a simulated online mode later. |
| Summoner's Journal | Stand-in | 28 of 45 missions have no progress source yet (each needs its counter wired). 18 rewards are stand-ins: items this server has no row for (Green Grass, High Cure, Summon Ticket, Metal / Jewel Key, Divine Light, …). |
| Content provider (proxy) | Ships disabled | Blank Slots tiles (#27) and first-visit art pop-in need it. |

## 4. In Evan's EU dump but not used

- 36 EU-only item icons (700001-701046) and 81 EU event quest banners (700100-711700, 91038xx-9108200): EU events this server does not run.
- About 640 `…__vN` versioned art files: the EU client's own cache-busting names, which the Global client never requests (no `__v` string in libgame.so).
