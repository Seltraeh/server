# Session 19 visual check: handoff (October 23, 2026)

For the next Claude session continuing the S19 Fresh client visual check and its fixes. Evan drives
the goal; an agent drives the client from PowerShell and fixes what breaks. Read this first, then
`docs/SESSION_19_CLIENT_RESULTS.md` (the full results log). The walkthrough the checks follow is
`docs/CLIENT_SESSION_19_FRESH_VISUAL_STEPS.md`.

Everything was stopped cleanly at hand-off: client closed, test server and relay stopped, ports
29960/29961/29962 free. Until the October 24 deploy below, the live server (port 9960,
`deploy/gme.sqlite`, in-tree exe `6ee7d454…`) was never touched. Session 20 (October 23–24) ended the
same way. The S19 Fresh package now runs
the fixed proxy DLL `9633061d…` (4.1). The QA candidate in `visual/bin` is now `887e68e0…` (all fixes
including F; the previous `d567b518…` is in `visual/bin_d567b518/`). The QA save was reset to tutorial
2/0 after the full route was played (its completed state is `visual/save_after_tutorial_run2_complete.sqlite`).
On October 24 (09:21–09:28) the completed save went in once more for the post-tutorial check (4.4). After
it the 2/0 save was put back and everything was stopped again. At 09:38 the live server was redeployed
with these fixes (section 1, Deployment). It is running on 9960 with the live save.

## 1. Status at a glance

| Area | State |
|---|---|
| S19 goal (fresh install gets every master table, real names/text, starters, item and area art) | **Confirmed visually.** Steps 1–9 of the walkthrough are recorded in the results doc. |
| Fix A: Ishgria world gate open from login (froze the tutorial at the world select) | **Fixed, wire-tested, confirmed in the client.** |
| Fix B: Terms & Conditions disclaimer black, misplaced, unreadable | **Fixed and confirmed in the client** (title's Terms link). |
| Fix C: server drops idle keep-alive connections after 60 s | **Fixed and proven at socket level.** Not the full cause of the title hang (see 4.1). |
| Fix D (Session 20): title login hang | **Fixed in the proxy source and confirmed in the client.** The proxy's debug console blocked the game: a click in it held every console write, including the login's on the main thread. The live client's update package is staged and waits for Evan's signature; see 4.1. |
| Fix E (Session 20): second tutorial freeze on the quest select (tuto8, fresh accounts) | **Fixed in content and confirmed in the client.** The quest select's modal request deleted the plate tap area tuto8 had just created; one `PLAYWAIT` line in `tuto8.txt` (applied with Evan's go-ahead) fixes it. The mission-select step works too; see 4.2. |
| Fix F (Session 20): the summon tutorial never started | **Fixed in the server source; the whole fresh-account route is confirmed in the client.** MissionEnd's mission-2 checkpoint wrote chapter 50, and the client ends the tutorial for any chapter above 11. It is 10 again, and tuto8 → battle → tuto15 summon → tuto16 → done now plays through. See 4.4. |
| Full regression runner on `887e68e0` | **Passed (October 24):** 62 entries, 0 failed, 2,663 checks; the mission validator equals its baseline (3,117). Results in `visual/regressions_s20_full/`; `out/qa/current/SUMMARY.*` and the per-suite logs were put back. |
| After the tutorial (completed save) | **Confirmed in the client:** no tutorial replay at login; the three completion gifts claimed (gems +5, zel +10,000, Cure +3, checked in the save); the Town footer opens the town. tuto10–14 never run on the Windows client (4.4). |
| Deployment | **Server deployed October 24, 09:38** with Evan's go-ahead. The live in-tree exe is `f72310df…` (linked 09:36:43). The server stopped around 09:46 (log ends at its debug-CLI exit; no crash dump). The machine then rebooted at 19:49, and the server was started again at 20:01 on the same exe. It was built from the same sources and object files as the tested `887e68e0`, so the whole working tree is live, including earlier QA-only fixes. Smoke on it: tutorial checkpoint 21/0, story gates 39/0, DB exception atomicity 70/0. Live save backed up first. **Client DLL: update package staged, not installed.** It waits for Evan's signature (4.1). Nothing was committed. |

## 2. What changed in the source (uncommitted, in the working tree)

