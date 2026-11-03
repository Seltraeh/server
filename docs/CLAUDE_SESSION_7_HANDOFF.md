# Claude Session 7 — handoff: authoritative content provider (Phases A–B)

> **Independent repair QC, October 3 (evening):** the three original blockers
> are resolved. Rebuild, original regression assertions, expanded broker tests,
> live broker/proxy integration and real-content policy checks pass. Ready for
> isolated client acceptance; rendering/performance remain unverified in-game.
> See the repair-verification section of [QC_SESSION_7_REVIEW.md](QC_SESSION_7_REVIEW.md).

> **Subsequent QC, October 3:** corrections required before deployment. New
> regression cases expose lost broker invalidation, stale in-flight cache
> publication, and unenforced scene-generation isolation. See
> [QC_SESSION_7_REVIEW.md](QC_SESSION_7_REVIEW.md) for reproductions and results.
> The original session results below are retained as historical evidence.
>
> **Repair, October 3 (evening):** all three findings are fixed and every
> regression the review added passes unchanged — see **§8**. The candidate is
> now `e24234c1…` (§5); the reviewed candidate `3d6d45e6…` must not be
> installed. The integrated client proof is still pending.

Assignment: [CLAUDE_SESSION_7.md](CLAUDE_SESSION_7.md); design authority
[OFFLINE_CONTENT_LOADING_PLAN.md](OFFLINE_CONTENT_LOADING_PLAN.md). October 3,
2026 (times below are local). Nothing is committed, pushed or deployed: the live server
was down at the start and stays down, the deployed server EXE is unchanged
(2026-10-02 21:01, sha256 `0a3d6718…`), the installed client and its
LocalState were only read, and the live save was only copied (backup API).

Evidence labels: **x86-confirmed** (read from the Windows binary),
**unit-tested** / **wire-tested** / **live-tested** (proxy ↔ real broker over
loopback, no client), **client-pending** (needs the maintainer's play).

**Outcome in one paragraph.** The Windows x86 loader contract is recovered and
recorded; the offline-proxy now contains a default-off provider that serves
owned assets from the server's `game_content` into RAM (no client asset
mirror), behind module-identity + masked-signature guards; the server has a
read-only content broker with exact-path serving, aliases, collision
reporting, all-or-nothing bundles, digests and watcher-driven generations.
Everything is tested up to a live C++ client ↔ C++ broker run; **the
integrated client proof (Town and Zeal drawn from provider bytes in the real
game) was not run in this session** — at the maintainer's choice it is left to
their acceptance run (§5). No speed or smoothness claim is made.

## 1. Files changed and why

### offline-proxy (`C:\Users\Evan\BF\offline-proxy`, branch `fps-cap` at `d3aa843`, uncommitted)

