# Claude Session 3 handoff — September 30, 2026

For independent Codex QC.  Nothing was committed, pushed, deployed or released;
the live server, the live save (`deploy/gme.sqlite`) and the live client
installation were not modified.  Every test ran against disposable SQLite
backup copies under `out/`, with an explicit isolated config and its own port,
on the isolated Debug build at
`out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe`.
**No Windows-client playthrough was performed in this session.**  Nothing
below is client-confirmed unless it says so.

This document also stands in for the Session 2 handoff, which was never
written (QC 2026-09-30); §5 is the Session 2 record.  The September 29 and
September 30 QC documents are kept unchanged as historical evidence:

* [QC review 2026-09-30](QC_DIFF_REVIEW_2026-09-30.md) — the two defects fixed in §2
* [Diff inventory 2026-09-30](DIFF_INVENTORY_2026-09-30.md) — superseded by §6 below
* [Client checklist 2026-09-30](CLIENT_DIFF_CHECKLIST_2026-09-30.md) — Session 3
  additions appended at its end; every case there is still **Not tested**
* [Bugfix handoff 2026-09-29](BUGFIX_HANDOFF_2026-09-29.md) and
  [remaining work 2026-09-29](BUGFIX_REMAINING_AND_CLIENT_QA_2026-09-29.md) — historical

Status labels used: **implemented** (code/data changed), **automated-test
verified** (isolated wire/data tests pass), **client-confirmed** (played),
**already fixed, regression verified**, **blocked** (exact missing evidence
named), **investigated** (findings recorded, nothing changed).

## 1. Issue ledger