| File | Change | SHA-256 now |
|---|---|---|
| `gimuserver/gme/common/PermitPlace.cpp` | World gates (`0Cq2AlXW`) built per player. A GateMst gate is permitted when its `need_mission_id` is 0 or cleared; ids with no GateMst row keep the old blanket permit. Ishgria (gate 2, needs 1577 "Facing the Sealed God") now waits for that clear; the Vortex (99, need 0) stays open. Log line counts gates. | `8d9c1b35…` |
| `scripts/test_story_gates_wire.py` | Four gate checks (no clears: 1 and 99 open, 2 closed, 100 open; 1577 cleared: 2 open). | `19bd495d…` |
| `gimuserver/controller/WebTerms.hpp` | The `/bf/web/terms.htm` page (the client shows it in a transparent native web view) now has light text, a shadow and a translucent panel, with a top offset in `vw` units that puts the text inside the dialog box under "Privacy Policy". The wording is unchanged. | `c38c8fd6…` |
| `standalone_frontend/main.cpp` | `setIdleConnectionTimeout(0)` after `loadConfigFile`: the server never drops an idle connection the client still pools. Set in code on purpose: no config toggle. | `131f81a0…` |
| `gimuserver/gme/handlers/Mission.cpp` | Session 20, Fix F: `kSecondTutorialCheckpoint` 50 → 10, with the client evidence in its comment (checkChapter `ch > 11` ends the tutorial; 50–54 are the triggerable band after 12). | in QA `887e68e0…` |
| `scripts/test_tutorial_checkpoint_wire.py` (new), `scripts/run_bugfix_regressions.py` | Fix F's suite: 21 checks, registered as a server suite (default port 19974). `d567b518` fails its 3 chapter checks. | |
| `deploy/game_content/content/tuto/tuto8.txt` (git-ignored content; live content root) | Session 20, Fix E: `*,type=PLAYWAIT,param=0.5,#` inserted as line 23. Applied October 24 with Evan's go-ahead. Original saved as `out/qa/current/session19/visual/tuto8_before_playwait.txt` (`cfa85733…`). | `271511d9…` |
| `offline-proxy/OfflineMod.win32/console_log.cpp`, `.h` (new), `main.cpp`, `curl_exp.cpp`, `CMakeLists.txt` | Session 20, Fix D. Console lines are queued and written by a proxy thread with `WriteConsoleW`, so a console that is selecting or paused can no longer block a game thread. The prints and their text are unchanged. | DLLs: 29960 `9633061d…` (in the S19 package), 9960 twin `bfeaba0b…` |

**QA candidate** (server fixes A, B, C and F): `out/qa/current/session19/visual/bin/gimuserverw.exe`, SHA-256
`887e68e03cff982c05f605bf9c542cfecbe6fb325651195599a932f8ce51d1c5` (Session 20, tag s20tutorial). The
previous candidate `d567b51859579c96ced3c147d95ca64482ba95cc73d39b5b50fbfd212fca8aa5` (A–C) is kept in
`visual/bin_d567b518/`. Build script:
`bash out/qa/current/session19/visual/build_server_visualfix.sh <tag>`. It links through a temporary
runtime-output override and verifies the live exe and the S19 candidate (`02b95f90…`) are unchanged.
**Stop every test server first:** a running exe makes the link fail with LNK1168. The server forks,
so stop by exe path, not by PID: `Get-CimInstance Win32_Process -Filter "Name = 'gimuserverw.exe'"`
and filter `ExecutablePath -like '*session19\visual\bin*'`.

**Tests run:**
- story gates 39/0 on the fixed build; the unfixed S19 build fails exactly the Ishgria check (38/1).
- Targeted regression run on the gate-fix build `1a59c1bd` (`out/qa/current/session19/visual/regressions_targeted/`): battle simulator 93, mission progression 50, Lizeria 23, Enhancement 36, metal parades 18, town unlocks 23, feature visits 4, Research Lab 39, all 0 failed. The mission validator equals its baseline (3,117).
- On `d567b518`: story gates 39/0, content broker 82/0, DB commit contention 21/0 (`suites_d567b518.log`).
- Keep-alive probe (`keepalive_probe.py <port> 90`): `d567b518` answered the second request after 90 s idle; `02b95f90` aborted the socket (WinError 10053).
- **Full regression runner on `887e68e0`** (October 24, 08:54–09:19; it supersedes `d567b518`): 62 entries,
  0 failed, 2,663 checks. Against the October 22 run on `02b95f90` (61 entries, 2,640), the differences are:
  - the new tutorial checkpoint suite (21);
  - story gates 35 → 39;
  - reward-lock snapshots 24 → 22 (random slots prizes; see the results doc).
