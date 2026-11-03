# Session 19 isolated client results

October 23, 2026 — fresh client and fresh isolated server setup prepared, then the first launch was observed and the new-player route driven as far as the Mistral quest select, where a tutorial input lock stopped it (below). The second launch reshowed the Terms and Conditions screen; Evan passed it, and the second-launch checks and the Vortex were then driven. Items and Frontier Gate stay unreached behind the same tutorial lock.

## Setup and preflight

Created and registered the never-launched AppX identity `gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w!App` in `out/qa/current/session19/content_client_s19/package`. The package contains 1,361 hard links to the extracted client and uses the Session 18 isolated client DLL (`cfb825da79de8ca7a68db12009e9a9c66f5181bdb0af38d6a7660d3b44aba545`) on port 29960. Registration and loopback exemption succeeded. The package LocalState directory exists and is empty; no client process has been launched from this identity by setup.

The isolated server is Session 19 `out/qa/current/session19/bin/gimuserverw.exe`, SHA-256 `02b95f90c98bcf921a247cc305c0244917c3a65274546ffc7eb23a074ade9272`, using C8 policy SHA-256 `388185d921265ca5197bc8e27671e36037e2d4703484116ec8cd2a7b118583ea` and final manifest SHA-256 `9b3068e69b88e3408662bb51c6d0d9a4f0aa61a3c8452526a72d6a49ea51cd04`. It runs on `127.0.0.1:29960`, PID 2868, against a new blank database at `out/qa/current/session19/content_client_s19/saves/s19_first_launch_blank/gme.sqlite`; the database was created as zero bytes and initialized by the server, without copying the live save.

The live bootstrap audit passed before launch: 169 announced rows, all required deliveries available, 29 response tables served, 104 text dictionaries served, initial units available, and `corrupted_mst_check = 1`. Audit exit code was 0. Prediction JSON is `out/qa/current/session19/bootstrap/first_launch_prediction.json` (SHA-256 `049e24c1f2c25d0e7f8b0abb977727ec7323ba8d8e7b88fa000e1ba9196ba9bb`); console log is `out/qa/current/session19/bootstrap/first_launch_audit.log` (SHA-256 `26da5f7f43631f83de2fd22a8857ec497a6608ef1c2886293ad7f371e3c98b04`).

The reusable command and exact package paths are documented in `docs/CLIENT_SESSION_19_STEPS.md`. The standard harness was not modified; `out/qa/current/session19/content_provider_client_test_s19.py` supplies the isolated identity mapping.

## Client observations

Times are local (UTC−4); the server log is UTC. Captures are in `out/qa/current/session19/visual/run/`; the walkthrough is [CLIENT_SESSION_19_FRESH_VISUAL_STEPS.md](CLIENT_SESSION_19_FRESH_VISUAL_STEPS.md). Input was driven by `out/qa/current/session19/visual/bf_drive.ps1`: one window, found by its exact title, with foreground and bounds checks before each input, and PrintWindow captures only. No consent or age attestation was automated. The user passed the first-launch agreement and consent screens by hand at about 07:16.

### First launch, read only (07:16–07:23)

Initialize announced 169 table versions, 21 of them staged. LocalState received exactly the predicted master tables (`first_launch_prediction.json`): 142 of 142 table parts, 29 of 29 response tables and 103 of 103 text dictionaries. Nothing was missing or extra, and the server logged no 404. The client passed its login table checks with no REDOWNLOAD notice and no network error, and stopped at the Background DLC Warning Prompt.

### New-player route, driven (07:33–07:59)

