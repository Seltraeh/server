# Claude Session 8 — handoff: repeated sphere rows (#34) and the isolated provider proof

> October 4 independent QC: five sphere/warehouse suites passed (175 checks).
> One harness readiness issue needs correction before isolated client testing.
> Player confirms existing spheres visible and ordinary mission navigation fixed;
> fresh crafting on the identified candidate remains a separate acceptance case.
> See QC_SESSION_8_REVIEW.md and the next assignment CLAUDE_SESSION_9.md.

Assignment: [CLAUDE_SESSION_8.md](CLAUDE_SESSION_8.md). October 3–4, 2026 (local
times). **Nothing is committed, pushed or deployed.** The live save was only read
(SQLite backup API); the installed client, its LocalState and the in-tree server
EXE were only read; no app reset, cache removal or package change was made.

Evidence labels as in the handbook: **x86-confirmed** (read from
BraveFrontier.Windows.exe), **arm64** (libgame.so, names and semantics only),
**wire-tested** (isolated server + copied save), **client-pending**.

**Outcome.** The broken Equip Sphere list is explained by the Windows binary:
the client gives every copy it crafts one provisional row id until a warehouse
list replaces its rows, and the Equip Sphere list keys each row's drawing on that
id. The fix is server-only: the ItemMix reply now carries the authoritative
warehouse, which the client parses before any unit or sphere screen can exist.
Built and wire-tested in a QA candidate; **not deployed; #34 stays open until the
repeated-row screen passes in the client.** For the content provider the reviewed
DLL is unchanged; the server candidate changed and was retested; a harness defect
that would have broken the first maintainer step was fixed; the isolated test
package and fixture are prepared, and the remaining steps are the maintainer's.

## 0. State found at the start

- **No server or client was running** when this session started (~23:00;
  bf_ctl status: 0 processes, 9960 not listening), contrary to the brief's
  preparation note. Nothing had to be avoided or stopped, and nothing was
  started on 9960 during the session.
- In-tree EXE `out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe`,
  2026-10-03 21:55:56, sha256 `fb77bf6fcf6a09329c087a8f92e2ae89fe108bf681ca371a633c448a8de70db5`.
  **Provenance:** a relink-only build. `.ninja_log` shows the 21:55 build ran only
  the dyndep scans and the link; no object was compiled. Every object came from
  the 20:36 QA build that produced the reviewed QA server `3c1a1a56…`
  (ContentBroker.cpp.obj 20:36:51), and no source was newer than that QA EXE. So
  it is the **same object code as the reviewed Session 7 QA server** (broker
  included, policy `enabled: false`), linked to the in-tree path. It served the
  player's 21:56–21:58 session; its ItemMix replied `{}`.
- Installed client `gumi.BraveFrontier` 2.19.6.0: EXE `fb4f3c25…` (= the
  analysis copy), `libcurl.dll` `9ae05b17…` (original proxy, no provider).
  Developer Mode on; loopback exemption only for the installed package.
- Git: as Session 7 left it (§11 lists both repositories). The shared CMake cache
  had no runtime-output override.

## 1. What the player did (recovered, read-only)

| Evidence | Value |
|---|---|
| `deploy/log/http_log_4P5GELTF_20261003_215801.log` | ONE ItemMix: `JTf2jY5o[0].4HqhTf3a = "2005:19"`, reply `{}` |
| `deploy/mst/recipe_mst.json` 2005 | product 30000 × 1, materials `10400:5`, 500 Karma, unlimited |
| `deploy/mst/item_mst.json` 30000 | Famous Blade, sphere, max stack 1, short description "Boosts Atk by 5%" |
| Save copy `out/qa/current/session8/evidence/live_save_copy_2026-10-03.sqlite` (backup API, integrity ok, sha256 `04e5e2ae…`) | `user_items` 30000 = 19, 10400 = 5; recipe 2005 counted 19; Karma 700,367, Zel 2,242,846 (the screenshot header); 0 warehouse rows for 30000 yet (minted at the next sync); seen mark 26; unit 1000 wears 37500 in frame 18 |
| Requests after 21:58:01 | none — the relaunch outcome was not observed |

So the batch was 19 Famous Blades, identified from the request and the MST, not
from the artwork. All 19 are held: **no inventory loss is established.**

## 2. The consumer, end to end

