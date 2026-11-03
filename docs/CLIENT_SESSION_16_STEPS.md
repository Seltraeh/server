# Client steps, Session 16 — a cold package against the startup coverage contract: the fallback run (~60 minutes)

**Completed October 21:** Codex exercised this bounded native route after the user
accepted consent. Read `SESSION_16_CLIENT_RESULTS.md` and `QC_SESSION_17_REVIEW.md`.
Do not repeat setup, swap or controls: C16 now has 3,147 owned cached assets from
the intentional final provider-off run. It is not a cold baseline. Game/harness
are stopped and the parked file is restored. Instructions below are the run record.

**Latest status, October 21: Session 17 independent QC passed. R1-R3 are complete:** the
registered C16 layout now holds `45a2c1b1…`, its cache snapshot is empty and preflight has
zero outstanding items. Resume at **Part A**, not setup or the DLL swap. The user must
handle the first-launch consent screen, then verify the installed-hook log before gameplay.
See [QC_SESSION_17_REVIEW.md](QC_SESSION_17_REVIEW.md). Earlier notices below are historical.

**October 21 native attempt: paused pending Session 17 hook-installation repair.** C16
registration, loopback exemption, state copy and zero-asset baseline are already complete.
Do not repeat/reset that setup. The game and harness were stopped before login. Read
[SESSION_16_CLIENT_RESULTS.md](SESSION_16_CLIENT_RESULTS.md) before resuming. The user
must handle the first-launch legal-age/parental-consent screen.

**October 21 Codex QC: cleared for this isolated fallback visual run.** See
[QC_SESSION_16_FINAL_REVIEW.md](QC_SESSION_16_FINAL_REVIEW.md) for independently rerun
checks and remaining limits. This is not clearance for deployment or startup-screen removal.

**October 21, later (Claude, Session 17): the hook-installation defect is repaired in a new
DLL, `out\qa\current\session17\proxy_29960_s17\libcurl.dll` `45a2c1b1…`, automated-tested and
returned for Codex QC ([CLAUDE_SESSION_17_HANDOFF.md](CLAUDE_SESSION_17_HANDOFF.md)). It is staged,
not swapped: the C16 layout still holds `8634857a…`, whose startup path declined.** After QC,
resume with "Resume after Session 17" below instead of step 0, then Parts A-E.

Nothing here
touches your normal server, the installed client, your save or the C1 test package
(C1 stays the accepted fallback; do not launch it during this run).

