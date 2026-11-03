# Bug backlog status — October 8, 2026

October 11: #24 moved to Issues fixed after player acceptance. Session 15 audio/UI and post-quest prompt removal also accepted on the isolated candidate. Next implementation directive: [CLAUDE_SESSION_16.md](CLAUDE_SESSION_16.md). The Word document was not edited.

October 20 (Session 16): no backlog item moves. Startup work is not on the Word
list; its state is kept separately here. **Implemented and automated-tested** (not
client-tested, not deployed): the startup coverage report, the Session 16 DLL's
census and background-download-prompt decisions, and the C7 families
([OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) §8,
[OFFLINE_CONTENT_PHASE_C7.md](OFFLINE_CONTENT_PHASE_C7.md)). With the shipped content
the report is incomplete (16 startup lists not yet derivable, 67 required names not
in game_content, 26 families whose files exist in no source, Home's layout among
them), so on a real client both decisions stay the game's own. Corrected after both
QC reviews the same day ([QC_SESSION_16_REVIEW.md](QC_SESSION_16_REVIEW.md),
[QC_SESSION_16_CORRECTION_REVIEW.md](QC_SESSION_16_CORRECTION_REVIEW.md)) and returned
for QC; the C16 cold run ([CLIENT_SESSION_16_STEPS.md](CLIENT_SESSION_16_STEPS.md)), a
test of the fallback only, waits for QC. The
"Refreshing the latest data files" notice of the C6 run was the game's master-table
corruption recovery after an interrupted launch, not the image swap (likely, not
proven; §8.5). #24 and the Session 15 prompt removal remain player-confirmed on the
isolated candidate and undeployed. The Word document was not edited.

Updated by Session 14 the same day: #24 (reproduced, root cause, candidate fix,
client pending) and the Session 13 Farm report (explained). No item moves; the
Word list was not opened or edited this session.

Updated by Session 15 (October 9): #24 after the player's Session 14 run (the
script guard removed the early white but not the missing final white) -- a new
candidate in the proxy DLL, client pending. No item moves; the Word list was not
opened or edited.

Reconciles the maintainer's list against current source, data and recorded
evidence. Source document: `C:\Users\Evan\Downloads\Bug fixes BF.docx`, SHA-256
`76e8949a6e88ab697e51e84335a2e41b969163bb33e7d3afea42aa11dfe546e3`, last
modified 2026-10-03 10:14; read through a copy only (a Word lock file showed it
open). **The Word document was not edited.** Its text, extracted verbatim, is
in `out/qa/current/session13/bug_doc_text.txt`; quotations below keep its IDs,
spelling and punctuation exactly (both the remaining and the fixed list carry
three items numbered #22, distinguished here as 22-1, 22-2 and 22-3 by their
order in the document).

Status classes follow the handbook's evidence model:

| Class | Meaning |
|---|---|
| **Player-confirmed** | The maintainer observed the reported behaviour fixed in the client. Scope is what was played, nothing wider. |
| **Implemented, server-tested** | Code or data exists and encrypted wire tests pass on the current server candidate (e30e2adf: 51 runner entries, 1,992 checks, 0 failed, Session 12; no server source has changed since). No client observation yet. |
| **Partial** | Part of the report is fixed or confirmed; a named part is not. |
| **Unresolved** | No fix exists, or a fix could not be identified. |

## Summary of defensible moves

| Move | Items |
|---|---|
| Remaining → Fixed | #34 (player-confirmed reported workflow) |
| Stay in remaining, add a status note | #12, #15, #17, #18, #20, #22-1, #22-2, #22-3, #23, #24, #26, #27, #30, #33, #39 |
| Stay in Fixed, add a scope note | #8, #11, #13, #16, #19, #40 |
| Fixed, but the wording does not match the evidence | #14 (the fix was Tilith 50253; Goddess Tilith 50254 still has the defect), #25 (the SP system is fixed; frog eligibility in the client is not observed) |
| Test question answered | Legend Stone: Merit yes, Guild no (client catalogue) |

## Test item