| Step | Windows x86 (`fb4f3c25…`) | arm64 name |
|---|---|---|
| Craft applied at the tap: Karma, materials, product | 0xA5FC50 (sphere) / 0xA4EAC0 (item) call `incWarehouseItem` 0xD49790 | `MyTownSphereMixScene::createItem`, `MyTownItemMixScene::mixItem` → `GameUtils::incWarehouseItem` |
| New row of a max-stack-1 item named `0x7FFFFFFF - itemId` | `mov [row+0x14], 0x7FFFFFFF-x` @0xD49AFE (also 0xD703C8, 0x6E546D); helper 0x6E5580 | `incWarehouseItem` @0x118A0A8 |
| Batch queued when the craft scenes go invisible; a new request every 50 crafts, appended without dedupe | builder 0x384160, from the sphere/item mix and list scenes (0xA5D535, 0xA59814, 0xA49EC0, 0xA4FFC8) | `GameUtils::createItemMixConnectList`, `ConnectRequestList::addObject(req, false)` |
| Queue sent in order by the transition scene, then the next scene is built | StepScene ~0x829D70 (state 0 `accessPhp`, state 1 HttpConnector, state 3 `"4P5GELTF"` Daily Task notice only) | `StepScene::updateEvent` |
| Footer navigation is a StepScene; Equip Sphere is a plain push | — | `GameScene::checkTouchFooter` (34 StepScenes); `UnitDetailUserUnitScene::SphereSelectSceneMove` / `UnitBoxScene::touchEnded` → vtable +0x2F8 = `GameScene::pushScene` (no flush) |
| Every body key parsed, whatever the request | `parseTag` 0x493170 → `parseBodyTag` 0x493770 | `GameResponseParser::parseTag` / `parseBodyTag` |
| `9wjrh74P` / `VSRPkdId` then re-derive every lock | 0x4951B5–0x495220 → `UserWarehouseInfoList::setFavorite` 0x5C0930 | `parseBodyTag` +0x2C94 |
| Equip Sphere entry per row: frame id = `IntToString(row[+0x14])`, item, wearer | builder 0x8D31D0 (store @0x8D3565) | `ItemSphereSelectScene::setSphereList(bool)` |
| Sprites keyed on the frame id; a row whose key exists is skipped | `setSphereList(int,bool)` 0x8D0B00 (skip @0x8D0F95) | same, @0x172F868 |
| Visibility and labels keyed on it; labels made/removed on a flip | 0x8D68A0; `createSphereText` 0x8D6C30 / `removeSphereText` 0x8D71D0 | `updateSphereVisible` @0x1732F78 |

Also checked: crafting has **no** box-capacity gate — `WAREHOUSE_OVER_ERROR` has
one use, the Town harvest (x86 0xA538E4; arm64 `MyTownTopScene::collectItem`);
the box limit blocks mission entry (`checkOverItemBox`). The town synthesis
scenes queue nothing but the ItemMix batch; `decWarehouseItem`'s own requests
(unequip a consumed sphere, summoner edit) are queued at the tap, before it.
Session 3's note that town crafts "go out at once" was wrong: they are queued.

## 3. Why the list broke, and what it was not

Nineteen local rows, one key `2147453647`: in a rebuild every row fetches the
same cached sprite set and moves it, so it ends at one row; in an incremental
build the duplicates are skipped and never positioned. Visibility for the key is
decided by whichever duplicate the loop reaches last, and the one label set sits
at whichever duplicate last flipped it. The list's height is computed from the
entry count, so the scroll range spans all rows. Result: blank rows, at most one
frame and one floating name/description, while rows with distinct ids (the worn
37500 copy) draw normally.

- Missing art would leave the frames and labels drawn with empty icons.
- Bad geometry: the height is count-based and correct; the rows have no sprites.
- A filter (`GameUtils::filterSphere`) removes entries; it does not leave gaps.

The screenshot is consistent with the aliasing; it is not the proof — the proof
is the binary, plus the model in the new suite, which shows the 19-way key
collision on the pre-fix server (§5).

## 4. The fix (server only)

`ItemMixResp` (packet-generator `assets/net/items.kdl`) now declares the
existing warehouse trio — `9wjrh74P` (whole warehouse), `VSRPkdId` (lock set),
`bd5Rj6pN` (item dictionary) — and `gimuserver/gme/handlers/ItemMix.cpp` fills it
from `gme::loadWarehouseSnapshot` inside the craft's transaction, after the
effects, for **every accepted batch** (crafted, partly affordable, or refused:
the client applied all of it locally). Errors are unchanged.

