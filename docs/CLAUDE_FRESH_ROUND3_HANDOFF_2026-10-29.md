# Fresh session, round 3: handoff (run October 29–30, 2026)

The round of [CLAUDE_FRESH_ROUND3_2026-10-29.md](CLAUDE_FRESH_ROUND3_2026-10-29.md), after the round-2 QC ([QC_FRESH_ROUND2_2026-10-29.md](QC_FRESH_ROUND2_2026-10-29.md)). It was started automatically when Evan's usage window reset. This is a transient coordination note.

Nothing was staged, committed, pushed or deployed. No package was reset, signed or installed. No live save was written. The live save was opened read-only only for the helper tests' refusal check. The Word tracker was not touched; its last update is Codex's October 28 notes, and this matrix is the latest status. Per-item status: the round-3 column of [BUG_ACCEPTANCE_2026-10-27.md](BUG_ACCEPTANCE_2026-10-27.md).

QA output: `out/qa/current/fresh_round3_20261029/` (`R3/` below). Working notes with every number: `R3/PROGRESS.md`.

## 1. Results

| Target | Result | Runtime/data change |
|---|---|---|
| QA helper corrections (QC P2/P2/P3) | **Done.** New `scripts/qa/` with tests (43/0) and `scripts/QA_RUN_HELPERS.md`. | none (QA tooling only) |
| 59 vs 60 rapid taps | **Reconciled from the index:** 59 tap rows = Summon + confirm + **57 gate taps**. Matrix and round-2 handoff corrected. | – |
| Matrix intro: "unchanged" Word file | Corrected: the Word file received dated notes on October 28; the matrix is the latest status. | – |
| #27 slots | **Client-observed correct; blank tiles not reproduced with the provider DLL** (§3). | none |
| #39 Metal Parades | **Played in the client: all three tiers, plus replay and expiry.** Entry, lists, XP, boss waves and results match the archive. **Finding: the client holds at most one capture per battle wave** (reproduced deterministically and traced in `libgame.so`); the server pre-roll is correct. Not changed: Evan's call (§4). | none |
| #14 portraits | **Goddess Tilith 50254 defect reproduced, then fixed in a disposable candidate and accepted in the client** (crop `298,48,180,492`, staged `F_UNIT_MST` v1087). Seria 10233 and the control 50253 are unchanged; the unit screens are correct (§5). Not deployed. | data: v1087 download file in a disposable content root only; server unchanged |

The server binary and its MST, archive and system roots are unchanged, so no server candidate was built and the full regression was not rerun. The verified baseline is still round 1's candidate `03101975…`: 63 entries, 2,692 checks, 0 failed (October 28). The only data candidate is the #14 client download file (`F_UNIT_MST` v1087) in a disposable content root. It was checked with `test_mst_bootstrap.py` (36/0), `check_mst_bootstrap.py` and the client run. The #39 fixtures are disposable archive copies used only by QA runs.

The round resumed automatically on October 30 after the usage reset, for #14's fix and #39's client play.

## 2. QA helpers (`scripts/qa/`, maintained; round-2 copies kept under `fresh_round2_20261029/tools` as evidence)

