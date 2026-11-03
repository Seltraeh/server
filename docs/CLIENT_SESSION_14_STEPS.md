# Client steps, Session 14 — double gates and the Farm (one sitting, ~25 minutes)

**Run only after Codex has reviewed this candidate** ([CLAUDE_SESSION_14_HANDOFF.md](CLAUDE_SESSION_14_HANDOFF.md)).
Nothing here touches your normal server, your installed client or your save.

What needs your eyes:

1. **#24.** The white screen over the second gate is the client's own reveal
   flash firing too early: it counts every animation on screen, and other
   screens (the energy-overflow glow in the header, a download icon) add to
   the count. Part A forces the double-gate sequence, but the whiteout itself
   still depends on client animation state and is not guaranteed. Part B runs the
   candidate scripts, which add an invisible guard so the count can no longer
   match while a gate is up.
2. **Farm.** In Session 13 the Farm's harvest happened to be six empty taps.
   Part B plants a known harvest so every tap has a stated expectation.

| | Identity |
|---|---|
| Package | **[ContentTest C1]** (`gumi.BraveFrontier.ContentTestC1_99p3jr0gh0z6w`), DLL `523c77ed…`, port 29960 only |
| Server | `out\qa\current\session13\bin\gimuserverw.exe` `90e3eff9…` (unchanged since Session 13) |
| Part A policy (today's scripts) | `out\qa\current\session13\family_policy_c3.json` `5b9c5e52…` |
| Part B policy (candidate scripts) | `out\qa\current\session14\family_policy_c4.json` `8bdcd08e…` (C3 plus the 14 gate scripts, 9 of them guarded) |
| Summon gates | `out\qa\current\session14\mst_gate_test` (`gacha_effect_mst.json` `87c2113b…`): **Test Gate: Omni** is ALWAYS the double gate (rainbow, then black); **Test Gate: 7 Star** is ALWAYS the single black gate; **Test Gate: 5 Star / 6 Star** are always double gates (gold or red, then rainbow). 1 gem each |
| Fixture | `out\qa\current\session14\fixtures\s14_gate_town_kit.sqlite` `287e9b91…`: Selena only, 50 gems, energy 100 over a max of 27 (the header's overflow glow is on, as in Session 13), the known Town harvest below |
| Saves | new names `s14_control` (Part A), `s14_candidate` (Part B), optional `s14_flat` (Part C, fixture `s14_gate_flat_kit.sqlite` `554a80b7…`, energy 27 of 27) |

Close any older test client first and stop any harness with Ctrl+C: all test
packages share port 29960. Close the game between parts.

## Commands (PowerShell, in `C:\Users\Evan\BF\BF-WorkingDirRust`)

Before:

```
python scripts\content_provider_client_test.py --identity c1 check --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session14\family_policy_c4.json
```

```
python scripts\content_provider_client_test.py --identity c1 snapshot s14_before --policy out\qa\current\session14\family_policy_c4.json
```

### Part A — today's scripts (~5 minutes)

Terminal A, wait for `READY`:

```
python scripts\content_provider_client_test.py --identity c1 server --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session13\family_policy_c3.json --save s14_control --seed out\qa\current\session14\fixtures\s14_gate_town_kit.sqlite --mst-root out\qa\current\session14\mst_gate_test
```

| # | Do | Expected (the reproduction) |
|---|---|---|
| A1 | Summon > **Test Gate: Omni**, tap through at a normal pace. Twice. | A rainbow gate; after the first tap it changes to black -- and the screen turns WHITE over the second gate about two seconds after that tap, until the unit appears. That is #24. |
| A2 | Summon > **Test Gate: 7 Star**, once. | A single black gate opens normally (no white screen over it). |

Close the game normally, then Ctrl+C in Terminal A. Note for each pull: white
or not, and when. If A1 does NOT turn white, write that down and carry on:
it is useful evidence, not a failure of the run.

Collect Part A before proceeding, so rotation cannot erase this comparison:

```
python scripts\content_provider_client_test.py --identity c1 collect
```

### Part B — candidate scripts (~12 minutes)

Terminal A, wait for `READY`:

```
python scripts\content_provider_client_test.py --identity c1 server --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session14\family_policy_c4.json --save s14_candidate --seed out\qa\current\session14\fixtures\s14_gate_town_kit.sqlite --mst-root out\qa\current\session14\mst_gate_test
```

| # | Do | Expected |
|---|---|---|
| B1 | Summon > **Test Gate: Omni**, three times: twice at a normal pace, once tapping fast. | Rainbow gate, TOUCH, tap; the colour change to black stays visible; TOUCH, tap; the gate opens and the unit is revealed. **No white screen over either gate.** A brief white flash at the very end, right before the reveal, may or may not appear; note which. |
| B2 | Summon > **Test Gate: 7 Star**, twice. | The single black gate shows TOUCH, opens, the unit is revealed. |
| B3 | Summon > **Test Gate: 5 Star**, once. | A gold or red gate changes to rainbow, opens, reveals. |
| B4 | Town: tap the **Farm** six times, slowly, watching what flies up each time. | 1: a Don Nut icon. 2: Zel (150). 3: Karma (20). 4: a Buru Nut, Zel (120) and Karma (15). 5: **nothing at all** -- this entry is empty on purpose (the Session 13 case). 6: a Don Nut. The sparkle goes after the sixth tap. |
| B5 | Tap the **Mountain** (3), **River** (8) and **Forest** (10) until their sparkle goes. Then go back Home. | Mountain: Green Pelt + Zel; Paladin Stone + Zel + Karma; Zel + Karma. River: mostly Green Drop with Karma, the third tap Karma only. Forest: mostly Green Grass with Zel/Karma, the eighth tap Zel + Karma only. |
| B6 | (optional) Upgrade the Farm with Karma now that it is empty. | The upgrade succeeds but **no new sparkle** -- by design an upgrade never refills; this test save's harvest period does not end during the session. |

Do not play quests in Part B (it keeps the balance check exact, though the
checker accounts for them). Close the game, then Ctrl+C in Terminal A.

Then check the harvest the server credited (read only):

```
python scripts\verify_town_harvest.py out\qa\current\session14\fixtures\s14_gate_town_kit.sqlite out\qa\current\content_client_c1\saves\s14_candidate\gme.sqlite
```

It lists what every tap should have shown and should end with four `OK` lines.
An INCONCLUSIVE result means the snapshots cannot establish the credits (for
example after a harvest reroll); report it rather than treating it as a pass.
Collect Part B before any optional Part C:

```
python scripts\content_provider_client_test.py --identity c1 collect
```

### Part C — optional, energy not overflowing (~3 minutes)

Tests whether the header's overflow glow is one of the extra animations.

```
python scripts\content_provider_client_test.py --identity c1 server --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session13\family_policy_c3.json --save s14_flat --seed out\qa\current\session14\fixtures\s14_gate_flat_kit.sqlite --mst-root out\qa\current\session14\mst_gate_test
```

Summon > **Test Gate: Omni**, twice, today's scripts. The model predicts **no**
white screen over the second gate here (one fewer animation on screen). Close
the game, Ctrl+C.

### After

```
python scripts\content_provider_client_test.py --identity c1 snapshot s14_after --policy out\qa\current\session14\family_policy_c4.json
```

```
python scripts\content_provider_client_test.py --identity c1 collect
```

## If something goes wrong

- A white screen in Part B: a screenshot or short video, which pull, which
  gate (rainbow / black / gold / red), normal or fast taps. Keep going.
- A crash or hang: stop, run `collect`, and keep `%LOCALAPPDATA%\CrashDumps`.
- To get today's behaviour back at any time, run the Part A command: the
  client then reads its own cached scripts again. Nothing else changes.

## What I check afterwards from your files

- Provider log: in Part B the gate scripts were served text-mode from the
  candidate root (`served gWaitRainbow.txt ... text-mode`), in Part A they were
  not; tripwire writes 0; no read failures.
- `snapshot_s14_before` vs `snapshot_s14_after`: the client's cached copies of
  the gate scripts were not rewritten (the candidate never lands in its cache),
  and the scripts only Part B needed (`gChange(Gold-Rainbow).txt` or
  `gChange(Red-Rainbow).txt`, `gOpenRainbow(SSR).txt`) are still absent from
  LocalState: owned names are served, never downloaded.
- The server logs of each save and the `verify_town_harvest.py` output.