| Issue | Status | This session (S3) and root cause | Evidence / remaining |
|---|---|---|---|
| QC P1 — legacy MissionEnd settles an issued run | implemented, automated-test verified | `Mission.cpp` settlement: a mission-id serial now settles only a battle that predates serials (`open_mission_serial IS NULL`), once. Root cause: the low-serial branch checked the open *mission*, not whether the open *battle* was legacy. | Codex edge test 0 of 2 → 2 of 2 passed; new ownership matrix 42 of 57 → 57 of 57 (§2). |
| QC P1 — stale Continue charges and overwrites resume | implemented, automated-test verified | `MissionBreak.cpp` rewritten as one transaction: ownership (issued serial = open run; mission-id serial only while pre-serial), conflicting-body and replay checks run **before** any write; stale/unknown/foreign/conflicting get the ordinary reply with no charge, mark or record. | Same tests; includes injected failure, race with MissionEnd, no gems, restart, pre-upgrade (§2). |
| #34 crafted spheres | implemented, automated-test verified; client **not** tested | S3: lock multiplicity from the Windows consumer (§3.1); unsent-copy lock set refused whole when it cannot be mapped (unchanged safety). S2: placeholder resolution, seen mark, row-id Merit delivery, lock set on 12 replies. | Placeholder 39 of 39, consumer model 21 of 22 → 22 of 22, reward locks all pass (22–24 checks, varies with slot prizes), integration 18 of 18, sphere 35 of 35. Limits §3.2. |
| Performance (tombstones) | implemented, automated-test verified | `Warehouse.cpp` reads live rows only; migration `30092026_WarehouseLiveRowIndex` adds a partial index. Tombstones are kept. | 3,200 live + 20,000 tombstones: UserInfo 1,832 → 334 ms, MissionEnd 1,769 → 308 ms (§9). |
| #30 stale locked tile | implemented, automated-test verified; client **not** tested | A locked MissionStart now replies `GmeErrorCommand::ReturnToGame` (6) with a player-facing message instead of `Close` (4). Root cause: every handler error is `Close`, which `GameScene::noticeOK` answers with `CommonUtils::appExit`. | Story gates 32 of 35 on the pre-fix build → 35 of 35. Needs the client check that OK lands on Home. |
| #15 missing EU/special units | implemented (data), automated-test verified | `tools/gen_unit_archive.py` (git-ignored) skipped 114 Global units: 113 because the wiki title suffix "(4★)" failed its English filter, 1 for a no-break space. Regenerated `deploy/archive/unit.json` (+114 rows, 0 existing rows changed). "Deemo and the girl" is ONE unit, Deemo and the Girl: 50563/50564/50565 (Light 4–6★) and Dark 60864/60865. | Roster test 18 of 18 (grant by present, evolve 50563→50564, restart). 31 JP-only/unnamed units left out on purpose; 30646 blocked (no name anywhere). Art gaps §10. Client art/overview/battle rendering not tested. |
| V1 Legend Stone | investigated | Merit: client row `80000059` Legend Stone, 4,000 Merit, `4:110100:1`. Guild: the client's own `guild_point_exchange_mst` (121 rows) has **no** Legend Stone; no offer invented. Event bazaar: two wiki-sourced offers (tokens 13 and 61). | Merit delivery/payment tests pass (regression suites). Guild absence is the client catalogue, recorded not changed. |
| #14 Seria vs Tilith | investigated | The earlier #14 fix and its client confirmation (Sept 27) are Tilith 50253's Home crop. Swordswoman Seria 10233's Home crop frames her face correctly — no Seria defect found. **New:** Goddess Tilith 50254 (5★) still has the old off-panel rectangle `245,-5,135,369`; 50253's authored `303,50,180,492` frames it (evidence `out/claude_session3/portraits/`). | Not changed: fixing it means a new F_UNIT_MST download version in the served content folder, i.e. a deployment. Needs approval. |
| #25 frog SP | partial; **blocked** for spending | S2: fusion SP gains. S3 found **`FeSkillGet` (GroupId `nSQxNOeL`, key `nZ2bVoWu`) is not registered**: spending SP on the Enhancements screen closes the client. `UserUnitInfo.fe_skill_info` (`Fnxab5CN`) is never stored or sent. | Request shape decoded (§10). Needs the `fe_skill_info` format and the owned-unit update path before implementing; no stub added. |
| #22A Lukroar gate | investigated, automated-test verified (data/wire only) | Unit 51247 is the only Lukroar; effects 900/901 (authored 2026-09-13) reuse the 7★ scripts; every script, SAM page, plist and sound exists; the client cache decodes byte-identical (PNG cache = byte+i² mod 256). No server/data defect found. | Summon reveal 273 of 273. Black void not reproduced; client check pending. |
| #24 double-gate flash | already fixed (2026-09-11/13 script patch), regression verified (data) | Section D of the summon suite checks the patched shape; the stock APK scripts fail it (negative control). | Visual acceptance pending. |
| #22B / #22C | investigated | Client text is complete (Galtier LS 801271: 290 chars, report cut at 252); both screens read client text. Client layout. | Unit text 8 of 8. Client check pending. |
| #12 Vortex Trials | investigated | "Vortex Trials" is AreaMst 100003: 17 areas, 40 missions, **0 archived**, 19 with client mission scripts, 10 with wiki boss data; only the category tile is permitted. | `scripts/inventory_trials.py`; table in `out/claude_session2_2026-09-29/12_trials_inventory.md`. Content authoring is the next task. |
| #18 large bosses | investigated | 11 special-art bosses: 10 run only "Attack random" (ai id 1), 1 mixed (Juggernaut authored in two Research Lab trials); several HP 10000 placeholders; Xie Jing has no art at all. | `scripts/inventory_bosses.py`; `out/claude_session2_2026-09-29/18_boss_inventory.md`. |
| #11/#17/#19/#20/#23/#26/#27/#33/#39/#40 | unchanged this session | See §5 and the client checklist. #17 blocked on boss stats/capture rates; #20 needs a client run. | — |

## 2. Priority 1: the two reproduced mission-run defects

### Reproduction (unchanged Codex test, before any change)

```
python scripts/test_mission_run_edges_wire.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe
FAIL  legacy result cannot close or reward a serial-issued run  -- {'open_before': 1000000002, 'open_after': 0, 'zel_delta': 423}
FAIL  stale continue cannot charge gems, replace resume data or mark the current run continued  -- {'current': 1000000004, 'stale': 1000000003, 'gem_delta': -1, 'break_after': '1000000003'}
independent mission edges: 0 passed, 2 failed      (exit 1)
```

Log `out/claude_session3/edges_before.log`, executable sha256 `4ea52754a793bca9…`,
preserved with its DLLs in `out/claude_session3/exe_before_fix/`.  Running it
unchanged necessarily re-created its fixture folder
`out/qc_2026-09-30/mission_edges/` (config, save copy, server log, 18:09);
Codex's captured logs `mission_edges.log` / `edge_probe.log` were not touched.
Every later run of it went through `out/claude_session3/redirect.py`.

