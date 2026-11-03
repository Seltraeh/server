# Session 12: two short client retests

Two runs, both in the **[ContentTest C1 Yes] Brave Frontier** package (the
one used for Session 11's runs A-C). Do not reset, re-register or edit that
package or its settings. Each run seeds a NEW named save from a verified
fixture; your normal save, the installed client and every earlier test save
(`c2_kit`, `leader_after`, `small`, `yes_visual`) are not touched.

Candidates: server **e30e2adf** (`out/qa/current/session12/bin/gimuserverw.exe`,
new: the clearunits repair); proxy DLL **523c77ed** (unchanged, already in the
package); policy `out/qa/current/session11/family_policy_c2.json` (unchanged).
The DLC prompt, background downloads and download screens are unchanged; a
legacy download by itself is not a failure.

Run every command from `C:\Users\Evan\BF\BF-WorkingDirRust` in a normal
PowerShell. Close any older harness server (Ctrl+C) and any running test
client first. Leave the server terminal running during a run; stop it with
Ctrl+C before starting the next. If `server` prints NOT READY or FAILED, stop
and send its output.

Optional pre-flight (read-only; it should end `0 item(s) outstanding`):

```powershell
python scripts\content_provider_client_test.py --identity c1yes check --exe out/qa/current/session12/bin/gimuserverw.exe --policy out/qa/current/session11/family_policy_c2.json
```

## Not requested this time, and why

- **Leader A/B (Berdette).** Both the Training party screen and the battle
  take the helper's leader skill from the unit's master data, never from the
  stored value, so the two saves cannot differ in text or in combat (traced in
  both client binaries; [CLAUDE_SESSION_12_HANDOFF.md](CLAUDE_SESSION_12_HANDOFF.md) §3).
  The Session 11 comparison is withdrawn, not failed.
- **Touka preview.** Session 10 already showed it; its seven owned files are
  identical under C1 and C2, and the seven files it downloaded were effects,
  which stay legacy. Nothing changed that it could test.
- **Performance, prompt branches, restarts, crafting, Vargas.** Done or
  deferred; nothing changed.

## Automated evidence (already passed) versus what only you can confirm

| Item | Automated (no client) | Needs your eyes |
|---|---|---|
| Small roster | Real `clearunits` command, then the UserInfo payload: every squad slot names an owned unit, one leader per squad, the active squad led by Selena; no dangling rows; repeat / empty / two-user cases (24 checks) | Home panes and Manage Squad open without the Session 11 crash |
| Archive thumbnail | The tile the Archive draws is `unit_ills_thum_<id>.png` (not `_100`); the harness's own swap / restore / park / restore against the real broker, with the real provider and hook code (over fake game functions) releasing exactly the drawn texture's key at the next scene change and serving the new bytes (proxy 340 checks, driver 10) | The swapped picture actually appears; what a parked thumbnail looks like |

## Run 1: repaired small roster

Fixture `out/qa/current/session12/fixtures/small_roster_repaired.sqlite`
(SHA-256 `402b2fb0…`; manifest beside it): made by the repaired `clearunits`
from the Session 11 crafting kit. One unit, **Selena** (unit 20011, user unit
1000), leader of all ten squads, squad 1 active; the three spheres the deleted
units wore are back in the item box (24 of 100 rows used). The broken
`small` save and its seed are kept unchanged as evidence.

```powershell
python scripts\content_provider_client_test.py --identity c1yes server --exe out/qa/current/session12/bin/gimuserverw.exe --policy out/qa/current/session11/family_policy_c2.json --save small_repaired --seed out/qa/current/session12/fixtures/small_roster_repaired.sqlite
```

The line before READY must say `(seeded from ...small_roster_repaired.sqlite)`.
If it says `(kept from earlier runs)`, stop: that save already existed.

Wait for READY on 29960, launch **[ContentTest C1 Yes] Brave Frontier**, then:

1. Log in. **Home:** the squad shows Selena alone. Tap the first unit pane,
   then the second (both crashed in Session 11).
2. **Units -> Manage Squad:** opens; squad 1 shows Selena as leader. Back.
3. **Units:** the list holds one unit, Selena.
4. Start any quest you can enter with this squad; finish it or give up.

Expected: no crash on any of these screens, no missing art. A crash: note the
screen and continue with step 5 (the crash dump is read from
`%LOCALAPPDATA%\CrashDumps`).

5. Close the game, Ctrl+C in the server terminal, then:

```powershell
python scripts\content_provider_client_test.py --identity c1yes collect
```

## Run 2: Archive thumbnail replacement and recovery (Blaze, No. 8318)

Fixture `out/qa/current/session11/fixtures/crafting_kit.sqlite` (SHA-256
`e0efedf8…`, the seed of Session 11's `c2_kit`): its Archive holds **Tidelash
Stormsurge Galea** (No. 8295, unit 820278) and **Immortal Firewing Blaze**
(No. 8318, unit 810048). The target is the file the Archive tile actually
draws, `unit_ills_thum_810048.png`; Session 11 changed `..._100.png`, which the
Archive only asks for and never draws.

```powershell
python scripts\content_provider_client_test.py --identity c1yes server --exe out/qa/current/session12/bin/gimuserverw.exe --policy out/qa/current/session11/family_policy_c2.json --disposable --save archive_swap --seed out/qa/current/session11/fixtures/crafting_kit.sqlite
```

It should print `disposable root ...: kept from earlier runs (0 change(s) in
place)` and `(seeded from ...crafting_kit.sqlite)`. Wait for READY, launch
**[ContentTest C1 Yes] Brave Frontier**, then Menu -> **Unit Archive**: find
No. 8295 (Galea) and No. 8318 (Blaze) and note both thumbnails.

Each `swap`, `restore` and `park` below waits until the broker has published
the change and prints `published by the broker after N s` (about 15-35 s for
a swap or restore, a few seconds for a park). Change screens only after that
line. "Leave and come back" means: leave the Archive (for example to Menu),
then open Unit Archive again.

1. In a second PowerShell (same folder):

   ```powershell
   python scripts\content_provider_client_test.py --identity c1yes snapshot s12_before --policy out/qa/current/session11/family_policy_c2.json
   python scripts\content_provider_client_test.py --identity c1yes swap unit_ills_thum_810048.png --like unit_ills_thum_820278.png
   ```

   Leave and come back. **Expected: No. 8318's thumbnail now shows Galea**,
   the same picture as No. 8295. (While you stay in the Archive nothing
   changes; the switch happens at the screen change.) Only the small
   thumbnail file is swapped: Blaze's large illustration on his own page is
   unchanged, and anywhere else that shows his small thumbnail shows Galea's
   until the restore.

2. ```powershell
   python scripts\content_provider_client_test.py --identity c1yes restore
   ```

   Leave and come back. **Expected: No. 8318 shows Blaze again.**

3. ```powershell
   python scripts\content_provider_client_test.py --identity c1yes park unit_ills_thum_810048.png
   ```

   Leave and come back. **Record what happens**: a Downloading screen (and
   its count), what No. 8318's tile shows (blank, Blaze, something else), a
   hang or a crash. This is not yet observed on a client. The static
   prediction: the provider withdraws all eight of Blaze's files, the Archive
   asks the game to download those it needs, and the tile then draws blank
   (the game's empty 2x2 texture), because the provider keeps refusing the
   withdrawn name. If the client hangs or crashes, run the step 4 `restore`
   before relaunching it.

4. ```powershell
   python scripts\content_provider_client_test.py --identity c1yes restore
   python scripts\content_provider_client_test.py --identity c1yes disposable
   python scripts\content_provider_client_test.py --identity c1yes snapshot s12_after --policy out/qa/current/session11/family_policy_c2.json
   ```

   Leave and come back. **Expected: Blaze's thumbnail is back without
   restarting anything.** `disposable` must end with `0 differ` and report
   changes `none`. The park may have written Blaze's files into the test
   package through the game's own download; the snapshots record that, it is
   expected and nothing reads them while the provider serves Blaze.

5. Close the game, Ctrl+C in the server terminal, then:

   ```powershell
   python scripts\content_provider_client_test.py --identity c1yes collect
   ```

## What to send

For each run: any crash (and the screen), any Downloading screen and its
count, and for Run 2 what No. 8318 showed after each of the four steps. The
logs need no further action: `collect` copies them into
`out/qa/current/content_client_c1_yes`, and now also keeps every collect under
`out/qa/current/content_client_c1_yes/collected/<date-time>/`, so the second
run's collect no longer replaces the first's.

## Recovery

- A run interrupted while Blaze is swapped or parked (terminal closed, PC
  restarted): start the Run 2 server command again (the disposable root and
  its changes survive restarts), then run the `restore` command from step 4.
- A crash in Run 1: close the game, Ctrl+C, `collect`; nothing else is
  needed. The `small_repaired` save stays as it is for diagnosis.
- Nothing here writes the normal save, the installed client, the real
  `deploy/game_content` files or the other test packages.