Why this point is correct (all from §2): the client has already applied every
craft before the request leaves; the reply is parsed inside the StepScene that
sends it, before the next scene is built; footer navigation is a StepScene and
Equip Sphere is only ever pushed afterwards; nothing applies the crafts again
after the reply; a batch of more than 50 crafts or several sessions arrive as
several requests answered in order, each a full list, the last one complete;
nothing else is queued behind the batch from the synthesis scenes. No client
hook was needed. Spheres are not stacked, no copy is deleted or chosen
arbitrarily, nothing is hidden, and nothing forces Home or a restart.

Remaining limits (documented, not changed):

- **ItemMix replay.** The request has no identity; a byte-identical retry after a
  lost reply crafts again (costs paid per craft). Not identifiable server-side.
- **Lost list.** The seen mark advances when the reply is built; a client that
  never got it keeps its placeholders, and a lock set or sale naming them is
  refused unchanged until the next list (tested).
- **Karma on a server-refused craft.** Only a stale client can send an
  unaffordable batch; its inventory is corrected at once, its Karma figure only
  at the next login. Team info was deliberately not added: StepScene's ItemMix
  branch reads `UserTeamInfo` Brave Points for its Daily Task notice.
- Other local appends without a list in their reply: TownUpdate (harvest) and
  ItemEdit (battle item bar) — stackables, where the placeholder only names an
  overflow row. Not changed; see §10.
- The queue analysis is static (arm64 navigation, x86 flush/parse).

Files: parent `gimuserver/gme/handlers/ItemMix.cpp`, `gimuserver/gme/common/Common.hpp`
(comment), packet-generator `assets/net/items.kdl` (schema: three optional
fields + docs).

## 5. Tests

QA candidate: see §7. Logs in `out/qa/current/session8/`.

New `scripts/test_craft_row_identity_wire.py` (port 19971, in the shared runner)
models the recovered consumer — local append, 50-craft requests, sorted-key
parse, Equip Sphere key aliasing, the lock request builder — and checks the
captured 19-craft batch, 120 crafts over two sessions, first/middle/last copy
lock, equip (one slot, second slot), direct transfer, sale, every refusal,
mixed worn/locked/free copies, sorting/filtering, a stackable result (Cure, cap
99), insufficient material (partial and none), a full box, a lost reply with
stale placeholder sell/lock/equip, the retry limitation, cost/count
conservation, restart and row-id uniqueness.

| Suite | Candidate | Pre-fix server `3c1a1a56…` (negative control) |
|---|---|---|
| test_craft_row_identity_wire.py (new) | **53 / 0** | 37 / 16 — every failure is the reply contract or its consequence: model keys `{'2147453647': 19}` after the craft, `{'2147453647': 120}` after the flush, a placeholder Cure row, phantom copies not removed on a short craft, the full-box list, the stale placeholder cases |
| test_warehouse_consumer_model_wire.py (reworked) | **25 / 0** | 2 failures at part A (no list in the craft reply), then stops (traceback: no new ids to act on) |
| test_warehouse_placeholder_wire.py (reworked) | **41 / 0** | 31 / 10 |
| test_warehouse_integration_wire.py (craft step reworked) | **19 / 0** | 16 / 3 |
| test_sphere_qc_wire.py, test_sphere_equip_format_wire.py, test_reward_lock_snapshots_wire.py | 35 / 0, 37 / 0, 22–24 / 0 | — |

The three reworked suites asserted the old consumer (`'9wjrh74P' not in` the
craft reply; placeholder actions on just-crafted copies). Their placeholder
coverage now uses a grant whose reply carries no list — the remaining real
source of client-named rows — and each still checks every refusal it had.

**Exact replay of the captured request** (`capture_craft_evidence.py`, the
logged body verbatim, on a copy rolled back to just before the craft;
`evidence/craft_{prefix,candidate}.json`):

| | Pre-fix `3c1a1a56…` | Candidate `a2022e91…` |
|---|---|---|
| Reply | `{}` | `9wjrh74P` (39 rows) + `VSRPkdId` + `bd5Rj6pN` (22) |
| Famous Blades listed in the reply | — (the client keeps 19 rows named 2147453647) | 19, ids 27–45, all distinct |
| 30000 / 10400 / Karma / recipe count | 0→19 / 100→5 / 709,867→700,367 / 0→19 | same |
| Warehouse rows for 30000 | 0→0 (minted at the next sync) | 0→19 |
| Next login of the CURRENT save | 19 rows, 19 distinct ids | same |