### A. Legacy results (Mission.cpp, the ONE RESULT PER BATTLE block)

`open_mission_serial` is added by `29092026_MissionRunSerials` without a
default, and every MissionStart and every settled result writes it.  So NULL,
and only NULL, means the open battle was started by a server that sent the
mission id as the serial.  New rule:

* issued serial (≥ 1,000,000,000): settles only if it equals `open_mission_serial`;
* mission-id serial: settles only if `open_mission_serial IS NULL` and
  (`open_mission_id IS NULL` or equals the mission) — once, since settlement
  writes 0/0 and 0 is not NULL;
* anything else: the existing zero-reward current-state reply (an error would
  close the client); a serial never issued to this player is still refused
  before any change (not a genuine-client flow).

### B. MissionContinue (MissionBreak.cpp)

Client evidence: `MissionContinueRequest::createBody` @0x13A7528 writes
`MissionInfo::getMissionSerialID` into both `Kz7qfSs5.k9cxD7Ba` and
`5PR2VmH1.k9cxD7Ba`, and `MissionScene::createSuspendData` into `K2gIYm0h`;
`MissionGameOverScene::initContinue` is its only constructor site.  The serial
a resumed battle holds comes back from the login's `5PR2VmH1`
(`MissionRestartScene::initialize` @0x17FC7A8 → `setMissionSerialID`).

One transaction reads the user row and decides, before any write:

* **owned** — issued serial = open run; mission-id serial only while pre-serial;
* **conflicting** — the two serials differ, or status 0;
* **replay** — the resume record already holds this serial *and this exact
  suspend blob*.  A second revival later in the same battle carries new
  suspend data; a resend carries the same bytes.  A false positive costs one
  free revival, the direction the handler already chose ("never refuse").

