# Client steps, Session 18 — the Session 18 candidate on the warm C16 package (optional, ~30 minutes)

**Correction (QC, October 22):** the server for this run is the corrected candidate
`out\qa\current\session18\bin_correction\gimuserverw.exe`, which also binds the manifest to the
response tables the server serves ([CLAUDE_SESSION_18_HANDOFF.md §10](CLAUDE_SESSION_18_HANDOFF.md)).
The DLL, the C8 policy and the manifest are unchanged, and so is everything this run expects to see.
The pre-correction server `bin\gimuserverw.exe` (`995a088d…`) is kept for reference only.

October 22, 2026. **Nothing here is required to accept Session 18's automated work**, and none of it
repeats the Session 16/17 controls (restarts, the missing file, the server's content switched off):
those are done ([SESSION_16_CLIENT_RESULTS.md](SESSION_16_CLIENT_RESULTS.md)). Run it only when QC
asks for a client observation of the Session 18 candidate. It still tests the **fallback**: the
startup report stays **incomplete** (6 startup lists still cannot be derived, 5 required files exist
nowhere, 9 families need such a file), so the DLL leaves the LocalState census and the
background-download prompt to the game. Nothing here removes a startup screen.

What it measures: the login/Home route under the C8 policy. On the Session 16/17 runs the game
itself downloaded `layout_home2.csv`, `layout_dungeon_select.csv`, the two mission-result layouts,
`layout_skill_cutin.csv` and 17 more layouts, because C7 did not own them. C8 owns all 22, so on
this run the provider should serve them instead (`served …` lines, no `QUEUED` lines for them).

Not touched: your normal server, the installed client, your save, the C1 package (`09056ee5…`
stays), C16's consent and registration. C16 is **warm** (3,147 owned copies from the October 21
provider-off control), so snapshot counts are compared with the October 21 final snapshot, not with
zero.

| | |
|---|---|
| Package | **[ContentTest C16] Brave Frontier** (`gumi.BraveFrontier.ContentTestC16_99p3jr0gh0z6w`), consent already accepted by the player; port 29960 only |
| Its folder | `out\qa\current\session16\content_client_c16\package` |
| DLL now | `45a2c1b1…` (Session 17, QC-verified) |
| DLL for this run | `out\qa\current\session18\proxy_29960_s18\libcurl.dll` `cfb825da79de8ca7a68db12009e9a9c66f5181bdb0af38d6a7660d3b44aba545` |
| Server | `out\qa\current\session18\bin_correction\gimuserverw.exe` `4eded167851c4b30eb0ae5bf23833a2b08bb6adb873c0d8c35ba2a9ba8e19856` |
| Content policy | `out\qa\current\session18\policies\family_policy_c8.json` `388185d921265ca5197bc8e27671e36037e2d4703484116ec8cd2a7b118583ea` |
| Startup manifest | `out\qa\current\session18\coverage\content_startup_s18.json` `f9749ef2d0f16900765ea382ffa4a0e1afd9f535c69e9760f57c47ddc699b975` |
| Save | `s16_cold` (kept from the October 21 runs) |

All commands run in PowerShell in `C:\Users\Evan\BF\BF-WorkingDirRust`. **Terminal A** runs the
server; **Terminal B** is for everything else. Close every Brave Frontier window and stop any harness
(all test packages share port 29960) before S1.

## S1. Verify the files (read only)

```
python -c "import hashlib,sys;[print(hashlib.sha256(open(p,'rb').read()).hexdigest(),p) for p in sys.argv[1:]]" out\qa\current\session18\proxy_29960_s18\libcurl.dll out\qa\current\session18\bin_correction\gimuserverw.exe out\qa\current\session18\policies\family_policy_c8.json out\qa\current\session18\coverage\content_startup_s18.json
```

Each hash must equal the table above.

## S2. Does C16 hold the manifest's inputs? (read only)

```
python scripts\check_client_inputs.py --manifest out\qa\current\session18\coverage\content_startup_s18.json --localstate "$env:LOCALAPPDATA\Packages\gumi.BraveFrontier.ContentTestC16_99p3jr0gh0z6w\LocalState" --package out\qa\current\session16\content_client_c16\package
```

Expect 19 `OK` lines (the client build, the package's resourceMap, 10 master tables, 6 server
tables, and `server tables: bound to the 6 response tables in …\deploy\mst`) and `this client holds
exactly the files the manifest was derived from`. Anything else: stop and report it (the candidate
would keep the census even with a complete report).

## S3. Swap in the Session 18 DLL (LocalState untouched)

```
python scripts\content_provider_client_test.py --identity c16 dll --file out\qa\current\session18\proxy_29960_s18\libcurl.dll
```

