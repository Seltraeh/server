# Session 11: the smallest client checks left

October 6 player results supersede the expectations below: Vargas and crafting
passed. Pause A3 (Archive thumbnail replacement), A4/B (leader comparison), and
C (small roster). See SESSION_11_CLIENT_RESULTS.md. The small fixture contains
deleted-unit references and must be repaired before another client run. Do not
repeat its crashes. The remaining Run D command is now written in full below.

Four short runs. Nothing here repeats the C1 prompt branches or restart
sequences: those are done. Use the existing test packages; do not reset them,
re-register them or edit their settings. Every save below is a COPY made for
the run; your normal save and the Session 10 test saves are not touched.

Candidates (unchanged since Session 10): server **270832d6**, C1 proxy DLL
**523c77ed** in both C1 packages. New is only the policy the harness serves:
`out/qa/current/session11/family_policy_c2.json` (17,445 names; C1 was 16,639).
Background downloads and the DLC prompt are untouched; a remaining legacy
download is expected and is not a failure by itself.

Run every command from `C:\Users\Evan\BF\BF-WorkingDirRust` in a normal
PowerShell. Leave the server terminal running during each run; stop it with
Ctrl+C before starting the next. Close any older harness or test client first.
If `server` prints NOT READY or FAILED, stop and send the output.

## Run A: C2 art, crafting kit, replacement and recovery ([ContentTest C1 Yes])

```powershell
python scripts\content_provider_client_test.py --identity c1yes server --exe out/qa/current/session10/bin/gimuserverw.exe --policy out/qa/current/session11/family_policy_c2.json --disposable --save c2_kit --seed out/qa/current/session11/fixtures/crafting_kit.sqlite
```

Wait for READY on 29960, then launch **[ContentTest C1 Yes] Brave Frontier**.
This package visited Vargas, the Archive and Training in Session 10 (with
background downloads on), so those should open without a Downloading screen;
if one appears, note the screen -- the logs will name the files. Town's
Synthesis and Spheres screens were never opened in this package and may
download their own interface once (legacy, expected).

1. **Vargas:** Units -> open **Ignis Halcyon Vargas** (10017). Art correct, no
   download. (This account has no alternate art unlocked, so no toggle is
   expected; the screen still asks for the alternate files, which the
   provider now answers.)
2. **Archive:** Menu -> Unit Archive. The list draws; units in Dual Brave
   Burst pairs show their DBB-marked thumbnails. Open **Touka Kirishima**
   (840767) and play its animation preview.
3. **Replacement and recovery** (second PowerShell, Archive list visible).
   Each `swap`, `park` and `restore` waits until the broker has published the
   change and says so ("published by the broker after N s"); a swap or restore
   takes about half a minute at this scale. Leave and re-enter the Archive only
   after that line:

   ```powershell
   python scripts\content_provider_client_test.py --identity c1yes snapshot before_park --policy out/qa/current/session11/family_policy_c2.json
   python scripts\content_provider_client_test.py --identity c1yes swap unit_ills_thum_810048_100.png --like unit_ills_thum_820278_100.png
   ```

   Leave the Archive and come back: **Immortal Firewing Blaze**'s thumbnail now
   shows **Tidelash Stormsurge Galea**. Then:

   ```powershell
   python scripts\content_provider_client_test.py --identity c1yes restore
   ```

   Leave and come back: Blaze's own thumbnail is back. Then:

   ```powershell
   python scripts\content_provider_client_test.py --identity c1yes park unit_ills_thum_810048_100.png
   ```

   Leave and come back: record what happens to Blaze's thumbnail, including
   any download, blank image, stale image, hang or crash. The broker must
   withdraw the asset, but the client's visual response is not yet verified.
   If the client hangs or crashes, restore from PowerShell before relaunching.
   Then:

   ```powershell
   python scripts\content_provider_client_test.py --identity c1yes restore
   python scripts\content_provider_client_test.py --identity c1yes disposable
   python scripts\content_provider_client_test.py --identity c1yes snapshot after_park --policy out/qa/current/session11/family_policy_c2.json
   ```

   Leave and come back: the thumbnail is back without restarting anything.
   `disposable` must end "0 differ" with changes "none". Compare snapshots:
   report any provider-owned files that changed. The deliberately parked
   thumbnail may be downloaded through the legacy path while unavailable;
   do not assume that occurred without the snapshots and logs.