| Quoted | Status | Evidence | Remaining check |
|---|---|---|---|
| "Does the legend stone appear in the merit/ guild shop?" | **Merit: implemented, server-tested. Guild: not in the client's catalogue.** | Merit Exchange offer `80000059` "Legend Stone", reward `4:110100:1`, 4,000 Merit, limit 99, in the client's own `achievement_trade_mst` (`deploy/mst/achievement_trade_mst.json`, 241 rows); `test_merit_exchange_wire.py` buys two (8,000 points, two stones, 97 left), 66/0. The client's `guild_point_exchange_mst` (121 rows) has no Legend Stone; none was invented. Item 110100 is also offered by the Event Bazaar (`deploy/archive/event_exchange.json`). | Open the Merit Exchange in the client and confirm the Legend Stone row, its cost and stock, buy one, reopen. Recorded client observation: none. |

## Issues remaining

| ID | Quoted (verbatim) | Status | Evidence | Remaining acceptance |
|---|---|---|---|---|
| #12 | "Vortex Trials are an empty stub and will need to be built out" | **Unresolved** | Current `deploy/archive/mission.json`: 0 of the 40 Vortex Trials missions (AreaMst 100003) are archived — unchanged since Session 3's inventory (`out/claude_session2_2026-09-29/12_trials_inventory.md`). Adjacent work exists but is not this: Research Lab Trials 001–003 (2000000–2000002) and The Creation God (2100004) are archived and server/model-tested (`test_research_lab_wire.py` 39/0, `test_research_lab_ai.py` 97/0), not client-played. | Author the Vortex Trials encounters (stages, bosses, AI, rewards, unlock chain), then play each. Do not count the Research Lab encounters toward this item. |
| #15 | "A bunch of EU units are missing from the game still including special units like Deemo and girl" | **Partial** | 114 Global units restored to `deploy/archive/unit.json` (Session 3; `test_unit_roster_wire.py` 18/0), including Deemo and the Girl 50563/50564/50565 and 60864/60865. Player-confirmed only for three supplied Deemo forms: inventory, squad and a mission-10 battle (October 1–2). 31 JP-only units left out on purpose; 30646 blocked (no name anywhere); known art gaps (e.g. 850977 lacks `unit_anime`/`unit_cgg`). | Deemo evolution 50563→50564 and battle skills; spot-check other restored units (overview, Archive, battle); an inventory of what "EU units" covers beyond the 114. |
| #17 | "Fire Mecha God isn’t dropping after playing the battle multiple times (Vortex Battle)" | **Unresolved** | No archived battle fields or drops Fire Mecha God 10354 (searched every monster and drop in the current mission archive). The Sanctum Lv. 6 missions 100320–100325 exist without the Mecha God bosses the wiki names; sourced stats and capture rates are still missing. A Fire Mecha God reached the player only as an evolution-test present (Session 6). | Source the boss stats and capture rule, author the encounters, then a capturable and a non-capturable run. |
| #18 | "Big boss units aren’t implemented and are replaced by placeholder units (Only Juggernaut is currently visible, but his battle AI might not be)" | **Unresolved** | `deploy/archive/ai.json` holds 12 AI records: "Attack random", the Training dummy's never-acts record, and 10 Research Lab boss records (Trials 001–003 and The Creation God: Karl, Grahdens, Juggernaut, Demon Abaddon, Creator Maxwell, Inception God Maxwell). Outside those encounters, 10 of the 11 special-art bosses inventoried (`out/claude_session2_2026-09-29/18_boss_inventory.md`) still use "Attack random" in their own missions, several with 10,000-HP placeholders; Xie Jing has no art. | Per boss: stats, phases/AI, art size, results and replay. Substantive authoring, not a configuration fix. |
| #20 | "Finishing “A flash of Lightning” stage in Asekutt Wasteland results in the client crashing" | **Unresolved** | Assets and prerequisites were found valid (September 29); no crash reproduced, no fix. | First clear and replay of mission 234 on an eligible copied save, through the Cordelica unlock; keep the crash dump (`%LOCALAPPDATA%\CrashDumps`) and server log. |
| #22-1 | "User reported that the unit Lukroar’s gate was a black void (no gate was presented but the touch icon was present)" | **Unresolved** (no defect found) | Unit 51247; summon effects 900/901 reuse the 7★ scripts; every script, SAM page, plist and sound exists and the cache decodes byte-identical (Session 3); `test_summon_reveal_wire.py` 273/0. No server or data defect found; the black void was not reproduced. Session 13: the gate's SuperAnim and particle files are now in the C3 provider policy (served from game_content when the provider runs); its scripts and sounds stay with the game's cache. | Film a Lukroar summon (normal and special gate) next to a known-good summon. |
| #22-2 | "User shows the unit Omni Amadream has a bad unit overview screen name where certain things like unit number, rarity (Omni), element symbol (light), and unit’s name are all too jumbled together to look correct." | **Unresolved** (client layout) | The overview reads the client's own text and layout; the data is complete (`test_unit_text_data.py` 8/0). Nothing server-side to change was found. | Screenshot Omni 51337's overview header at the supported resolution beside another long-name Omni. |
| #22-3 | "User reported Omni Galtier’s leader skill was cut off at “…Dark types & 20% damage reductio” missing the n and possibly more of the unit’s leader skill due to truncation." | **Unresolved** (client layout) | Leader skill 801271 is 290 characters in the data; the report cut at character 252; the screen reads client text. | With refreshed MSTs, scroll/wrap the full text on 860428; compare Durumn 840278. |
| #23 | "User reported that after finishing any stage in the tower of misteral, the user got a cutscene for Morgan being unlocked only after the final mission is completed. There is also an issue with an already run cutscene like “Unlocked Farm” after the first stage in adventurers’ prairie playing any time the stage was completed." | **Partial** | Farm part player-confirmed (October 2): the Farm scene plays once, mission-10 replays show no scene, ordinary repeats return to the stage list. Scenario-marker upload/omission/persistence server-tested (`test_mission_progression_wire.py` 50/0, `test_story_gates_wire.py` 35/0). The Morgan / Tower of Mistral part is not established: no final-stage (mission 85) or first-clear next-area presentation has been observed. | Fresh copied progression: an intermediate Tower of Mistral stage (no Morgan scene), the qualifying final stage 85 (scene once), replay both; first clear of a final stage such as mission 23 (next area presented once). |
| #26 | "Units like Roglizer and Fuu don’t have the proper effect on resource gathering when set as squad leaders. They modify resource/ experience gain" | **Partial** | Leader skill process 97 (Player EXP Boost) from the owned leader plus the resolved helper, additive, server-tested for Roglizer 61286/61287 (`test_leader_exp_boost_wire.py` 21/0). Fuu's drop effect and other resource multipliers are not modelled; no client observation. | Same fixed mission with no qualifying leader, each Roglizer, Fuu, and helper combinations; displayed and stored payouts must agree. |
| #27 | "Slots can only return a handful of the actual pool of items and not all items are awarded to the players inventory. On top of that, the user is reporting blank tiles when pulling from the multi-roll (from 2x to 10x) where the artwork is white or the tile is missing all together." | **Partial** | Prize pool and awarding: 54-row table from the Global wiki "Slots" rev 658840, reel strips and a transactional SlotAction, server-tested (`test_brave_slots_wire.py` 251/0); rates and consolation are authored policy. Blank tiles: client-side — `RandallSlotResultListScene::downloadFiles` (arm64 0x1A72AB4) requests nothing, so a tile is blank when its prize art is not already cached. That is an asset-availability problem: unit art is provider-owned since Phase C1, item art is not yet (the next candidate family, docs/OFFLINE_CONTENT_PHASE_C3.md §6). | 1 through 10 rolls with mixed and duplicate prizes; every tile drawn; costs and inventory exact after relog. |
| #30 | "After clearing all stages in Cordelica, the user was granted access to Lizeria despite not completing the second pre-requisite of clearing Palmyna beforehand. We will need to check for similar requirements for other areas on the wiki and ensure the feature gate is set up correctly to accommodate." | **Implemented, server-tested** | Story prerequisites are all-of, direct entry follows them, a stale locked tile returns Home with a message (cmd 6): `test_lizeria_qc_wire.py` 23/0, `test_story_gates_wire.py` 35/0. Special-mode compound permits were still any-of at Session 3. | Four copied saves (neither of 666/20067 cleared, either alone, both): Land 4 and its tiles need both; a stale tile lands on Home. |
| #33 | "If a user sets their level to 999 through a save editor, or reaches it naturally, completing a mission freezes the client on mission end when the correct behavior should prevent the user from gaining anymore experience. Not to mention, level 999 unlocks double energy regeneration speed. " | **Implemented, server-tested** | `test_bugfix_qc_wire.py` (41/0): the level-1000 animation sentinel exists; 999/0, 999/4294967290 and 998→999 complete at level 999 with zero residual XP; energy regenerates two per tick at 999 and one at 998, odd costs included. Result animation, edited saves and the timer display were never played. | Natural 998→999 and edited level-999 copied saves: the result screen exits; two energy per 180-second tick; relog. |
| #34 | "Crafting spheres in the town isn’t being handled properly. Most likely being stubbed and the proper server emitter isn’t sending to the client to immediately show the made sphere. Additionally, the spheres are being treated like they can be stacked with a quantity like regular items instead of taking up it’s own row. The MST should show that the max stack is 1." | **Player-confirmed** | The reported repeated-sphere workflow is player-confirmed (handbook, Session 8 fix deployed 2026-10-04); `test_craft_row_identity_wire.py` 53/0, `test_sphere_qc_wire.py` 35/0. Documented known limit: favorites/sale retries of newly crafted copies before a refresh can be ambiguous. | None for the reported workflow. |
| #39 | "Mega Metal Parade has the wrong units as enemies. Those stages should have progressively higher rarity as the higher stages are access. Lower stage, lower enemy rarity. Ghosts and kings should be gods and crystals with higher HP. Look for the stages on the wiki to confirm health pool, but they should be max defense." | **Implemented, server-tested** | Super 100601 and Mega 100614 waves rebuilt from the Global wiki (revs 630717/630719/630716) with `scripts/gen_metal_parades.py`; `test_metal_parades_wire.py` 18/0. EXP left at the MST's 5,000 (the wiki says 7,000); DEF and capture rates partly authored. Not played. | Normal 100600, Super 100601, Mega 100614: every wave, battle, capture, result and replay. |

