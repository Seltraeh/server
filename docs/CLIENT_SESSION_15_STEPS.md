# Client steps, Session 15 — the summon white screen (#24) and sounds and screens from the server (~40 minutes)

**QC cleared this isolated candidate for the visual run** ([QC_SESSION_15_REVIEW.md](QC_SESSION_15_REVIEW.md)).
Nothing here touches your normal server, your installed client or your save.

What changed since Session 14:

- **#24:** the white screen is now decided by the test DLL itself. It waits for
  the **final gate's opening** and turns the screen white about two seconds into
  it -- the same moment your Session 14 5-star pull showed -- no matter what else
  is animating (the energy-overflow glow, a download icon, tap ripples). The gate
  scripts are back to today's (the client's own cached copies).
- **Sounds and Home/Summon art:** the same DLL can now play sounds straight
  from the server's game_content, in memory, without downloading them into the
  game's cache; the server also hands out the Home and Summon screens' images
  (item icons, quest banners, summon banners, Home art). Part C turns both on
  and proves each with one deliberately swapped file.

What needs your eyes (and ears):

1. Every summon: **no white over a gate still waiting for your tap**, and **a
   white screen after the last gate opens, before the unit appears** --
   including the single black gate (7 Star) and the Omni double gate, which had
   none in Session 14.
2. Part C: music, sound effects and screens look and sound as usual; one gate
   sound and one summon background are replaced on purpose, then put back.

