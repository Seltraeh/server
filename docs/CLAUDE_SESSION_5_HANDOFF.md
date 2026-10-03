# Claude Session 5 — handoff

Assignment: [CLAUDE_SESSION_5.md](CLAUDE_SESSION_5.md). Two agents worked it on
October 2. The first fixed four items and left no write-up; the second (this
document) took the player's follow-up report — Farm/Mountain still not usable,
spheres falling off units — fixed both, deployed, and records the whole session.
Nothing is committed or pushed.

## 1. Player report, October 2 ~11:15, and what it was

> "I no longer see the unlock scene for farm after seeing it once, but now farm
> isn't unlocked when it says it was inside of town. The same thing with the
> mountain tile in town. … I would equip a sphere and after some play there would
> be no sphere(s) equipped to the unit."

### 1.1 Farm and Mountain opened mid-session but stayed inert — FIXED (server-tested)

Evidence, live save and logs: missions 11 and 12 first-cleared at 10:32:01 and
10:35:18; the town entered at 11:05:03 (its BadgeInfo, then the 11:05:13 craft);
the only UserInfo of the session was the 10:29:14 login. The MissionEnd replies
DID add 11 and 12 to UT1SVg59. The Farm/Mountain rows still read
`period_start 0, tap_cnt 0, drop_info ''`.