Not owned / conflicting / replay → ordinary reply (`fEi17cnx`), no charge, no
continue mark, resume record untouched.  Otherwise mark + charge (never
refused: `loopContinue` @0x17FB474 resumes on any answer) + resume record,
written directly so a failure rolls the charge back.  No protocol field was
added.  `MissionRestart` (the resume screen's request) still writes nothing and
only answers `team_info`; its serial is logged, not trusted.

### Evidence

| Test | Pre-fix executable (passed of total) | Fixed executable |
|---|---|---|
| `test_mission_run_edges_wire.py` (Codex, unchanged) | 0 of 2 | 2 of 2 |
| `test_mission_run_ownership_wire.py` (new) | 42 of 57 | 57 of 57 |

The matrix covers: pre-serial saves (both NULL, and mission-only), legacy after
issued starts, other missions, superseded runs, unknown and another player's
serials, retries, restart for issued and pre-serial runs; for Continue also
conflicting bodies, status 0, pre-serial continues, no gems, an injected
failure (trigger on the resume write) with retry, a MissionEnd racing a
Continue, and restart with replay then a new revival.  On the pre-fix build
the race left a live resume record behind a settled battle.
Logs: `out/claude_session3/ownership_{before,after}.log`, `edges_{before,after}.log`.

### Tests changed to match the client (assertions unchanged)

Three suites sent the **mission id** as the MissionEnd serial after a
serial-issuing MissionStart — a request no real client sends, which settled only
through the loophole fixed in A.  Each now echoes the serial its start issued:

* `scripts/test_bugfix_qc_wire.py` (`start()` records, `end()` defaults to it) —
  5 failures without this; one check ("simulator supplied items cannot consume
  owned stock") had also gone vacuous, since nothing settled at all;
* `scripts/test_research_lab_wire.py` (tracked) — same;
* `scripts/test_warehouse_integration_wire.py` — the lock-set-on-MissionEnd check.

## 3. Priority 2: inventory identity, locks, interrupted replies

### 3.1 What the Windows client actually does with crafted copies

Disassembled from `BraveFrontier.Windows.exe` (x86, image base 0x400000):

| Step | Evidence |
|---|---|
| A craft tops up rows below `ItemMst::getMaxNum`, then appends new rows named `0x7fffffff − item_id`; sphere 30000 has max stack 1 (`m9gd5h1u`), so **one row per crafted copy, all sharing that id**. | arm64 `GameUtils::incWarehouseItem` @0x118A0A8 |
| Town crafts go out at once (`MyTownItemMixConnectScene`, `ItemMixConnectScene`); only raid crafts are queued (`RaidUtils::createItemMixConnectList`). Sells go out at once (`ItemSellConnectScene`). | arm64 xrefs |
| A lock is **queued**: `ItemDetailScene::touchEnded` sets the viewed row's flag and adds an `ItemFavoriteRequest` to `ConnectRequestList` (`addObject` compares request ids with those already queued); the body is built when `StepScene` sends the queue. | arm64 @0x17145A4–0x17145C8; `ConnectRequestList::addObject` @0x13914F8 |
| The body: one key per ROW, duplicates kept; each resolved to the FIRST row with that id; sent only if that row is locked (`[row+0x40]==1`). | Windows RVA 0x4B4060, getAllKeys 0x5C09C0, getObjectAtItemIndex 0x20D090 |
| After any reply carrying `VSRPkdId`, every row's flag = "my id is in that list". | `UserWarehouseInfoList::setFavorite` @0x12BCA34 |

So with k crafted copies of one species the request says "placeholder × k"
(the first copy is locked) or nothing (a lock on copy 2..k is never sent), and
after the reply the client shows all k locked or all k unlocked.

**Fix (ItemFavorite.cpp):** k placeholder nodes lock up to k unsent copies
(oldest first, stored before equipped) and echo the placeholder once per copy
locked.  Session 2 locked exactly one, so after the reply the client showed
all three copies locked while the server protected one of them.  Unchanged, deliberately: any entry that
cannot be mapped refuses the whole set with no change (Codex's
`favorites: invalid full-set update does not erase old locks` and the
foreign-row check depend on it; losing a lock is worse than a stale icon).

**Client limitation, not solvable server-side:** a lock put on the 2nd..kth
new copy is never transmitted; after the reply it shows unlocked on both sides
until the next warehouse list gives each copy its own id.  Recorded in the
client checklist.

### 3.2 Remaining limits (documented, not changed)

* **Placeholder sell replay.** `ItemSell` replies `{}`, so a sell retried after
  a lost reply cannot be told from a second genuine sell (identical bodies) and
  would sell a second unsent copy.  The only server-side fix is a warehouse list
  on the sell reply, which replaces client row objects while the sell list scene
  may hold them — the risk the Session 2 prompt ruled out.  Needs client
  evidence of the network-retry path first.
* **Lost warehouse list.** The seen mark advances when a reply with
  `9wjrh74P` is built.  If that reply is lost, a placeholder lock set is refused
  whole (no lock lost) and a placeholder sell is refused (nothing sold twice);
  the client shows its own state until its retry or next list.  Tested
  (`test_warehouse_consumer_model_wire.py`, part C).
* **Species-level spends** (craft materials): the client picks rows by
  priority (deck/equipped/favorite/summoner frame) then pointer order
  (`decWarehouseItem` @0x1189074); the server takes unlocked rows newest first.
  Identical copies make this cosmetic until the next list.
* **Capacity:** the client blocks quest entry above `max_warehouse_count`
  (DefineMst `5pjoGBC4` = 3,200 ceiling); grants are not capped server-side.
* **Mission run history** is kept whole: `missionForSerial` needs old runs to
  answer late results with a zero-reward reply instead of a session-closing
  "unknown battle"; 100,000 rows cost nothing measurable (§9).

## 4. Other Session 3 changes

* **#30 stale locked tile** — `Handlers.hpp` gains `returnHome` /
  `HandleResult::refuseToHome`; `GmeControllerHandlers.cpp` sends
  `GmeErrorCommand::ReturnToGame` (6) and the message as written for it, `Close`
  (4) for everything else.  Client path: `MessageResponse::readParam` →
  `GameScene::checkResponseMessage` @0x1615E4C (cmd 6 → notice −3992) →
  `GameScene::noticeOK` @0x16098AC jump table: −3992 rebuilds `HomeScene2`
  (non-raid), −3999 (`Close`) calls `CommonUtils::appExit`, −3998 (`Continue`,
  empty URL) only dismisses.  Only MissionStart's "Mission locked" uses it.
* **#15** — see §1; generator patch (git-ignored file, so recorded here):

  ```python
  # tools/gen_unit_archive.py, main(): the client-name fallback
  name = name.replace("\u00a0", " ")
  wiki_name = (wiki.get(uid) or {}).get("name")
  if name and (not wiki_name
               or (not ENGLISH.match(str(wiki_name)) and ENGLISH.match(name))):
      wiki[uid] = {"name": name}
  ```
  Previously: `if name and not (wiki.get(uid) or {}).get("name")`.  Self-check
  regenerates all 2,258 prior rows unchanged; backup of the old archive
  `out/claude_session3/unit.json.before`.

## 5. Session 2 record (its handoff was never written)

Root causes and changes, all implemented and automated-test verified, none
client-confirmed.  Counts are checks passed of total, pre-fix build (or data)
versus fix, from `out/claude_session2_2026-09-29/*_before.log` / `*_after.log`:

| Issue | Change | Before → after |
|---|---|---|
| #34 | Placeholder ids `INT32_MAX − item` resolved to unsent rows (`warehouse_seen_id`, `29092026_AddWarehouseSeenMark`); ItemSell/ItemFavorite/ItemSphereEqp; Merit delivery by row id; UnitSell in one transaction; `item_favorite` (VSRPkdId) on the 12 replies that carry `9wjrh74P` | placeholder 18 of 35 → 35 of 35 (39 of 39 after the S3 rework); integration 6 of 18 → 18 of 18 |
| #19 | Battle serials: `user_mission_runs` (serials from 1,000,000,000), `open_mission_serial`, `MissionRuns.hpp`; MissionEnd settles by serial; Continue maps serial → mission | settlement 8 of 22 → 22 of 22 |
| #25 | `fe_sp/fe_used_sp/fe_max_sp` columns (`29092026_UnitEnhancementPoints`, defaults 10/0/100 per Global wiki Unit Skills rev 656738) mapped to `bFQbZh3x/3RgneFpP/GIO9DTif`; UnitMix SP gains (port of the client table, eligibility rarity ≥ 8, SBB 10, max level) | SP 3 of 36 → 36 of 36 |
| #26 | Leader skill process 97 (Player EXP Boost), additive leader + helper (`Friends::helperLeaderUnit`) | 14 of 21 → 21 of 21 |
| #30 | Story prerequisites all-of (`satisfiedAll`), `storyMissionUnlocked` for direct entry | 17 of 32 → 32 of 32 (35 of 35 with the S3 checks) |
| #27 | 54-row Brave Slots table from the Global wiki "Slots" rev 658840 (`scripts/gen_brave_slots_table.py`), reel strips, SlotAction transaction; weights authored | table 10 of 21, old build 249 of 250 → 251 of 251 |
| #39 | Super/Mega Metal Parade tiers (`scripts/gen_metal_parades.py`; wiki revs 630717/630719/630716); EXP left at the MST's 5,000 (wiki says 7,000) | 16 of 18 → 18 of 18 |
| #22A/#24, #22B/#22C | Data/wire audits (§1) | 273 of 273, 8 of 8 (no fix; audits) |

Session 2 blockers still open: #17 (the "Sanctum Lv.6" missions 100320–100325
lack the Mecha God bosses the wiki names — rev 659707, bosses
10354/20344/30334/40334/50394/60344 — with no sourced stats or capture rates);
#27 blank multi-roll tiles are client-side (`RandallSlotResultListScene::downloadFiles`
@0x1A72AB4 requests nothing); #20 needs a client run; raid/Grand Quest/campaign
item bars untested; #26 other multipliers unmodelled; #30 special-mode compound
permits still any-of.

