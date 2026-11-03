# Session 9 client checks

> For the updated `47fe25fc` candidate, use
> [CLIENT_SESSION_9_ROUND2_STEPS.md](CLIENT_SESSION_9_ROUND2_STEPS.md).
> The instructions below record the earlier baseline run.

These steps use a separate test package, copied save, and port 29960. Your
normal client remains separate. Session 9 has unresolved database commit and
intermittent broker replacement failures; see [QC_SESSION_9_REVIEW.md](QC_SESSION_9_REVIEW.md).
Do not deploy this candidate or repair the live save as part of these checks.

## Set up once

Run from `C:\Users\Evan\BF\BF-WorkingDirRust` in PowerShell:

```powershell
python scripts\content_provider_client_test.py check
Add-AppxPackage -Register 'C:\Users\Evan\BF\BF-WorkingDirRust\out\qa\current\content_client\package\AppxManifest.xml'
```

In an elevated PowerShell window (Windows requires elevation for this setting):

```powershell
CheckNetIsolation.exe LoopbackExempt -a '-n=gumi.BraveFrontier.ContentTest_99p3jr0gh0z6w'
```

Launch **[ContentTest] Brave Frontier** once and close it to create its LocalState.
Then, in the checkout's PowerShell window:

```powershell
python scripts\content_provider_client_test.py state --stale-town
python scripts\content_provider_client_test.py check
python scripts\content_provider_client_test.py server --exe out/qa/current/session9/bin/gimuserverw.exe
```

Keep the normal installed client closed during setup and the save-copy step.
Wait for **READY** on **29960**, and leave this terminal running. If it reports
NOT READY or FAILED, stop and provide the named log instead of launching play.
The explicit `--exe` matters: the current default is the older Session 8 server.

## What to verify now

Launch **[ContentTest] Brave Frontier**. Choose **No** when asked to download
missing DLC in the background for this test. This candidate still uses legacy
downloads, including additional downloads when opening other screens. Their
contents depend on inventory, archive unlocks and screens visited. The previously
observed 2.54 GB is one installed cache, not a required size or completion target.
A second terminal in the checkout can report cache counts:

```powershell
python scripts\content_provider_client_test.py progress
python scripts\content_provider_client_test.py snapshot before
```

Take the before snapshot once the current download has completed. You do not
need to visit every screen or match the installed client's file count first.
If you already visited Town or other screens, no reset is necessary: take a new
baseline using `snapshot before_repeat`, repeat the checks below, then use
`snapshot after_repeat` and `collect`. This covers only the repeated interval,
not earlier first-use behavior. The snapshot tracks only provider-owned names;
other legacy assets may still download. Record downloads during the interval
so changed owned files can be investigated alongside provider logs rather than
automatically attributed to provider writes. Unchanged snapshots alone cannot
prove no write occurred. Then:

1. Open Town: check alignment and sprites despite the deliberately stale Town
   cache. Send a screenshot if anything shifts, disappears, or looks stretched.
2. If Zeal (60956) is available in the test roster, enter a battle and check its
   portrait, sprite, attack and burst animations. Report any crash or missing art.
   If unavailable, report that prerequisite instead of changing the normal save.
3. Close and reopen the test client while leaving the harness running. Check
   account/settings and equipped units/spheres persist.
4. Note visible loading delays or stutters and the screen/action that causes them.

After testing:

```powershell
python scripts\content_provider_client_test.py snapshot after
python scripts\content_provider_client_test.py collect
```

Provide your observations and the collection path printed by the tool. The logs
are needed to establish that the hooks actually served the 21 owned assets and
that those assets were not written to cache; correct-looking art alone is not
proof. Stop the harness with Ctrl+C in its own window. Avoid a global server
stop command while the test harness is running.

## Defer until the corrections land

- Same-size asset replacement and missing-file/recovery checks.
- Formal broker-on versus broker-off performance comparison.
- Live-save leader-skill repair and Training Ground own-helper validation.
- Cache-free startup/login acceptance (not implemented by this candidate).

## Feedback for the next offline-patch session

Player observation, October 4: Windows Settings could not reset the development
test package ("We couldn't reset this app. Try again in a bit."). Cause is not
established. Do not uninstall or clear LocalState just to recover a missed
snapshot; preserve the current evidence and use a new baseline.

The future cache-free patch must account for inventory/archive-dependent lazy
downloads and the fresh-launch background-DLC prompt. Trace the prompt's flags
and queue consumers before bypassing it; select behavior compatible with local
game_content rather than merely hiding the dialog. Acceptance must include
different saves, archive browsing, newly acquired units, and first visits to
screens without relying on a pre-populated cache.

Your previous sphere and mission-return tests are already confirmed; they do
not need repeating for this session.