Binary-confirmed (arm64 libgame.so): `MyTownTopScene::setLocationInfo` @0x18F1574
draws a tile only when `UserClearMissionInfoList::isExist(need_mission_id)`, and
sparkles it only when its `s8TCo2MS` row has `tap_cnt >= 1`; `collectItem`
@0x18F24A8 returns silently on a tile with no taps. There is no lock overlay. A
locked tile goes out with no taps, and **nothing but UserInfo ever re-sent
`s8TCo2MS`**, so the newly opened Farm passed its gate and sat there empty and
unresponsive until a relaunch. The earlier test ("after the next login the Farm
has a harvest") passed because it relogged.

Fix: MissionEnd now carries the complete tile list (`town_location_detail`,
`s8TCo2MS`, optional — omitted on a result that settles nothing), rolled by
`Town::locationState` against the post-clear cleared set, beside the recipe list
it already re-sent for the same reason. A tile opened by the clear gets its first
3-hour period in that reply; any tile whose period expired re-rolls, so the
refresh now reaches a long session at its next clear. Race-free: the client
batches all taps into one TownUpdate flushed on leaving town (captured
`"2:9,4:10"` alongside the Home BadgeInfo), long before a battle ends; the list
is a row-0 full replace (`UserTownLocationDetailResponse::readParam` @0x1404090).
BadgeInfo (the town's own entry request) was deliberately NOT used: it goes out
in the same second as that tap flush.

The live save needs no repair: its never-rolled Farm/Mountain roll at the next
login or the next clear of anything (tested as "live-save shape").

### 1.2 Spheres fell off units — FIXED (server-tested; regression in uncommitted work)

Evidence: 10:39:39 `ItemSphereEqp a2utCvs8 = "19:-1:1000:31000:0"` → `{}`; the
save still had unit 1000's slot empty and sphere 31000 unworn in row 19, while
the 10:55:40 equip on two-slot unit 1034 (`"18:20:1034:37500:36300"`) persisted.

Cause 1, the reported loss. `ItemSphereEqpRequest::createBody` @0x13A7054 copies
each field from the unit's own strings — `getEquipItemFrameID` (+0x1d0),
`getEquipItemFrameID2` (+0x1f0), `getUserUnitID`, `getEquipItemID` (+0x1e0),
`getEquipItemID2` (+0x200), resolved through the vtable relocations. So every
unit without a Sphere Frog sends frame2 `-1`, and an unequip sends empty items
(`"0:0:1000::"`). The September 29–30 warehouse-identity rewrite parsed every
field as unsigned and refused such batches **silently**; the committed handler
had been lenient. The client had already drawn the equip; the next HomeInfo unit
rebuild (`4ceMWH6k`) put the unit back as the server had it. QC passed because
every test sent `0` where the client sends `-1`.

Cause 2, the next failure in line. A "frame" is not a sphere type: it is the
warehouse row of the worn copy. `ItemSphereSelectScene::eqpSphere` @0x1738980
writes the chosen row into it, `UserUnitInfoList::updateSphereEquipList`
@0x12BA7F8 keys the client's "who wears this row" map on it, and the sphere
picker, the "equipped / total" header and `decWarehouseItem` all read that map.
The server stored and sent `ItemMst.sphere_type` (unit 1034 had 1 and 6 — rows
holding a material stack and another item). Any later batch carrying an
unchanged slot named the copy by that value and was refused as "copy
unavailable": every one-slot edit of a two-slot unit, and every move.

Cause 3. The client never changes a count on equip/unequip and its picker skips
a 0-count sphere row (`setSphereList` @0x172F264). The server listed worn copies
at 0, so they vanished from the picker, and a local unequip left a 0 row that
looked like a lost sphere (and `incWarehouseItem` could top it up with the next
crafted copy).

Fix: the parser accepts the client's real tokens (signed, empty = 0, `-1` frame
= no copy) and refuses only genuinely malformed batches — whole, and now LOGGED.
A worn slot's frame is its copy's `instance_id`; the warehouse reconcile repairs
older saves (sphere_type frames, or a worn sphere with no row) on the next sync,
never touching a `-1`; UserInfo syncs the warehouse before reading units, so the
login reply already agrees. Worn copies go out as count-1 rows (still excluded
from `user_items`). Unit sphere icons come from item ids
(`GameUtils::setEquipSphereIcon`), so nothing visual depends on the frame except
frame2's `-1` test.

## 2. The rest of Session 5 (first agent, verified here by its tests only)

| Item | State | Evidence |
|---|---|---|
| Farm scene replay + mission completion going Home | Fixed by sending clear mission/dungeon/area ids only on a first clear (neutral `0`/`""`/`0` on repeats), mapped from `MissionResultFriendRequestScene::changeNextScene` @0x18C301C | test_mission_progression_wire 50/50. **Player-confirmed: the Farm scene no longer repeats.** Stage-select return and final-stage presentation are not yet client-confirmed. |
| Training Grounds Begin crash | Mission 6000000 was never permitted; `SandbagCheckScene` stored a null mission and Begin dereferenced it. PermitPlace now permits it; six dummies 10000000–10000005 | test_battle_simulator_wire 87/87. Client: pending. |
| Merit Exchange 400 Ignis Shards | Server read `9Hau45Jj` (1 on every row) as the limit; the shop's limit is `S8rdp9zk` (`sliderSet` @0x1A62FBC) | test_merit_exchange_wire 66/66. Client: pending. |
| Evolution supply bundle | `scripts/grant_test_bundle.py` + `scripts/test_bundles/evolution_2026-10-02.json`; test_grant_bundle_wire on a copy | **Not delivered to the live save.** The manifest is now stale: path A needs a Goblin 10050 and the save has none (it had 4 at 09:51); Mimics 7 → 2. Paths B (Deemo 50563, unit 1074) and C (Selena 20011, unit 1000) still hold. Revise A before any live grant. |
| Sphere crafting availability | Legitimately locked: the Sphere House (facility 1) needs mission 21, which this save has not cleared | Unchanged; play mission 21. |

## 3. Files changed

This agent (parent `C:\Users\Evan\BF\BF-WorkingDirRust`):
- `gimuserver/gme/handlers/ItemSphereEqp.cpp` — parser, frame = row, logged refusals
- `gimuserver/gme/common/Warehouse.cpp` — frame repair in reconcile; worn copies sent as 1
- `gimuserver/gme/handlers/UserInfo.cpp` — warehouse snapshot before the unit read
- `gimuserver/gme/handlers/Mission.cpp` — MissionEnd town tiles
- `gimuserver/gme/common/Common.hpp`, `Friends.hpp`, `handlers/ItemMix.cpp` — comments corrected (frame is not sphere_type)
- `scripts/test_town_unlock_wire.py`, `scripts/test_sphere_equip_format_wire.py` — new
- `scripts/run_bugfix_regressions.py` — both registered
- `scripts/test_sphere_qc_wire.py`, `scripts/test_warehouse_placeholder_wire.py` — the "worn copy listed at zero" assertions replaced by the binary-confirmed shape (count 1, named by the unit's frame)
- `scripts/test_mission_progression_wire.py` — first-11 Farm harvest checked in the MissionEnd reply itself
- `scripts/bf_testkit.py` — `IsolatedServer.stop` without `taskkill /T` (§4)
- `scripts/gen_battle_simulator.py`, `deploy/archive/ai.json`, `deploy/archive/mission.json`,
  `scripts/test_battle_simulator_wire.py` — the never-acting dummy AI (§9)
- `docs/CLAUDE_SESSION_5_HANDOFF.md`, `docs/CLIENT_REMAINING_2026-10-02.md`,
  `docs/BF_OFFLINE_SERVER_HANDBOOK.md` (test-server stopping; the override's "Built:" line)

This agent (submodule `packet-generator`):
- `assets/net/handlers.kdl` — `MissionEndResp.town_location_detail` (`s8TCo2MS`, optional)
- `assets/net/user.kdl` — `equipitem_frame_id`/`2`, `UserWarehouseInfo.instance_id`/`item_num` docs
- `assets/net/items.kdl` — `ItemSphereEqp` request format docs

First agent, today: `gimuserver/gme/common/PermitPlace.cpp/.hpp`,
`gimuserver/gme/handlers/AchievementAction.cpp`, `gimuserver/gme/handlers/Mission.cpp`
(clear ids), `scripts/Build-Dev.ps1`, `scripts/bf_testkit.py`,
`scripts/grant_test_bundle.py`, the `scripts/test_*_wire.py` suites dated 09:41–09:54,
`scripts/test_bundles/`; submodule `assets/mst/achievement.kdl`,
`assets/net/achievement.kdl`, `assets/net/mission.kdl`. Older uncommitted work from
Sessions 3–4 is unchanged and still pending acceptance.

## 4. Automated results

Full run, `python scripts/run_bugfix_regressions.py out/qa/current/bin/gimuserverw.exe`,
11:47:35–11:53:03, QA executable 31,420,416 bytes, 11:43:26, sha256 `0dab0622b411…`
(`out/qa/current/SUMMARY.md`): **39 suites, 1457 checks passed, 3 failed — all three
the documented known-open reproducers** (delayed Continue retry 1, SP reset 2). No
unexpected failure; validate_missions baseline unchanged at 3117 findings.

| Suite | Fixed build | Pre-fix build (09:42, isolated) |
|---|---|---|
| test_town_unlock_wire.py (new) | 23 / 23 | 8 passed, 15 failed (`out/qa/current/test_town_unlock_wire_before.log`) |
| test_sphere_equip_format_wire.py (new) | 37 / 37 | 19 passed, 18 failed (`…/test_sphere_equip_format_wire_before.log`) |
| test_mission_progression_wire.py | 50 / 50 (+1 in-session Farm check) | — |
| test_sphere_qc_wire.py, test_warehouse_placeholder_wire.py | 35 / 35, 39 / 39 (worn-row assertions updated) | — |

Both new suites were re-run after the test-kit fix below, with the live server up:
23/23 and 37/37, and the live server survived every isolated start/stop.
`test_reward_lock_snapshots_wire.py` legitimately varies between 22 and 24 checks
(one pair per slot pull whose random prize carries the warehouse list).

Test kit: `IsolatedServer.stop` no longer uses `taskkill /T`; it kills the test
server's own PID and only children created after it started (`_children`).
Verified read-only against the live server's real parent link: an older process
naming the PID as its parent is excluded.

## 5. Build and deployment

- One tree: `out/build/debug-win64` (Ninja Multi-Config, VS 18 MSVC 14.51.36231,
  VS-bundled vcpkg) through `rebuild.bat -Jobs 6`. The KDL change forced the full
  209-step rebuild (11:39–11:43) into the temporary override
  `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=out/qa/current/bin` while the live server
  held the in-tree EXE; the override was removed with `-U` at 11:54 and the
  deployment build was a relink only. Log: `out/qa/current/build.log`. The cache no
  longer holds the override.
- Deployed: `out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe`,
  **2026-10-02 11:54:51**, 31,420,416 bytes. Started 11:55 with
  `tools/bin/bf_ctl.ps1 -Target server -Action start`: PID 8944 owns
  127.0.0.1:9960, console child 20360, stdout in `deploy/log/server_stdout.log`;
  all 79 migrations found, none pending (no schema change); save
  `PRAGMA integrity_check` ok; HTTP 200.
- **Outage, cause undetermined.** At 11:53 the previous live server (PID 23516,
  launched 10:28 from a shell that had since exited, stdout not captured) and the
  client were both gone. Last request 11:14:48; no crash dump, no error event.
  Either the player closed them, or the test kit's `taskkill /T` met a recycled
  parent PID (23516's parent 18340 was dead). The kit is fixed either way.
- Save backup before deploy: `out/backups/2026-10-02_session5/gme_before_town_sphere_fix.sqlite`
  (backup API, 11:54, integrity ok, sha256 `4bbc71fc544d…`). Rollback EXE:
  `gimuserverw_0942_before_town_sphere_fix.exe`. The truncated-on-start Oct 1
  stdout log is kept as `server_stdout_2026-10-01.log`.
- No live supply grants. No hand edits to the save: the next login rolls the
  Farm/Mountain and repairs Vargas's (unit 1034) sphere frames 1/6 → rows 18/20.

## 6. Client-confirmed vs pending

- Confirmed by the player ~12:45 on the 11:54 build: Farm and Mountain harvestable;
  no Farm scene after mission 10; Muramasa on Selena through Home, mission 10 and
  a relaunch; unequip returns it; Vargas slot-2 edit keeps Ragna Blade; direct move
  Vargas → Selena; repeat of mission 11 returns to the stage list; Training Grounds
  enterable without a crash. Earlier: Vargas SP persistence, three Deemo forms in
  battle, two potions used = two consumed.
- Pending: dummies never acting (§9); the Training Grounds Menu settings
  (Enemy/Battle Settings, Save/Load Conditions); 400 Ignis Shards; final-stage
  area presentation.

## 7. Not implemented / open risks

- ShopUse type-9 SP reset — unfinished (known-open suite, 2 checks).
- Delayed Continue retry A/B/A — overcharges and rewinds (known-open suite).
- Crafted-copy lock/sale-retry ambiguity under the client's shared placeholder id
  remains as documented in Session 3. The worn-copy count change makes the
  client's own `incWarehouseItem` append a new placeholder row after a craft
  instead of topping up a worn row, which is what the placeholder model assumed;
  still client-unverified.
- Equipped copies now count toward the client's storage total
  (`getActiveCount` counts rows above 0), as the client's design implies. A
  player near the storage cap may see it reached slightly sooner than before.
- Tiles also refresh at every settled MissionEnd (3-hour rule), not only at login.
- Evolution bundle manifest stale (§2). Merit/Training/routing need client passes.
- Town: BadgeInfo still carries no tile state, so a session that never finishes a
  mission still sees tiles refresh only at login.

## 8. Artifacts

| Path | Purpose | State |
|---|---|---|
| `out/qa/current/SUMMARY.md`, `SUMMARY.json`, `runner.log`, `<suite>.log`, `<suite>/` | Latest full run; per-suite fixtures (SQLite backup copies, configs, server logs), recreated each run | Current |
| `out/qa/current/bin/` | Reusable scratch output of the override build: QA EXE + PDB + DLLs | 263 MB after cleanup |
| `out/qa/current/*_before*` | Pre-fix reproductions of both reports | Keep until the client confirms |
| `out/qa/current/build.log` | Override build, then deployment relink | Current |
| `out/backups/2026-10-02_session5/` | Oct 1 EXE (first agent), 09:42 EXE, pre-deploy save, Oct 1 stdout | Recovery points — keep |

Cleanup performed: `out/qa/current/bin/gimuserverw.ilk` (329 MB, MSVC incremental
link state for the scratch EXE; regenerable, not in use). Reported, not removed:
`out/qa/current/bin/gimuserverw_before.exe` is byte-identical (MD5 `f5e17c47…`) to
the backup folder's `gimuserverw_before.exe`; the dated folders elsewhere under
`out/` (28 GB in all) are earlier sessions' evidence and were not touched.

## 9. Training Grounds follow-up, ~12:45 — dummies fixed, Load Conditions explained

Player report: the dummies attack with a broken animation ("lightning strike
effect without moving, 2–3 seconds of no sprite movement, then teleport from my
units into a step back"), and "Load Conditions" no longer opens the settings.

**Dummies — FIXED (server-tested, client pending).** Evidence: the 12:43:01
MissionStart sent every dummy AI 1 "Attack random", move speed and both move
types 0, and only an idle animation (`8hoyIF9Q` type 1). The client content ships
`monster/cgs/unit_idle_cgs_10000.csv` and NO move or attack sequence for them, and
the Global wiki says Training Grounds enemies "will not attack back". The first
agent relied on `act_min = act_max = 0`, but `MonsterUnit::initTurnChild`'s count
does not gate the first action of a turn (it only grants extras — `initMove`,
`loopEndWait`), so the dummies attacked with no art. They now carry their own AI
60000001, one row `turn_end`: `BattleUnit::setAiTargetList` @0x10FD940 matches it
as an inlined 64-bit constant (@0x10FE4CC), targets the monster itself and sets
action type 4 — the turn ends with no move and no attack. Data only:
`deploy/archive/ai.json` (+1 record), `deploy/archive/mission.json` (six `ai_id`s),
both written by `scripts/gen_battle_simulator.py` (`--check` covers both files).
Tests: test_battle_simulator_wire 87/87 with the min/max check replaced by "every
row of the dummy's AI is turn_end"; test_research_lab_ai 0 failures (now also
"AI 60000001 never hangs across 40 turns x 6 HP bands"); validate_missions 3117
unchanged. Deployed by restart at 13:07 (PID 21308; log: "12 AI archive records");
save backed up first as `out/backups/2026-10-02_session5/gme_before_dummy_ai.sqlite`.

**Load Conditions — client-side by design; nothing server-side found missing.**
Its own help text (`SANDBAG_OPTION_SAVE_BUFF`): "You can save your Squad's current
condition. Tap the 'Load Conditions' button to load them as many times as you
like." `SandbagBattleManager::touchBeganChild` (touch 1033) calls `loadBuffList()`,
which parses what `saveBuffList()` stored in the client-side `SandbagConfigInfo`
and does nothing when nothing was saved. The settings live in the battle's Menu
(touch 1000 → scene 1704 `SandbagMenuScene`): 1028 Enemy Settings
(`SandbagSetEnemyScene`, 1712), 1030 Battle Settings (`SandbagBattleOptionScene`,
1717), 1032 Save Conditions (confirm → `saveBuffList`), 1031 Reset LOG, 1029 Items,
1010 Options, 1004 unit info, 1005 quit. The scene ids were decoded from
`GameScene::getGameScene`'s jump table. Server-side prerequisites checked: the
`featureCheck` reply already sends `sandbag_enable: 1`, and all nine
`layout_sandbag_*.csv` plus `training_ground/sandbag_enemy01-06.png` are served.
If the Menu does not show these, that is a new, unexplained client observation.

## 10. Training Grounds, ~13:40 — confirmed, plus the target reticle

Player-confirmed: dummies no longer attack; every Menu setting works; HP/element
changes reload the battle and apply. Reported: the target reticle sits above the
dummy. Mechanism (arm64): `PlayerParty::lockOn` @0x10CA254 centres the reticle
(anchor 0.5, 0.5) on `getTargetCursorDispPos` = the centre of the unit's touch rect
(`getDefaultTargetTouchRect`: 70 % of the idle animation's part bounding box,
minimum 48, from `EdgeAnime::getSize(1)`) plus MonsterMst `cursor_disp_pos`;
`GameSprite::setPosition` → `CommonUtils::convertPosition` flips `layerHeight − y`,
so battle coordinates are y-down and a positive offset moves the reticle down.
Measured on the player's screenshot: reticle centre y ≈ 81, dummy body ≈ 164–217
(centre ≈ 190), x aligned. The offset "0,-40" (an invented default every archived
special monster carries; production `unit_mst` is "0,0" for 2,186 of 2,291 rows)
became "0,70" via `gen_battle_simulator.py`; simulator suite 93/93 (+ a reticle
check per dummy). Deployed by restart at 14:00 (PID 7236); save backed up as
`out/backups/2026-10-02_session5/gme_before_reticle_offset.sqlite`. Not explained:
why the touch-rect centre sits ~70 px above this sprite's body; the calibration
is empirical. The same "0,-40"/"0,-60" offsets on the 22 special bosses are
unverified in-client.
