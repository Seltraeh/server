# Session 10: first C1 visual acceptance

Use **[ContentTest C1] Brave Frontier**, not either older Start/taskbar entry.
QC has prepared and registered this new package, copied account-state files
only, and captured `snapshot_c1_prelaunch.json`: zero of the 16,639 configured
asset names were cached. The original clients and their settings were not reset.
The C1 package has not been launched by QC.

This is expanded unit-art coverage, NOT a fully cache-free game. Some units,
alternate art, skill effects, voice, items and other resources still use legacy
downloads. The login/download screens and DLC prompt have not been removed.

## 1. Enable the NEW package's loopback access once

In **Administrator PowerShell**, run:

```powershell
CheckNetIsolation.exe LoopbackExempt -a '-n=gumi.BraveFrontier.ContentTestC1_99p3jr0gh0z6w'
CheckNetIsolation.exe LoopbackExempt -s
```

Confirm the list contains **contenttestc1**, not just contenttest. No package
registration or Windows Reset is needed; QC already registered it.

## 2. Start the exact candidate and persistent test save

Close both older clients and any previous harness. In a normal PowerShell:

```powershell
cd C:\Users\Evan\BF\BF-WorkingDirRust
python scripts\content_provider_client_test.py --identity c1 check --exe out/qa/current/session10/bin/gimuserverw.exe --policy out/qa/current/session10/family_policy.json
python scripts\content_provider_client_test.py --identity c1 server --exe out/qa/current/session10/bin/gimuserverw.exe --policy out/qa/current/session10/family_policy.json --save c1_visual --seed out/qa/current/session10/fixtures/roster_new_units.sqlite
```

Wait for **READY** on **29960**. The server hash should begin **270832d6** and
the proxy is the C1 enqueue-filter candidate (**523c77ed**). Leave this terminal
running. If check reports anything missing, or server reports NOT READY/FAILED,
stop and send the output. A port-in-use error means another harness is still
running; do not kill every server by name.

The first start seeds a COPY containing your 109-unit roster plus ten test
unit presents and room to receive them. Later starts with this same command
keep `c1_visual`; `--seed` does not overwrite an existing named save. None of
these test changes reaches your normal save.

## 3. First launch and screen checks

Launch **[ContentTest C1] Brave Frontier** from Start. If the background-DLC
prompt appears, choose **No**, and note the choice. If absent, report that;
do not reset the app or edit settings.dat to force it.

Let any current download finish. In a second PowerShell at the checkout:

```powershell
python scripts\content_provider_client_test.py --identity c1 snapshot c1_before --policy out/qa/current/session10/family_policy.json
python scripts\content_provider_client_test.py --identity c1 progress --policy out/qa/current/session10/family_policy.json
```

The prelaunch snapshot already covers first-start cache growth, so taking this
snapshot after the initial download does not lose that evidence. File-count
overlap with the normal cache is NOT a download completion percentage.

Visit these screens, noting the first visit separately from a repeat visit:

1. **Home and Unit List:** scroll your roster; open several unit details from
   different elements. Check thumbnails, portraits, names and animated sprites.
2. **Party Edit:** select and swap units; confirm portraits appear promptly and
   there are no blank images, wrong units, freezes or crashes.
3. **Archive:** browse units you own and some unlocked entries you do not own.
   Open full artwork and any available animation previews. Browse item/sphere
   pages too, but their downloads are not necessarily covered by C1.
4. **Town:** verify alignment remains correct. This is a brief regression check
   with the new DLL, not a repeat of the old asset-edit experiment.

Record which screen triggers Downloading, any repeated download on revisiting,
and rough time from tap to usable screen. A remaining download alone is not a
failure; logs must identify whether its files are inside C1's configured scope.

## 4. Newly acquired units and battle

1. Open Presents. Receive the ten test unit presents whose description mentions
   **C1: art in the unit families, never on this account**. Other presents may
   already be there; do not assume every present was added for this test.
2. Return Home, then Unit List and Archive. Check their new portraits and detail
   art appear. Add two or three of those units to a party.
3. Enter Training Ground or a short mission. Observe idle, movement, normal
   attacks and Brave Burst animations when available, plus battle portraits.
   Record missing textures, wrong frames, clipping, popups, freezes or crashes.
4. Finish the mission and verify the expected result/map flow. If affordable,
   craft/equip one sphere or claim an available reward; record balances and
   resulting inventory. Do not grind or modify your normal save for a test.

## 5. Check persistence across BOTH client and harness restart

1. Return Home so queued actions have been sent. Close and reopen only C1.
   Verify received units, party and any crafted/equipped item persist.
2. Collect the first interval before stopping:

```powershell
python scripts\content_provider_client_test.py --identity c1 snapshot c1_after --policy out/qa/current/session10/family_policy.json
python scripts\content_provider_client_test.py --identity c1 collect
```

3. Close C1, Ctrl+C the harness, and run the EXACT server command from section
   2 again. It should say **kept from earlier runs** for `c1_visual`.
4. After READY, reopen C1. Confirm the same units, party, equipment and claimed
   presents remain correct. Revisit Unit List and one previously tested screen.
5. In the second terminal:

```powershell
python scripts\content_provider_client_test.py --identity c1 snapshot c1_after_restart --policy out/qa/current/session10/family_policy.json
python scripts\content_provider_client_test.py --identity c1 collect
```

Close C1 and stop its harness with Ctrl+C. Report the observations below and
leave the package and collected logs in place for QC.

| Observation | What to report |
| --- | --- |
| DLC prompt | Appeared? No selected? |
| First/repeat screen visits | Art correctness, downloads, delays |
| Test presents | Received units and whether their art appeared |
| Battle | Idle/move/attack/BB, portrait correctness, crashes |
| Persistence | Client restart and then harness restart, separately |
| Snapshot outputs | Configured-name count and cached-name counts |

Zero configured cached names is useful evidence, not proof by itself. Provider
activity, signatures, failures and write-tripwire logs will be reviewed too.

## Not in this first C1 run

- Do NOT use `edit --height` or rename files while serving `family_policy.json`:
  it reads the real game_content tree, not the disposable 21-file fixture.
  Replacement/missing-file visual checks need a separate fixture-mode run.
- Do not reset either older client or run `state` again after starting play.
- Testing the Yes prompt branch needs another fresh Settings identity or a
  deliberate prepared fixture. Do not claim both answers tested on this run.
- The small-roster fixture, leader-before/after comparison and controlled
  broker-off timing are follow-up checks after this first result is reviewed.
