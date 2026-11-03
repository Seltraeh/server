# Session 9, round 2: client acceptance

**Current scope: perform sections 1, 2 and 5 only.** Final QC found an
intermittent live-provider CSV adoption failure. Sections 3 and 4 are prepared
for a later run, but are ON HOLD until it is resolved. No cache reset is needed.

Use these steps instead of the earlier Session 9 checklist. Your Town/Zeal
baseline and earlier sphere confirmations remain recorded. These checks cover
the newly changed commit handling and live asset replacement. They do not
establish a cache-free client.

## 1. Start the updated candidate

Close both clients. In PowerShell at `C:\Users\Evan\BF\BF-WorkingDirRust`:

```powershell
python scripts\content_provider_client_test.py check --exe out/qa/current/session9/bin/gimuserverw.exe
python scripts\content_provider_client_test.py server --exe out/qa/current/session9/bin/gimuserverw.exe
```

Check the printed SHA-256 starts with **47fe25fc** and wait for **READY** on
**29960**. Leave this terminal running. Launch **[ContentTest] Brave Frontier**.
Do not rerun registration, `state`, or Windows Reset. The test package is
already set up. Run subsequent commands in a second terminal at the checkout.

Each harness start recopies the normal save: test-only progress from a previous
harness run is not retained by this setup. For the persistence checks below,
restart only the CLIENT, leaving the harness running. The server-restart case
is covered separately by automated tests.

Downloads may still occur based on the roster, archive and visited screens.
Do not wait for the cache to match the normal client's size. If the DLC prompt
appears, choose No and record that. If it does not, continue: the handoff found
an existing stored Yes setting in this package; do not edit settings.dat.

## 2. Check saved gameplay and restart the client

After current downloads finish:

```powershell
python scripts\content_provider_client_test.py snapshot round2_before
```

1. Craft one affordable sphere. Record its count and material/currency cost;
   return from crafting so its queued request is sent.
2. Equip or favorite that sphere. Record the unit and selected copy.
3. Complete a short mission. Confirm results appear normally and you return to
   the mission area. If convenient, use one potion and check one was consumed.
4. Optional, if resources permit: make one enhancement purchase or summon and
   record the resulting balance and skill/unit. Do not spend time grinding or
   change the normal save to supply this test.
5. Close and reopen **[ContentTest]** with the harness still running. Confirm
   the crafted sphere, equipment/favorite, balances and any optional purchase
   persist. Report errors, duplicated rewards, lost changes, or unusual waits.

Do not deliberately lock the database yourself; automated tests cover that.

## 3. Same-size Town asset replacement

**ON HOLD — do not perform in the current run.**

This intentionally changes the Town layout to demonstrate that the client
notices an edit. Only the disposable broker fixture is edited.

1. Go to Home, leaving Town. In the second terminal:

```powershell
python scripts\content_provider_client_test.py edit --height 985
```

2. Wait two seconds, then enter Town. Record whether alignment visibly changes.
   Take a screenshot. If nothing changes, leave and reenter once and report
   that result; do not clear the cache to force a pass.
3. Return Home and restore the normal height:

```powershell
python scripts\content_provider_client_test.py edit --height 1161
```

4. Wait two seconds and reenter Town. Normal alignment should return without
   a client reset or server restart. A crash, persistent broken layout, or
   failure to restore is a failed check. Always perform the restore command,
   even if the first edit did not appear.

## 4. Missing-file recovery (separate isolated fault check)

**ON HOLD — do not perform in the current run.**

This deliberately removes one fixture asset temporarily. It may expose a
client error or crash; that is the observation being tested, not expected
normal play. Keep the harness running. Close the TEST CLIENT first so an
already-loaded RAM copy does not hide the missing file.

Temporarily rename this exact fixture file in the second terminal:

```powershell
Rename-Item -LiteralPath 'C:\Users\Evan\BF\BF-WorkingDirRust\out\qa\current\content_client\broker_root\sam\MapVillage\MapVillage_640x809.png' -NewName 'MapVillage_640x809.png.qc-hidden'
```

Wait two seconds, open **[ContentTest]**, then enter Town. Record the exact
popup, missing art, hang, or crash. Do not keep retrying a broken screen.
Close the test client and ALWAYS restore the file:

```powershell
Rename-Item -LiteralPath 'C:\Users\Evan\BF\BF-WorkingDirRust\out\qa\current\content_client\broker_root\sam\MapVillage\MapVillage_640x809.png.qc-hidden' -NewName 'MapVillage_640x809.png'
```

Wait two seconds, relaunch the test client, and check Town recovers. If the
first rename reports a missing file or existing destination, stop this check
and provide the error. Do not change paths to the normal game_content tree.
No visible failure is inconclusive: the logs must show whether the missing
asset was requested or some other copy masked the fault.

## 5. Collect results and stop

```powershell
python scripts\content_provider_client_test.py snapshot round2_after
python scripts\content_provider_client_test.py collect
```

Record the approximate time of each check, screenshots, and whether it passed:

| Check | Result to report |
| --- | --- |
| Craft/equip/favorite + client restart | Counts, costs, persistence, any errors |
| Mission | Results, destination, potion count if used |
| Optional SP purchase/summon | Cost, result, persistence |
| Town 985 then 1161 | Visible change and successful restoration |
| Missing texture then restore | Exact failure behavior and recovery |

Close the client, then Ctrl+C the harness. Leave collected evidence in place.
The isolated test changes do not transfer to the normal save.

## Later, with a prepared fixture

Leader-skill repair/Training Ground helper comparison, stale-cache precedence,
and a controlled broker-on/off performance comparison need separate preparation.
Do not repair the live save or reset either client for them. Cache-free login,
background-prompt removal and full-family coverage are not implemented yet.