## 6. Revisions, inventory, migrations, data, protocol, build

* Parent HEAD `7851dba8ebcc97b4d4793aae358b3d18b47a6ad2` = `mine/main`, unchanged;
  submodule `packet-generator` `2efdaa5a297ae0dd6ed5240e2f66ac762b08aa40`,
  unchanged, with 11 modified KDL files (none edited in Session 3).
* Modified tracked parent paths now **37**: the 33 in the 2026-09-30 inventory
  plus `deploy/archive/unit.json`, `gimuserver/gme/handlers/GmeControllerHandlers.cpp`,
  `gimuserver/gme/handlers/Handlers.hpp`, `scripts/test_research_lab_wire.py`.
* Untracked relevant files now **37** (backups and `deploy/mst_download_staging/`
  excluded): the 29 in the 2026-09-30 inventory, the three QC documents Codex
  wrote after taking it, this handoff, and four Session 3 tests —
  `scripts/test_mission_run_ownership_wire.py`,
  `test_warehouse_consumer_model_wire.py`, `test_reward_lock_snapshots_wire.py`,
  `test_unit_roster_wire.py`.  Check with
  `git status --short | grep '^??' | grep -v '\.bak\|mst_download_staging'`.
* **Git-ignored inputs the tests read** (present on this machine, not in git):
  `tools/sgtext/all.tsv`, `tools/sgtext/units.tsv`, `tools/wiki_units/units.json`,
  `tools/wiki_bosses/boss_catalog.json`, and the changed generator
  `tools/gen_unit_archive.py` (§4).