- How to run it: `python scripts/run_bugfix_regressions.py out/qa/current/session19/visual/bin/gimuserverw.exe`.
  It replaces `out/qa/current/SUMMARY.*`, `validate_missions.json` and the per-suite logs, so back them up
  first and restore them after. This run did; its own copies are in `visual/regressions_s20_full/`.

## 3. How the visual check is run

**Prerequisite:** Evan runs the Claude desktop app as administrator for this work. Frida's attach to
the AppContainer client needs elevation, and every input and capture in this session ran elevated.

### 3.0 Start, check, stop

1. Confirm nothing is running: `Get-Process BraveFrontier.Windows -ErrorAction SilentlyContinue`
   returns nothing, and `Get-NetTCPConnection -State Listen -LocalPort 29960,29961 -ErrorAction SilentlyContinue`
   lists no listener.
2. With the client closed, set the save's tutorial state for the test (3.1).
3. Start the server (option A or B in 3.1). Wait about 30 s, then launch the client.
4. Capture (`bf_drive.ps1 -Action shot -Out <png>`), act, capture again. Record each step in
   `docs/SESSION_19_CLIENT_RESULTS.md`.
5. Stop:
   1. Close the client: `bf_drive.ps1 -Action close`.
   2. Stop the relay:
      `Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*http_relay.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId }`.
   3. Stop the server by exe path (section 2).
   4. Repeat step 1 to confirm everything is down.

### 3.1 The pieces

- **Client:** AppX package `gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w`. Window title
  `[ContentTest S19 Fresh] Brave Frontier`. Launch with
  `Start-Process explorer.exe 'shell:AppsFolder\gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w!App'`.
  The DLL talks to `127.0.0.1:29960`. Never reset or reinstall the package.
- **Save:** `out/qa/current/session19/content_client_s19/saves/s19_first_launch_blank/gme.sqlite`, user
  `QnJjm9ao` ("S19Fresh"). **Current tutorial state: `tutorial_status` 2, `tutorial_end_flag` 0**, left
  that way to reproduce Open 2. To play normally set 12/1:
  `python -I out/qa/current/session19/visual/tools/tutorial_state_qa.py <save>/gme.sqlite QnJjm9ao 12 1`.
  Change it only with the client closed. Backup from before the first change:
  `out/qa/current/session19/visual/save_before_tutorial_end.sqlite`.
- **Server, option A (simple):** the harness on 29960 with the QA candidate.
  `python out/qa/current/session19/content_provider_client_test_s19.py --identity s19 server --exe out/qa/current/session19/visual/bin/gimuserverw.exe --policy out/qa/current/session18/policies/family_policy_c8.json --manifest out/qa/current/session19/coverage/content_startup_s19_final.json --save s19_first_launch_blank --timeout 420`.
  ⚠ A harness restart **truncates** `<save>/server.log` and rewrites `<save>/config.json`, so copy the
  log aside first. Stop it with Ctrl+C or by exe path.
- **Server, option B (traceable, used for most of this session):** server on 29961 plus a logging relay on
  29960.
  `out/qa/current/session19/visual/bin/gimuserverw.exe "C:\Users\Evan\BF\BF-WorkingDirRust\out\qa\current\session19\content_client_s19\saves\s19_first_launch_blank\config_relay_29961.json"`.
  The config path must be **absolute, passed as argv[1]**; without it the Debug exe binds the live 9960
  and the live save. Then
  `python -I out/qa/current/session19/visual/http_relay.py 29960 29961 out/qa/current/session19/visual/http_relay.log`.
  The relay logs request lines, status lines, and each connection's open and close. Start both
  hidden (`Start-Process -WindowStyle Hidden`).
  ⚠ Give the server about 30 s before launching the client. A broker that isn't ready answers 503
  `not-ready`, and the provider then drops to status-only polling.