## Issues fixed

| ID | Quoted (verbatim) | Status | Evidence | Note for the list |
|---|---|---|---|---|
| #24 | "This may have been fixed on our current build, but a user is experiencing a white light bleed effect on summon gates with two gates. Essentially, the crack/breakthrough effect will happen on the first gate, but the first click is always treated as a unit being revealed and covers up the second gate. I’m not currently experiencing this issue on double gates but that doesn’t mean we shouldn’t check for integrity and consistency with cracking/breakthrough animations suppressing the white screen bleed that covers up a unit reveal transition screen typically." | **Player-confirmed on isolated candidate (October 11)** | Session 15 Parts A/B passed: single/double gates, normal/overflow energy and normal/rapid taps; no early white, required final reveal flash present. DLL follows final-opening state, not global animation count. See [QC_SESSION_15_REVIEW.md](QC_SESSION_15_REVIEW.md). | Reported visual fix accepted; live deployment remains separate. |
| #8 | "Guild/Merit shop stock + Expiry Time removal" | **Player-confirmed** (expiry); stock server-tested | Exchange expiry fix player-confirmed (handbook, September 27); stock and payment `test_merit_exchange_wire.py` 66/0; Guild exchange 121 offers with a persistent wallet/stock. | Keep. |
| #13 | "Burst Queen wasn’t leveling brave burst and SBB all in one go (no spill over)" | **Player-confirmed** | September 27 confirmation; October 2 Berdette chain reached BB 10 / SBB 10 with Burst Queens as expected. | Keep. |
| #14 | "Seria 4* was being drawn weird on the home screen portrait." | **Player-confirmed for Tilith 50253 only** | The September 27 fix and its confirmation are Tilith 50253's Home crop. Swordswoman Seria 10233's crop frames correctly in the data (not client-checked). Goddess Tilith 50254 still has the old off-panel rectangle `245,-5,135,369` (current `unit_mst`); fixing it needs a new downloadable F_UNIT_MST version — a deployment decision, not taken. | Reword as Tilith 50253, and add 50254 as a remaining item if wanted. |
| #16 | "Mystery frog wasn’t resetting to level 1 and changing the unit type correctly." | **Player-confirmed** | September 27 confirmation; random odds are authored policy. | Keep. |
| #11 | "Imperial Capital Randall is still crashing the client, specifically the battle simulator (the enemy units are special training dummy enemy units that aren’t capturable)" | **Player-confirmed** | Training Grounds (6000000) entry without a crash, passive dummies, settings, HP/element changes and the reticle (October 2–3); `test_battle_simulator_wire.py` 93/0. | Keep. |
| #19 | "Battles that supply items or taking items into battle results in getting to keep those items or duplicating unused items respectively. That is not the intended mechanic." | **Partial**: ordinary case player-confirmed | Two used potions consumed two (October 2). Settlement and ownership suites pass (23/0, 57/0, 2/0). Raid, Grand Quest and campaign item bars untested. | Keep, scoped to ordinary missions. |
| #25 | "Max level omni rarity units can’t fuse with frogs to gain SP points. This could be due to the SP handler being stubbed. They report that the frogs are greyed out in the client which indicates this is an illegal interaction for the client to make, and not a server stub issue. We want to confirm what is allowed to grant SP instead of relying on a user that hasn’t played the game in several years. The client is most likely correct in greying out frogs for SP gain." | **Partial** | SP storage, purchase (`FeSkillGet`) and the one-gem reset are player-confirmed (Vargas, October 2); fusion SP gains server-tested (`test_enhancement_sp_wire.py` 36/0, `test_fe_skill_purchase_wire.py` 27/0). The October 1 greyed-out frogs came from a save without the SP migration; frog selection on a migrated save, below and at the SP cap, has not been observed. | Fixed as an SP system; the eligibility question in the report is still unobserved. |
| #40 | "Evolving a low rarity unit keeps it’s low rarity brave burst instead of getting the new brave burst (SBB and UBB when applicable, per rarity)" | **Player-confirmed** for the Berdette chain | Berdette 5★→6★→7★: BB/SBB/leader skill per form as expected (October 2); `test_evolution_qc_wire.py` 26/0 across three recipe families. UBB visibility at SBB 5 was not determined. | Keep. |