* Session 3 edits to existing files: `Mission.cpp`, `MissionBreak.cpp`,
  `ItemFavorite.cpp`, `Warehouse.cpp` (untracked), `MigrationManager.cpp`,
  `Handlers.hpp`, `GmeControllerHandlers.cpp`; tests `test_bugfix_qc_wire.py`,
  `test_research_lab_wire.py`, `test_warehouse_integration_wire.py`,
  `test_warehouse_placeholder_wire.py`, `test_story_gates_wire.py`.
* **Migrations** (all create-if-absent, applied on first start of a copied
  save, verified by the startup test and every suite): Session 2's
  `29092026_AddWarehouseSeenMark`, `29092026_UnitEnhancementPoints`,
  `29092026_MissionRunSerials`; Session 3's `30092026_WarehouseLiveRowIndex`
  (partial index `warehouse_live_rows(user_id,instance_id) WHERE item_num>0 OR equip_unit_id!=0`).
  Codex's `29092026_AddUserInfoOpenMission` and `29092026_PersistentWarehouseRows`
  are unchanged.  A migrated save needs this executable; running an older server
  against it is untested.
* **Protocol**: no new keys.  Behaviour: MissionContinue's no-op reply for
  stale requests; ItemFavorite echo multiplicity; one error now carries cmd 6.
* **Data**: `deploy/archive/unit.json` +114 rows (Session 3); Session 2's
  `brave_slots.json` (archive + system), `mission.json` parades.  The
  executable alone deploys none of these: the archive/system/MST files must
  ship with it.  No MST download version was generated.
* **Build**: `powershell -NoProfile -ExecutionPolicy Bypass -File out/bugfix_2026-09-29/build_isolated.ps1 -Jobs 4`
  (incremental; reuses `out/build/debug-win64/vcpkg_installed` read-only; not a
  clean build).  Generated packet headers come from the submodule KDL at build
  time.  Final executable and hash: §7.1.

## 7. Tests

### 7.1 Final full run

Command: `python out/claude_session3/run_s3.py run1` (sequential; each suite's
fixtures under `out/claude_session3/run1/`; exit codes in `results.json`).

