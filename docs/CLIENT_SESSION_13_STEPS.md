# Client steps, Session 13 — effects served by the provider (one run, ~15 minutes)

What is new and needs your eyes: with the Phase C3 policy the provider also
serves SuperAnim animations (summon gates, Home auras, battle and burst
effects, Victory / Level Up) and particle effects from game_content
([OFFLINE_CONTENT_PHASE_C3.md](OFFLINE_CONTENT_PHASE_C3.md)). Automated checks
cover delivery and hook behavior; this run samples actual rendering and
responsiveness, not every possible effect or game feature. Nothing
here replaces the two Session 12 checks ([CLIENT_SESSION_12_STEPS.md](CLIENT_SESSION_12_STEPS.md));
do not repeat earlier workflows.

| | Identity |
|---|---|
| Package | **[ContentTest C1]** (`gumi.BraveFrontier.ContentTestC1_99p3jr0gh0z6w`), its DLL `523c77ed…` talks to port 29960 only — your normal server and installed client may keep running |
| Server | `out\qa\current\session13\bin\gimuserverw.exe` `90e3eff9…` (Session 12's server plus the broker fix for bundles that share members) |
| Policy | `out\qa\current\session13\family_policy_c3.json` `5b9c5e52…` (53,766 names) |
| Save | new name **`c3_effects`**, seeded once from `out\qa\current\session13\fixtures\c3_effects_kit.sqlite` `e2244ed2…`: one unit (Selena, level 12, BB 10), **50 gems**, 109 free unit slots, summoner level 22 — Session 12's repaired one-unit roster plus gems |

The harness's pre-flight reported "0 item(s) outstanding" for this package and
policy on October 8, and the exact `server` command below was smoke-tested
without a client (READY with 53,766 names, about 40 s).

## Commands (PowerShell, in `C:\Users\Evan\BF\BF-WorkingDirRust`)

Close any older test client and stop its harness with Ctrl+C first. All test
packages share port 29960; run only this harness for this sequence.

1. Before:

   ```
   python scripts\content_provider_client_test.py --identity c1 snapshot c3_before --policy out\qa\current\session13\family_policy_c3.json
   ```

2. Terminal A — leave it running; wait for the `READY` line:

   ```
   python scripts\content_provider_client_test.py --identity c1 server --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session13\family_policy_c3.json --save c3_effects --seed out\qa\current\session13\fixtures\c3_effects_kit.sqlite
   ```

3. Start **[ContentTest C1]** from the Start menu and play the checks below.
4. Close the game normally, then Ctrl+C in Terminal A.
5. After:

   ```
   python scripts\content_provider_client_test.py --identity c1 snapshot c3_after --policy out\qa\current\session13\family_policy_c3.json
   ```

   ```
   python scripts\content_provider_client_test.py --identity c1 collect
   ```

## Checks

For each, note **OK**, or what you saw (a screenshot for anything wrong).

| # | Do | Expect |
|---|---|---|
| 1 | Home: watch it for a few seconds. | The usual Home animations; no error box. |
| 2 | Summon: Rare Summon with gems, two or three times (tap through gates normally, once tap fast). | Gates open, crack and change colour as usual; the unit is revealed. No white screen covering the reveal, no black void, no "… is missing." box. |
| 3 | Unit Archive: open a unit you just summoned, then its description / skill view. | Its effects animate. |
| 4 | Quest: add a unit you just summoned to Selena's squad (Selena is already at her level cap), take them into an early stage you have cleared (e.g. mission 10), fire Brave Bursts, win. | Burst and hit effects drawn; the Victory animation, and Level Up for the new unit. |
| 5 | (optional) Town. | Building icons animate. |

Check 4 edits a squad on Session 12's repaired one-unit roster. If Manage Squad
crashes or hangs, stop the run and collect logs; do not keep attempting entry.
If it opens but cannot select the summoned unit, record that and use Selena
alone only if normal navigation still works. A Level Up is conditional on the
new unit earning enough EXP; no Level Up by itself is not a rendering failure.
Likewise a unit's static description need not animate. Record which summon,
burst, hit and Victory effects actually appeared rather than assuming every
unit or screen exercises all effect families.

A **Downloading** screen may still appear on some screens: sounds, menu art,
item art and layouts are not in this phase. That is expected; note where it
appeared and the file count it showed. What would be wrong is a missing or
broken animation, or an error box.

## What I check afterwards from your files

- The provider log (`collect`): the `.sam` / page / `.plist` names of these
  screens served by the provider, `tripwire writes 0`, no read failures.
- `snapshot_c3_before.json` versus `snapshot_c3_after.json`: no owned name
  written into the package's LocalState during the run.
- Its perf lines: the provider's memory against the Session 11 runs (the C3
  index costs about 25 MB in tests).