| File | SHA-256 (prefix) | What changed versus round 2 |
|---|---|---|
| `qa_run.py` | see §7 | New shared layer. Run names must match `[A-Za-z0-9][A-Za-z0-9_-]{0,62}`. The run folder must resolve to a direct child of `<qa>/visual/saves`, and the qa dir must lie below `out/qa/current`; traversal, rooted, drive and junction escapes are refused. `QaError` replaces asserts. Process identity is pid + creation FILETIME + image read through a Win32 handle, with command lines from a CIM snapshot tied to the same creation time. `db_probe` returns free / held / missing and is labelled "some process holds it". |
| `prep_run.py` | | Validated run name; refuses an existing run, the live save, a source without exactly one account, MST/archive/system roots missing any `deploy/` json, and `--set` outside an allowlist or non-integer. New `--archive-root`. Forward-slash absolute config paths. |
| `prep_chain.py` | | Follows **all** comma prerequisites of the mission, its dungeon and its area (the server's rule), transitively, with cycle and missing-row refusal. Refuses an already-cleared target and a copy without exactly one account. Optional squad copy from the live save (read-only). Reproduces 234 → 53 and 84 → 32; Lizeria 700 → 259, including 666 and 20067. |
| `run_ctl.py` | | Start refuses busy ports, an existing client and a held database. It records each process it starts in `run.json` (pid, creation time, image, required command-line parts), requires the listener's owner to be the recorded server (or a child with its image) or relay, and on failure stops only what it recorded. Stop re-verifies identity on the handle it terminates through and refuses mismatches (exit 1). The client gets WM_CLOSE only when it is the sole S19 client. |
| `http_relay_delay.py` | | Round-1 relay, body unchanged; maintained copy. |
| `test_qa_run.py` | | 43 checks (run: `python scripts/qa/test_qa_run.py`). Dummy processes that listen themselves or through a forked child, never listen, exit, or bind late. An unrelated twin with the identical command line survives every stop and failed start. Tampered creation time, image or command line → refused. Plus name, containment, junction, prep, chain and probe cases. Logs: `R3/test_qa_run_1.log` (failed: a PowerShell quoting bug in `snapshot()`, fixed), `_2.log` 42/0, final 43/0 (archive-root case added). |

Real use: every round-3 run was started and stopped with `run_ctl.py`.

- On start, the server's own pid owned 29961 and the relay owned 29960.
- Only one `gimuserverw` process plus its `conhost.exe` exists. Round 1's "two servers" came from the old PowerShell launcher.
- Stop closed the client by WM_CLOSE and terminated exactly the recorded relay and server.

## 3. #27 slots

Fixtures, all copies of `save_after_s20_completed_check.sqlite` with 300 Brave Medals set in the copy:

- `s1_fixture` uses the disposable archive `R3/archive_slots_fixture`, built by `R3/tools/make_slots_fixture.py`: hard links, except `brave_slots.json` (`19fbc02b…`).
  - It keeps the 54 production rows; only `jewel_god` (unit 50133), `item_70300` (God Crystal ×3), `sphere_30008` (Sparkle Edge) and `raid_medal` keep weight 1.
  - Production `deploy/archive/brave_slots.json` is `db4a403d…`, untouched.
  - Zel and gem rows are dropped at server load (the popup can't draw them), so units, items, spheres and medals are the reachable reward types.
- `s2_control` uses the production archive.

Results:

- **Counts.** The count box steps +1 per tap and stops at ×10.
  - The client asked for 1, 2, 3, 3, 4, 5, 6, 7, 8, 8, 9, 10 and 10 pulls; the server played exactly each.
  - The repeats of 3 and 8 are taps that landed during the machine's return animation (driver timing).
  - One SPIN tap in that animation sent nothing (`S1_roll11_peek.png`).
- **Awards.** For every roll, the inventory delta equals the server's logged pulls: medals −3 per pull and +1 per medal prize, +1 unit per Jewel God, +3 per God Crystal prize, and one unique warehouse row per sphere. The per-roll before/after files are `R3/visual/saves/s1_fixture/inv_*.json`, with the per-roll log `rolls.log` (UTF-16).
- **Mixed and duplicate.** For example, a 10-pull roll gave Jewel God ×4, Sparkle Edge ×3 and Raid Medal ×3.
  - Control, 10 pulls on production odds: Healing Imp ×2, Burst Frog, Jewel King, Light Crystal, Smoke Bomb ×3, Beacon ×10, Carnage Edge and Raid Medal ×2. Inventory: +5 units, items +10 and +3, one sphere row, medals 297 − 30 + 2 = 269.
- **Tiles.** "Receive Rewards" first shows "?" cards and reveals them in sequence (`S1_roll11_tilelist`, about 18 s to 24 s).
  - Every settled list showed all N tiles with art, in the server's pull order (`f_033909` fixture, `C2_r10/f_033675` control).
  - Item and sphere tiles carry a name; unit and medal tiles don't.
  - The single "GET!!" popup's background counter shows the balance after charging and before medal prizes. The machine, after return, shows the credited balance (= database).
- **Relog** (stop, then start the same copy). The machine shows 95 medals, the database value, with the count reset to ×1.
- **Art source.** `unit_ills_thum_50133` and `item_thum_70300` were served by the content broker on demand (the S19 DLL's provider is active); Sparkle Edge was already cached.
- **Remaining dependency (not reproduced, not closed).** The live client runs the May proxy with the broker off. There, the result list can only draw art already cached, since `RandallSlotResultListScene::downloadFiles` requests nothing. Blank tiles therefore remain plausible on the live client until the provider DLL and broker ship. A provider-off client run was not done.

## 4. #39 Metal Parades

The wiki pages' current revisions are still those cited by `scripts/gen_metal_parades.py`: Parade:Metal 630717, Parade:SMP 630719, Parade:MMP 630716, all 2019-11-22 (`R3/wiki/`, fetched through the MediaWiki API).

Matches the wiki:

- Normal: Ghost 10 / King 35 HP, 5 battles; its boss "Dark King + 2 Ghosts" is one of the page's formations.
- Super: no Ghosts, King 35 / God 100 HP, 5 battles, 3,000 EXP.
- Mega: no Ghosts or Kings, God 100 / Crystal 300 HP, 5 battles.

Discrepancies:

- **Super's boss stage is a lone Dark God, and Super has no Crystal.** The wiki gives "1 God and 2 Kings" or "1 Crystal". The generator's comment says it kept the base parade's lone-boss finale, so this is authored. It was not changed this round, because choosing which King/God elements form the boss would still be authoring; this is a decision for Evan.
- Mega EXP 5,000 (MST and archive) vs wiki 7,000: the known version difference.
- Authored: DEF 999999 (the pages' "1 damage per hit"), capture 60 %, and the stage 1–4 layouts.
- MST energy 5 vs archive 0; the wiki says 0 energy since 2018-03-21.

Entry costs 1/3/5 Metal keys (`DungeonKey.cpp`, DungeonKeyMst usage pattern).

### Client play (October 30)

Run `m1_parade` was a copy of the completed S19 save, made with `R3/tools/prep_parade.py`:

- 9 Metal keys and a squad copied read-only from the live save.
- Level 150 in the copy, because the level-1 cost limit refused the squad.
- Server `03101975`; captures in `R3/visual/run/M1_*`, `M2_*` and `M3_*`.

| Check | Result |
|---|---|
| Entry | Parade Garden is the last tile of the main Vortex list. The locked Metal tile offers "Use 1 / 3 / 5 Keys"; the confirmation says "Time limit is 30 mins"; the server logs each spend. |
| 100600 Metal Parade! | List: six elemental parades plus "Metal Parade!", Energy 0, Battles 5. On AUTO: battle 1 Fire/Water/Earth Ghost, boss Metal King. Clear: XP 1,000 (archive and wiki), Zel 92, Karma 74, level 150 → 151. First clear queued 2 presents. |
| Replay | Within the window the tile opens straight to the list with no key spent (8 kept). The replay also paid 1,000 XP and gave no second first-clear reward. |
| Expiry | After 30 minutes the tile re-locked and the 1/3/5 choice returned. |
| 100601 Super | 3 keys. The list shows 100608–100613, which share the plain elemental names in `mission_mst`, plus "Super Metal Parade!". **Boss wave: a lone Metal God (Dark God)**, as authored (`M3_super_boss.png`); the wiki discrepancy in §4 stands. Clear: XP 3,000 (archive and wiki). |
| 100614 Mega | Run `m2_mega`: m1's copy with the key state reset in the copy instead of a second 30-minute wait. 5 keys; the list holds only "Mega Metal Parade", Energy 0, Battles 5. Boss: a lone Dark Crystal. Clear: XP 5,000 (MST; wiki 7,000, the known difference). |
| Results and database | Every "Units Acquired" list equals the units the copy gained. Zel and Karma match the clear screens. |

#### Finding: at most one capture per battle wave (client design; not changed)

- **What happened.** The client captured 5, 4 and 4 of 16 monsters (100600 twice, then 100601), well below the authored 60% per monster.
- **The server's pre-roll is correct.** `R3/tools/parade_capture_rate.py` ran 100 starts of 100600 on an isolated testkit fixture: 951 of 1,600 = 59.4%.
- **Deterministic reproduction.** I played Mega on `R3/archive_capture_fixture2`, where every 100600/100614 monster is at 100% and everything else is production.
  - All 14 captures were pre-rolled, but the client acquired 5: exactly the **first** monster of each wave (Fire God, Thunder God, Water God, Light Crystal, Dark Crystal).
  - The in-battle counter went ×04 → ×05 at the boss.
- **Cause, read in `libgame.so` with `tools/bin/so_*`:**
  - `MissionDropInfoList::parse` @0x12671C8 splits `drop_info` (LE6JkUp7) on `@`, then each record on `|`. It calls `MissionDropInfo::parseUnitDropInfo` @0x126617C once, on field [1].
  - That function splits on `:` only and builds one drop (`groupOrder:_:unitId:level:type[:extraSkill]`).
  - `MissionDropInfoList::getObject` @0x1267150 returns the first record whose battle order matches.
  - So the client cannot hold two captures in one wave. The extra drops the server joins with `-` (`MissionArchiver::encodeMissionDropInfo`) are dropped. As a side effect, the first drop's type token reads `"1-1"` and token [5] lands in its extra skill.
- **Arithmetic check.** With one capture per wave, the expected count per run is Σ(1 − 0.4ⁿ): 4.7 for 100600 and 4.4 for 100601, matching what was observed.
- **Options for Evan** (no runtime change made):
  - (a) Accept it as the original client's rule.
  - (b) Emit at most one drop per wave. That gives a clean wire and the same player outcome, and needs a candidate and full regression.
  - (c) If more captures are wanted, the only lever is per-wave odds; per-monster odds above one per wave never reach the player.
- **Documentation gap.** `packet-generator/assets/net/mission.kdl`'s `drop_info` doc does not state the one-per-wave rule.

## 5. #14 portraits

Fixture: copied save `p1_portraits` (`R3/tools/prep_portraits.py`) with Tilith 50253 (control), Seria 10233 and Goddess Tilith 50254 (leader) in one Home squad. The rows are copies of the save's own first unit with only species and element changed: a display fixture, not a stats test. Captures: `P1_06_home.png`, zoom `P1_06_home_squad_zoom.png`.

- **Tilith 50253 (control):** framed on face and upper body; the September 27 authored crop `303,50,180,492` holds.
- **Seria 10233:** framed correctly (`545,33,174,475`). The report's literal unit shows no defect.
- **Goddess Tilith 50254: defect reproduced.** The panel shows hair and dress, with the face cut off at the right edge. Its MST crop is still `245,-5,135,369`.

**Fix (October 30): candidate built and accepted in the client; not deployed.**

- **Crop.** `298,48,180,492` (x, y, w, h) on `unit_ills_full_50254.png` (906×606, `5700048b…`). It keeps the 50253 fix's 180×492 size, since both arts share the canvas and the figure height, and centres it on her face and body. The candidates are rendered as panels in `R3/crop/crop_candidates.png`.
- **Staging** (`R3/tools/stage_unit_crop.py`), into disposable roots only:
  - `R3/crop/unit_mst.json` (`0c38f831…`) is the proposed `deploy/mst/unit_mst.json`. It differs from deploy's (`193d3b9f…`) in exactly that one line, with CRLF line endings kept.
  - `R3/content_root_crop/` is a hard-link mirror of the round-1 content root. Its new `manifest.json` (`74f51627…`, version 1087) and new `Ver1087_2r9cNSdt.dat` (`2a5d342e…`) were written by `tools/gen_mst_download.py`'s own `build()`, with only the output folder and row source redirected.
  - The deploy staging folder and `deploy/mst/unit_mst.json` were re-hashed afterwards: unchanged.
  - v1087 decrypts to exactly the proposal's 2,291 rows.
- **Server data roots unchanged.** Initialize carries no UnitMst, and no server code reads `home_img_pos`, so the only client-visible change is the download file.
- **Checks.** `test_mst_bootstrap.py` 36/0. `check_mst_bootstrap.py --document-root R3/content_root_crop` exits 0 ("F_UNIT_MST required available v1087"). The full regression was not rerun: the server binary and its MST/archive/system roots are unchanged.
- **Client run `p2_crop`** (copy of the `p1_portraits` save, server `03101975`).
  - The server offered v1087, and the client fetched `/mst/Ver1087_2r9cNSdt.dat` from the crop root.
  - Home (`P2_04_home.png`; before/after `P2_04_home_before_after.png`): **Goddess Tilith is framed on her face and upper body**, and 50253 and Seria are unchanged.
  - Manage Squad draws battle sprites, not the crop, and all three look fine.
  - The View Units thumbnails and the unit info art for all three are correct (`P2_info_sheet.png`). That screen uses other position columns, which are unchanged.
- **To deploy (Evan's decision):**
  1. Apply the one-line change to `deploy/mst/unit_mst.json`.
  2. Run `python tools/gen_mst_download.py F_UNIT_MST 1087`. The generator is deterministic, so it should reproduce `2a5d342e…`.
  3. Restart the server.
- **Side effect.** The S19 client's LocalState now caches v1087, so a later run on a v1086 root re-fetches v1086.

## 6. Untested / blocked

- #14: deployment of v1087 (Evan's decision) and a player check on the live client after its download.
- #39: Super's wiki boss formation and the one-capture-per-wave policy are Evan's decisions. Elemental parades (100602–100613), the Jewel and Imp parades, and a natural-progression save were not played.
- #27 with the live (provider-off) client configuration.
- Natural-progression saves (all fixtures are synthetic).
- Audio, and frames between captures.
- New dependency recorded: #27's live blank-tile fix depends on shipping the provider DLL and enabling the broker.
- **Leftover process:** `gimuserverw.exe` pid 16196 is a zombie from my first `parade_capture_rate.py` run.
  - That run passed a relative archive root; the server's working directory is its fixture, so it hit a fatal error at boot.
  - Stop-Process could not end it, which matches the known "until reboot" behaviour.
  - It never listened on any port. Its config is `out/qa/current/round3_parade_capture_rate/config.json`. The script now resolves the path.

## 7. Hashes

Round-3 files (SHA-256 prefixes; recomputed at the end of the round):

- `scripts/qa/qa_run.py` `ce2be287b1e7043b`
- `scripts/qa/prep_run.py` `e971111b41acbd56`
- `scripts/qa/prep_chain.py` `95379559da64b9c1`
- `scripts/qa/run_ctl.py` `ae4cf4bc40061e3b`
- `scripts/qa/http_relay_delay.py` `6c25da91eb8ab9a4`
- `scripts/qa/test_qa_run.py` `29a57432f12b8c31`
- `scripts/QA_RUN_HELPERS.md` `4cfe75eba15c25ee`
- `docs/BUG_ACCEPTANCE_2026-10-27.md` `c6dde68b662f0f3d` (after the October 30 update; backup `R3/BUG_ACCEPTANCE_before_round3_resume.md`)
- `docs/CLAUDE_FRESH_ROUND2_HANDOFF_2026-10-28.md` `801690e802b7cc19`
- `out/qa/current/fresh_round3_20261029/tools/make_slots_fixture.py` `b110304bed7d9d65`
- `out/qa/current/fresh_round3_20261029/tools/slots_inventory.py` `10c72dd5ddad0429`
- `out/qa/current/fresh_round3_20261029/tools/slots_rolls.ps1` `1760e47e9718c4f7`
- `out/qa/current/fresh_round3_20261029/archive_slots_fixture/brave_slots.json` `19fbc02b2a356f1d`
- #14: `R3/tools/stage_unit_crop.py` `8034ada93f79c3ad`; proposal `R3/crop/unit_mst.json` `0c38f8311dd4f66f` (deploy today `193d3b9fd13dd242`); `R3/content_root_crop/content/F_UNIT_MST/Ver1087_2r9cNSdt.dat` `2a5d342e8932e6cc`; its `manifest.json` `74f51627400b00bc`; deploy staging unchanged (manifest `ac5291b79cbee4b0`, Ver1086 `549aa0da695d174f`)
- #39: `R3/tools/prep_parade.py` `52404a5ace3c55b4`; `R3/tools/parade_capture_rate.py` `2e3685783e64ee73`; `R3/tools/make_capture_fixture.py` `c7f25950b58977ef`; fixture `archive_capture_fixture/mission.json` `51490c975f44f5dd` (100600 only, used by the wire check) and `archive_capture_fixture2/mission.json` `63c67960833f4fbc` (100600 + 100614, used by the Mega client run)

Identities unchanged: candidate `03101975…`, S19 DLL `9633061d…`, live save `239f9e69…`, live tuto8 `271511d9…`, in-tree EXE `f72310df…`.

## 8. Next bounded round

1. Evan's decisions:
   - deploy the #14 crop (steps in §5);
   - Super's boss formation;
   - the capture policy (§4: accept one capture per wave, emit one drop per wave, or change per-wave odds).
2. If (1) changes server data or code: build a candidate, run the full regression with old reports kept, then do a client check.
3. Optionally, a provider-off #27 run to show the live blank-tile behaviour.
4. Reboot before the next round to clear zombie pid 16196 (§6).

Release gates are unchanged:

- The packaging check must require the corrected tuto8 bytes; the generic tool's absent-target case exits 0.
- Missing content (#12/#15/#17/#18).
- Startup area/alias/family completeness.
