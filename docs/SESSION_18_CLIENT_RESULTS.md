# Session 18 client route results

October 22, 2026 — bounded visual route completed on the warm C16 package using the corrected Session 18 server and C8 policy.

## Current result

The clean registered AppX launch reached TitleScene, login, and Home with Selena. The single documented route then completed: Quest → Grand Gaia → Mistral → Adventurer’s Prairie → **Start of Adventure**. Selena and helper Cerulean Knight Stein entered the mission; auto battle won it. The result screens identified Adventurer’s Prairie / “Start of Adventure,” showed 342 Zel, 13 Karma, and 20 XP, and listed five units acquired. There was no visible Next control. Two taps at the bottom-center (window coordinates 601,903 in the 1202×934 capture) advanced the result summary to Units Acquired and then returned to the Mistral map, where Adventurer’s Prairie showed CLEAR.

At login the screen passed through Connecting and Checking files (100%), then showed a Today’s Tasks overlay. Closing that overlay exposed Home; after the required ten-second wait, Home showed Selena. No consent/age gate, runtime error dialog, or visible background-download screen appeared in this clean activation. The log records `background-download prompt: the player's stored answer "false" stands`; this observation does not change the incomplete coverage status. Screenshot sampling cannot rule out a brief screen between captures and does not assess audio.

The client log for the clean route confirms the S18 hooks and startup-coverage path installed (lines 16715–16716), provider generation 1 active with 75,303 owned names and zero collisions declined (16729), TitleScene (16736), the stored-false prompt decision (16908), and HomeScene2 (16959). The route log records provider responses for `layout_home2.csv` (17009), `layout_dungeon_select.csv` (17185 and 18376), `layout_skill_cutin.csv` (17824 onward), `layout_mission_result_top.csv` (18336), and `layout_mission_result_unit.csv` (18355). No `QUEUED` entry for those five layouts appears in this run block. The latest census remains incomplete with six unknown startup lists; this is still a fallback test, not evidence of cacheless startup.

Snapshot `snapshot_c16_s18_route.json` reports 3,152 policy-owned names and 104 other asset files, versus the October 21 final snapshot’s 3,147 and 109. Both snapshots contain the same 3,256 filenames: no new filename appeared. The five count changes are classification changes for already-present files: `layout_dungeon_select.csv`, `layout_home2.csv`, `layout_mission_result_top.csv`, `layout_mission_result_unit.csv`, and `layout_skill_cutin.csv` changed from `owned:false` to `owned:true` under the S18 policy. Their prior on-disk sizes, timestamps, and hashes are unchanged. This count delta is not evidence that the game downloaded five new files.

Current evidence:

- Client log: `out/qa/current/session16/content_client_c16/collected/20261022-150322/offlinemod_content.log`, SHA-256 `48a6675a30eff0de745cbac3ff34492ccbfbce2bbeeae4fe45d4d1831d6993a5` (cumulative log; current clean route block starts at line 16715).
- Route snapshot: `out/qa/current/session16/content_client_c16/snapshot_c16_s18_route.json`, SHA-256 `12ac6d46478f08a3f1f9e4544758fc148e9689ee9ea8b68b23608bfa70b8fa92`.
- Server log: `out/qa/current/session16/content_client_c16/saves/s16_cold/server.log`, SHA-256 `fc4415bd3514e9356c6ec51520fd38ba3e659faaaa04322547452fa350519ed3`.

## Candidate and input checks

All four files matched the Session 18 procedure hashes:

- DLL `out/qa/current/session18/proxy_29960_s18/libcurl.dll`: `cfb825da79de8ca7a68db12009e9a9c66f5181bdb0af38d6a7660d3b44aba545`
- Corrected server `out/qa/current/session18/bin_correction/gimuserverw.exe`: `4eded167851c4b30eb0ae5bf23833a2b08bb6adb873c0d8c35ba2a9ba8e19856`
- C8 policy `out/qa/current/session18/policies/family_policy_c8.json`: `388185d921265ca5197bc8e27671e36037e2d4703484116ec8cd2a7b118583ea`
- Startup manifest `out/qa/current/session18/coverage/content_startup_s18.json`: `f9749ef2d0f16900765ea382ffa4a0e1afd9f535c69e9760f57c47ddc699b975`

The read-only C16 input check returned 19 `OK` lines, including the package resource map, ten LocalState master tables, six response tables, and the binding of those six server tables to `deploy/mst`. It concluded that the client held exactly the files used to derive the manifest. Candidate swap and preflight passed; preflight reported `0 item(s) outstanding` and port 29960 free.

The isolated server was the corrected candidate, PID 17844, on `127.0.0.1:29960`; it reached `READY`, served the disposable 75,303-name provider root, and kept the existing `s16_cold` save. Coverage reported six unknown startup lists, background coverage of 72,189 OK / 5 absent / 9 missing, login coverage of 2,257 OK, and 14 problems. The coverage checker concluded that the DLL builds the census and leaves the background-download prompt to the player. No claim is made that the startup is cacheless or that the remaining fallback gaps are resolved.

## Earlier launch attempts (historical)

The first activation selected the package EXE path directly and produced an AppX runtime error (`msvcp120_app.dll` missing). This was an operator launch-path error, not evidence of a Session 18 candidate defect. A later registered-AppX attempt reached TitleScene and logged provider activity, but displayed sequential Windows System Error dialogs for `vccorlib120_app.DLL` and `msvcr120_app.dll`, owned by `csrss.exe`. That route was stopped before the title was tapped. Those dialogs were not reproducible in the clean activation below; the old log and collection remain preserved at `collected/20261022-140209/offlinemod_content.log` (SHA-256 `e1bf11d7af9603e87f0b575a4932c43fcba0da6e51f7b995799ae50e5b6bfbc5`). The cause of those earlier dialogs was not determined.

## Cleanup and limits

The clean client was closed, the S18 server was stopped, and port 29960 has no listener. The C16 package DLL was restored to Session 17, SHA-256 `45a2c1b18e3ad1da07db735282636bf62f52003c5552e43347ba05ea1a41c1c8`. No BraveFrontier or test-server process remained at cleanup. C16 LocalState and C1 were left untouched; historical logs and snapshots were preserved.

The clean registered activation completed Home/Selena and one mission-10 route to the Mistral map. It does not verify exact duration or absence of brief intervening download screens, audio, complete startup coverage, or behavior on other missions. The missing five asset roots, nine dependent families, and six unknown startup lists remain open fallback work.