The pre-fix replay reproduces the player's saved state exactly, and the last row
is a server-side prediction for the "relaunch" client check.

**Shared runner** (`run_bugfix_regressions.py` on the candidate, 00:10–00:24):
**45 entries, 1,773 checks passed, 0 failed**, no expected failures;
`validate_missions.py` at its 3,117-finding baseline (H15 842, M2 20, M3 921,
M4 1334 — a baseline, not a clean result). Session 7's last run was 44 entries /
1,712: + 53 (new suite) + 6 (placeholder 39→41, integration 18→19, consumer
model 22→25) + 2 (`test_brave_slots_wire.py` 248→250, which moves with its
random prizes) = 1,773.
`out/qa/current/SUMMARY.md` (copy: `session8/SUMMARY_session8.md`; Session 7's
summary kept as `session8/previous_SUMMARY_session7_fix.md`). Note the summary's
"26 changed paths" counts this session's and Session 7's uncommitted files.

## 6. Content provider

### 6.1 Candidates

- **Proxy DLL unchanged:** `C:\Users\Evan\BF\offline-proxy\build\bin\libcurl.dll`
  = `out/qa/current/session7/candidate/libcurl.dll`, sha256
  `e24234c11765275f775a093e7c47e12a44e6a6ba05591962474c7b466eef3f7b`; no proxy
  source is newer than it. Offline tests on it today: 287 passed, 0 failed,
  1 live-only skip (`proxy_offline_tests.log`).
- **Server candidate changed** (the sphere fix; broker code untouched): see §7.
  Retested against it: broker wire **70 / 0**, generation QC **17 / 0**, real-tree
  policy **8 / 0** (21 owned, 0 collisions, 0 unavailable; inside the runner), and
  the live proxy ↔ broker integration `test_content_provider_live.py` **driver
  5 / 5, proxy 313 passed / 0 failed / 0 skipped** (12 fetches, 1,273 KB, 21.0 ms;
  `content_provider_live.log`). Same results as the reviewed repair; these
  overlap and are not a sum of unique checks.

### 6.2 Harness defect found and fixed (`scripts/content_provider_client_test.py`)

The installed package's publisher is `CN=BraveFrontier` (PublisherId
`99p3jr0gh0z6w`); `BraveFrontierAppxClient`'s manifest says `CN=MyBraveFrontier`.
`layout` kept the source publisher, so the test package's family would not have
been `gumi.BraveFrontier.ContentTest_99p3jr0gh0z6w`: `state`, `snapshot` and
`collect` would have looked in a folder that does not exist, and the printed
loopback command would have named the wrong family. `layout` now pins the
installed publisher. Also added: a read-only `check` pre-flight; `server` now
defaults to the Session 8 candidate, prints its hash, refuses while any
`gimuserverw` or the installed client runs, and warns if another process binds
9960 while it runs (Windows allows the double bind silently).

### 6.3 Prepared (only `out/qa/current/content_client` written)

`layout --appx C:\Users\Evan\BF\DecompFrontier-Installer\build\BraveFrontierAppxClient
--dll out/qa/current/session7/candidate/libcurl.dll`: 1,361 hard links (no data
copied), identity `gumi.BraveFrontier.ContentTest` / `CN=BraveFrontier`, DLL
`e24234c1…`. `fixture`: the 21 owned files. `check` (`content_check_before.log`):
everything ok except the three maintainer steps below.

### 6.4 Maintainer procedure (switch-over; the DLL is built for 9960)

The test package and the installed client both talk to 127.0.0.1:9960, so the
normal server is stopped for the test. Commands from the server checkout.