4. **Training, leader BEFORE:** Randall -> Training Grounds. At the friend
   select pick your own **Inferno Berdette** (10824) and note the leader skill
   the party screen shows (expected: none). Fight briefly; quit or finish.
5. **Crafting:** Town. The kit holds exactly the materials for three crafts
   each of Lunar Treat, Nian Gao, Rainbow Cupcake and Adorned Egg (the
   **Synthesis** facility) and of Mystic Lantern (the **Spheres** facility).
   Craft all fifteen. Expected: no item-box or shortage message, every craft
   succeeds, the kit materials end at zero, and the outputs (three Mystic
   Lanterns as three rows) are in the item list. The item box count should
   read about 24/100 before and about 23/100 after (the server's own run of
   the same batch on a scratch copy: eight material stacks used up, four
   usable stacks and three sphere rows added).
6. Close the game, Ctrl+C the server, then:

   ```powershell
   python scripts\content_provider_client_test.py --identity c1yes collect
   ```

## Run B: leader AFTER (same package)

```powershell
python scripts\content_provider_client_test.py --identity c1yes server --exe out/qa/current/session10/bin/gimuserverw.exe --policy out/qa/current/session11/family_policy_c2.json --save leader_after --seed out/qa/current/session11/fixtures/leader_after_kit.sqlite
```

Training Grounds -> pick **Inferno Berdette** (10824) again. Expected leader
skill: **30% boost to Atk and max HP for all Units**. Close the game, Ctrl+C.
Collect this run before starting C:

```powershell
python scripts\content_provider_client_test.py --identity c1yes collect
```

The text check alone does not confirm the combat modifier. Also record the
same party member's max HP in A and B with otherwise identical party/equipment.

## Run C: small roster (same package)

```powershell
python scripts\content_provider_client_test.py --identity c1yes server --exe out/qa/current/session10/bin/gimuserverw.exe --policy out/qa/current/session11/family_policy_c2.json --save small --seed out/qa/current/session11/fixtures/small_roster_kit.sqlite
```

Log in, Home, Units (one unit), Archive, and start and finish one quest.
Expected: no crash, no missing or wrong art. Close the game, Ctrl+C, then
`--identity c1yes collect` again.

## Run D: performance, provider on versus off ([ContentTest], the Session 7-9 package)

That package previously cached 17,424 of the 17,445 policy names. This does
not prove the route's legacy assets are cached. If either run downloads,
let it finish and repeat the same route before comparing timings. Do not
visit Town (its map is the one bundle that package never cached).

Route: Home -> Units -> open one unit -> back -> Menu -> Unit Archive -> open
one unit -> back -> Home. Then close the game.

```powershell
python scripts\content_provider_client_test.py --identity test server --exe out/qa/current/session10/bin/gimuserverw.exe --policy out/qa/current/session11/family_policy_c2.json --save perf --seed out/qa/current/session11/fixtures/crafting_kit.sqlite
```

Launch **[ContentTest] Brave Frontier**, take the route, close the game,
Ctrl+C. Start the provider-off comparison with this complete command:

```powershell
python scripts\content_provider_client_test.py --identity test server --exe out/qa/current/session10/bin/gimuserverw.exe --policy out/qa/current/session11/family_policy_c2.json --save perf --seed out/qa/current/session11/fixtures/crafting_kit.sqlite --broker off
```

Wait for READY, launch **[ContentTest] Brave Frontier** (not C1 Yes), and take
the same route. If it downloads, let that finish and repeat the route once.
Close the client, then Ctrl+C in the server terminal. Then:

```powershell
python scripts\content_provider_client_test.py --identity test collect
python out/qa/current/session11/perf_compare.py out/qa/current/content_client/offlinemod_content.log.1 out/qa/current/content_client/offlinemod_content.log
```

(If there is no `.log.1`, pass only the `.log`.) Send the comparison output.
That package runs the reviewed Session 9 DLL (01786e64, no queue filter); its
read path is the same code as the C1 DLL's. This is a read-path comparison,
not full performance acceptance of the C1 DLL or proof that login/download
screens can safely be removed. Keep both complete logs; if rotation loses a
run, do not interpret a single remaining log as an on/off comparison.

## What to send

For each run: anything that downloaded on a screen listed as cached, any
missing or wrong art, crashes, and the leader skill text in A4 and B. The
collected logs are picked up from the harness folders.