| Suite | Exit | Result | Log |
|---|---|---|---|
| warehouse_placeholder | 0 | 39 passed, 0 failed | `out/claude_session3/run1/warehouse_placeholder.log` |
| warehouse_integration | 0 | 18 passed, 0 failed | `out/claude_session3/run1/warehouse_integration.log` |
| mission_settlement | 0 | 22 passed, 0 failed | `out/claude_session3/run1/mission_settlement.log` |
| enhancement_sp | 0 | 36 passed, 0 failed | `out/claude_session3/run1/enhancement_sp.log` |
| leader_exp_boost | 0 | 21 passed, 0 failed | `out/claude_session3/run1/leader_exp_boost.log` |
| brave_slots | 0 | 250 passed, 0 failed | `out/claude_session3/run1/brave_slots.log` |
| metal_parades | 0 | 18 passed, 0 failed | `out/claude_session3/run1/metal_parades.log` |
| story_gates | 0 | 35 passed, 0 failed | `out/claude_session3/run1/story_gates.log` |
| summon_reveal | 0 | 273 passed, 0 failed | `out/claude_session3/run1/summon_reveal.log` |
| battle_simulator | 0 | 72 passed, 0 failed | `out/claude_session3/run1/battle_simulator.log` |
| bugfix_qc | 0 | 41 passed, 0 failed | `out/claude_session3/run1/bugfix_qc.log` |
| evolution_qc | 0 | 23 passed, 0 failed | `out/claude_session3/run1/evolution_qc.log` |
| lizeria_qc | 0 | 23 passed, 0 failed | `out/claude_session3/run1/lizeria_qc.log` |
| sphere_qc | 0 | 35 passed, 0 failed | `out/claude_session3/run1/sphere_qc.log` |
| mission_run_edges | 0 | 2 passed, 0 failed | `out/claude_session3/run1/mission_run_edges.log` |
| mission_run_ownership | 0 | 57 passed, 0 failed | `out/claude_session3/run1/mission_run_ownership.log` |
| warehouse_consumer_model | 0 | 22 passed, 0 failed | `out/claude_session3/run1/warehouse_consumer_model.log` |
| reward_lock_snapshots | 0 | 24 passed, 0 failed | `out/claude_session3/run1/reward_lock_snapshots.log` |
| unit_roster | 0 | 18 passed, 0 failed | `out/claude_session3/run1/unit_roster.log` |
| unit_text_data | 0 | 8 passed, 0 failed | `out/claude_session3/run1/unit_text_data.log` |
| regressions | 0 | all six suites passed | `out/claude_session3/run1/regressions.log` |
| research_lab_ai | 0 | 0 failure(s) | `out/claude_session3/run1/research_lab_ai.log` |
| debug_startup | 0 | PASS (startup, fresh migration, HTTP, console) | `out/claude_session3/run1/debug_startup.log` |
| audit_handlers | 0 | 152 registrations; 0 structural errors (plus the tool's informational empty-branch/no-await listing) | `out/claude_session3/run1/audit_handlers.log` |
| validate_missions | 1 | 3,117 findings — item lists identical to out/qc_2026-09-30/missions.json (0 added, 0 removed) | `out/claude_session3/run1/validate_missions.log` |
| diff_check | 0 | no whitespace errors (CRLF notices only) | `out/claude_session3/run1/diff_check.log` |
| generator_diff_check | 0 | no whitespace errors | `out/claude_session3/run1/generator_diff_check.log` |

Totals: **1,037 passing assertions** across the 20 counted suites; the 14
suites of the QC report total 906 (896 there, +4 from the placeholder rework,
+3 story-gate checks, and brave slots' random prizes: its conditional checks
give 247–251 per run).  `reward_lock_snapshots` likewise varies (22–24) with
whether a slot prize carries the warehouse.  `validate_missions` exit 1 is the
unchanged baseline, not a pass.  `audit_handlers` exit 0.

Final executable: `out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe`,
31,196,160 bytes, 2026-09-30 19:06, sha256
`a9eb31bcf433d2566ebfe6b56e0ed47674cc3266b5c4b613cc56fbc69364e341`.  Every suite
in the table ran against this build.

### 7.2 Failure-before / pass-after (Session 3)

| Change | Test | Before (pre-fix build, passed of total) | After |
|---|---|---|---|
| Legacy MissionEnd / stale Continue | `test_mission_run_edges_wire.py` | 0 of 2 | 2 of 2 |
| same | `test_mission_run_ownership_wire.py` | 42 of 57 | 57 of 57 |
| Lock multiplicity | `test_warehouse_consumer_model_wire.py` | 21 of 22 | 22 of 22 |
| Stale tile → Home | `test_story_gates_wire.py` (3 new checks) | 32 of 35 | 35 of 35 |
| Tombstone reads | `out/claude_session3/perf_probe.py` | 1,832 ms | 334 ms |
| #15 archive | `test_unit_roster_wire.py` | — (data rule; the old archive lacks 114 units) | 18 of 18 |
| #24 script shape | summon suite §D vs stock APK scripts | stock gWaitGold/gWaitRed fail all 6 D checks each (`out/claude_session2_2026-09-29/24_stock_script_negative_control.log`) | 273 of 273 |

### 7.3 Not run

* No Windows-client playthrough, screenshots or video of any case.
* No clean-from-scratch build; no Release build.
* No raid, Grand Quest, campaign or Frontier Gate continue/settlement tests.
* No test against the live save or the live port; no old-server-on-new-save test.
* `validate_missions.py --wire` (live-permit mode) not run.

## 8. Client checks

Performed: **none**.  The updated list is at the end of
[CLIENT_DIFF_CHECKLIST_2026-09-30.md](CLIENT_DIFF_CHECKLIST_2026-09-30.md)
("Session 3 additions"); nothing there is marked passed.

## 9. Performance (diagnostic, not a client FPS measure)

Hardware: AMD Ryzen 5 3600 (6 cores / 12 threads, 3.6 GHz), 15.9 GB RAM,
Crucial P1 NVMe SSD, Windows 11.  Debug build, localhost, one test server per
fixture, Python client on the same machine; "server" = HTTP round trip
including server work, decode timed separately.  Copied save; 5 warm samples
after one warm-up.  `out/claude_session3/perf_probe.py`, results
`perf_results.json` (after) and `perf_results_before_live_index.json`.

| Scenario | rows | UserInfo median (before → after the live-row change) | MissionEnd median (same) | first UserInfo |
|---|---:|---|---|---|
| 100 live copies | 107 | 57 → 63 ms | 34 → 27 ms | ~110 ms |
| 1,000 live | 1,007 | 147 → 145 ms | 111 → 94 ms | ~240 ms |
| 3,200 live (client ceiling) | 3,207 | 318 → 308 ms | 295 → 292 ms | ~550 ms |
| 3,200 live + 20,000 tombstones | 23,207 | **1,832 → 334 ms** | **1,769 → 308 ms** | ~560 ms |
| 100 live + 100,000 mission runs | 107 | 63 → 64 ms | 37 → 34 ms | ~115 ms |

Twenty consecutive MissionEnds at 3,200 copies: 291 → 272 ms, no growth.
Decode on the test side: 4–9 ms.  Reply size ~513 KB at 3,200 copies (~250 KB
is the rest of UserInfo).  Limits: Debug only, single user per save, no client
parse/render time, and the 3,200-copy cost is still linear in live rows
(reconcile + list build every warehouse reply).

## 10. Blockers, risks and next priorities

1. **#25 SP spending** — implement `FeSkillGet` (`nSQxNOeL`/`nZ2bVoWu`, body
   `bx56032l{pn16CNah, edy7fq3L, ri6D9yBi}` from
   `UnitDetailVirtuallyInfoScene::feSkillGetConnect` @0x1BDF7AC →
   `FeSkillGetRequest::setParam`, third argument the scene member at +0x3C0).
   First decode `fe_skill_info` (`UserUnitInfoBase::getFeSkillInfoList`
   @0x12B80A4, `isFeSkill` @0x12B7F78) and how an owned unit is updated after
   the reply; the reset goes through ShopUse (`DefineMst::getResetFeSkillDiaCnt`).
   Until then SP can be gained but not spent, and trying closes the client.