| File | Why |
|---|---|
| `OfflineMod.win32/content/cp_core.{h,cpp}` | Pure core: flat-name policy and LocalState path split, client-state name guard, MSVC text-mode emulation, FileCrypt, CNG SHA-256, `bfcp 2` protocol parser (per-file identities), `ContentIndex` (present / absent / declined), bounded single-flight `ByteCache` keyed by content identity with scene tokens (§8), masked signature + PE identity matching |
| `OfflineMod.win32/content/cp_provider.{h,cpp}` | `WinInetBroker` (InternetOpenUrlA, no WinINet cache writes, bounded timeouts) and `Provider` (background status/index polling, deltas pending until a safe point, fetches bound to and verified against the pinned identity, `Superseded` reads, busy retries, fast-fail window, dormancy when the server switches the broker off) |
| `OfflineMod.win32/content/cp_hooks.{h,cpp}` | Build verification and installation in a **separate** Detours transaction; IAT tripwires |
| `OfflineMod.win32/content/cp_hooks_logic.cpp`, `cp_hooks_internal.h` | Hook bodies shared with the tests: getFileData / isFileExist / existsLocalFile / map lookup / getTexture thunk / scene changes / drawScene sampler / fopen+remove tripwires |
| `OfflineMod.win32/content/cp_log.{h,cpp}` | `%TEMP%\offlinemod_content.log` (package `AC\Temp`) + OutputDebugString, rotated at 8 MB |
| `OfflineMod.win32/content/cp_signatures.h` | Generated site table (15 sites, relocation bytes masked) |
| `OfflineMod.win32/main.cpp` | Calls `ContentProvider_Attach/Detach` after/before the existing transaction (no change to the connection or FPS hooks) |
| `CMakeLists.txt`, `OfflineMod.win32/CMakeLists.txt`, `serverconfig.h.cmake` | `OFFLINEMOD_CONTENT_PROVIDER` (default 0 → no provider code compiled), `OFFLINEMOD_BUILD_TESTS`, `bcrypt` link |
| `tests/` (`test_main.cpp`, `fake_broker.h`, `test_util.h`, `CMakeLists.txt`, `fixtures/make_signatures.py`, `fixtures/bf_2.19.6.0_signatures.json`) | 287 offline checks (226 at hand-off, then the review's QC assertions and the repair's, §8) + a 26-check opt-in live-broker case; signature generator and JSON fixture |
| `docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` | The recovered x86 contract (summary in §2) |
| `README.MD`, `.gitignore` | Provider section; `/build/` ignored (the README's own build folder) |

### BF-WorkingDirRust (`main` at `e8cdefc`, uncommitted; packet-generator `a46b7df` untouched)

| File | Why |
|---|---|
| `gimuserver/content/ContentBroker.{hpp,cpp}` (new) | Read-only broker: reparse-safe index (FindFirstFileExW), policy, aliases, collisions, bundles, a SHA-256 identity for every owned name (computed before it is offered), ReadDirectoryChangesW watcher with debounce and overflow rescan, exact-path confined reads that record changes through the writer path and can be bound to a version (§8) |
| `gimuserver/controller/OfflineModController.{hpp,cpp}` | `GET /offline_mod/content/v1/status`, `/index`, `/file` (`d=` binds the read to a version: 409 when it is gone) |
| `gimuserver/drogon/GimuServer.cpp` | Start the broker after the archives; stop it on shutdown |
| `deploy/system/content_provider.json` (new, tracked dir) | Phase B policy: **`"enabled": false`**, exact scope (21 files), 6 aliases, 2 bundles, extensions, state-name exclusions |
| `scripts/test_content_broker_wire.py` (new) | Broker wire suite (67 checks at hand-off, 70 after the repair) |
| `scripts/test_content_generation_qc.py` (new, the review's) | Reads racing the watcher; extended by the repair to 17 checks (§8) |
| `scripts/check_content_policy.py` (new) | Dry-runs the shipped policy against the real tree (7 checks at hand-off, 8 after the repair) |
| `scripts/test_content_provider_live.py` (new) | Drives the proxy's live case against a real broker on a fixture root |
| `scripts/content_provider_client_test.py` (new) | Client-acceptance harness for a side-by-side test package (§5) |
| `scripts/bf_testkit.py` | `Fixture.create(document_root=, system_root=)` |
| `scripts/run_bugfix_regressions.py` | The broker suite, the generation QC and the policy check join the runner (44 entries) |
| `docs/CLAUDE_SESSION_7_HANDOFF.md` | This file |
| `docs/QC_SESSION_7_REVIEW.md` (new, the review's) | The quality review the repair answers; left as written |

`docs/CLAUDE_SESSION_7.md` (the assignment) was already untracked and is left as found.

## 2. Windows binaries and the recovered contract

Supported: `gumi.BraveFrontier` **2.19.6.0 x86** — installed package and
`BraveFrontierAppxClient` are byte-identical for these files.

| File | SHA-256 | TimeDateStamp / SizeOfImage / Entry |
|---|---|---|
| BraveFrontier.Windows.exe (24,280,064 B) | `fb4f3c25f02944ca9e0f80fe5555998f0d6fae5e9901e4cf09795be90c8e3c49` | 0x6253F827 / 0x01756000 / 0x00ED273D |
| libcocos2d_v2.2.5_Windows_8.1.dll (3,775,488 B) | `5488d108e45a776a9cc8fd4c3ab3f4504a67dbc6d5fa2749d4af9ba6f1066a42` | 0x624A9105 / 0x0039E000 / 0x0023ABDE |

Full contract with every RVA, ABI and caller:
`offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md`. Key facts (all
x86-confirmed):

- **Existence:** `CommonUtils::existsLocalFile` (0x237EB0) answers from an
  in-memory map built by enumerating LocalState (0xE415D0, `_findfirst64i32`),
  released by the login scene (0x8E4330), with a `stat` + `fopen` size fallback;
  no size/hash/version check. The map lookup (0xE417E0) is also called by a
  download-list builder under the game's mutex.
- **Reads:** cocos `CCFileUtils::getFileData` (0x135EB0, vtable slot 12) does
  `fullPathForFilename` → `fopen(mode)` → `new[]` (msvcr120_app heap) →
  `fread`; callers free with `msvcr120_app!operator delete[]`. CSV loaders pass
  `"r"`, so CRT text mode (CRLF→LF, ^Z ends) is part of the contract.
  `getTexture` (0x2353D0; ECX path, DL force-decode, stack bool) inlines the
  FileCrypt decode, decodes by flag or by missing PNG/JPEG magic, and retries
  undecoded on a bad image. The two-image loader (0x65FB70) decodes both reads
  unconditionally. SuperAnim (0x62B270) reads `.sam` through getFileData,
  `remove()`s a corrupt file, and loads pages through `CCTextureCache::addImage`
  (0x632AC0; a missing page → "<path> is missing." dialog). Town
  (0xA51010) uses `existsLocalFile("MapVillage.sam")` → LocalState, else the
  bundled copy.
- **Writes / direct CRT:** download completion 0x222ED0 (optional inline
  FileCrypt encode) → writer 0x223090 (`fopen "wb"` under the writable path);
  `saveFile` 0x238330; ZIP extractor 0x391EE0; audio reader 0xEC0030
  (`fopen_s`); XMLFileProcessor reader 0x1757F0; script parser 0x107229.
- **Stored form is per download call site** (LocalState `common/button`: 188 raw
  / 819 encoded), so the provider serves raw bytes, clears getTexture's
  force-decode for owned paths (custom-ABI thunk), and serves encoded bytes only
  to the two-image reader's call sites (return addresses 0x65FD4A / 0x65FDF7).
- **Threads / ownership:** getFileData runs on the render thread and on game
  worker threads; buffers come from `msvcr120_app!operator new`; the LocalState
  prefix (`CCFileUtilsWinRT::getWritablePath`, WinRT ApplicationData) is resolved
  only on the render thread. No cancellation exists in the read path; the
  provider bounds it (connect 1.5 s, receive 5 s, busy retries 30/60/90 ms, 3 s
  fast-fail after a transport failure).
- **Safe points:** cocos `replaceScene/pushScene/popScene/runWithScene` (used
  by `BaseScene::sceneFadeOut`, x86 0x63D6E0). SuperAnim nodes unload their
  definition on destruction, so re-entering Town re-reads the SAM; changed
  textures are released with `CCTextureCache::removeTextureForKey` using the
  exact keys the game used.
- **Transport:** the AppContainer cannot read `game_content` (no
  ALL APPLICATION PACKAGES / package SID in the ACL chain), and granting it is a
  maintainer security decision — so the loopback server is the read-only byte
  broker, into RAM only. WinINet requests use `NO_CACHE_WRITE | RELOAD`.
- **Embedded `OFFLINE_DEPLOY`:** `game_frontend/bootstrap_windows.cpp`'s
  `OfflineMod_startup` is a `// TODO: populate` stub, so no embedded server
  exists to test; the embedded proxy variant compiles (§3).

## 3. What is integrated, tested, inferred, unsupported

| Claim | Status |
|---|---|
| Broker index / policy / aliases / collisions / bundles / confinement (traversal, encoded traversal, junction, file symlink, absolute paths) / exact-path only / state names refused | wire-tested (67/67) |
| Same-size edit with restored mtime → new generation; touch / identical rewrite → none; delete → tombstone; new differing copy → collision; watcher overflow (4,000-file burst) → rescan that keeps a same-size edit; no torn reads under concurrent writers; writer-held file → bounded 503; dormant / rejected / absent policies; fixture `root` | wire-tested |
| Shipped policy on the real 75,931-file tree: exactly 21 owned, 0 collisions, 0 unavailable, aliases pick the player-confirmed Town copies | wire-tested (`check_content_policy.py`) |
| Proxy core: names, text mode (= CRT, incl. real `unit_cgg_60956.csv`), FileCrypt, protocol rejects, index apply, LRU bound, single-flight, provider pinning / tombstones / declined collisions / busy / not-ready / corrupt digest / fast-fail / epoch change / dormancy, concurrency, hook logic with fake game functions, prefix only on the render thread, register-level getTexture thunk ABI, RTTI name reader, frame sampler, signatures vs the real binaries and unknown-build declines | unit-tested (226/226 + 1 live-only skip); mutation-checked (thunk DL clear, state-name guard) |
| Proxy's own WinINet client + provider ↔ the real broker: byte-exact reads, text-mode CSV through the hook, same-size SAM edit invisible until the safe point then served, Zeal bundle withdrawn / restored | live-tested (246/246 + driver 5/5) |
| The game actually consumes provider bytes; Town alignment from a stale cache; Zeal sprite/portrait/animation; same-size edit after a scene re-entry; missing-texture behaviour; no LocalState writes; frame pacing | **client-pending** |
| Hook ABIs match the real code at runtime | inferred from x86 disassembly + register-level thunk test; **client-pending** (a desktop process cannot host cocos2d: its load hangs outside the app container, tried) |
| Unsupported in Phase B | audio, scripts, plists, fonts, video, zip bundles, MST (`Ver*.dat`), everything outside the 21 names; download queue not intercepted (Phase C) |

## 4. Build, tests, measurements, logs

Builds:

- **Proxy:** no configured tree existed (only stale in-source `.vcxproj`
  files; no `CMakeCache.txt` anywhere). One tree was configured at
  `offline-proxy/build` with the `deploy-vs18` user-preset settings
  (VS 18 2026, Win32, `x86-windows-static`, RelWithDebInfo,
  `OFFLINEMOD_DEPLOY=OFF`) plus `-DOFFLINEMOD_CONTENT_PROVIDER=1`, using
  `VCPKG_ROOT=C:\Users\Evan\BF\vcpkg` for the configure (the environment's
  `C:\BF\vcpkg` does not exist). MSVC 19.51.36260, curl 8.12.1 from the pinned
  baseline. Variants built in the same tree and then restored:
  embedded (`OFFLINEMOD_DEPLOY=ON`) compiles with the provider; default
  (`CONTENT_PROVIDER=0`) contains no provider code. Final tree state = candidate.
  ```
  cmake -S . -B build -G "Visual Studio 18 2026" -A Win32 -DCMAKE_TOOLCHAIN_FILE=C:/Users/Evan/BF/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x86-windows-static -DCMAKE_CONFIGURATION_TYPES=RelWithDebInfo -DOFFLINEMOD_DEPLOY=OFF -DOFFLINEMOD_CONTENT_PROVIDER=1
  cmake --build build --config RelWithDebInfo
  ```
- **Server:** QA builds only, into `out/qa/current/bin` through a temporary
  `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG`, removed afterwards (cache verified
  clean); the in-tree EXE was never relinked. Final QA EXE sha256
  `d6a8d59e…` (2026-10-03 18:18).

Results (logs under `out/qa/current/session7/`):

| Run | Result |
|---|---|
| `run_bugfix_regressions.py` on the final QA EXE (18:18–18:26) | **43 suites, 1,692 checks passed, 0 failed**, no expected failures; `validate_missions.py` matches its 3,117-finding baseline (H15 842, M2 20, M3 921, M4 1334) — a baseline, not a clean result. Previous suites unchanged (1,618 at publication + the new broker 67 and policy 7). `out/qa/current/SUMMARY.md`, `session7/runner_final.log` |
| `test_content_broker_wire.py` | 67 passed, 0 failed |
| `check_content_policy.py` | 7 passed, 0 failed (the rollout switch is printed, not judged) |
| `content_provider_tests.exe` (offline) | 226 passed, 0 failed, 1 skipped (live case) |
| `test_content_provider_live.py` on the final binaries | driver 5/5; proxy 246 passed, 0 failed |

Measurements (warm OS cache; nothing on the client side was measured):

- Broker index build, full tree (75,931 files): **15.9 s → 3.7–3.9 s** after
  switching the walk to FindFirstFileExW. Cold-cache time not measured.
- Live provider fetches over loopback: 9 fetches / 1,273 KB in 14.9–30.1 ms
  total across runs (≈2–3 ms per file), including SHA-256 on both sides.
- **Not measured:** title-to-home, scene-ready, frame-time p50/p95, peak RAM,
  persistent writes in the client. The candidate logs, per scene change and
  every 30 s, `perf (...)` lines (frame ms p50/p95/p99/max, working set / peak /
  private, provider reads / RAM hits / KB fetched / fetch ms / failures,
  tripwire counters), so a broker-off and broker-on run of the same scenes are
  directly comparable.

Housekeeping notes:

- The publication-run summaries the Oct 3 status cites were preserved as
  `session7/previous_SUMMARY_publication_2026-10-03.{md,json}` before the
  runner replaced `SUMMARY.*`.
- A test server from my first, mis-fixtured broker run (missing
  `features.json` → fatal at plugin init, before the broker starts) became an
  unkillable zombie (PID 18208, `HasExited=True`), the known Drogon pattern. It
  still locks `out/qa/current/bin/gimuserverw.exe.locked-by-zombie-18208`
  (renamed so the QA EXE could be relinked) and
  `out/qa/current/content_broker/server/gme.sqlite`; the broker suite skips a
  locked folder (hence `server2`, `server_*2`). Both clear at the next reboot
  and can then be deleted.

## 5. Candidate, activation, rollback, client checklist

**Candidate (after the QC repair, §8):** `C:\Users\Evan\BF\offline-proxy\build\bin\libcurl.dll`
(copy with PDB: `out/qa/current/session7/candidate/`), x86, 2,839,552 bytes,
sha256 `e24234c11765275f775a093e7c47e12a44e6a6ba05591962474c7b466eef3f7b`
(PDB `31738788…`). It speaks `bfcp 2` and needs a server built from the
repaired broker: against an older (`bfcp 1`) broker it stays inactive — the
game keeps its own cache — and logs `provider waiting: status unreadable: not
a bfcp 2 status`. The
reviewed candidate `3d6d45e649af26d6b01f1570ad35286992b3dabd7eda8b93cb6984b41a86a5ee`
is superseded — do not install it.
Currently installed (rollback) DLL: `9ae05b17ed0f910174ad0673d2bb3ef62301c89e818f120dbcc57ca60489a2c2`
= `DecompFrontier-Installer\build\libcurl.dll`.

**Two switches.** Compile-time: the DLL contains the provider only when built
with `OFFLINEMOD_CONTENT_PROVIDER=1`. Runtime: the provider activates only when
the server's `deploy/system/content_provider.json` has `"enabled": true`
(shipped `false`). With the switch off — or a server without the broker — the
DLL logs why and the game uses its own cache exactly as today.

**Activation (maintainer)**

1. Server: back up the save (backup API), `bf_ctl.ps1 -Target server -Action stop`,
   build in-tree (`rebuild.bat` or the usual `cmake --build` with
   LIB/INCLUDE), set `"enabled": true` in `deploy/system/content_provider.json`,
   start, then check `GET http://127.0.0.1:9960/offline_mod/content/v1/status`
   → first line `bfcp 2`, `enabled 1`, `ready 1`, `owned 21`, `collisions 0`,
   `unavailable 0`.
2. Client (in-place package **update**, keeps LocalState — never uninstall, that
   deletes the account key files):
   1. Close the game. Copy `%LOCALAPPDATA%\Packages\gumi.BraveFrontier_99p3jr0gh0z6w\LocalState\*.dat`, `*.xml`, `*.ini` to a backup folder.
   2. In `DecompFrontier-Installer\build\BraveFrontierAppxClient` (the
      unpacked layout of the installed package: identity `gumi.BraveFrontier`,
      `CN=BraveFrontier`, 2.19.6.0, signature files already removed): keep the
      old `libcurl.dll` aside, copy the candidate in, and change
      `Version="2.19.6.0"` to `Version="2.19.6.1"` in `AppxManifest.xml`.
   3. `makeappx.exe pack /d <that folder> /p BraveFrontier_2.19.6.1.appx /o`
      and `signtool.exe sign /a /fd SHA256 /f build\myKey.pfx /p <your password> BraveFrontier_2.19.6.1.appx`
      (`C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\`).
   4. `Add-AppxPackage .\BraveFrontier_2.19.6.1.appx`; confirm
      `Get-AppxPackage gumi.BraveFrontier` reports 2.19.6.1 and the installed
      `libcurl.dll` hash is the candidate's. The loopback exemption is per
      package family and is unaffected.
3. Launch; `%LOCALAPPDATA%\Packages\gumi.BraveFrontier_99p3jr0gh0z6w\AC\Temp\offlinemod_content.log`
   must show `content provider hooks installed` and
   `provider active: … 21 owned name(s)`. A `DECLINED` line names the reason
   (build mismatch, missing export, Detours failure) and nothing was hooked.

**Rollback**

- R1, no client change: `"enabled": false` and restart the server. A running
  game hands its names back at the next scene change (`provider dormant`);
  after a relaunch it never activates.
- R2, remove the DLL: repeat 2.2–2.4 with the old DLL (`9ae05b17…`) and
  `Version="2.19.6.2"`.

**Client checklist** (record times; the log lines named are the evidence)

1. **Town with a stale cache:** close the game, back up and then replace
   `LocalState\MapVillage.sam` with the shipped unframed copy
   (`BraveFrontierAppxClient\Assets\Resources\MapVillage.sam`). Open Town:
   labels on their buildings (framed file served) and
   `served MapVillage.sam <- sam/MapVillage/MapVillage.sam`. Restore the backup afterwards.
2. **Zeal (60956) as a helper, first battle:** sprite, portrait and
   animations present; `served unit_anime_60956.png`, the CGG/CGS lines, and
   `getTexture unit_… -> raw provider bytes: FileCrypt force-decode cleared`.
3. **Same-size edit after a safe reload:** in Town, run
   `python tools/frame_town_for_2x3.py --remove` (same size, unframed), leave
   and re-enter Town: labels ~44 pt high, and the log shows
   `pending generation`, `safe point … changed: mapvillage.sam`,
   `released decoded copy`. `--apply` and re-enter: aligned again.
4. **Missing file:** move `deploy/game_content/content/unit/img/unit_ills_thum_60956.png`
   out, change scene once, start a battle with Zeal: expect the game's own
   missing-asset path (download error / no Zeal), **not** a white blob; log:
   `read …: removed from game_content`. Restore and change scene: Zeal returns.
   The exact in-game outcome is unknown (Phase C question) — record it.
5. **Restart / account:** relaunch; same account and progress; the backed-up
   `*.dat` / `*.xml` files unchanged.
6. **Normal play and FPS:** a quest, a summon, Home; compare the `perf (…)`
   lines with a run after R1 (broker off) for the same scenes. No
   `TRIPWIRE` lines expected; any is a finding.
7. **Scene coherence (added by the repair):** with nothing being edited on
   the server, the log must show no `failing it until the next scene change`
   lines and every `perf` line `(superseded 0)`; any is a finding. (An edit
   made while a scene is open is adopted at the next scene change — check 3 —
   so it is only refused if that same scene reads the file for the first time
   afterwards.)

A side-by-side test package (own identity and LocalState) can be prepared with
`scripts/content_provider_client_test.py` (`layout` → register → elevated
`CheckNetIsolation LoopbackExempt -a -n=gumi.BraveFrontier.ContentTest_99p3jr0gh0z6w`
→ `state --stale-town` → `fixture` → `server` → play → `snapshot` / `collect`);
it was **not** run.

## 6. Next bounded tasks

**Phase C (evidence from §2):**

1. Intercept the download queue for owned names (x86 FileLoader near the
   constructor 0x634C00; arm64 `addLoadFile` 0xFBE8DC /
   `addLoadFilePriority` 0x1CD8AA8) so an absent owned name fails immediately
   through the game's own error path — no network, no LocalState write — and
   confirm the bulk pass (login users of the map, 0x8E2960 / 0xA02E40) skips
   owned names.
2. Prefetch a scene's owned closure on the background thread at scene entry
   (scene preflight via the resourceMap reader 0x16F850), so render-thread
   reads hit RAM; move remaining fetches off the render thread.
3. Generate bundles server-side (unit: cgg/cgs/anime/ills; SAM: `.sam` + the
   pages its image table names) and widen the scope family by family, each with
   `check_content_policy.py`.

**Phase D:** inventory the login/download screens (LoginScene
`requestResourceBases` / `updateEvent`, the map build/release at
0xE415D0 / 0x8E4330) as state machines before removing any wait, then measure
title-to-home and scene-ready broker-off vs broker-on with the `perf` lines.

## 7. Repository status at hand-off (before the QC review)

Nothing committed or pushed. `out/`, the proxy's `build/` (now git-ignored),
saves, logs and client binaries are outside the source changes.

**BF-WorkingDirRust** (`main` @ `e8cdefc`, = `mine/main`; packet-generator
`a46b7df`, clean):

```
 M gimuserver/controller/OfflineModController.cpp
 M gimuserver/controller/OfflineModController.hpp
 M gimuserver/drogon/GimuServer.cpp
 M scripts/bf_testkit.py
 M scripts/run_bugfix_regressions.py
?? deploy/system/content_provider.json
?? docs/CLAUDE_SESSION_7.md            (the assignment, untracked before this session)
?? docs/CLAUDE_SESSION_7_HANDOFF.md
?? gimuserver/content/
?? scripts/check_content_policy.py
?? scripts/content_provider_client_test.py
?? scripts/test_content_broker_wire.py
?? scripts/test_content_provider_live.py
```

**offline-proxy** (`fps-cap` @ `d3aa843`, = `origin/fps-cap`):

```
 M .gitignore
 M CMakeLists.txt
 M OfflineMod.win32/CMakeLists.txt
 M OfflineMod.win32/main.cpp
 M OfflineMod.win32/serverconfig.h.cmake
 M README.MD
?? OfflineMod.win32/content/
?? docs/
?? tests/
```

Both server builds left as found: the shared CMake cache has no runtime-output
override and the in-tree `gimuserverw.exe` is the deployed 2026-10-02 21:01
build. The proxy tree `offline-proxy/build` is configured for the candidate
(standalone, provider on).

## 8. QC repair — October 3, evening

[QC_SESSION_7_REVIEW.md](QC_SESSION_7_REVIEW.md) found three P1
content-consistency defects. All three are fixed. Every assertion the review
added passes unchanged — none was weakened, skipped or marked expected-fail —
and, as the review asked, `TestProvider()`'s old expectation that a first read
accepts newer bytes was replaced: it now expects that read refused, no repeat
request in the same scene, and the new bytes after the safe point.

| Finding | Repair |
|---|---|
| **1 — broker reads swallowed a generation change** (`read()` stored the new digest before `applyChanged()` compared against it) | `read()` writes nothing. Bytes whose digest differs from the published identity go through `publishObserved()` under the writer lock: it re-hashes the file against the identity still on record (`applyChanged`), bumps the generation only for a real change, and answers with the generation whose identity those bytes have — or 503 `changed-during-read` when the file moved on again (the proxy retries busy answers), 404 when the change withdrew the name. Comparison and update happen once, under the writer lock, so a reader can never overwrite newer metadata or attach another generation to its bytes. |
| **2 — in-flight reads repopulated the cache after a scene change** | The RAM cache and its single-flight are keyed by content identity (path + digest). A read takes its index entry, identity and scene token in one critical section and is registered before it fetches. Every adoption advances the token; a fetch that completes under an older token still answers the readers that asked for it but is never cached (`dropped` stat). |
| **3 — scene pinning did not constrain fetched bytes** | Protocol `bfcp 2`: every F row carries the SHA-256 identity of the copy it serves (owned copies are hashed before a name is offered; a name without one is reported unavailable, never left to the stale cache). Reads are bound to the pinned identity (`/file?p=…&d=<digest>`, 409 `version-unavailable` when it is gone), and the proxy checks bytes against the pinned identity, not only the response's own digest, so even a broker that ignores `d=` cannot mix versions. Such a read returns `ReadStatus::Superseded` — NULL to the game, like a missing file, logged `failing it until the next scene change` — and is not requested again in that scene; the next safe point serves the new version. This is the review's accepted bounded fallback; keeping old versions (or scene-closure bundles) is Phase C. |

Found and fixed while testing the repair (not in the review):

- With identities required, a hash that failed during an atomic replace
  cleared the identity and withdrew the name (404s in the wire suite's
  concurrent-writer test). Hashing now shares delete access and retries the
  replace window like `read()`, and a failed re-hash keeps the recorded
  identity (reads still verify their own bytes).
- The identity pass ran the authored globs a second time over the full tree
  (real-tree index build 5.0 s warm). They are now matched once per file:
  **4.2–4.3 s warm** on the 75,931-file tree. The pre-repair candidate measured
  3.7–3.9 s earlier in the session; that is not an A/B on one run.
- The `perf` line buffer was doubled to 512 bytes: `sprintf_s` aborts the
  process on overflow, and the line gained a counter (`superseded`).

Mutation checks (each reverted and rebuilt afterwards): caching late fetches
again → 3 failures; skipping the pinned-identity check → 2 failures. The first
restore kept the mutated file's old timestamp, which MSBuild then did not
recompile; the restored sources were touched and everything rebuilt before any
result below was taken, and no source is newer than the candidate.

Results on the final binaries (logs `out/qa/current/session7/fix_*.log`,
`runner_fix.log`, `out/qa/current/SUMMARY.md`):

| Run | Result |
|---|---|
| `content_provider_tests.exe` (offline) | **287 passed, 0 failed**, 1 skipped (live). Includes the review's 4 isolation assertions, `TestVersionBinding` (outstanding first read with a joined waiter; a path evicted earlier; a broker restart overlapping the adoption; a broker that ignores `d=`; an edit reverted within a scene) and scene-token cache tests |
| `test_content_generation_qc.py` (the review's, extended) | **17 passed, 0 failed**: the review's 4, plus the generation sent with the racing read, no second bump at the watcher flush, the identity in the index, version-bound reads (409 / 200 / 400 `bad-digest`), an identical rewrite with a restored timestamp, and concurrent readers during same-size, timestamp-restored edits (one identity per generation, only bounded 503s, nothing stale left) |
| `test_content_broker_wire.py` | **70 passed, 0 failed** (+ protocol line, identities in the index, the delta's identity); concurrent-writer run: 216 reads served, 24 bounded 503s, no withdrawal |
| `check_content_policy.py` (real tree) | **8 passed, 0 failed** (+ every served name published with the SHA-256 of its copy); 21 owned, 0 collisions, 0 unavailable |
| `test_content_provider_live.py` | driver **5/5**; proxy **313 passed, 0 failed** (26 live, including a real-broker 409 → `Superseded` → served after the safe point); 12 fetches, 1,273 KB, 21.9 ms |
| `run_bugfix_regressions.py` (44 entries; the generation QC joined) | **1,712 passed, 0 failed**, no expected failures; `validate_missions.py` at its 3,117-finding baseline. Two unrelated suites' check counts move with their random prizes (brave slots 251→248, reward lock 22→24), both 0 failed |

Binaries: candidate DLL `e24234c1…` (§5). QA server
`out/qa/current/bin/gimuserverw.exe` sha256
`3c1a1a564c5c8f698e06940f9fe6febb27ce4ef40d21188993888ff21b91644f`
(2026-10-03 20:37), built through the temporary output override, which was
removed afterwards (cache verified clean). The in-tree `gimuserverw.exe` is
still the deployed 2026-10-02 21:01 build (`0a3d6718…`). Not rebuilt after the
repair: the default (provider-off) and embedded (`OFFLINEMOD_DEPLOY=ON`) proxy
variants. The repaired files contain no deploy-variant branches, so they are
inferred unaffected, not re-verified.

Still pending, unchanged by the repair: the integrated client proof and every
client measurement — Town/Zeal rendering and same-size safe reload,
missing-texture recovery, persistent asset writes, login/account
preservation, gameplay regressions, frame pacing (§5 checklist, now with
item 7). As the review recommends, run it first in the separate test package
(`scripts/content_provider_client_test.py`) with stale assets and preserved
account state, before any in-place update. Also unchanged: the download queue
is not intercepted and reads are synchronous on the caller's thread (Phase C,
§6). A read that discovers an unpublished change waits for one index recompute
before it is answered; that cost was not measured on the full tree.

Housekeeping: the zombie from the first broker run (PID 18208, §4) is gone and
its locks are released, so `out/qa/current/bin/gimuserverw.exe.locked-by-zombie-18208`
and the `out/qa/current/content_broker/server*2` folders are now plain
leftovers the maintainer can delete (left in place here).

### Repository status after the repair

Nothing committed or pushed; `out/`, the proxy's `build/`, saves, logs and
client binaries are outside the source changes.

**BF-WorkingDirRust** (`main` @ `e8cdefc`; packet-generator `a46b7df`, clean):

```
 M gimuserver/controller/OfflineModController.cpp
 M gimuserver/controller/OfflineModController.hpp
 M gimuserver/drogon/GimuServer.cpp
 M scripts/bf_testkit.py
 M scripts/run_bugfix_regressions.py
?? deploy/system/content_provider.json
?? docs/CLAUDE_SESSION_7.md            (the assignment, untracked before this session)
?? docs/CLAUDE_SESSION_7_HANDOFF.md
?? docs/QC_SESSION_7_REVIEW.md         (the review, left as written)
?? gimuserver/content/
?? scripts/check_content_policy.py
?? scripts/content_provider_client_test.py
?? scripts/test_content_broker_wire.py
?? scripts/test_content_generation_qc.py
?? scripts/test_content_provider_live.py
```

**offline-proxy** (`fps-cap` @ `d3aa843`):

```
 M .gitignore
 M CMakeLists.txt
 M OfflineMod.win32/CMakeLists.txt
 M OfflineMod.win32/main.cpp
 M OfflineMod.win32/serverconfig.h.cmake
 M README.MD
?? OfflineMod.win32/content/
?? docs/
?? tests/
```