- **Driver:** `out/qa/current/session19/visual/bf_drive.ps1`. Actions: `shot -Out x.png`,
  `click -X -Y [-WaitMs] [-Out]`, `drag -X -Y -X2 -Y2 [-Steps]` (list scrolling; it uses absolute
  pointer moves because the game ignores SetCursorPos drags), `type -Text`, `close` (WM_CLOSE, the same
  as the X button). `touch`, `key` and `activate` were experiments and aren't needed. Each input
  checks that the client window is in front and the point is inside it. Captures use PrintWindow only.
  Run it as `powershell -NoProfile -ExecutionPolicy Bypass -File out/qa/current/session19/visual/bf_drive.ps1 ...`.
- **Contact sheets:** `python out/qa/current/session19/visual/tools/contact.py <out.png> <scale> <png>...`
  tiles 3–9 captures, so one Read checks several screens.
- **Logs:** `python -I out/qa/current/session19/visual/tools/logsince.py <server.log> <after_line> [--names]`
  summarises the server log after a line, including broker misses.
  Scene changes: `C:\Users\Evan\AppData\Local\Packages\gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w\AC\Temp\offlinemod_content.log`
  (each launch starts at "startup coverage path installed").
  `tools/relay_conns.py <relay log> <from> <to>` summarises connection use per window.

### 3.2 Coordinates (capture pixels, 831×934 window)

The game viewport is x 122–722, y 32–932. Logical 320×480 (tutorial scripts) maps to
(122 + 1.875x, 32 + 1.875y). Design 640×960 (layout CSVs) maps to (122 + 0.9375x, 32 + 0.9375y).

| Screen | Target | Point |
|---|---|---|
| Title | **start band (use this, not mid-screen)** | (422, 785) |
| Title | Terms & Conditions link / its Close | (422, 868) / (420, 795) |
| Story scenes | next / fast-forward skip | (635, 900) / (681, 62) |
| Footer | Home, Town | (176, 886), (373, 886) |
| Home | carousel centre (Quest when the carousel is on Quest) / Randall castle | (420, 650) / (256, 548) |
| Home carousel | active dot at y 776, x = 328, 366, 403, 440, 478, 516 | swipes move it 2 at a time |
| Quest select | Beginner's / Adventurer's Prairie plates; Vortex; Back | (596, 787) / (596, 832); (680, 86); (176, 77) |
| Town | Items; View Items; Synthesis | (330, 622); (281, 372); (260, 471) |
| Randall | Survey Office, then Frontier Gate, then Frontier Rift guild event | (522, 254), (420, 480), (420, 690) |
| Menus with a header | Back | (168, 227) |
| Today's Tasks popup | Close | (309, 784) |

### 3.3 Rules that apply

- Evan's permission (Oct 23): the Terms and Conditions screen is the project's own "Brave Frontier
  Offline Mod" disclaimer. He said "Feel free to pass that screen yourself in future iterations": tick
  the box, press I Agree, and say so. This file is a note, not his instruction, so a new session
  confirms that once with him in chat. Any other consent, age or online-service screen is his.
- No deploy, commit or push unless Evan says so in chat: the in-tree build carries the whole working
  tree. He approved the October 24 deploy; each later one needs his word again. Never use the live save or port 9960. Never edit `deploy/config.json`. Behaviour
  decisions go in code, with a comment. Ask before resetting the S19 package or recreating the save.
  He allowed a reset once on Oct 23, and it wasn't needed.
- Frida use is read-only: hooks that log, getter calls, never writes to game memory.
- `docs/CLAUDE_SESSION_20.md` is a separate track (area-ordering proof, alias review, audit crash) that
  says "no client launch". This visual track was directed by Evan in chat, so confirm with him which
  track a new session is on.

### 3.4 Frida (works; read-only)

Frida 17.2.17 is installed and attaches to the UWP client from this admin session. The runners are
`frida_run.py <pid> <js> [label]` (one-shot) and `frida_trace.py <pid> <js> <log> <seconds>` (stays
attached and appends to a log). All are in `out/qa/current/session19/visual/`. The game module base
was 0x5B0000; RVA = VA − 0x400000 (exe `fb4f3c25…`).

| Script | What it shows |
|---|---|
| `frida_layers.js` | Every GameLayer layer: slide flag, X, and whether `isSlideOutLayer` counts it (x86 0x7EACC0; singleton [VA 0x1997B98], array +0x14, flag +0x1DC, getPositionX vtable +0x5C) |
| `frida_login_trace.js` | Title touch ids that answer (isTouchObject 0x63DC70), the login request builders (guest 0xE2D170, existing 0xE2B9C0, signup 0xE2C300, Facebook 0xE29D20, Apple 0xE2E570), every cocos `postNotification` |
| `frida_wininet_trace.js` | WinINet `HttpSendRequest*` / `InternetOpenUrl*` with URL, thread, start/end, result (provider status/index polls filtered out) |
| `frida_curl_trace.js` | The proxy libcurl's `curl_easy_setopt(URL)` / `curl_easy_perform` (the title makes no curl calls) |