| Step | Result | Captures |
|---|---|---|
| 1 Prompt | **No** at 07:33:51; the prompt closed at once. | `00_before`, `01_after_no` |
| 2 Opening, name | English text throughout (Lucius, Tilith, Karl); dialogue boxes readable; name `S19Fresh` accepted. | `02_opening`, `06`–`09` |
| 3 Starter choice | All four starters drawn with names, portraits and English text: Thunder Eze, Fire Vargas, Water Selena, Earth Lance. The broker served `unit_ills_full_10011/20011/30011/40011` at 11:37:30. **Selena** picked; the story then names her ("how did you manage to summon Selena?!"). | `13`–`17` |
| 4 Tutorial battle | Beginner's Prairie, "Basics of Battle": units, Mossy, a chest drop, the Cure item, BOSS INCOMING and King Mossy all animate with damage numbers. CONGRATULATIONS, then Quest Clear Reward (Zel 1026, Karma 104, XP 5). Mossy was acquired with a full unit page (No.47, Oracle, BB "Gnawing Wood"). MissionEnd recorded the first clear at 11:48:33. **The tutorial summon was not reached**: this route runs fusion and squad tutorials first. | `38`–`97` |
| — Fusion, squad | Fusion of Selena with Mossy (Zel 200, +102 XP; Lv 1→3; level-up animation, her fusion line). UnitMix was logged at 11:54:32. The squad screen shows leader skill "Water Spirit's Power" with its description. | `122`–`147` |
| 5 Home | Renders fully: status bar, squad with Selena as leader, menu row, banner carousel, footer. The locked menu icon (castle with padlock) is drawn, not an empty box. The Home footer icon plays its unlock padlock animation; Town, Summon and Social carry badges. | `158_home_settle` |
| 6 Items | **Not reached** (tutorial input lock). The Town's Synthesis, Upgrades and Items labels are drawn. In the second session, Home accepted only the tutorial's Quest area even after the hand stopped showing: MENU and Town did nothing (12:28 UTC). | `122`, `2_65`–`2_68` |
| 7 Mistral | The quest select renders: `area_0001.jpg` and two quest plates (`mission_select/dungeon_name_frame_off.png`), Beginner's Prairie and Adventurer's Prairie, each with an animated NEW AREA tag. **Egor Mountains is not listed** at one clear, so reference image 1 could not be compared. | `165`, `175_stuck_0806` |
| 8 Vortex | Reached in the second session through the world select's Vortex button. **Open on a new account**, with no lock. Seven top-level entries, banners drawn: Vortex Trials, Enhancement (NEW FEATURE), Travellers' Saga, Seasons Past 2019–2021, Seasons Past Before 2019, Frontier Hunter (NEW FEATURE, "Event Period") and Parade Garden. Vortex Trials holds The Uprising, Loving Goddess Tilith (NEW, SP Dungeon), Battles of Brave Frontier 2 and Mythological Entities. **New Frontier Gate and King of Fighters are not Vortex tiles at all**: the Vortex block is land-99 areas below 200000 (`kVortexAreaIdMax`, `ServerCache.cpp:31`), and they are areas 3000001 and 800142. `frontier_rift_banner.png` is the banner of New Frontier Gate's dungeons 8000001–8000010, so it is drawn only inside Frontier Gate, which this account can't open (see step 6). The header reads "Server Time: Oct 23, 12:21 PST", but 12:21 is UTC. | `2_47`–`2_62` |
| 9 Second launch | Closed normally (WM_CLOSE; the game exited at 08:06:42). Relaunched by AppUserModelID at 08:06:51; the window was up within 1 s. **The Terms and Conditions / Privacy Policy screen was shown again** (08:07:03); Evan passed it at about 08:15. The title screen followed. After the tap at 08:16:45: "Connecting…", then **"Checking files…" 0% at 08:16:55 to 100% at 08:17:02 (about 7 s)**. **No background-download prompt.** Initialize announced the same 169 versions, and **the second session downloaded no master table** (0 `/mst/` requests; no warning or error). It did not land on Home: the tutorial resumed at `tuto2.txt` (Tilith's post-battle scene) from the server's `tutorial_status` 2. It **skipped the fusion and squad tutorials**, so the client keeps its own record of finished sub-tutorials. It then returned to Home at tuto8's first step. | `2_00`–`2_43` |

### Tutorial input lock on the Mistral quest select (open; for Session 20)

After the squad tutorial the client fetched `tuto/tuto8.txt` (11:57:05). On the 320-wide logical canvas, its lines 15–19 put an arrow at (159,258) and a tap area of (98,255) 123×129 on Home, followed by `change_dungeon_select_scene`. Lines 23–24 then put an arrow at (248,395) and a tap area of (190,392) 103×60 on the quest select. That is capture (488..681, 767..880), which covers both quest plates.

What happened:
- The Home tap landed in its area.
- The client then opened the **world select** (Grand Gaia / Ishgria: `layout_landScene.csv`, `areaselect_gran_ish.jpg` and both gate buttons, 11:58:43).
- Grand Gaia opened Mistral's quest select (`layout_dungeon_select.csv`, 11:59:04).
- tuto8's second arrow never appeared.
- From 07:59 to 08:06 the scene ignored every tap (both plates, the Mistral plate, Back) while its animations kept running.
- The server logged no error and received no request.

**Cause, confirmed in the client (October 23, afternoon).** Home's Quest handler, `HomeScene2::touchEnded2` @0x16F52E4 (arm64), calls `GateMstList::getActiveCountNormal`, which counts the permitted gates of GateMst type 0, and then changes to scene 2001 (the world select) at two or more, otherwise to scene 61 (the Grand Gaia quest select that tuto8 waits for). PermitPlace's static base permitted every gate 1–100 from first login, including gate 2, **Ishgria** (type 0). Its shipped GateMst `need_mission_id` is **1577**, "Facing the Sealed God", the God Lucius finale of Grand Gaia Crater (dungeon 1570; `tools/sgtext/missions.tsv`; the wiki's Grand_Gaia_Crater page). Gate 99 (Vortex) ships with 0 and stays open from the start.

**Fix (built and wire-tested, not deployed).** `PermitPlace.cpp` now builds the gate rows per player: a GateMst gate is permitted when its `need_mission_id` is 0 or cleared, and ids with no GateMst row keep their old blanket permit. The log line now counts gates. Ishgria thus opens when 1577 is cleared (MissionEnd carries the permits). No save is stranded: the live save (14 clears), its backup and this save hold no Ishgria clear without 1577 (read only). Regression: `scripts/test_story_gates_wire.py` gained four gate checks and passes 39/0 on the fixed candidate. The unfixed Session 19 candidate fails exactly the Ishgria check (38/1). The progression-related suites pass on the fixed candidate (`out/qa/current/session19/visual/regressions_targeted/`): battle simulator 93/0, mission progression 50/0, Lizeria 23/0, Enhancement 36/0, metal parades 18/0, town unlocks 23/0, feature visits 4/0, Research Lab 39/0. The mission validator equals its baseline (3,117 findings). Candidate: `out/qa/current/session19/visual/bin/gimuserverw.exe`, SHA-256 `1a59c1bda4e728a8a1f5978bc52752a22fb2c907b1214560cfe49e3c38b8c883`, built by `out/qa/current/session19/visual/build_server_visualfix.sh`. The live in-tree EXE `6ee7d454…` and the Session 19 candidate `02b95f90…` are unchanged.

**To confirm**: on an isolated copy, withhold gate 2 until mission 1577 is cleared, then replay from this checkpoint. The current save is `tutorial_status` 2, `tutorial_end_flag` 0, and the client has sent no TutorialUpdate yet. The second launch resumed at tuto8's Home step as expected. The world select still appears, and its Vortex button works. The Vortex's Back returns straight to Home.

Separately, area 20100 "Bectas" (land 100) has `need_mission_id` 0 and is permitted at zero clears (`land(s) [1,100]` in the PermitPlace log).

### The second tuto8 lock on the quest select: fixed (Session 20, October 23–24)

With the world select fixed, Home's Quest opens the Grand Gaia quest select, and tuto8's plate arrow never appears. Reproduced three times (22:20, 22:40, 22:46) and traced with read-only Frida probes (`frida_script_probe.js`, `frida_touch_probe.js`, `frida_home_tap_probe.js`, `frida_tuto_lifecycle.js`).

**Not the main thread.** The launch that froze here in Session 19 kept drawing at full rate (`perf (periodic)`, 1,856 frames per 30 s) for 14 minutes after the freeze. It is not the console block above.

**Not the Home tap either.** On Home, `HomeScene2::touchBegan` (x86 0x8961F0) gives the touch to the Shiny Summon banner first (`InteractiveBannerUserInterface::touchBegan` 0xFB5D0, rectangle 207.5,302.5 112.5×46). That banner declined, `ScriptEngine::touchBegan` (0x64CCA0) ran, and `isTouchObject(-20000)` answered 1, so the Home step completed normally. Session 19's leading hypothesis is ruled out.

**Cause: the quest select deletes the plate step's tap area right after tuto8 creates it.** `frida_tuto_lifecycle.js` timeline, milliseconds after attach:
- 12311: `replaceScene` to `DungeonSelectScene2`. Home's tutorial nodes (tags −20000, −20001, −20002) are removed, as expected.
- 12642: the script's line 19 (`id=100 change_dungeon_select_scene`, answered by `!isSlideOutLayer()`) passes on the quest select's first `updateEvent`, and lines 23–24 create the arrow and the tap area (`setTag(-20000)`). The script waits at line 25.
- 12678: `pushScene`, the modal ConnectScene for `GetAchievementInfo` (server log: mode 1, category 1).
- 12856: `popScene`.
- 12879: `removeChild(-20000)`. The quest select's `onSceneVisible` (x86 near 0x96C972; arm64 `DungeonSelectScene2::onSceneVisible` → `GameScene::removeMask`) runs again when the modal pops and removes the tutorial's mask and tap area.
- From then on, every frame `isFinishedCmd` (x86 0x64AB50) reports arrow done, area done, and wait (kind 13) not done. The wait ends only when `ScriptEngine::touchBegan` sets `[engine+0xE3]`, which needs `isTouchObject(-20000)`. No node with that tag exists any more (a read-only walk of all 110 GameLayer layers found none), so no tap can satisfy it.

**Why the request lands after the script.** `DungeonSelectScene2::updateEvent` (arm64 0x1852EFC) runs `ScriptEngine::process()` first, then a request state machine that the constructor starts at state 2. State 2 sends `UpdateInfoLight` when `TierTourneyMst::getEventFlag()` is 0; otherwise it moves to state 1, which sends `GetAchievementInfo` when `FUNC_FEATURE_GATING` is set. That global comes from `featureCheck.php` (`"feature_gate": 1` in `deploy/system/features.json`). Both paths show the modal ConnectScene. The quest select's layers are already settled at its first `updateEvent` here (all at X = 0), so line 19 passes in the same frame, and the modal comes two frames later.

**Fix (applied October 24 with Evan's go-ahead).** One line in `deploy/game_content/content/tuto/tuto8.txt`: `*,type=PLAYWAIT,param=0.5,#` is now line 23, after the `STOP` that ends line 22 and before the plate arrow. The script creates the arrow and tap area half a second later, after the modal round trip; the scene and its script are paused while the modal is on top. `PLAYWAIT` takes seconds (tuto1 uses 1–4).
- File: 2,328 bytes, SHA-256 `271511d995619d20956883bdc77d6b0253d86a8784cc97ba3b2471c65f89f16b`. The BOM and CRLF endings are kept.
- The original (2,299 bytes, `cfa85733…`) is saved as `out/qa/current/session19/visual/tuto8_before_playwait.txt`.
- The original was a hard link shared with `out/qa/current/session16/content_client_c16/disposable_root/tuto/tuto8.txt`. The new file replaced the path instead of being edited in place, so that Session 16 fixture keeps the original.
- `deploy/game_content/` is git-ignored and is also the live server's content root. The live broker is off (`deploy/system/content_provider.json`), so the live client keeps its cached copy; new installs download the fixed script.

**Confirmed in the client (October 24, 07:10–07:15).** No Frida attached; server `d567b518` behind the relay; tutorial state 2/0. The broker served the new script (`sha256:271511d9…`). The Home step, then:
- **Quest select:** the TOUCH hand appears on Beginner's Prairie about 2 s after the Home tap and stays after the `GetAchievementInfo` round trip (push 121.78 s, pop 121.95 s): `run/24_03_quest_t2s.png`, `run/24_04_quest_t5s.png`.
- **Plate tap → mission select:** the hand and focus mask sit on the first mission, "Element & Skill", through `MissionSelectScene2`'s own `GetAchievementInfo` round trip (push 154.58 s, pop 154.78 s): `run/24_05_mission_t2s.png`, `run/24_06_mission_t6s.png`. That request goes out at scene start, before the script's step, so this step needed no change.
- **Mission tap → MissionCheckScene:** the hand points at Begin Quest with its message (tuto8 line 40): `run/24_07_check_t2s.png`, `run/24_08_check_t6s.png`. No request on this transition.
- The run stopped there, before the battle. The save is still at tutorial 2/0, and its pre-run backup is `out/qa/current/session19/visual/save_before_playwait_test.sqlite`.

### The Terms and Conditions screen (open; Evan's request)

Evan, 2026-10-23: this screen is the project's own disclaimer, "Brave Frontier Offline Mod". Its text is drawn in black, sits in the wrong place and is hard to read (in the captures it overlaps the BRAVE FRONTIER logo behind the "Terms and Conditions" title). He wants it fixed later. He also said the screen may be passed during test runs; the agent passed it on later relaunches of this session's test client and says so each time.

The page is the server's `/bf/web/terms.htm` (`gimuserver/controller/WebTerms.hpp`), unstyled HTML. The title shows it in a native web view (`TitleScene::showTermsAndConditions` -> `WebViewScene`, `TERMS_AND_CONDITIONS_URL`), transparent over the dialog. The view's rectangle comes from the client's platform layer (`LayerWebView::setWinPosition`/`setWinSize`). On this client it starts at the dialog's top edge, over the header, while the dialog's text box sits empty between the gold lines. The fixed candidate keeps the wording and adds readable styling: light text with a shadow, a sans-serif font, and a translucent dark panel filling the view, which also shows the view's real bounds. The text's vertical offset will be corrected from those measured bounds. Not yet seen on screen: the acceptance has stuck, so the screen no longer appears by itself; the title's Terms & Conditions link should open it.

### Third session (fixed candidate, 13:03–13:50)

The candidate ran behind a logging relay: the client talks to 127.0.0.1:29960 (`out/qa/current/session19/visual/http_relay.py`, request and status lines only), which forwards to the server on 29961 (`config_relay_29961.json`, a copy of the fixture config with only the port changed). The log is `out/qa/current/session19/visual/http_relay.log`.

- **The gate fix, confirmed in the client.** UserInfo now reports "permitting 99 gate(s)" (1–100 without Ishgria), and the normal permit channel is 121 rows, down from 122. On the tutorial's Home step, Quest went **straight to the Grand Gaia quest select** (`HomeScene2` -> `DungeonSelectScene2`); the world select no longer appears.
- **A second tutorial lock remains on the quest select.** tuto8's next arrow never appeared there, and taps were ignored. The scene runs the script only while `UserInfo::isTutoPlaying` (`DungeonSelectScene2::initialize` @0x184D418 sets +0x3FB; `updateEvent` @0x1852F2C drives `ScriptEngine::process`), and the stall persisted with the world select gone. So this is a separate cause, not yet found. To reach the rest of the walkthrough, the QA account's tutorial was marked finished in the isolated save only (`tutorial_status` 2 -> 12, the client's own finished chapter; `tutorial_end_flag` 1). Backup before the change: `out/qa/current/session19/visual/save_before_tutorial_end.sqlite`. The live save was not touched.
- **Step 6, Items: passes.** Home took normal input; Town and Items opened. The item box shows Cure ×1 in its frame (1/100). The Synthesis list was scrolled to the end: every recipe (owned or not) has real item art in a frame, with English names and descriptions. No broker request or miss happened while scrolling, so the art was already in place.
- **Step 8, Frontier Gate: reached, collision not observable.** Randall (the castle icon on Home, open as recorded) -> Akras Summoners' Hall Survey Office -> Frontier Gate opens after Noel's English intro. Its quest list shows gates 1–20, 90 and 91, all rendered. The bare `frontier_rift_banner.png` is the banner of the rift dungeons 8000001–8000010 (FrontierGateMst 101+). The client lists none of those gates, and the "Frontier Rift GUILD EVENT" entry opens an empty FG+ quest list. So neither copy can be seen in its own spot on this build. New evidence for the Session 20 alias decision: the Survey Office's Frontier Rift banner is a different file, `frontier_rift_banner_FR4.png` (requested by that name, served from `challeng/`). The C8 alias target `challeng/frontier_rift_banner.png` (copy B, `6bcda33e…`) is byte-identical to that FR4 banner. Copy A (`dungeon/`, `5e5bda74…`) is the only distinct art for the bare name. This supports aliasing the bare name to `dungeon/`, as Session 19 proposed.
- **Daily login:** the Daily Login Spin paid 200,000 Karma (201,104 shown), and Today's Tasks rendered with item art.
- **Terms screen, fixed text placement.** The title's Terms & Conditions link opens the page in its Close dialog. The measured view is x 161–683 (522 px wide), y 116–613. The disclaimer now sits inside the dialog's box under "Privacy Policy", in light text on a translucent panel (`run/10_03_terms_fixed.png`, before: `run/10_01_terms_link.png`). The final layout uses width units so it also fits the shorter first-launch agreement view, which wasn't seen again (the acceptance has stuck). Candidate after this change: `d2af5a37f864b7e4743c24f8d6085f90425aebab6dc0bedb904a73af29bac3b8`.
- **The Vortex "Server Time" header is inconsistent between sessions.** In the second session it read "Oct 23, 12:21 PST" at 12:22 UTC (equal to UTC). In this session it read "Oct 23, 09:30 PST" at 17:30 UTC (UTC−8, correct for PST). Same machine, different server binaries; the cause is unknown and not part of Session 19.

### The title login, traced (Frida, read-only hooks)

`out/qa/current/session19/visual/frida_login_trace.js` hooks the x86 login request builders (found by string reference: guest `0xE2D170`, existing `0xE2B9C0`, signup `0xE2C300`, Facebook `0xE29D20`, Apple `0xE2E570`), `BaseScene::isTouchObject` (`0x63DC70`, logging ids that answer true) and the cocos2d `postNotification` exports. Log: `frida_login_trace.log`.

- A tap on the start button band (`start_btn`, design y 747–859, capture y ≈ 732–837) answers touch id 1 and logs in cleanly. The trace: `GumiLiveManagerLogin`, then the guest login, then `GumiLiveLoginSuccessful`, and `GET /accounts/guest/login/` on the relay (18:02:14).
- A mid-screen tap 51 s after launch answers no touch id, and the title correctly ignores it.
- The x86 title handler (`0xC89CB0`): touch id `0xD` first deletes the saved keychain and sets the login type to `enumGumiLiveLoginInvalid`. Then ids 1 and `0xD` both create `GumiLiveScene` (state 3 for `0xD`, otherwise 0, 4 or 6). Which on-screen element is `0xD` is still open.

### The title hang after the first touch (solved in Session 20; see the next section)

Evan reported it ("hanging on the splash screen after the first touch"), and it was reproduced with the trace (18:10:53). A tap more than a minute after launch answered touch id 1, posted `GumiLiveManagerLogin` and entered the guest-login builder. The request never reached the relay, and `GumiLiveScene` waited forever. Early taps (32–46 s after launch) always worked; late ones (62–859 s) mostly hung. It is not a plain timeout, though. A tap at 115 s worked (18:14:23), and so did two far later ones, at about 230 s (13:24:51) and 3 h 42 min (17:32:44). Both of those launches had shown the Terms page first (`GET /bf/web/terms.htm` at 13:24:30 and 13:50:54).

**A server fault found on the way (fixed, but it is not the whole cause).** The client keeps keep-alive connections to the server. The server (drogon's default `idle_connection_timeout`, 60 s) closed an idle one, which the relay logged as "server closed its side" 61 s after the game's `/dls`. `/dls` sets `Connection: close`, but that did not close the connection. `standalone_frontend/main.cpp` now sets `setIdleConnectionTimeout(0)` in code. Socket-level proof (`out/qa/current/session19/visual/keepalive_probe.py`, two requests on one socket with 90 s idle between): the fixed candidate `d567b518…` answered the second request 200 OK; the unfixed Session 19 build `02b95f90…` aborted the socket (WinError 10053).

**The hang still reproduces on `d567b518`** (18:20:39). The tap answered id 1, `GumiLiveManagerLogin` was posted and the guest-login builder ran. That launch had a single live connection (#6, polled every 4 s, never idle-closed), and still no request arrived. So the request is lost inside the client before it reaches the network. The WinINet trace (`frida_wininet_trace.js`) shows the game sends `/dls` with `HttpSendRequestA`; the provider uses `InternetOpenUrlA`. No late tap has been traced with it yet. The curl trace shows no curl use at the title.

**Next step:** launch with `frida_wininet_trace.js` and `frida_login_trace.js` attached and tap the start band (capture y ≈ 785) at least 70 s after launch. If `HttpSendRequestA` for `/accounts/guest/login/` starts and never ends, the send blocks inside WinINet. If it never starts, the request is stuck in the game's queue before WinINet.

### The fresh-account route through the summon tutorial (Session 20, October 24)

Played from tutorial 2/0 on the S19 Fresh client (server behind the relay, no Frida): tuto8 → the tutorial battle (tuto9) → the summon tutorial (tuto15) → the farewell (tuto16) → done.

**The tutorial battle works.** In "Element & Skill" every tuto9 step appeared and answered: the element lessons with their callouts, then the Thunder Sparky's attack. Battle 2 taught Brave Burst: a long press on Selena brought up the Brave Command wheel, and a flick up and to the right fired her Brave Burst ("WIN!"). The boss fell in 5 turns. Captures `run/25_*` (first run) and `run/26_00`–`26_09`.

**What broke: the summon tutorial never started.** After the first run's result screens the client showed "Tutorial Complete" and reported `TutorialUpdate` 12/1. tuto10–16 were never requested (`run/25_17_result5.png`).
- **Cause:** `MissionEnd` wrote chapter **50** after mission 2 (`kSecondTutorialCheckpoint`).
- `TutorialStepScene::checkChapter` (arm64 0x1B9275C) starts with `ch > 11 → setTutoEndFlg(1)`, so any chapter above 11 ends the tutorial. It clamps the chapter to 12, which is what the client then reported.
- The opening sequence is 1–9, then 10 (tuto15), 11 (tuto16), 12. The client's own skip path sets 10 after the battle (`MissionScene::updateEvent`, `setTutoChapterID(10)`).
- 50–54 (tuto10–14) are the triggerable tutorials. `GameScene::checkTouchFooter` moves a finished player from 12 to 50 when `getTriggerableTutorialEnabled()`. The Windows client never sets that flag, so they never run there (see "The triggerable tutorials never run on Windows" below).
- The September change from 10 to 50 rested on tuto14's `battle_ui_on`. The manual Brave Burst problem it was meant to fix was `bb_timer` (features.json), and Brave Burst worked in this battle.

**Fix (server source, uncommitted):**
- `gimuserver/gme/handlers/Mission.cpp`: `kSecondTutorialCheckpoint` is 10 again, and its comment now gives the client evidence above. The `BETWEEN 1 AND 9` guard is unchanged: only a player still in the opening band moves and is paid the 5 gems.
- New suite `scripts/test_tutorial_checkpoint_wire.py`, registered in `run_bugfix_regressions.py`. It covers mission 2 at chapters 2 and 9 (→ 10, +5 gems, end flag 0, and UserInfo reports 10); a loss; replays at 10, 12 and 50 (nothing changes, no gems); and the mission-1 checkpoint.
- QA candidate `out/qa/current/session19/visual/bin/gimuserverw.exe` `887e68e03cff982c05f605bf9c542cfecbe6fb325651195599a932f8ce51d1c5` (build tag s20tutorial). The previous candidate `d567b518…` is kept in `visual/bin_d567b518/`. The live in-tree exe `6ee7d454…` is unchanged.
- **Tests on `887e68e0`:** tutorial checkpoint 21/0, battle simulator 93/0, mission progression 50/0, settlement 23/0, run edges 2/0, run ownership 57/0, story gates 39/0, DB exception atomicity 70/0 (`visual/regressions_s20_tutorial/`; `out/qa/current/SUMMARY.*` and those logs were put back afterwards). **Control:** `d567b518` fails exactly the three chapter checks (it writes 50) and passes the other 18.
- **Full regression runner on `887e68e0` (October 24, 08:54–09:19): 62 entries, 0 failed, 2,663 checks.** The previous full run (`02b95f90`, October 22) had 61 entries and 2,640 checks. Every difference is accounted for:
  - tutorial checkpoint: 21 (new);
  - story gates: 35 → 39 (Fix A's four gate checks);
  - reward-lock snapshots: 24 → 22. A slots pull carries the warehouse list only for some prizes, and each such pull adds 2 checks. The suite prints the count ("slots: 1 of 3 pulls carried the warehouse list" this run, 2 of 3 last time).
- The mission validator equals its baseline (3,117 findings). The run's summary, logs and `validate_missions.json` are in `visual/regressions_s20_full/` (console output: `visual/full_regressions_887e68e0.log`). `out/qa/current/SUMMARY.*`, `validate_missions.json` and the 102 per-suite logs were restored from `visual/qa_backup_before_full_s20/` afterwards. The one log the run added was removed after it was copied.

**Confirmed in the client (second run, 07:41–07:54).**
1. The server wrote chapter 10 and 6 gems (5 for the summon, 1 dungeon-clear gem).
2. After the results the client loaded `tuto15.txt`: Tilith's farewell, then Karl ("…You received 5 Gems… Now summon yourself a strong Unit!").
3. The script opened Town ("Tap Summon"), then the summon top (Rare Summon, "Try tapping Summon"), then the gate detail ("Summon once for 5 Gem(s). Available: 6").
4. The GachaAction round trip opened the gate: **"RARE!!" Undine** (gems 6 → 1, units 6 → 7).
5. Then Karl's "Well done…", `tuto16.txt` (Karl's farewell, "I bid you farewell…"), Home with "Tutorial Complete…", and `TutorialUpdate` 12/1 with the three completion gifts queued.

Captures `run/26_10`–`26_20`. The QA save was then put back to 2/0 (completed state kept as `visual/save_after_tutorial_run2_complete.sqlite`).

**Cosmetic, pre-existing:** the summon screens draw the ★ in "Summon a 3★ unit!" as a box ("3□"). 349 stock gacha rows use the same U+2605, so this is the Windows client's summon-screen font, not something gate 2000's caption introduced.

### After the tutorial: login, gifts and Town (Session 20, October 24, 09:21–09:28)

Server `887e68e0` on 29961 behind the relay (`server_relay_29961_l.log`, `http_relay_s20_complete.log`), no Frida. The completed save (`save_after_tutorial_run2_complete.sqlite`, 12/1) was put in for the launch.
1. **No replay.** Title, start band, "Checking files…", then the Daily Login Spin (day 2, "GET!! Water Crystal"), Today's Tasks, and Home. No tutorial script was requested and no `TutorialUpdate` was sent; the save stayed 12/1. The leader is the tutorial's Undine.
2. **The three completion gifts.** Home → Rewards (the book icon right of INFO) → Present Box (badge 7). Its first three rows were "Cure x3", "10000 Zel" and "5 Gems", each labelled "Tutorial Complete Bonus". Each was received on its own (`PresentReceipt` presents 7, 6, 5). The header changed straight away: zel 6,878 → 16,878, then gems 1 → 6. In the save: gems +5, zel +10,000, and Cure (item 20000) 1 → 4 on the same warehouse row. Presents 5–7 are marked received; the other four (login bonuses and raid medals) are untouched. Town → Items → View Items shows "Cure ×4", 1/100.
3. **Town footer.** It opened the town directly, with no tutorial. That is the footer that would start tuto10 if the triggerable flag were set (next section).

The server log for the session has no errors and no broker misses. Captures `run/27_00`–`27_14`. Afterwards the client was closed, the server and relay were stopped, and the 2/0 save was put back. The post-check state is kept as `visual/save_after_s20_completed_check.sqlite`.

### The triggerable tutorials never run on Windows (Session 20, October 24)

tuto10–14 (chapters 50–54) are the "Karl is poisoned" chain: tuto10 (the poisoning), tuto11 (Town → Synthesis → facility upgrade → antidote), tuto12 (the antidote, Seria), tuto13 (item setup, then Home → quest select → mission select → a quest), and tuto14 (battle UI). tuto13 repeats tuto8's quest-select steps without a `PLAYWAIT`, so it would hit Fix E's race. That raised the question whether anything reaches it.

On arm64 the chain is live:
- `UserInfoResponse::readParam` sets the flag for chapter 12 or 50–53 (0x13FF248).
- The end of the opening tutorial sets it too (`TutorialStepScene::updateEvent` 0x1B93328).
- `checkTouchFooter` then sends the Town footer (tag −1894, scene 900 = `MyTownTopScene`) to `TutorialStepScene` at chapter 50 instead.

**The x86 client clears that flag (`UserInfo` byte +0x241, bit 0) everywhere and never sets it:**
- `UserInfoResponse::readParam` (the `9sQM2XcN` branch: chapter stored at 0x52517A, then 0x52518D): for chapter 12 or 50–53 it does `and byte [esi+0x241], 0xFE` where arm64 sets the flag.
- `TutorialStepScene::updateEvent`'s two tutorial-end paths (0xC95515, 0xC9559C) do the same.
- `checkTouchFooter`'s Town branch (0x80AD4A) tests the bit, then opens scene 0x384 (900) when it is clear.
- All 355 `UserInfo::shared` (0x5A71E0) call sites were scanned for accesses to bytes +0x240–0x243 that follow the call. They are tests, `and 0xFE`/`0xFD` clears, and the `or 2` "started" bit inside the Town branch. Nothing sets bit 0. The other whole-byte writes at +0x241 in the image belong to other classes.

Step 3 above is the client confirmation: a finished player's Town tap opens the town. So tuto13's race can't be reached, and `tuto13.txt` stays unchanged. The live save's copies are all 12/1, which matches no one having seen this chain on Windows.

### The title hang, solved: the proxy's debug console (Session 20, October 23 evening)

**Cause.** Neither the server nor the network. Every proxy build without `OFFLINE_DEPLOY` opens a debug console with `AllocConsole`. That includes Evan's `deploy-vs18` preset and the `libcurl.dll` of every installed package, the live client too. The proxy then prints each redirected `InternetConnect` and `HttpOpenRequest` to that console from the calling thread. The console opens over the title (screen 156,156–1149,675 on this machine). A click that lands in it starts a QuickEdit selection, and its title becomes `Select "…BraveFrontier.Windows.exe"`. While a console is selecting, every write to it blocks until the selection ends. The login is sent from the game's main thread: `CCHttpClient::send` → `WindowsHttpClient::send` (x86 0x3950A0, the game's Windows.Web.Http client) → WinINet `InternetConnectW` → the proxy's `MyInternetConnectW` → `wprintf`. So the main thread stopped inside that `wprintf`. It drew no more frames, sent no login request, and `GumiLiveScene` waited forever.

**Evidence.**
- Reproduced at will (21:55). A tap in the middle of the title hit the console, and the game ignored it. Then the start band: `pushScene(GumiLiveScene)` at 71.9 s, no login on the relay, and the proxy's per-frame `perf (periodic)` lines stopped.
- Frida (read-only) stack of the blocked main thread: a `KERNELBASE` wait ← `write_text_ansi_nolock` … `__stdio_common_vfwprintf` ← `wprintf` ← `MyInternetConnectW` (`main.cpp:107`) ← `Windows.Web.Http.dll`. Symbolised with the Session 18 PDB through DbgHelp. Files: `frida_mainstack_hung1.json`, `frida_threads_hung1.json`.
- The console capture `run/20_07_proxy_console_select.png` shows the selection exactly where the tap landed.
- Releasing the selection (an Escape posted to the console at 22:00:39) let the blocked login out within the same second: `GET /accounts/guest/login/`, `action.php`, `featureCheck.php`.

**Why it looked time-dependent.** Every hung launch had an earlier tap in the middle of the title before the start tap (17:53, 18:08 and 18:17, and the morning launches 3–5). Every working launch had only start-band, Terms-link or Close taps, which all lie below the console. Launch age, server build, idle timeout, relay and the Terms page play no part. Four single late taps this evening (172–186 s after launch, on both `d567b518` and `02b95f90`, with and without Frida) all logged in. The "input ignored" launches below are the same thing: those inputs went into the console.

**Fix (proxy source, uncommitted).**
- `OfflineMod.win32/console_log.cpp` / `.h` queue console lines and write them from a thread of the proxy's own with `WriteConsoleW`. `main.cpp` and `curl_exp.cpp` print through it. A held console now only delays the console. The console's look and content are unchanged, and text can still be selected in it.
- Built with the Session 18 recipe (`out/qa/current/session19/visual/build_proxy_consolefix.sh`): `proxy_29960_consolefix/libcurl.dll` `9633061d…` and the port-9960 twin `proxy_9960_consolefix/libcurl.dll` `bfeaba0b…` (not deployed).
- The 29960 DLL is now in the S19 Fresh package. The Session 18 DLL it replaced (`cfb825da…`) is saved as `package_libcurl_s18_before_consolefix.dll`.
- Proxy unit tests: 677 passed, 0 failed, 5 skipped (they need a live environment). The build cache and `build/bin` were restored.

**Acceptance (22:11).** The console was clicked into `Select` mode on purpose, then the start band was tapped. The login went out (22:11:56) and the tutorial story played (`run/20_13_fix_game_running.png`). Frames kept coming, and the console stayed held (`20_13_fix_console_held.png`). An Escape then flushed the queued lines with none dropped (`20_14_fix_console_released.png`). The control is the same sequence on the Session 18 DLL above: it froze until the console was released.

**Driver fix.** `bf_drive.ps1` `click` and `drag` now refuse a point another window covers, after raising the frame once. The old check only asked whether the frame was the foreground window, so the agent's mid-screen taps went into the console.

**Not this cause:** the tuto8 quest-select lock (Open 2). That launch kept drawing at full rate for 14 minutes after the freeze (`perf (periodic)` at 1,856 frames per 30 s), so its main thread was never blocked.

**Side note.** drogon 1.9.12 replaces each handler's `setCloseConnection(true)` with the request's keep-alive (`HttpServer.cc:793`). So `/dls`, the guest login and `OfflineModController` never close their connections. Harmless here; recorded so nothing relies on those calls.

### The earlier "input ignored" launches (revised)

The proxy log shows the title taps on launches 3, 4 and 5 did register: each pushed `GumiLiveScene` (62 s, 283 s and 859 s after start). Then GumiLive's guest login never completed, and the title stayed unchanged because GumiLiveScene draws nothing. So the input was not ignored; the title hung after the tap, which matches Evan's report of a splash that hangs after the first touch. On launch 6, with the relay in front of the server, the same tap sent `GET /accounts/guest/login/` at 13:03:06 and the login completed normally. The cause of the hung logins is not identified: the server logs no guest login, because the fixture's `extra_log` is off.

### Evidence overwritten by the restart (disclosed)

Restarting the harness server on the fixed candidate (12:31) truncated `saves/s19_first_launch_blank/server.log`. The Session 19 first-launch server lines (MST downloads, Initialize 169/21) are no longer in that file. Their results are recorded above, and the downloaded tables remain in the package LocalState with their write times. The targeted regression run also replaced the ten per-suite logs it ran in `out/qa/current/`; their Session 19 results stay in `session19/regressions/SUMMARY.*`, and `out/qa/current/SUMMARY.*` was put back to the Session 19 full run.

### Launches where injected input is ignored (superseded)

Superseded by "The earlier 'input ignored' launches (revised)" above: the taps did register, and then the login hung. This first reading is kept for the record. On launches 3, 4 and 5 (09:45, 09:53, 12:33), the title ignored every injected input: SendInput clicks, injected-move taps, InjectTouchInput touches, a title-bar activation click and an Escape key. The client sent no request. The clicks land on the game's own CoreWindow, which is responding. UI Automation shows no element over it. Every injected tap that has ever worked came after a physical click by Evan in that same game process (the consent screens at 07:16, the Terms screen at about 08:15). Likely, though not proven: the window needs one real click per launch before it accepts injected input. Not yet ruled out: the title path that skips the Terms screen leaves taps disabled even for a real click.

### Report (template)

```
Step 1  prompt answer: No   time to next screen: immediate (07:33:51)
Step 2  text OK? yes   anything odd: none
Step 3  four starters shown (names / portraits): yes, all four   picked: Selena
Step 4  tutorial battle / summon OK? battle yes; summon not reached   notes: fusion + squad tutorials OK
Step 5  Home OK? yes   locked icons drawn? yes (castle + padlock; Home footer unlock animation)
Step 6  item thumbnails + frames OK? YES (third session, tutorial marked done on the QA save): item box + whole Synthesis list framed, real art
Step 7  Mistral banners OK? yes   Egor Mountains banner: not listed (dungeon 370, needs mission 365)
Step 8  Vortex open? yes, 7 entries, banners OK   New Frontier Gate: reached via Randall > Survey Office; its rift gates are not listed, so neither copy is visible   King of Fighters: not offered anywhere
Step 9  second launch: consent? YES, T&C shown again (Evan passed it)   prompt? no   file check time: ~7 s   reached Home? via the tutorial replay (tuto2, fusion/squad skipped)
Other:  FIXED world-select detour (Ishgria gate); a second tuto8 lock on the quest select remains; T&C disclaimer restyled and moved into the dialog box; Vortex "Server Time" inconsistent between sessions; earlier "ignored" title taps were hung GumiLive logins
```