Expect `…\package\libcurl.dll: now Session 18 candidate (Session 17 + coverage bound to the client's
own master tables), isolated port; QC pending (sha256 cfb825da79de…, port 29960)` and the Session 17
DLL kept under `dll_history`.

## S4. Pre-flight

```
python scripts\content_provider_client_test.py --identity c16 check --exe out\qa\current\session18\bin_correction\gimuserverw.exe --policy out\qa\current\session18\policies\family_policy_c8.json --manifest out\qa\current\session18\coverage\content_startup_s18.json
```

Expect the layout DLL line naming the Session 18 candidate and `0 item(s) outstanding`.

## S5. Server (Terminal A; wait for `READY`)

```
python scripts\content_provider_client_test.py --identity c16 server --exe out\qa\current\session18\bin_correction\gimuserverw.exe --policy out\qa\current\session18\policies\family_policy_c8.json --manifest out\qa\current\session18\coverage\content_startup_s18.json --disposable --save s16_cold --timeout 420
```

It prints `startup manifest … installed`, `persistent save … kept from earlier runs`, then `READY`.
Terminal B:

```
python scripts\content_provider_client_test.py --identity c16 coverage --expect incomplete
```

Expect `startup coverage for build 6253F827-01756000-00ED273D on 127.0.0.1:29960: incomplete -- 6
startup list(s) the manifest could not derive`, then
`set background: required 72203, ok 72189, absent 5, unowned 0, missing 9, ambiguous 0, unavailable 0`
and `set login: required 2257, ok 2257, absent 0, unowned 0, missing 0, ambiguous 0, unavailable 0`
(the harness serves a copy holding only the policy's files, so the 9 roots C8 leaves unowned read
`missing` there), 6 `unknown startup list:` lines, 14 `problem:` lines, 11 `client file the manifest
was derived from:` lines, and last `-> the DLL builds the census and leaves the background-download
prompt to the player`.

## S6. The route (the same as October 21, once)

| # | Do | Expected |
|---|---|---|
| R1 | Launch **[ContentTest C16]**; title, tap to start. | No consent screen (already accepted). The login check; no background-download prompt (the stored answer "No" stands). |
| R2 | Home. Wait ten seconds. | Home with Selena. Count any download screen. |
| R3 | Quest → Mistral → Adventurer's Prairie → Start of Adventure, any helper; win. | As on October 21. |
| R4 | Through the results (tap the bottom-center) back to the quest area. | As on October 21: no Next control outside repeat mode is the stock screen (`out\qa\current\session18\check_result_cue.py`). |

Then Terminal B:

```
python scripts\content_provider_client_test.py --identity c16 collect
python scripts\content_provider_client_test.py --identity c16 snapshot c16_s18_route --policy out\qa\current\session18\policies\family_policy_c8.json --all
```

Stop the server with Ctrl+C in Terminal A.

## What I check from your files

- `offlinemod_content.log` (collected): `startup coverage path installed for build
  6253F827-01756000-00ED273D: … carrying exactly the provider's own detour` (a `DECLINED` line: stop,
  report); `LocalState census #… built [caller +…]: … (startup coverage incomplete: 6 startup list(s)
  the manifest could not derive)`; `background-download prompt: the player's stored answer "false"
  stands`; never `LEFT OUT`.
- The route improvement: no `addLoadFilePriority layout_home2.csv -> QUEUED`,
  `layout_dungeon_select.csv`, `layout_mission_result_top.csv` / `_unit.csv` or `layout_skill_cutin.csv`
  line; `served layout_home2.csv <- _dlcbundle/layout_home2.csv` instead (or `present (provider)`).
  Expected `QUEUED` names: client state (text-dictionary ZIPs) and, if the route reaches them, the 9
  roots C8 leaves unowned (`effect_prt_1011_white.plist`, `effect_prt_021.plist`, the two
  `…BattleEffect…R.sam` and five layouts).
- Snapshot `c16_s18_route` against the October 21 final snapshot (3,147 owned / 109 other): new
  owned copies are files the game downloaded itself on this route (each one is evidence).

## Rollback

```
python scripts\content_provider_client_test.py --identity c16 dll --file out\qa\current\session17\proxy_29960_s17\libcurl.dll
```

Expect `now Session 17 build … (sha256 45a2c1b18e3a…)`. The Session 16/17 server, C7 policy and
manifest commands of [CLIENT_SESSION_16_STEPS.md](CLIENT_SESSION_16_STEPS.md) work as before.
C1 is untouched by everything here.

## If something goes wrong

- A crash or hang: `collect`, keep `%LOCALAPPDATA%\CrashDumps`, stop the server.
- "Refreshing the latest data files. Application will close now": collect and stop; do not relaunch
  in a loop. Report it.
- S2 not all `OK`: do not run S3-S6.
