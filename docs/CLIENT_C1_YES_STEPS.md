# C1 background-DLC Yes branch — October 5, 2026

Prepared: **[ContentTest C1 Yes] Brave Frontier**, separate package identity,
fresh Settings, copied account state, reviewed C1 DLL `523c77ed`, and independent
persistent save `yes_visual` seeded from the same new-unit fixture as the No
branch. The app has not been launched. Prelaunch snapshot has zero of 16,639
configured names cached. Your No-branch package/save/evidence remain intact.

## 1. One-time exemption

In Administrator PowerShell:

```powershell
CheckNetIsolation.exe LoopbackExempt -a '-n=gumi.BraveFrontier.ContentTestC1Yes_99p3jr0gh0z6w'
```

Expect OK. Registration is already done. Do not launch the app yet.

## 2. Start the harness

Close other Brave Frontier clients and stop any previous harness with Ctrl+C.
These test packages share port 29960, so run only one harness at a time.
In normal PowerShell:

```powershell
cd C:\Users\Evan\BF\BF-WorkingDirRust
python scripts\content_provider_client_test.py --identity c1yes check --exe out/qa/current/session10/bin/gimuserverw.exe --policy out/qa/current/session10/family_policy.json
python scripts\content_provider_client_test.py --identity c1yes server --exe out/qa/current/session10/bin/gimuserverw.exe --policy out/qa/current/session10/family_policy.json --save yes_visual --seed out/qa/current/session10/fixtures/roster_new_units.sqlite
```

Wait for READY on 29960 and server hash starting **270832d6**. Leave this
terminal running. The prepared save should say **kept from earlier runs**;
that is expected even on your first launch because QC seeded it already.

## 3. Choose Yes and play while background downloads run

Launch **[ContentTest C1 Yes] Brave Frontier** from Start. At the background-DLC
prompt choose **Yes**. If the prompt is absent, stop and report it rather than
resetting the app. Daily spin/calendar prompts may appear before Home; note
their order. No need to spin solely for this test.

In a second normal PowerShell at the checkout, immediately after reaching Home:

```powershell
python scripts\content_provider_client_test.py --identity c1yes snapshot yes_after_prompt --policy out/qa/current/session10/family_policy.json
python scripts\content_provider_client_test.py --identity c1yes progress --policy out/qa/current/session10/family_policy.json
```

Do not wait for all background work to finish before these interactions:

1. Browse Unit List and party edit; open several portraits/details.
2. Browse Archive, including Touka. Note the names/counts of download prompts
   and whether repeating the view causes another download.
3. Receive the ten C1 unit presents and inspect their details. Test a short
   mission with some of those units; check idle, attacks, BB when available,
   portraits, and the results/map transition.
4. Visit Town and revisit the same roster/Archive screens. Note freezes,
   missing art, crashes, repeated downloads, or lag compared with the No run.

The seed has the same over-capacity materials as the original No-branch seed.
Skip crafting; do not spend time selling inventory to make room for this test.
The earlier No-branch sales were not copied into this independent fixture.
Downloads of non-owned resources remain expected; total cache size is not a
completion target. This run tests coexistence with background downloading,
not removal of the existing download screens.

## 4. Collect, then verify restarts

Before closing the client:

```powershell
python scripts\content_provider_client_test.py --identity c1yes snapshot yes_after --policy out/qa/current/session10/family_policy.json
python scripts\content_provider_client_test.py --identity c1yes progress --policy out/qa/current/session10/family_policy.json
python scripts\content_provider_client_test.py --identity c1yes collect
```

Close/reopen the Yes client with the harness running; check the prompt stays
dismissed, art and party state are correct, and previously viewed screens do
not unnecessarily download again. Then close it, Ctrl+C the harness, rerun
the exact server command above, wait for READY and reopen the Yes client.
Check received units, party and visuals again, then:

```powershell
python scripts\content_provider_client_test.py --identity c1yes snapshot yes_after_restart --policy out/qa/current/session10/family_policy.json
python scripts\content_provider_client_test.py --identity c1yes collect
```

Report prompt choice, screen-specific downloads, any visual/performance issue,
and results of the two restart checks. Close the client and Ctrl+C its harness
when finished. Leave data/logs in place. Do not reset packages, change Settings,
or edit/remove real game_content files. QC will correlate provider activity and
write-tripwire logs with the snapshots before declaring acceptance.