## Not on the list, found while reconciling

- **Farm harvest (Session 13 player report, explained in Session 14 -- no
  defect found).** "Mountain, forest and river show drops; Farm shows no
  visible drops; Farm upgrade does not produce more harvest sparkle."  The
  run's own fixture had pre-rolled the Farm period as six all-empty entries
  (`0:0:0`: no item, no Zel, no Karma); the client draws nothing for such an
  entry but still uses the tap (`MyTownTopScene::collectItem` @0x18F244C).
  Credits checked against the fixture's pre-claim balances
  (`scripts/verify_town_harvest.py`): every plot paid exactly its entries
  (items; Zel 304 + 380 from the quest; Karma 122 + 43 - 500 for the Farm
  upgrade; 27 taps counted). The upgrade came after the Farm was exhausted,
  and an upgrade never refills (it re-rolls only remaining taps; periods are
  three hours). Under the authored Farm level-1 rates (20 % item, 20 % Zel,
  20 % Karma per tap) an all-empty six-tap period has a 1.8 % chance; the
  rates are unchanged. Server-tested: `test_town_harvest_wire.py` 24/0. Client
  check with known lists: [CLIENT_SESSION_14_STEPS.md](CLIENT_SESSION_14_STEPS.md) Part B.
- Goddess Tilith 50254's Home portrait (see #14).
- The C3 provider policy (Session 13) puts summon-gate and battle SuperAnim
  effects and particle plists under the provider; screens that drew stale
  cached copies of those (the October 2 Town and gate rounds) would no longer
  depend on the cache once the provider is activated. That is a content
  delivery change, not a fix of any item above, and is client-pending
  ([CLIENT_SESSION_13_STEPS.md](CLIENT_SESSION_13_STEPS.md)).

Current-source checks made for this document (read only): the mission
archive's trial and Mecha God content, `ai.json`, the Merit/Guild exchange
masters, Tilith 50253/50254 and Seria 10233 crop columns, and the C3 policy's
owned names. Nothing in the save, deploy data or the Word document changed.
