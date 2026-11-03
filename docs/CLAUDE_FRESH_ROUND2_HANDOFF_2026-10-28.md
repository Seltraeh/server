# Fresh session, round 2: handoff (run October 29, 2026)

The bounded round of [CLAUDE_FRESH_ROUND2_2026-10-28.md](CLAUDE_FRESH_ROUND2_2026-10-28.md); the file name keeps that assignment's date. This is a transient coordination note, excluded from a release like the other `CLAUDE_*` notes.

Nothing was staged, committed, pushed or deployed. No package was reset, signed or installed. The Word tracker was not edited. Per-item status is the new round-2 column of [BUG_ACCEPTANCE_2026-10-27.md](BUG_ACCEPTANCE_2026-10-27.md). This round does not close the write-up.

QA output is in `out/qa/current/fresh_round2_20261029/` (called `R2/` below; about 1 GB, mostly gate burst frames).

## 1. Results

| Target | Result | Source change |
|---|---|---|
| 1. #24 single/double gates on the current DLL | **Regression passed**, 7 pulls (below). Every double gate stays visible and tappable after its change, with no white over it. Every pull has exactly one white after the final opening, then the reveal. | none |
| 2. #30 Lizeria prerequisites | **Client-observed fixed.** Neither, 666 only and 20067 only (the report) show no Lizeria. Both shows one unlock scene, then the Lizeria map, and mission 700 starts. A stale tile is refused with "still locked" and returns Home with nothing spent. | none |
| 3. #23 Tower of Mistral/Morgan + Farm control | **Client-observed fixed for the reported cases.** The mission 10 replay plays no Farm scene. Intermediate 84 first clear: no Morgan scene. Final 85 first clear: `map1-ending.txt` once, then Morgan. The 85 replay and a relaunch repeat nothing. | none |
| 4. #33 remaining check | **Done.** Header 427 → 429 at the first tick, then 431/431 "Energy is full." at the due second; relog 431/431. | none |

**No runtime or data source changed this round.** The tracked server diff SHA-256 is still `88c473d5e9042b0223f7b11f1bcb3201fe2c1e71a1d4b347d50e53383380ebad`, the value the round-1 QC verified. No file under `scripts/`, `gimuserver/`, `standalone_frontend/` or `tools/bin/` is newer than that review. So no candidate was built and the full regression was not rerun. The round-1 results stand: candidate `03101975…`, 63 entries, 2,692 checks, 0 failed.

Issue-to-changed-file map for this round: no runtime file for any target. QA tooling only, in `R2/tools/` (§3). Docs: this handoff and the acceptance matrix (its round-1 copy is `R2/BUG_ACCEPTANCE_before_round2.md`).

## 2. Identities (checked at the end; all unchanged)

| What | SHA-256 |
|---|---|
| Server used for every run: `out/qa/current/fresh_round1_20261028/bin/gimuserverw.exe` | `03101975…` |
| S19 Fresh client DLL (`content_client_s19/package/libcurl.dll`; the current isolated test DLL, not a release package) | `9633061d…` |
| Content root: round 1's hard-link mirror `fresh_round1_20261028/content_root` (corrected tuto8 `271511d9…`) | – |
| Test gate MST root `R2/mst_gate_test` (185 tables hard-linked to `deploy/mst`; `gacha_effect_mst.json` weights only) | `87c2113b…` |
| Live save `deploy/gme.sqlite` (read only, for unit rows) / live tuto8 / in-tree EXE | `239f9e69…` / `271511d9…` / `f72310df…` |
| Source saves: S19 completed `save_after_s20_completed_check.sqlite` / S19 blank | `a2f601fe…` / `7b026db2…` |

No crash dump was written during the round.

## 3. Fixtures and tools (all in `R2/tools/`)