1. Close the game. `powershell -NoProfile -ExecutionPolicy Bypass -File tools\bin\bf_ctl.ps1 -Target server -Action stop`
2. `python scripts\content_provider_client_test.py check` — only the next three items open.
3. Register (Developer Mode, current user): `powershell -NoProfile -Command "Add-AppxPackage -Register 'C:\Users\Evan\BF\BF-WorkingDirRust\out\qa\current\content_client\package\AppxManifest.xml'"`
4. **Elevated** prompt: `CheckNetIsolation.exe LoopbackExempt -a -n=gumi.BraveFrontier.ContentTest_99p3jr0gh0z6w`
5. Launch "[ContentTest] Brave Frontier" once from Start (creates its LocalState), close it.
6. `python scripts\content_provider_client_test.py state --stale-town` (copies only `*.dat/*.xml/*.ini` from the installed LocalState; plants the stale unframed Town).
7. `python scripts\content_provider_client_test.py server` — isolated server, copied save, broker on the fixture; leave it running.
8. Launch the test package. Its first run downloads its own asset cache through the legacy path (~71,000 files, ~2.5 GB, about half an hour; Phase C will remove this). Then `python scripts\content_provider_client_test.py snapshot before`.
9. Run §6.5, then `snapshot after` and `collect` (copies `offlinemod_content.log`).
10. Ctrl+C the harness server; `powershell -NoProfile -Command "Get-AppxPackage gumi.BraveFrontier.ContentTest | Remove-AppxPackage"`; elevated `CheckNetIsolation.exe LoopbackExempt -d -n=gumi.BraveFrontier.ContentTest_99p3jr0gh0z6w`; restart the normal server with `bf_ctl.ps1 -Target server -Action start` and confirm 9960.

Rollback is trivial: nothing in the installed package, its LocalState or the live
save is touched; removing the test package removes everything it created.

### 6.5 Client acceptance (record the log lines; Session 7 §5 has the full forms)

| Check | Pass evidence |
|---|---|
| Hooks active | `content provider hooks installed`, `provider active: … 21 owned name(s)` (a `DECLINED` line names its reason) |
| Stale-cache Town uses the framed bytes | labels on buildings; `served MapVillage.sam <- sam/MapVillage/MapVillage.sam` |
| Zeal 60956 first battle | portrait, sprite, animations; `served unit_anime_60956.png`, the CGG/CGS lines, `force-decode cleared` |
| Same-size edit at a safe reload | `content_provider_client_test.py edit --height 985` while in Town, leave and re-enter: labels move; `pending generation`, `safe point … changed: mapvillage.sam`; `--height 1161` restores |
| Unavailable pinned version fails recoverably, then recovers | move `out\qa\current\content_client\broker_root\unit\img\unit_ills_thum_60956.png` (the fixture, never `deploy\game_content`) out of the folder, change scene, enter Zeal's battle: the game's own missing-asset path, no white blob/crash, log `read …: removed from game_content`; move it back, change scene: Zeal returns. The exact in-game outcome is a Phase C question — record it |
| No owned-asset persistent writes | `snapshot before/after` agree for the 21 names |
| Account/settings preserved | relaunch: same account, settings, purchased skills, equipment |
| No frame-pacing regression | `perf (…)` lines with the broker on vs `server --broker off`, same scenes; no `TRIPWIRE`; every `perf` line `(superseded 0)` |

Desktop broker tests (§6.1) are not game evidence; only this table is.
Deferred until it passes: old-version retention / bundle prefetch, download-queue
interception, loading-screen removal (Phase C, then D).

## 7. Builds and hashes

| Artifact | SHA-256 | Notes |
|---|---|---|
| QA server candidate `out/qa/current/session8/bin/gimuserverw.exe` | `a2022e914a0146a749c81a5ddcc9e1401df181bf846dc11d6468cd1ab2ab9024` | 2026-10-03 23:58:16, 31,954,944 B; full rebuild (KDL), shared tree, temporary `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG` removed afterwards (cache verified clean); `build_qa.sh`, `build_server_qa.log` |
| In-tree server (normal) | `fb77bf6f…` | unchanged, 2026-10-03 21:55:56 |
| Pre-fix control `out/qa/current/bin/gimuserverw.exe` | `3c1a1a56…` | Session 7's reviewed QA server, unchanged |
| Proxy candidate | `e24234c1…` | unchanged |
| Proxy tests `offline-proxy/build/tests/content_provider_tests.exe` | `c40131c9…` | unchanged |

## 8. What is actually deployed

Nothing changed. Normal server = in-tree `fb77bf6f…` (not running at hand-off);
installed DLL `9ae05b17…`; policy `enabled: false`. The fix and the provider are
both inactive for normal play.

## 9. Deploying the sphere fix (maintainer)

1. Back up the save (backup API).
2. `powershell -NoProfile -ExecutionPolicy Bypass -File tools\bin\bf_ctl.ps1 -Target server -Action stop`
3. Confirm `grep RUNTIME_OUTPUT out/build/debug-win64/CMakeCache.txt` is empty, then build in-tree (`rebuild.bat`, or `cmake --build out/build/debug-win64 --config Debug --target gimuserverw` with LIB/INCLUDE). The objects are current, so this is a relink; the new hash will differ from `a2022e91…` (different output path) — record it.
4. `bf_ctl.ps1 -Target server -Action start`; confirm 9960. The provider stays off.
5. Client: `docs/CLIENT_SESSION_8_CHECKLIST.md` "After the sphere fix is deployed".