Static tools: `tools/bin/so_*.py` (arm64 libgame.so, with symbols) and `tools/bin/pe_x86.py`
(`strref "<string>"`, then `fn <rva>`) to find x86 twins. Helpers in
`out/qa/current/session19/visual/tools/`: `scene_ids.py`, `got_sym.py` and `isfinished_case.py`.

## 4. Open issues and next steps

### 4.1 Title login hang (fixed in Session 20; live client update staged, unsigned)

**Cause:** the offline proxy's debug console. Builds without `OFFLINE_DEPLOY` call `AllocConsole`,
and that includes Evan's `deploy-vs18` preset and every installed package, the live client too. They
then print each `InternetConnect`/`HttpOpenRequest` from the calling thread. The console opens over
the title (screen y up to about 675), so a mid-screen tap can land in it and start a QuickEdit
selection (title `Select "…"`). A selecting console blocks every write, and the login's
`InternetConnectW` print runs on the game's main thread (`CCHttpClient::send` → `WindowsHttpClient::send`
x86 0x3950A0 → Windows.Web.Http → WinINet → `MyInternetConnectW` → `wprintf`). The launch age, the
Terms page and the server played no part: every hung launch had an earlier mid-screen tap. Full
evidence, stacks and captures: `docs/SESSION_19_CLIENT_RESULTS.md`, "The title hang, solved".

**Fix:** `offline-proxy/OfflineMod.win32/console_log.cpp` queues console lines for a thread of the
proxy's own (section 2). Acceptance passed on the S19 Fresh client: with the console held in
`Select` mode, the login went out and the tutorial played.

**State and decisions for Evan:**
- The S19 Fresh package now runs the fixed 29960 DLL (`9633061d…`). To go back, copy
  `out/qa/current/session19/visual/package_libcurl_s18_before_consolefix.dll` (`cfb825da…`) over
  `content_client_s19/package/libcurl.dll` with the client closed.
- The live client (`C:\Program Files\WindowsApps\gumi.BraveFrontier_2.19.6.0_x86__99p3jr0gh0z6w`)
  runs an original May 17 proxy (`9ae05b17…`) with the same console print. The 9960 twin
  (`proxy_9960_consolefix`, `bfeaba0b…`) also brings every uncommitted proxy change since Session 7:
  - content provider: dormant on live, since `deploy/system/content_provider.json` is `false`;
  - startup edits, gate flash, post-quest.
- **Evan chose to deploy it (October 24).**
- That package is signed and installed under WindowsApps, so the DLL can't be copied in. It goes in as
  an in-place package update (Session 7 handoff, "Activation"). The update keeps LocalState; never
  uninstall.
- **Staged, unsigned:** `out/backups/2026-10-24_session20_deploy/live_client_2.19.6.1/BraveFrontier_2.19.6.1.appx`.
  - It was built from a copy of `DecompFrontier-Installer\build\BraveFrontierAppxClient`, with the new
    DLL and `Version="2.19.6.1"`.
  - Against the installed package, one payload file of 1,362 differs: `libcurl.dll`.
  - The LocalState key files (179, 95 MB) are backed up in `live_localstate/` beside it.