| | Identity |
|---|---|
| Package | **[ContentTest C1]** (`gumi.BraveFrontier.ContentTestC1_99p3jr0gh0z6w`), port 29960 only |
| Candidate DLL (installed in step 0) | `out\qa\current\session15\proxy_29960_s15\libcurl.dll` `af96f41b…` (Session 10's `523c77ed…` is kept for rollback) |
| Server | `out\qa\current\session13\bin\gimuserverw.exe` `90e3eff9…` (unchanged) |
| Parts A and B policy | `out\qa\current\session13\family_policy_c3.json` `5b9c5e52…` (unchanged; no sounds, no gate scripts owned) |
| Part C policy | `out\qa\current\session15\policies\family_policy_c6.json` `e3d29e25…` (C3, all 426 sounds, and 3,439 Home/Summon images) |
| Summon gates | `out\qa\current\session14\mst_gate_test`: **Test Gate: Omni** = rainbow then black (double); **Test Gate: 7 Star** = single black; **Test Gate: 5 Star / 6 Star** = gold or red, then rainbow (double). 1 gem each |
| Saves | new names `s15_overflow` (energy 100 over 27: the header's overflow glow is on, as in Session 14), `s15_flat` (27 of 27), `s15_audio` (27 of 27) |

Close any older test client first and stop any harness with Ctrl+C: all test
packages share port 29960. Close the game between parts.

## Commands (PowerShell, in `C:\Users\Evan\BF\BF-WorkingDirRust`)

### 0. Install the candidate DLL into the test package (the game must be closed)

```
python scripts\content_provider_client_test.py --identity c1 dll --file out\qa\current\session15\proxy_29960_s15\libcurl.dll
```

It prints `now Session 15 candidate (C1 + #24 gate flash + audio reader)` and the
rollback command (keep it). No re-registration; the package's data and settings stay.

```
python scripts\content_provider_client_test.py --identity c1 check --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session13\family_policy_c3.json
```

Expect `layout libcurl.dll: Session 15 candidate …; port 29960` and `0 item(s) outstanding`.

```
python scripts\content_provider_client_test.py --identity c1 snapshot s15_before --policy out\qa\current\session15\policies\family_policy_c6.json
```

### Part A — #24 with energy overflowing (~10 minutes)

Terminal A, wait for `READY`:

```
python scripts\content_provider_client_test.py --identity c1 server --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session13\family_policy_c3.json --save s15_overflow --seed out\qa\current\session14\fixtures\s14_gate_town_kit.sqlite --mst-root out\qa\current\session14\mst_gate_test
```

| # | Do | Expected |
|---|---|---|
| A1 | Summon > **Test Gate: Omni**, twice, tapping at a normal pace. | Rainbow gate, TOUCH, tap; it changes to black and **stays visible** (no white); TOUCH, tap; the black gate opens, then the screen **fades to white**; the unit is revealed. |
| A2 | **Test Gate: Omni** once more, tapping fast the whole time. | The same: no white over either gate, white after the black gate opens. |
| A3 | **Test Gate: 7 Star**, twice. | Single black gate, TOUCH, tap; it opens, then **white**, then the unit (Session 14 had no white here). |
| A4 | **Test Gate: 5 Star** once, then **Test Gate: 6 Star** once. | Gold or red gate, TOUCH, tap; it changes to rainbow; TOUCH, tap; it opens, then white, then the unit (as in Session 14's correct 5-star). |

Close the game normally, then Ctrl+C in Terminal A. Collect before Part B:

```
python scripts\content_provider_client_test.py --identity c1 collect
```

### Part B — #24 with energy not overflowing (~5 minutes)

```
python scripts\content_provider_client_test.py --identity c1 server --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session13\family_policy_c3.json --save s15_flat --seed out\qa\current\session14\fixtures\s14_gate_flat_kit.sqlite --mst-root out\qa\current\session14\mst_gate_test
```

| # | Do | Expected |
|---|---|---|
| B1 | **Test Gate: Omni**, once, normal pace. | As A1. |
| B2 | **Test Gate: 7 Star**, once, tapping fast. | As A3. |
| B3 | **Test Gate: 5 Star**, once. | As A4. |

Close the game, Ctrl+C, then:

```
python scripts\content_provider_client_test.py --identity c1 collect
```

### Part C — sounds and screens from the server (~20 minutes)

Take a separate baseline now. Parts A/B use C3, so sounds and the new UI images
were legitimately still cacheable there; comparing only to s15_before could
incorrectly blame those earlier writes on C6.

```
python scripts\content_provider_client_test.py --identity c1 snapshot s15_c6_before --policy out\qa\current\session15\policies\family_policy_c6.json
```

Sound settings: in the game's settings, make sure **music and sound effects
are on** at a volume you can hear (the game skips a sound entirely when its
volume is zero). Leave them as they are afterwards.

Terminal A, wait for `READY` -- this policy is larger: allow up to about five
minutes (the server indexes 57,631 names):

```
python scripts\content_provider_client_test.py --identity c1 server --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session15\policies\family_policy_c6.json --disposable --save s15_audio --seed out\qa\current\session14\fixtures\s14_gate_flat_kit.sqlite --mst-root out\qa\current\session14\mst_gate_test --timeout 300
```

**Before launching the game**, in Terminal B, make two deliberate swaps, one
after the other; wait for `published by the broker after … s` after each (one
to three minutes each). The gate's appearing sound becomes the title voice:

```
python scripts\content_provider_client_test.py --identity c1 swap bf253_se_gousei02.mp3 --like bf113_zin_title.mp3
```

and the Test Gates' summon background becomes a Christmas banner:

```
python scripts\content_provider_client_test.py --identity c1 swap gacha_rare_bg_img_20161028_5M.png --like gacha_rare_bg_img_20151223_xmas.png
```

| # | Do | Expected |
|---|---|---|
| C1 | Launch the game. Title, then Home. | Music, sounds and the Home screen as usual (its art now comes from the server). |
| C2 | Open **Summon** and select **Test Gate: 7 Star**. | The Test Gate's background is the **Christmas banner** instead of the usual one (your cached copy is the usual one: this proves the screen's image came from the server). Buttons, other banners and icons as usual. |
| C3 | Summon **Test Gate: 7 Star** -- the first summon of this launch. | When the black gate appears you hear the **title voice** instead of the usual gate sound (same proof, for sounds). TOUCH, tap: the usual opening sound, then white, then the unit. |
| C4 | Close the game. In Terminal B: `python scripts\content_provider_client_test.py --identity c1 restore` and wait for `published`. Launch again; Summon, select and summon **Test Gate: 7 Star**. | The usual background and the usual gate sound are back. |
| C5 | Play normally for about five minutes: Home, the Summon list, the item list (Items), the quest list (Quest), the Town (tap a few places), anything with music changes. | Every sound, music track, icon and banner as usual; note anything silent, cut short, blank or wrong. |

Close the game, Ctrl+C in Terminal A, then:

```
python scripts\content_provider_client_test.py --identity c1 collect
```

```
python scripts\content_provider_client_test.py --identity c1 snapshot s15_after --policy out\qa\current\session15\policies\family_policy_c6.json
```

Leave the candidate DLL installed unless asked to roll back.

## If something goes wrong

- A white screen over a waiting gate, or no white before a reveal: note which
  pull (A1..C4), which gate colour, normal or fast taps; a screenshot or short
  video if you can. Keep going, and collect at the end of the part.
- In Part C, a missing or wrong sound or image other than C2/C3's swaps: note
  what you were doing. If C2 showed the usual background or C3 played the usual
  gate sound, say so (that swap was not in effect).
- A crash or hang: stop, run `collect`, and keep `%LOCALAPPDATA%\CrashDumps`.
- If Part C's swaps are still in place at the end, run the `restore` command above.
- To put the Session 10 DLL back (game closed), run the rollback command step 0
  printed; it is:

```
python scripts\content_provider_client_test.py --identity c1 dll --file out\qa\current\content_client_c1\dll_history\523c77ed23479174f6f9ce2d222331f6209159048f8372987dbdd156aa21ef76.dll
```

## What I check afterwards from your files

- Provider log, each launch: `gate flash fix installed at exe …` and `content
  provider hooks installed (… audio reader on (sounds may be owned))`; a
  `DECLINED` line would mean that part ran stock -- not evidence.
- Parts A and B, each pull: `gate flash: the final gate is opening (GachaGateOpen…)`
  then `gate flash: white at frame 45 of GachaGateOpen…` (or a later frame if an
  update crosses that threshold); one white line per pull
  (10). In Part A's Omni pulls, also `the stock trigger would turn the screen
  white now (… GachaGateChange5_6.sam …) -- not the final gate's opening`. No `fault`.
- Part C: `served bf253_se_gousei02.mp3 … to the audio reader as a RAM file`
  carrying the title voice's size after the swap and its own after restore;
  `served gacha_rare_bg_img_20161028_5M.png …` with the Christmas banner's size,
  and its own size on the launch after restore; sounds and images served that your
  LocalState never held (no download: `snapshot_s15_c6_before` vs `snapshot_s15_after`
  show no new or rewritten owned file); no `TRIPWIRE`; `open … failed` /
  `removed from game_content` lines only if a file is genuinely missing.
- The server logs: the pulls answered effects 900 / 229 / 13-15 / 19-21.