## 10. Next bounded tasks

1. Client: relaunch and open Equip Sphere on the current save (checklist "Useful
   checks now" 1); after deployment, the repeated-row pass. Close #34 only then.
2. Client: the isolated provider proof (§6.4–6.5).
3. If the pass shows other placeholder sources: give TownUpdate / ItemEdit
   replies the warehouse trio after tracing their consumers the same way.
4. Then Phase C (scene readiness, background prefetch, legacy writes/downloads,
   families), then Phase D.
5. Comment-only, with the next server build: `ItemFavorite.cpp`, `ItemSell.cpp`
   and `ItemSphereEqp.cpp` still name "a craft" as the usual placeholder source;
   since this fix that is a craft whose reply was lost (`Common.hpp` already says
   so). Left untouched here so the tested candidate's code did not change.

Backlog unchanged and still open: stored leader skills on archive grants,
long-session harvest refresh, compound mission gates, mission 234, level-999
client results/energy, leader EXP/Fuu, slots/parades, portraits/text/gates,
missing EU units, Trials/boss/Fire Mecha, GuildUpdate. Deemo's three forms do not
close all of #15; Farm replay alone does not close all of #23.

## 11. Repository status at hand-off

Nothing committed or pushed. Build products, saves, logs and fixtures stay in
`out/` (git-ignored).

**BF-WorkingDirRust** (`main` @ `e8cdefc`):

| Path | Session 8 change |
|---|---|
| `gimuserver/gme/handlers/ItemMix.cpp` | the fix (§4) |
| `gimuserver/gme/common/Common.hpp` | comment only (placeholder sources) |
| `scripts/test_craft_row_identity_wire.py` (new) | the recovered-consumer suite |
| `scripts/test_warehouse_consumer_model_wire.py`, `test_warehouse_placeholder_wire.py`, `test_warehouse_integration_wire.py` | reworked to the recovered consumer (§5) |
| `scripts/run_bugfix_regressions.py` | + the new suite (Session 7's three broker entries were already there) |
| `scripts/content_provider_client_test.py` (Session 7, untracked) | publisher fix, `check`, `server` safety (§6.2) |
| `docs/BF_OFFLINE_SERVER_HANDBOOK.md` | one client-contract bullet |
| `docs/CLIENT_SESSION_8_CHECKLIST.md` (untracked) | rewritten with evidence |
| `docs/CLAUDE_SESSION_8_HANDOFF.md` (new) | this file |

Unchanged Session 7 work, still uncommitted: `gimuserver/controller/OfflineModController.{hpp,cpp}`,
`gimuserver/drogon/GimuServer.cpp`, `gimuserver/content/`, `deploy/system/content_provider.json`,
`scripts/bf_testkit.py`, `scripts/check_content_policy.py`, `scripts/test_content_broker_wire.py`,
`scripts/test_content_generation_qc.py`, `scripts/test_content_provider_live.py`,
`docs/CLAUDE_SESSION_7{,_HANDOFF}.md`, `docs/QC_SESSION_7_REVIEW.md`; `docs/CLAUDE_SESSION_8.md`
is the assignment.

**packet-generator** (`a46b7df`): `assets/net/items.kdl` — `ItemMixResp` gains three
optional fields and docs; `ItemMixReq` doc. A schema change: publish it here
first, then the parent pointer (handbook "Publishing").

**offline-proxy** (`fps-cap` @ `d3aa843`): untouched this session (Session 7's
uncommitted provider work as listed in its handoff).

Whitespace: `git diff --check` (with `cr-at-eol`) clean in both repositories.

`out/qa/current/session8/` holds: `build_qa.sh` and its logs, `bin/` (the QA
candidate + DLLs), suite and negative-control logs, `runner.log`,
`proxy_offline_tests.log`, `content_provider_live.log`, `content_*` harness logs,
`capture_craft_evidence.py`, and `evidence/` (the live-save backup copy, the
replay JSONs and their fixtures). `out/qa/current/content_client/` holds the
prepared test package layout and broker fixture. The live save was not written
(`deploy/gme.sqlite` still 2026-10-03 21:58:01, sha256 `a87325ef…`).