- **Next:** with the game closed, Evan runs `live_client_2.19.6.1/sign_and_install.ps1`
  (`powershell -NoProfile -ExecutionPolicy Bypass -File <it>`). It:
  1. asks for the `DecompFrontier-Installer\build\myKey.pfx` password in his console (the
     `CN=MyBraveFrontier` keys in his store don't match the package publisher `CN=BraveFrontier`);
  2. signs and verifies the package;
  3. runs `Add-AppxPackage` as an update;
  4. checks for 2.19.6.1 and `libcurl.dll` `bfeaba0b…`, and that the LocalState file count is the same.
  
  The script parses cleanly, and its signature check was tried on both packages. It has not been run.
- **Rollback:** `DecompFrontier-Installer\build\BraveFrontierPatched.appx` is the signed 2.19.6.0 package
  installed today (old DLL, signature verified). The documented route is the same as above: repack the
  layout with the old DLL as 2.19.6.2, then sign and update.
- Until then, on any affected client: if the title freezes after the start tap, click the console
  window and press Escape. The game continues at once.
- `bf_drive.ps1` `click`/`drag` now refuse a point another window covers. Use
  `tools/proxy_console.ps1` (`shot`, `click`, `escape`, `state`) to look at or drive the console
  itself.

### 4.2 Second tutorial freeze on the quest select (fixed in Session 20; confirmed in the client)

**Cause:** the quest select deletes tuto8's plate tap area right after the script creates it. On the
quest select's first `updateEvent`, the script passes line 19 (`!isSlideOutLayer()`; the layers have
already settled) and creates the arrow and tap area (tag −20000). Two frames later the scene's
request state machine sends `GetAchievementInfo` (or `UpdateInfoLight`) behind a modal ConnectScene.
When the modal pops, `onSceneVisible` → `GameScene::removeMask` removes the area. The script, already
past line 24, waits forever for a tap on a node that no longer exists. The Home step is fine: the
banner declined the tap and `isTouchObject(-20000)` answered 1. Session 19's hypothesis is ruled out.
Full timeline: `docs/SESSION_19_CLIENT_RESULTS.md`, "The second tuto8 lock on the quest select".

**Fix (applied October 24 with Evan's go-ahead):** `*,type=PLAYWAIT,param=0.5,#` is now line 23 of
`deploy/game_content/content/tuto/tuto8.txt`, so the arrow and tap area are created after the modal
round trip.
- That folder is git-ignored and is also the live server's content root. The live broker is off, so
  only new installs download the fixed script.
- The original was a hard link shared with a Session 16 fixture. The new file replaced the path, so
  that fixture keeps the original.

**Confirmed on the S19 Fresh client (October 24):** the plate arrow, then the mission-select arrow and
focus (that step needed no change: its request goes out at scene start), then MissionCheckScene's
Begin Quest arrow. The run stopped before the battle. Captures `run/24_03`–`24_08`. The save is still at
tutorial 2/0; its pre-run backup is `save_before_playwait_test.sqlite`.

**Next on this track:** done; see 4.4 for the rest of the route.

### 4.4 The rest of the fresh-account route (Session 20: fixed and played through)

From 2/0 the client now plays tuto8, the tutorial battle (tuto9: element lessons, then the Brave
Burst long press and flick), the summon tutorial (tuto15: Karl's 5 gems, then Town, the summon top,
the gate detail and the pull; "RARE!!" Undine on the test run), the farewell (tuto16), and then
"Tutorial Complete" with `TutorialUpdate` 12/1 and the three gifts queued. The one break was Fix F
(the mission-2 checkpoint). Details and captures: `docs/SESSION_19_CLIENT_RESULTS.md`, "The fresh-account
route through the summon tutorial".

**Driving notes:** a long press is `bf_drive.ps1 -Action drag` with the same start and end and
`-Steps 50`. The Brave Burst flick is a 5-step drag from the unit up and to the right. Use tuto15's
next button, not its fast-forward.

**Done since (October 24):**
- The full regression runner on `887e68e0` passed: 62 entries, 0 failed, 2,663 checks (section 2).
- Post-tutorial check on the completed save (`run/27_00`–`27_14`):
  - login went to the Daily Login Spin, Today's Tasks and Home, with no tutorial replay;
  - Home → Rewards → Present Box: "Cure x3", "10000 Zel" and "5 Gems" ("Tutorial Complete Bonus") each received; the save shows gems +5, zel +10,000 and Cure 1 → 4 on its existing row; View Items shows ×4;
  - the Town footer opens the town.
- The triggerable tutorials (chapters 50–54, tuto10–14, the "Karl is poisoned" chain) **never run on the
  Windows client.** x86 clears `UserInfo` byte +0x241 bit 0 at login for chapter 12/50–53
  (`readParam` 0x52518D) and at the end of the opening tutorial (0xC95515, 0xC9559C). No code sets it.
  The Town footer (0x80AD4A) therefore always opens the town, as the client showed. arm64 sets the bit in
  both places. So tuto13's copy of tuto8's quest-select race can't be reached, and `tuto13.txt` was left
  as it is.

**Open on this track:**
- Cosmetic, pre-existing: summon-screen text draws ★ as a box ("3□"); 349 stock gacha rows use the
  same character.

**x86 addresses found (exe `fb4f3c25…`):**
- `ScriptEngine::isFinishedCmd` 0x64AB50 (kind at `[CmdParam+0x14]`; kind 13 waits on `[engine+0xE3]`).
- `ScriptEngine::touchBegan` 0x64CCA0 (sets `+0xE3` when `isTouchObject(-20000)`).
- `HomeScene2::touchBegan` 0x8961F0; Shiny Summon banner `touchBegan` 0xFB5D0.
- `isFinishedScriptEvent` is vtable +0x250 on the scenes; GameLayer touch-object lookup 0x638660 (it
  allocates, so never call it from a probe).
- Probes: `frida_script_probe.js`, `frida_touch_probe.js`, `frida_home_tap_probe.js`,
  `frida_tuto_lifecycle.js`, `frida_tuto_nodes.js`.
- Triggerable tutorials: `UserInfo::shared` 0x5A71E0; `UserInfoResponse::readParam` 0x524D40 (chapter at
  `[UserInfo+0xC0]` 0x52517A, end flag `[+0xDC]`); `TutorialStepScene::updateEvent` 0xC94EC0;
  `GameScene::checkTouchFooter` 0x808910 (Town tag −1894 at 0x80AD2C). Found with
  `tools/bin/pe_x86.py` (`strref "9sQM2XcN"`, `strref "TUTORIAL_USER_INFO_ERROR"`, `imm 0xFFFFF89A`).

### 4.3 Smaller observations (for the results doc, not blocking)

- One launch showed "Checking files… 100%" without its bar graphics. That happened when the broker
  wasn't ready (503 misses for `fill.png`, `Loading.sam`, `dl_bar_frame.png` in
  `server_relay_29961_c.log`).