| Tool | Purpose |
|---|---|
| `prep_run.py NAME SAVE CONTENT [--mst-root DIR] [--set col=val]` | Round 1's copier, plus `--mst-root`. It refuses a root missing any `deploy/mst` table, since a missing table zombies the server. Paths are forward-slash absolute; account row printed. |
| `run_ctl.ps1 -Action start/stop -Name N` | Round-1 copy, defaulting to the `03101975` build. The server gets its config as argv[1]; an exclusive-open probe proves it holds the run's database; stop matches every candidate path. |
| `gate_seq.ps1 -Taps "x,y@ms;…" -DurationMs -IntervalMs -OutDir` | New. Timed taps plus PrintWindow frames about every 200 ms, with `index.csv` of frames and taps. It uses bf_drive's foreground and uncovered checks per tap. |
| `lizeria_state.py DB chains\|neither\|666\|20067\|both` | New. Clears both need chains (257 missions) and then sets exactly which of 666/20067 is cleared. |
| `prep_chain.py RUN NEXT` | Round 1's `prep_m234.py`, generalised. Chain-clears up to NEXT, copies five real unit rows from the live save (read only), and sets level 150 and energy 200. |

Saves (each a backup-API copy in `R2/visual/saves/<name>/`, with its config, server log, relay log and provider log):

| Save | Source and edits |
|---|---|
| `g1_overflow` | completed; gems 60, energy 100/15 (overflow glow on) |
| `g2_flat` | completed; gems 60, energy 15/15 |
| `l1_lizeria` | completed; level 150, energy 200, `chains`, then states as run |
| `t1_tower` | completed; `prep_chain.py … 84` (32 clears) |
| `e1_cap` | completed; 999, EXP 0, energy 427, refill due +400 s |

Synthetic clears write no scenario markers. The first visit after the chain clears replays the unlock presentations those clears imply, as the client would have shown them: in `l1_lizeria`, the Cordelica, Palmyna and Encervis scenes. The client then stores its own markers. Repeat-suppression was therefore judged only after that first visit.

## 4. Visual evidence (`R2/visual/run/`)

### #24: test gates (Veteran Summon list, category 4)

| Pull | Save | Gate | Taps | Frames / sheet | Provider log |
|---|---|---|---|---|---|
| A1 | g1_overflow | Omni (rainbow → black double) | normal (≈7 s apart) | `A1_omni_normal/`, `A1_sheet1/2` | stock trigger suppressed at the change; final opening `GachaGateOpen6`, white at frame 45 |
| A2 | g1_overflow | Omni | 57 gate taps every 350 ms (59 tap rows with Summon and confirm; corrected in round 3 from the index) | `A2_omni_rapid/`, `A2_change_window` | same three lines; one pull charged |
| A3 | g1_overflow | 6★ (red → rainbow double) | normal | `A3_6star_normal/`, `A3_sheet` | stock suppressed at `GachaGateChange4_5`; white at frame 45 of `GachaGateOpen5` |
| A4 | g1_overflow | 7★ (black single) | rapid | `A4_7star_rapid/`, `A4_open_window` | opening, then white at frame 45 |
| B1 | g2_flat | Omni | normal | `B1_omni_normal/`, `B1_open` | opening, then white at frame 45 ("the stock trigger fires on this frame too") |
| B2 | g2_flat | 7★ | rapid | `B2_7star_rapid/`, `B2_open` | opening, then white |
| B3 | g2_flat | 5★ (gold → rainbow double) | normal | `B3_5star_normal/`, `B3_sheet` | opening, then white |