2. **Client acceptance** of the checklist, especially the Session 3 additions
   (stale tile → Home; lock of the first vs second crafted copy; Deemo;
   continue/resume with the new rules).
3. **#12/#18 content**: author Vortex Trials and boss behaviour from the
   inventories, mission by mission, from the client scripts plus wiki notes.
4. **Goddess Tilith 50254 Home crop** — approve `303,50,180,492` and ship a
   new F_UNIT_MST download version (a deployment step).
5. **Art gaps**: 158 archived units lack `unit_anime`/`unit_cgg` on disk (135
   both, 23 cgg only; mostly metal/fodder units, one new: 850977).  Not invented.
6. **#17** stays blocked on sourced boss stats and capture rates.
7. Placeholder sell replay (§3.2) needs client retry evidence.

Deployment blockers: all client checks above; the #25 crash path; review of
the migrated-save/older-server compatibility; shipping archive/system/MST data
with the executable.

## 11. Evidence sources

* Windows client `C:\Users\Evan\BF\BraveFrontierAppxClient\BraveFrontier.Windows.exe`
  (x86) via `tools/bin/pe_disasm.py`; Android `libgame.so` (arm64) via
  `tools/bin/so_*.py`; addresses as cited above.
* Client cache (read-only): `%LOCALAPPDATA%\Packages\gumi.BraveFrontier_99p3jr0gh0z6w\LocalState`,
  `Ver288_F2Dz3QHU.dat` (F_MISSION_SCRIPT_MST, key `U91CxXGi`).
* Global wiki (bravefrontierglobal.fandom.com) unit pages cached in
  `tools/wiki_units/cache/` (Deemo and the Girl 4★/5★/6★, Dark 5★/6★), Slots rev
  658840, Metal/Super/Mega Metal Parade revs 630717/630719/630716, Mecha Gods
  rev 659707, Unit Skills rev 656738 (Session 2).
* Client MST rows cited by id: achievement_trade 80000059, guild_point_exchange
  (121 rows), defines `5pjoGBC4` = 3200, gacha_effect 900/901, unit 50253/50254/10233.