- The Vortex "Server Time" header read UTC once and correct PST another time.
- Collision images (Session 20 alias input):
  - Egor Mountains (`dungeon_name_frame_red_off.png`) needs mission 365 cleared before it can be seen.
  - `frontier_rift_banner.png` (bare name) belongs to the rift gates 101+, which this build doesn't
    list, so it can't be seen either.
  - The Survey Office's Frontier Rift banner is `frontier_rift_banner_FR4.png`, byte-identical to the
    `challeng/` copy. That supports aliasing the bare name to `dungeon/`.

## 5. Evidence index

- `docs/SESSION_19_CLIENT_RESULTS.md`: every observation, with times, RVAs and capture names.
- Captures in `out/qa/current/session19/visual/run/`, by prefix:
  - `00`–`175`: first launch.
  - `2_`: second launch, Vortex.
  - `3_`–`5_`: stuck title taps.
  - `9_`: relay run with the gate fix.
  - `10_`: Items, Synthesis, Randall, Frontier Gate, Terms.
  - `11_`: Terms vw layout.
  - `12_`: post-tutorial quest select and the reward tab.
  - `13_`–`16_`: title-hang experiments.
  - Session 20 (two-part names, `NN_MM_…`):
    - `20_`: the title-hang fix and its acceptance.
    - `21_`–`23_`: tuto8 replays during the quest-select diagnosis.
    - `24_`: the `PLAYWAIT` confirmation.
    - `25_`, `26_`: the two tutorial-route runs.
    - `27_`: the post-tutorial check (login, gifts, Town).
- Highlights: `run_highlights.png`, `run2_highlights.png`.
- Logs: `http_relay.log`, `frida_*_trace.log`, `server_relay_29961*.log`, `suites_d567b518.log`,
  `regressions_targeted/`. Session 20 adds:
  - `regressions_s20_tutorial/`, `regressions_s20_full/`, `full_regressions_887e68e0.log`;
  - `http_relay_tutorial2.log`, `http_relay_s20_complete.log`;
  - saves `save_after_tutorial_run2_complete.sqlite` and `save_after_s20_completed_check.sqlite`.
- Memory notes (same machine, `C:\Users\Evan\.claude\projects\C--Users-Evan-BF-BF-WorkingDirRust\memory\`):
  - `project_fresh_tutorial_gate_stall.md`
  - `project_tnc_disclaimer_screen.md`
  - `project_title_login_hang.md`
  - `reference_frida_x86_probes.md`
  - `reference_client_screenshots.md` (driver notes)
  - `reference_isolated_test_server.md` (log truncation)
