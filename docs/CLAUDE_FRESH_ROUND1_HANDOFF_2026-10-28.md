# Fresh session, round 1: handoff (October 28, 2026)

The first bounded implementation round of [CLAUDE_FRESH_SESSION_2026-10-27.md](CLAUDE_FRESH_SESSION_2026-10-27.md). This is a transient coordination note, excluded from a release like the other `CLAUDE_*` notes. Nothing was staged, committed, pushed or deployed. No client package was reset, signed or installed. The live save, live server binary and live content root are byte-identical to before (hashes in §6). Per-item status is in [BUG_ACCEPTANCE_2026-10-27.md](BUG_ACCEPTANCE_2026-10-27.md), now with a round-1 column. This round does not close the write-up.

All QA output is in `out/qa/current/fresh_round1_20261028/` (called `Q/` below).

## 1. Results at a glance

| Round item | Result |
|---|---|
| 1. tuto8 correction reproducible and robust | **Done.** Hash-checked, idempotent patch tool with tests. Race reproduced under 5 s held replies with the shipped script. The corrected script survives the same holds. The timer question is answered: PLAYWAIT counts frames, and the scene gets no frames under the modal. |
| 2. Bootstrap audit crash | **Fixed.** Shape and type checks mirror the server's Glaze read. 13 malformed manifests; offline and live agree. |
| 3. Stale docs and release dependencies | **Done.** Idle-timeout comment corrected (behaviour unchanged). Four checker files un-ignored, `BF_CLIENT_EXE`, `RELEASE.md` section. Handbook has a dated current-status entry. |
| 4a. #33 level 999 | **No freeze reproduced; an energy defect found and fixed.** Natural 998→999 and edited 999 (EXP 4,294,967,290) both exit the result screen, clear EXP to 0 and survive relog. The level-up refill lost two energy at 998→999 (857/431 shown instead of 859). Fixed and client-rerun. |
| 4b. #20 mission 234 | **Not reproduced.** First clear and replay with the Cordelica unlock and its cutscene: no crash, no dump, no server error. Tested conditions are recorded rather than a fix invented. |

## 2. Changed files (working tree; nothing staged)

SHA-256 prefixes. "before" is the QC inventory of October 27 (`out/qa/current/qc_20261027/source_inventory.json`). A dash means the file was clean at HEAD or did not exist.

| File | before | now | Change |
|---|---|---|---|
| `scripts/check_mst_bootstrap.py` | 665d5bd4bca5edb4 | 24a916e563a9fff5 | `read_manifest`: object shape first, then Glaze's typed read (a quoted, null, bool, fractional or out-of-int32 number skips the manifest). `part_names` is lazy, so a 2e9 part count stops at the first absent part as the server does. |
| `scripts/test_mst_bootstrap.py` | f3ea1d29aa38e7e1 | 73d91b5673afa9fb | 13 malformed staged manifests plus an accepting control, offline and in server mode; `read_manifest` unit cases. |
| `scripts/apply_content_patches.py` (new) | – | d54c0961d490e2c9 | Maintained content patches: report or `--apply`; refuses unknown bytes; replaces the path rather than editing in place. The tuto8 record carries the evidence in its `why`. |
| `scripts/test_content_patches.py` (new) | – | 55e658e3e78fc8eb | 13 cases on disposable roots: anchor refusal, usage error, absent, recovered shipped bytes, report-only, apply, hard-link twin kept, BOM/CRLF kept, idempotent no-rewrite, unknown refused, LF variant unknown. |
| `scripts/run_bugfix_regressions.py` | 8e8a84f5994946d8 | b0437e35e313d1fb | Registers `test_content_patches.py` (63 entries). |
| `scripts/test_bugfix_qc_wire.py` | – | f6446aa33bd45314 | Two level-up energy checks (998→999, 7→8) starting from a spent bar. |
| `standalone_frontend/main.cpp` | 131f81a0efc4e67f | 4735cd397b5dcff7 | Comment only: idle retention is defensive, not the title-hang cause (console QuickEdit). `setIdleConnectionTimeout(0)` kept. |
| `gimuserver/gme/common/Energy.cpp`, `.hpp` | – | 07ff71a933bc83bb / 50bda38a292cf380 | `refresh(..., storedLevel)`: derive the stored energy at the level it was stored under. |
| `gimuserver/gme/handlers/Mission.cpp` | 7ed18dc695ed3f60 | 93c8e68e9dbdf79d | Passes `currentLevel` to that refresh (one hunk; diff vs QC snapshot checked). |
| `tools/bin/pe_x86.py` (now visible to git) | – | ed4fe9ad2f953796 | `BF_CLIENT_EXE`; a missing client EXE exits with a named prerequisite. Original kept at `Q/prior/pe_x86.py.before`. |
| `.gitignore` | – | caa309c38b0a48d1 | `tools/` → `tools/*` with exceptions for exactly `pe_x86.py`, `x86_bundle_exclusions.py`, `x86_mst_bootstrap_check.py`, `x86_startup_rules_check.py` (verified: nothing else in `tools/` becomes visible). |
| `RELEASE.md` | – | 53f0940f501dfdf7 | "Content corrections and regression prerequisites". |
| `docs/BF_OFFLINE_SERVER_HANDBOOK.md` | e5e6c5227f90a473 | d7625ce312e7a935 | Dated current-status entry (October 28) above the historical sections. |
| `docs/BUG_ACCEPTANCE_2026-10-27.md` | – | 7136587d10a844ae | Round-1 column and findings. |