What this run is for. **It tests the fallback, not the removal of any startup screen.**
The Session 16 DLL may leave out the client's LocalState census and answer the
background-download prompt itself only when the server's report proves that every
file the startup needs is available locally, and only once the DLL's content provider
is active on the same content generation. With today's content the report says
**incomplete** -- sixteen of the client's startup lists cannot yet be derived exactly,
67 required files are not in the server's content, and 26 families need a file that
exists nowhere (Home's layout is one of them) -- so in this run the DLL leaves both to
the game, and you will see the prompt. What the run measures: how few downloads remain
on a brand-new package with the C7 content, that the C7 families look right, and that
a missing file and the server's content switched off both fall back to the game's own
behaviour. "Cold" means no cached image or sound: the package gets a copy of your
installed client's account state and master tables (step 0), so this is not a test of
a completely fresh installation.

What needs your eyes: at every step, **count each download or verification screen**
(the login check's progress, any "downloading" screen between scenes) and note roughly
how long it stays; and tell me anything that looks wrong (blank images, wrong art,
silence, a crash).

| | |
|---|---|
| Package | **[ContentTest C16] Brave Frontier** (`gumi.BraveFrontier.ContentTestC16_99p3jr0gh0z6w`), registered October 21 and launched once, to the consent screen (no login); port 29960 only |
| Its folder | `out\qa\current\session16\content_client_c16\package` (registered, loopback-exempt, account state copied) |
| DLL for this run | `out\qa\current\session17\proxy_29960_s17\libcurl.dll` `45a2c1b1…` (Session 15 + post-quest prompts + the Session 16 startup decisions, with the Session 17 attach-order fix), swapped in at step R1. The layout's current `8634857a…` is SUPERSEDED: its startup path declined before login |
| Server | `out\qa\current\session16\bin\gimuserverw.exe` `b2552da2…` |
| Content policy | `out\qa\current\session16\policies\family_policy_c7.json` `04995161…` (75,286 files) |
| Startup manifest | `out\qa\current\session16\coverage\content_startup.json` `3be2dae4…` |
| Save | new name `s16_cold`, seeded on the first start from `out\qa\current\session16\fixtures\s16_cold_kit.sqlite` `554a80b7…`: level 22, Selena alone, 27 of 27 energy, 50 gems, 8 friends; checked by `scripts/check_client_fixture.py` (squads, helpers, the first quest, a restart) |

All commands run in PowerShell in `C:\Users\Evan\BF\BF-WorkingDirRust`.
**Terminal A** runs the server and stays open; **Terminal B** is for everything else.

## 0. Prepare the new package (once, ~10 minutes)

Close every Brave Frontier window and stop any harness with Ctrl+C (all test packages
share port 29960). Your normal server may keep running.

Register it (normal PowerShell):

```
powershell -NoProfile -Command "Add-AppxPackage -Register 'C:\Users\Evan\BF\BF-WorkingDirRust\out\qa\current\session16\content_client_c16\package\AppxManifest.xml'"
```

Allow it to reach the local server (**Administrator** PowerShell):

```
CheckNetIsolation.exe LoopbackExempt -a '-n=gumi.BraveFrontier.ContentTestC16_99p3jr0gh0z6w'
```

Back in Terminal B, before launching anything, copy your account state (no images or
sounds; the same as C1 had):

```
python scripts\content_provider_client_test.py --identity c16 state
```

Expect `copied … client state file(s) (no assets)`. (If it says the package folder
does not exist, do **not** launch the game to create it -- stop and report; the C1 Yes
package was prepared the same way without a launch.) Then take the empty baseline
(about a minute):

```
python scripts\content_provider_client_test.py --identity c16 snapshot c16_prelaunch --policy out\qa\current\session16\policies\family_policy_c7.json --all
```

Expect `0 owned name(s) present in the test LocalState; 0 other asset file(s)`. Pre-flight:

```
python scripts\content_provider_client_test.py --identity c16 check --exe out\qa\current\session16\bin\gimuserverw.exe --policy out\qa\current\session16\policies\family_policy_c7.json --manifest out\qa\current\session16\coverage\content_startup.json
```

Expect `0 item(s) outstanding` (step 0 was completed on October 21 with the Session 16 DLL; do not
repeat it).

## Resume after Session 17 (instead of step 0, ~5 minutes)

Close every Brave Frontier window; the harness server must not be running (all test packages
share port 29960). Do not register again, reset the package or copy state again. Terminal B:

R1. Swap in the Session 17 DLL (no re-registration; LocalState untouched):

```
python scripts\content_provider_client_test.py --identity c16 dll --file out\qa\current\session17\proxy_29960_s17\libcurl.dll
```

Expect `…\package\libcurl.dll: now Session 17 candidate (post-quest + strict startup coverage decisions, startup sites verified around the provider's attach), isolated port; QC pending (sha256 45a2c1b18e3a…, port 29960)`
and `replaced: SUPERSEDED Session 16 corrected build … kept as …\dll_history\8634857a….dll`.

R2. Record what the October 21 launch (consent screen, no login) left in LocalState:

```
python scripts\content_provider_client_test.py --identity c16 snapshot c16_after_attempt1 --policy out\qa\current\session16\policies\family_policy_c7.json --all
```

Expect `0 owned name(s) present in the test LocalState`; note any other asset file it lists.

R3. Pre-flight:

```
python scripts\content_provider_client_test.py --identity c16 check --exe out\qa\current\session16\bin\gimuserverw.exe --policy out\qa\current\session16\policies\family_policy_c7.json --manifest out\qa\current\session16\coverage\content_startup.json
```

Expect `layout libcurl.dll: Session 17 candidate (post-quest + strict startup coverage decisions, startup sites verified around the provider's attach), isolated port; QC pending; port 29960`
and `0 item(s) outstanding`. Then Part A. The server start there prints `kept from earlier
runs` for `s16_cold` (seeded on October 21) instead of `seeded from`.

**The first check after A1:** run `python scripts\content_provider_client_test.py --identity c16 collect`
once the title shows, and look for `startup coverage path installed for build
6253F827-01756000-00ED273D` in the collected `offlinemod_content.log`. If it says `startup
coverage path DECLINED` instead, stop: the run tests nothing of the startup path (report the
line).

## Part A — first launch to Home (~15 minutes)

Terminal A (wait for `READY`; the first start can take a few minutes):

```
python scripts\content_provider_client_test.py --identity c16 server --exe out\qa\current\session16\bin\gimuserverw.exe --policy out\qa\current\session16\policies\family_policy_c7.json --manifest out\qa\current\session16\coverage\content_startup.json --disposable --save s16_cold --seed out\qa\current\session16\fixtures\s16_cold_kit.sqlite --timeout 420
```

It prints `startup manifest … installed`, `persistent save … (seeded from …s16_cold_kit.sqlite)`
and then `READY`. Terminal B:

```
python scripts\content_provider_client_test.py --identity c16 coverage --expect incomplete
```

Expect the first line
`startup coverage for build 6253F827-01756000-00ED273D on 127.0.0.1:29960: incomplete -- 16 startup list(s) the manifest could not derive`,
then `the report itself says complete 0; epoch …, generation 1, …, build match 1` (the epoch
and the policy digest differ on every start), the two set lines
`set background: required 72282, ok 72251, absent 5, unowned 0, missing 26, ambiguous 0, unavailable 0`
and `set login: required 2418, ok 2355, absent 62, unowned 0, missing 1, ambiguous 0, unavailable 0`,
16 `unknown startup list:` lines, 94 `problem:` lines -- among them
`problem: login missing layout_skill_cutin.csv` and `problem: background missing layout_home2.csv` --
and last `-> a Session 16 DLL builds the census and leaves the background-download prompt to the player`.
(The harness server serves a copy holding only the policy's files, so the 26 families the policy
does not own read `missing` here; on the full content tree the same names read `unowned`.) The
whole expected text is part `a` of `out\qa\current\session16\coverage_cli_expected.txt`. Then
launch **[ContentTest C16] Brave Frontier** from Start.

| # | Do | Expected |
|---|---|---|
| A0 | First launch only: the terms/privacy and legal-age/parental-consent screen. **The player** ticks the checkbox and agrees; automation must not. | The game continues to the title. |
| A1 | Title screen, then tap to start. | The usual title. |
| A2 | Watch every screen until the prompt. | The login check runs (first launch of a new package). Downloads may follow because coverage is incomplete. Record their count and approximate duration; the logs, rather than a promised duration, determine whether the requested names were expected. |
| A3 | The background-download Yes/No prompt appears: choose **No**. | It appears because the report is incomplete. (If it does NOT appear, note that and go on: with this content that would be a defect.) |
| A4 | Any login notices or rewards as usual, then Home. | Home with Selena in your squad. A short download screen on entering Home is possible (Home's layout family is incomplete). |

Wait about ten seconds at Home. Terminal B:

```
python scripts\content_provider_client_test.py --identity c16 collect
python scripts\content_provider_client_test.py --identity c16 snapshot c16_home --policy out\qa\current\session16\policies\family_policy_c7.json --all
```

## Part B — the first quest (~10 minutes)

| # | Do | Expected |
|---|---|---|
| B1 | Quest → Mistral → Adventurer's Prairie. | The quest list; a short download screen when it opens is possible (its layout family is incomplete). |
| B2 | Choose **Start of Adventure** (3 energy), any helper, start. | The battle loads; count any download screen before it starts. |
| B3 | Play to the end and win. | Units, enemies, effects and sounds as usual; no blank images. |
| B4 | Go through the results back to the quest area. | Stay in the mission area so the next stage can be selected; an unexpected return Home is a regression. No Windows Store rating or Facebook gem prompt; count any download screen on the result screens. |

Terminal B: `python scripts\content_provider_client_test.py --identity c16 collect`

## Part C — relaunch (~5 minutes)

| # | Do | Expected |
|---|---|---|
| C1 | Close the game from Home normally; launch **[ContentTest C16]** again. | Title, a shorter login, **no** background-download prompt (your No is stored), Home. Count download screens. |
| C2 | Open Quest → Mistral → Adventurer's Prairie (no need to play); back to Home. | Count download screens (compare with B1). |

Close the game. Terminal B: `python scripts\content_provider_client_test.py --identity c16 collect`

Before Part D, also check persistence across a server restart: with the game closed,
Ctrl+C in Terminal A, rerun the **exact Part A server command** (same `--save s16_cold`),
wait for READY and relaunch C16. Confirm the squad and completed quest progress remain.
Do not reseed or reset anything. Close the game, then in Terminal B:

```
python scripts\content_provider_client_test.py --identity c16 collect
python scripts\content_provider_client_test.py --identity c16 snapshot c16_before_controls --policy out\qa\current\session16\policies\family_policy_c7.json --all
```

This snapshot should still show zero provider-owned assets in LocalState. It separates
the normal-route result from Parts D/E, which intentionally allow legacy downloads.

## Part D — one startup file missing (~5 minutes)

Terminal B, game closed:

```
python scripts\content_provider_client_test.py --identity c16 park action_attack1.png
```

Wait for `published by the broker after … s`. Then:

```
python scripts\content_provider_client_test.py --identity c16 coverage --expect incomplete
```

Expect the same first line as in Part A (`… incomplete -- 16 startup list(s) the manifest could
not derive`), `generation 2`, the login line now
`set login: required 2418, ok 2354, absent 62, unowned 0, missing 1, ambiguous 0, unavailable 1`,
and one more line than in Part A: `problem: login unavailable action_attack1.png` (part `d` of the
expected text).

| # | Do | Expected |
|---|---|---|
| D1 | Launch **[ContentTest C16]**; reach Home. | Possibly a short download screen during login: the game fetches that one file itself. Everything else as in C1; no crash. |

Close the game. Terminal B:

```
python scripts\content_provider_client_test.py --identity c16 restore action_attack1.png
python scripts\content_provider_client_test.py --identity c16 collect
```

Wait for `published by the broker after … s: 1 requested name(s) restored` (about one
to two minutes).

## Part E — the server's content switched off (~10 minutes; always last)

This one lets the game download everything itself, so the package is no longer cold
afterwards. Ctrl+C in Terminal A, then in Terminal A:

```
python scripts\content_provider_client_test.py --identity c16 server --exe out\qa\current\session16\bin\gimuserverw.exe --policy out\qa\current\session16\policies\family_policy_c7.json --manifest out\qa\current\session16\coverage\content_startup.json --disposable --save s16_cold --seed out\qa\current\session16\fixtures\s16_cold_kit.sqlite --timeout 420 --broker off
```

It says `kept from earlier runs` for the save, then `READY … (broker off)`. Terminal B:
`python scripts\content_provider_client_test.py --identity c16 coverage --expect incomplete`
(expect `startup coverage for build 6253F827-01756000-00ED273D on 127.0.0.1:29960: incomplete -- broker disabled`,
no set lines and the 16 `unknown startup list:` lines; part `e`).

| # | Do | Expected |
|---|---|---|
| E1 | Launch **[ContentTest C16]**; reach Home; open Quest → Mistral once. | The original behaviour: long download screens at login and on entering Home. Count them and note roughly how long. No prompt (your No is stored). |

Close the game; Ctrl+C in Terminal A. Terminal B:

```
python scripts\content_provider_client_test.py --identity c16 collect
python scripts\content_provider_client_test.py --identity c16 snapshot c16_after --policy out\qa\current\session16\policies\family_policy_c7.json --all
```

Leave the C16 package installed for QC.

## If something goes wrong

- A crash or hang: stop, run `collect`, keep `%LOCALAPPDATA%\CrashDumps`, and stop the
  server with Ctrl+C.
- "Refreshing the latest data files. Application will close now": collect logs and note
  the step, then relaunch once. Its cause is unconfirmed; do not assume it is unrelated
  to this candidate. Stop and report if it repeats.
- If Part D's park is still in place: `python scripts\content_provider_client_test.py --identity c16 restore`.
- To go back to the accepted setup, simply use **[ContentTest C1]** with its Session 15
  commands; nothing in this run changed it.
- To remove the C16 package after QC has the evidence (not before):
  `powershell -NoProfile -Command "Get-AppxPackage gumi.BraveFrontier.ContentTestC16 | Remove-AppxPackage"`
  and, in Administrator PowerShell,
  `CheckNetIsolation.exe LoopbackExempt -d '-n=gumi.BraveFrontier.ContentTestC16_99p3jr0gh0z6w'`.

## What I check afterwards from your files

- Provider log (`out\qa\current\session16\content_client_c16\collected\…`), each
  launch: `content provider hooks installed …` and `startup coverage path installed for
  build 6253F827-01756000-00ED273D: … Its 9 sites matched the untouched image before the
  content provider attached and again after it, the downloaded-name lookup (0xE417E0)
  carrying exactly the provider's own detour`. A `startup coverage path DECLINED` line means
  the startup hooks were not installed and the game ran stock for that reason: that is not
  the incomplete-coverage fallback this run tests (stop and report it). The fallback is the
  `built` / `left to the player` lines below.
- Parts A–D, every launch: `LocalState census #… built [caller +…]: … name(s) in … ms
  (startup coverage incomplete: 16 startup list(s) the manifest could not derive)` -- or, for
  a census the game runs before the provider has adopted its index, `… (the provider has not
  adopted an index yet (starting, or its start failed))`; both are the game's own census.
  Part E: `… (the provider is dormant (broker switched off))`. Part A: `background-download
  prompt left to the player: startup coverage incomplete: 16 startup list(s) the manifest could
  not derive` (or `… left to the player: the content provider is not active`); Parts C–E:
  `background-download prompt: the player's stored answer "false" stands`. Never `LEFT OUT`
  with this content: a single `LEFT OUT` line is a defect to report.
- `decisions (…)` per scene and the `QUEUED` names: in Parts A–C only names the C7 policy
  does not own are expected -- the 26 unresolved families, the 67 absent names, client state
  such as the text-dictionary ZIPs (counted apart), and whatever the 16 unknown lists ask for
  that no source holds; each such name is evidence for the handoff's §4. Part D adds
  `action_attack1.png`; Part E queues thousands (the comparison).
- Snapshots: no owned file in the package's LocalState before Part D, one after it
  (`action_attack1.png`, fetched by the game in D1); every other asset file listed is
  something the game downloaded itself.
- The server log `out\qa\current\session16\content_client_c16\saves\s16_cold\server.log`:
  the `/content/` requests of each part, and the quest's MissionStart / MissionEnd for
  mission 10.