Gems: g1 60 → 56 for 4 pulls, g2 60 → 57 for 3 pulls. No server ERROR. The provider logs are copied beside each save (`offlinemod_content_after_g1.log`, `…_g2.log`). This is not the Lukroar black-gate report (#22-1).

### #30: Lizeria (`L1_*` … `L6_*`)

**Intended rule.** The [Global wiki Lizeria page](https://bravefrontierglobal.fandom.com/wiki/Lizeria), revision **675489** (2020-10-14; API copy `R2/wiki/Lizeria_rev675489.json`), says Lizeria is "unlocked by defeating Grantos in Giant's Ruins of Cordelica and Cardes the Malevolent in Spirit World Palmyna of Palmyna". In the MSTs, area 700 and dungeon 700 both need "666,20067". 20067 "The Giant God Awakes" is Giant's Ruins (Cordelica, land 2); 666 "The Open Seal" is Spirit World Palmyna (land 3). The server's rule (`PermitPlace.cpp storyLandUnlocked`) is all-of.

| State | Client | Server (PermitPlace log) |
|---|---|---|
| 20067 only (the report) | land select Mistral / Cordelica / Palmyna (`L1_20`) | lands [1,2,3,100], 262 missions |
| 666 only | same three (`L2_10`) | lands [1,2,3,100] |
| both | one Lizeria unlock scene (Eriole and Paris on La Veda, `L3_07`/`L3_09` sheets), then the Lizeria map, Erinecht Plains NEW AREA (`L3_09`) | lands [1,2,3,4,100], 264 missions |
| stale tile: 20067 removed with the Lizeria map still open, Begin Quest on 700 | "This quest is still locked. Clear the quests it requires first." (`L3_19`); OK → Home (`L3_20`); energy 200 and open battle 0 unchanged | – |
| relaunch, 666 only | Lizeria gone again (`L4_04`) | – |
| neither | three lands (`L5_04`) | lands [1,2,3,100], 261 missions |
| both again | Lizeria listed with no repeat scene (`L6_04`); Erinecht; mission 700 starts the battle (`L6_09`), energy 200 → 188, open battle 700 | lands [1,2,3,4,100] |

### #23: Tower of Mistral (`T1_*` … `T5_*`)

- **Farm control.** Replaying mission 10 gave results, then the map (`T1_18`), with no scene. The provider log shows no `map1-dungeon0-1end.txt` fetch in this launch (log lines 39404–53706). Its only fetches are in round 1's launches, which first-cleared mission 10.
- **Tower first entry.** Intro `map1-dungeon7.txt` (`T1_20`); not on re-entry (`T3_01`).
- **84 first clear** (`T2_11` sheet). Results, captures, then the Mistral map. **No Morgan scene.**
- **85 first clear** (`T3_11`, `T3_14`, `T3_16` sheets).
  - Results and captures, then `map1-ending.txt`: Tilith, "Finally, we can make our way to Morgan", "Savior of St. Lamia".
  - Then the Morgan map with Kagan Desert NEW AREA.
  - Marker afterwards: `0,0@0@0@0&80,1@0@1@0&|0,0` (`saves/t1_tower/markers_after_85_first.txt`).
- **85 replay** (`T4_11` sheet). Results and captures, then the map. No scene.
- **Relaunch** (`T5_04`). Quest opens on Morgan; no repeat.

### #33: energy to cap (`E1_*`)

The sheet `E1_ticks_sheet.png` has 17 header crops at about 19 s intervals.

1. 427/431 "REFILL IN 1:41", counting down to 0:08.
2. **429/431 "REFILL IN 2:49"** (a fresh 180 s tick).
3. Counting down to 0:02.
4. **431/431 "Energy is full."** at 17:54:27; due 17:54:10, so the capture is within the interval.

Relog (`E1_30_relog_zoom`): Lv 999, 431/431, "Energy is full". The save keeps 427 with the past timestamp, and the server derives the cap from it.

## 5. Limitations (not closed by this round)

- Captures are visual only: no audio, and screens between 200 ms gate frames or 19 s energy frames are not excluded.
- #24 ran on the current isolated test DLL, not a final release package. It must be repeated on the release DLL once one is built.
- #30 and #23 ran on synthetic progression (chain clears, copied strong units, synthetic markers). A natural-progression save would strengthen both. Kagan Desert and Erinecht's battle outcome were not played.
- The test-gate banner shows another gate's art and "6□"/"7□" (the known ★ glyph gap). This is cosmetic to the test fixture.
- Agent-driven runs are not player confirmations.

## 6. Next gates and recommendation

- **Release gate (unchanged, recorded).** The release builder must require the corrected `tuto8.txt` bytes. It must refuse absent, original or unknown bytes, not merely invoke `apply_content_patches.py`, whose report mode exits 0 when the target is absent.
- **Remaining scopes.** #22-1/2/3 presentation, #27 slot visuals, #26 resources, #39 parade acceptance, #25/#40 eligibility and burst, exact #14 portraits, missing content #12/#15/#17/#18, and startup area/alias/family completeness. No date for an all-fixes release until those scopes are resolved.
- **Recommended next bounded round.** The client-acceptance rows that need no new content:
  1. #39 Metal Parades (Normal/Super/Mega waves, capture, result, replay).
  2. #27 slots 1–10 with mixed and duplicate prizes, plus relog inventory.
  3. #14 Seria 10233 and Goddess Tilith 50254 portraits.

  Each fits the round-2 fixtures (`prep_chain.py`, `gate_seq.ps1` for timed captures).