Diffs: `Q/round1_clean_at_head.diff` (files that were clean at HEAD, so exactly this round) and `Q/round1_on_dirty_files_full.diff` (full working-tree diff of the already-dirty files; this round's hunks are the ones described above). Backups of edited files are in `Q/prior/`. The October 27 QC build output, the Session 19/20 candidates and saves, and `deploy/` are untouched.

Not changed: the packet-generator and the offline proxy. Their state is as in the QC report.

## 3. Builds

- **Round-1 server candidate** `Q/bin/gimuserverw.exe`, SHA-256 `03101975f6ba7a1c919b063eaeab627a004f6f497a03d0aba11ffd2b4bde9e03`. Built by `bash Q/tools/build_server_round1.sh round1` (the QC recipe with a temporary runtime-output override, logs in `Q/build/`). Configure, build and restore all exited 0, and the cache is clean. The in-tree live EXE `f72310df…`, the QC build `c9405702…` and the S20 candidate `887e68e0…` are verified unchanged.
- It contains all current sources (the QC-reviewed tree plus this round's energy fix and comment).
- Runs before the energy fix used the QC build `c9405702…` (tutorial A/B, C1, M1). C1R and C2 used `03101975…`.

## 4. Tests (commands from the repository root)

| Command | Server | Result |
|---|---|---|
| `python scripts/test_mst_bootstrap.py` | – | 36 passed, 0 failed (`Q/test_mst_bootstrap_offline.log`) |
| `python scripts/test_mst_bootstrap.py out/qa/current/qc_20261027/bin/gimuserverw.exe` | c9405702 | 49 passed, 0 failed. The live server announces none of the 13 malformed manifests and announces the control (`Q/test_mst_bootstrap_server_c9405702.log`). |
| control: old `staged_rows` on the same tree | – | AttributeError as QC reproduced (`Q/old_staged_rows_control.log`) |
| `python scripts/test_content_patches.py` | – | 13 passed, 0 failed (`Q/test_content_patches.log`) |
| `python tools/bin/x86_mst_bootstrap_check.py` / `x86_startup_rules_check.py` | – | 37/0 and 184/0. With `BF_CLIENT_EXE` pointing at a missing file: exit 1 and the prerequisite message (`Q/x86_*.log`). |
| `python scripts/test_qa_runner.py` | – | 30 passed, 0 failed |
| `python scripts/test_bugfix_qc_wire.py out/qa/current/qc_20261027/bin/gimuserverw.exe` | c9405702 | **41 passed, 2 failed**: the new energy checks (857, 33), the control (`Q/test_bugfix_qc_wire_c9405702_control.log`) |
| `python scripts/test_bugfix_qc_wire.py Q/bin/gimuserverw.exe` | 03101975 | 43 passed, 0 failed (`Q/test_bugfix_qc_wire_03101975.log`) |
| `python scripts/run_bugfix_regressions.py Q/bin/gimuserverw.exe` | 03101975 | **63 entries, 0 failed, 2,692 checks** (19:17–19:40); the mission validator equals its baseline (3,117). Against the October 24 run on `887e68e0` (62 entries, 2,663), the differences are all accounted for: `test_content_patches.py` +13 (new), `test_mst_bootstrap.py` 38 → 49, `test_bugfix_qc_wire.py` 41 → 43, and `test_brave_slots_wire.py` 248 → 251. The slots checks at lines 148/150 run only when a pull wins a unit or item, so random prizes vary that count. |

Before the full run, `out/qa/current/SUMMARY.*`, `validate_missions.json` and the 103 per-suite logs were copied to `Q/prior/qa_current_reports/` (they held the October 22 run, the stale top-level report the QC noted). The full run replaced them. It is now the top-level report, deliberately left in place as the latest complete run, and a copy is in `Q/regressions_full_03101975/`. Its console log is `Q/full_regressions_03101975.log`. This is a completed full pass on the round-1 candidate, unlike the October 27 partial log.

Proxy unit tests and the cargo workspace were not rerun: no proxy or packet-generator source changed.

## 5. Visual runs (isolated)

Setup, used for every run:

- Client: the S19 Fresh package (unchanged DLL `9633061d…`; never reset or reinstalled).
- Server on 29961 with its own config, passed as an absolute argv[1].
- Relay on 29960: `Q/tools/http_relay_delay.py`. It can hold replies to named group ids; ids travel in plaintext in the envelope.
- Each run uses its own SQLite-backup copy of a save, under `Q/visual/saves/<run>/`.

Forward-slash checks:

- Every config path is absolute with forward slashes (`Q/tools/prep_run.py` asserts it).
- Before the client launched, the server was confirmed to hold the run's database open (an exclusive-open probe in `Q/tools/run_ctl.ps1`).
- No stray `.sqlite` appeared.

Content root: a disposable hard-link mirror of `deploy/game_content` at `Q/content_root/` (152,219 links). Its `tuto8.txt` is a separate file, so `deploy/` was never written.

Input went through the established `bf_drive.ps1` (foreground and uncovered checks, PrintWindow captures), from a non-elevated session. Frida was not used. No consent, age or Terms screen appeared. Captures: `Q/visual/run/`.

| Run | Server | Save (copy of) | Condition | Observation |
|---|---|---|---|---|
| A | c9405702 | S19 blank, tutorial 2/0 | shipped tuto8 `cfa85733…` served (2,250 bytes text-mode in the provider log); replies to `YPBU7MD8`/`ynB7X5P9` held 5 s | **Race reproduced.** Hand on Home's Quest; quest select shows plates and NEW AREA but no arrow after the release (`A_11_sheet`). Two plate taps ignored (`A_13_sheet`), no request sent. |
| B | c9405702 | same | `apply_content_patches.py --apply` on the mirror → `271511d9…` (2,278 bytes served); same 5 s holds | **Correction holds.** No arrow during the held reply; the arrow appears after the release and stays (`B_11_sheet`). The plate tap opens the mission select, whose own held round trip also keeps its arrow (`B_21_sheet`). Begin Quest arrow on MissionCheckScene (`B_31_sheet`). Stopped before the battle; both copies stayed 2/0. |
| C1 | c9405702 | completed tutorial (12/1), set to Lv 998, EXP 1,023,049, energy 431 | Adventurer's Prairie "Start of Adventure" on auto | Level-up animation served. Result "XP to next Lv 0", Lv 999. Result, capture and unit pages exit to Home. Save: 999 / EXP 0. **Header 857/431** (expected 859, the defect). Relog: same. (`C1_*`) |
| C1R | 03101975 | same values | same mission | Result exits; save 999 / EXP 0; **header 859/431** (`C1R_21_header_zoom`). |
| C2 | 03101975 | completed, set to Lv 999, EXP 4,294,967,290, energy 425, refill timestamp +540 s | login, wait one tick, same mission, relog | Login fine, 425/431 "REFILL IN". **425 → 427** at the tick, countdown restarts at 3:00. Result "XP Obtained 0", exits; save EXP 0. Energy 424 after spending 3 (consistent). Relog 426/431 after the next tick (`C2_*`). |
| M1 | c9405702 | completed save + `Q/tools/prep_m234.py` (53 prerequisite clears, five real unit rows copied read-only from the live save, energy 100, then level 150 for the squad cost limit) | first clear of 234, relaunch, replay | Asekutt intro scene, 234 NEW. **First clear**: results, 6 captures, the Cordelica cutscene (Eriole: "head to Cordelica"), Cordelica region with Greskya Caves NEW AREA. Relaunch: land select lists NEW Cordelica; Morgan shows Asekutt CLEAR and Shrine of Lystia NEW AREA. **Replay**: results, captures, back to Asekutt; no cutscene repeat. Process alive at every step, no crash dump, no server ERROR. Save: 234 clear_count 2. |

The first M1 start (level 1) stopped at the client's own "Cost limit exceeded" dialog. That fixture issue was fixed by setting level 150 on the copy. Its log is preserved as `server_start1_level1.log`.

Operational notes:

- `run_ctl.ps1 stop` first matched only the default candidate's path, so one round-1 server kept running after a stop. A later start refused the busy port. It now stops both candidate paths.
- `bf_drive` coordinates: Begin Quest is at (422, 850); the battle AUTO toggle is (598, 407); the Home carousel moves one item per 120-px drag.

## 6. Preserved identities (checked at the end)

- Live save `deploy/gme.sqlite` `239f9e69…`. Read only, for unit rows; the live server was not running.
- Live content `deploy/game_content/content/tuto/tuto8.txt` `271511d9…`, link count 1.
- In-tree EXE `f72310df…`.
- S19 source saves: blank `7b026db2…` (still tutorial 2/0) and completed `a2f601fe…`. Both unchanged.
- The S19 Fresh package LocalState was updated by these launches, as in every earlier run. No files were removed from it.
- Prior evidence kept: `Q/prior/session19_mst_bootstrap/` (the bootstrap test's fixture before its regeneration), `Q/prior/qa_current_reports/` and the backups of edited files.

## 7. Remaining checks and gaps

- **tuto8:** a cold first visit to the quest select with its scene assets not yet in LocalState was not run (all runs had them cached). The x86 frame-pause under the modal was observed (A/B), not traced in x86 code. The release builder does not yet run `apply_content_patches.py`; it should refuse to package an unpatched root.
- **#33:** watch the 2-per-tick bar fill to the cap once on screen. After deployment, a save that levelled up on the old server keeps its two lost energy (harmless; no repair proposed).
- **#20:** no real-progression save reaching 233 exists. The fixture's synthetic clears plus a strong copied squad differ from a natural player's path, battle length and scenario markers. If the report returns, get the reporter's client and platform and their dump (`%LOCALAPPDATA%\CrashDumps`). Greskya Caves itself was not entered.
- **Observation:** the login campaign shows "Campaign Ends In: 0D 0H 0M" while still claimable (not investigated).
- **Unchanged from the assignment:** presentation/assets (#22-1/2/3, #27, the star glyph, #24 regression on the final DLL); progression/rewards (#23 Morgan, #30, #26, #39, #25, #40); missing content (#12, #17, #18, #15); the startup-provider release gates (area ordering, two aliases, five unknown groups, eight absent names, nine families).
- **Release preparation** (after client acceptance, per the assignment) has not started.

Suggested next round: the client-regression rows first. Start with #24 on the final DLL and #30's compound unlocks (both have server coverage and short client routes). Then #23's Tower of Mistral final stage, which can reuse `prep_m234.py`'s chain-clearing approach for mission 85.
