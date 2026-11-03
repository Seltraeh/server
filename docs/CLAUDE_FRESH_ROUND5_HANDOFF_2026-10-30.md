# Fresh session, round 5: handoff (October 30, 2026)

The round of [CLAUDE_FRESH_ROUND5_2026-10-30.md](CLAUDE_FRESH_ROUND5_2026-10-30.md), after the round-4 QC ([QC_FRESH_ROUND4_2026-10-30.md](QC_FRESH_ROUND4_2026-10-30.md)). This is a transient coordination note.

Nothing was staged, committed, pushed, deployed, signed or installed; there was no reboot and no live save or deploy content was written. `deploy/mst/{unit,leader_skill}_mst.json` and both staged F_* folders were hashed before and after: unchanged. The Word tracker was not touched; the matrix's new Round 5 column is the latest status. QA output: `out/qa/current/fresh_round5_20261030/` (`R5/`).

Processes: the only `gimuserverw.exe` running besides my runs was the round-4 leftover pid 11336. It holds the old `metal_parades` fixture, listens on no port and was not touched. Every run of mine was started and stopped by `run_ctl.py` (identity-checked).

**Scope actually done.** Phase 0, #22-2 and #22-3 (diagnosis), and Phase 3. The round budget ran out before #22-1, #26, #25 and #40; §6 lists exactly what remains, without fixes or conclusions for them.

## 1. Results

| Item | Status | Change |
|---|---|---|
| Phase 0: QC P2 temporary-file write-through | **Fixed, automated-tested** | `scripts/unit_mst_corrections.py`, test |
| #22-2 Amadream header | **Reproduced, cause located in client code; not fixed** (Evan's decision) | none |
| #22-3 Galtier leader text | **Diagnosed: every word is shown; no data truncation.** A data fix was tried in the client and withdrawn as ineffective. | none kept |
| Phase 3 content scope | **Done:** `docs/REMAINING_CONTENT_SCOPE.md` | new doc |
| #22-1, #26, #25, #40 | Not worked (§6) | — |

Runtime is unchanged, so no server was built: candidate `c28a8db9…` from round 4. Its regression evidence is **historical**:

- the first round-4 full run: 65 entries passed;
- the later 66-entry aggregate: one fixture-lock error (`test_metal_parades_wire.py`, the database held by pid 11336); that suite passed 18/0 in a fresh fixture. That aggregate is not a clean pass.

This round's only maintained code change is the correction tool, tested 34/0.

## 2. Phase 0: temporary files (`scripts/unit_mst_corrections.py`)

**Fix.** `replace_into` now creates its temporary file with `tempfile.mkstemp` in the destination folder, which is `O_CREAT|O_EXCL` under a random name, so no existing file or link is ever opened.

- The file is written and closed, then `os.replace`d over the target. That replaces the directory entry, so a linked target's other names keep their bytes.
- On any failure it removes only the temporary file it created.
- `propose` now goes through `replace_into`, and refuses an `--out` that is a link or not a regular file. It inspects the given path before resolving it, so a symlinked output can no longer redirect the write.
- The stale-part check in `stage` now matches only real part names, `Ver<N>_<key>[_<n>].dat`. A planted `Ver1087_….dat.tmp` had been refused as a split; the new test caught that.

**Tests** `scripts/test_unit_mst_corrections.py`: **34 passed, 0 failed, no skips** (`R5/test_unit_mst_corrections_5.log`). The 7 new checks:

- `propose` with a hard-linked `<out>.tmp` in place: the sentinel is unchanged and the output is the accepted `0c38f831…`;
- planted names untouched (still a hard link; dangling link still dangling);
- a symlinked output is refused and its target unchanged;
- `replace_into` over a hard-linked name swaps the entry and the other name keeps its bytes;
- an injected rename failure leaves nothing behind;
- no temporary files remain;
- staging with hard-linked old-style `.tmp` names in place reproduces the accepted v1087 part and manifest, with the sentinel intact.

**Probe.** The QC's own `temp_probe.py`, rerun on the fixed code (`R5/temp_probe_rerun`): "modified: False".

**Generalization.** The tool gained a `Spec` per table (`--table`, default `F_UNIT_MST`), and the unit table's module names are kept. A second table was tried and withdrawn (§4), so `SPECS` holds only `F_UNIT_MST`. The accepted portrait proposal is unchanged (`0c38f831…`, `R5/unit_check`).

## 3. #22-2 Omni Amadream 51337 header

- **Fixture.** Run `u1_text`: a copy of the completed S19 save, level 150, with a squad of Galtier 860428, Durumn 840278, Amadream 51337, Sero-Anya 850628 and Lukroar 51247 (`R5/tools/prep_units.py`; display fixture). Server `c28a8db9`, content root round-1 (F_UNIT_MST v1086, the version the S19 cache already held).
- **Observed** (`R5/visual/run/U1/info_sheet.png`).
  - Amadream's unit info header wraps "Intact Champion Amadream" onto two lines. The upper line, "Intact Champion", overlaps the "Unit No." line and the rarity and element icons: the "jumbled" header of the report.
  - Controls on the same screen render on one line: "Viper Blitzkrieg Durumn", "Sacred Purpose Lukroar", and "Ruinous Despoiler Galtier", which at 25 characters is longer than Amadream's 24. So the trigger is rendered pixel width, not character count.
- **Source of the text and geometry.**
  - The name comes from the client's dictionary, `sgtext_units_443.zip` (51337 = "Intact Champion Amadream").
  - The header is not in `layout_unit_detail_userunit.csv`, which only lays out the stats, spheres and skill boxes, so its label width and wrap are client code.
  - No data or layout file fixes it without changing the official name, so nothing was changed.
- **Decision for Evan:** accept as a client layout limit, or approve a client change: a narrowly scoped header label width or shrink-to-fit in the x86 unit-info scene, then a repack and signing. Before choosing, measure all names against that width with the client font, to see how many units are affected.

## 4. #22-3 Omni Galtier 860428 leader skill

**Data.** Leader skill 801271 is 290 characters (`deploy/mst/leader_skill_mst.json`, staged `F_LEADER_SKILL_MST` v750 identical): line 1 is 185 characters, then `<br>`, then line 2 of 101 characters. Only three leader texts contain `<br>`; the other two (8950277/8950278) belong to no unit.

**Client** (`R5/visual/run/U1/galtier_ls_timeline.png`, `U2/galtier_v751_timeline.png`, `U2/galtier_v751_full.png`):

- The leader skill box (`layout_unit_detail_userunit.csv` `leader_skill_desc` 15,806 610x52, font 24) is a horizontal ticker.
- Its first piece is raw characters 0–252: line 1, plus line 2 up to "…20% damage reductio". That is exactly the cut in the report.
- After line 1 has scrolled past, the remainder, "n from Dark, Light types in Guild Raid", scrolls in beside line 1 at mid height.
- So **every word is shown**, but line 2 is split mid-word and its end appears out of place.
- Control Durumn 840278 (349 characters, its Guild Raid part joined with ". " on one line) scrolls through to "…for all allies in Guild Raid" in one line (`U1/durumn_ls_timeline.png`).

**Data change tried, withdrawn.** I staged `F_LEADER_SKILL_MST` v751 with 801271's `<br>` replaced by ". ", every word kept (`R5/content_root_ls`, part `5bb98ba4…`). The client downloaded it (`GET /mst/Ver751_4dE8UKcw.dat`), but the screen was identical to v750.

- The display text comes from the client's own dictionary, `sgtext_leaderSkillDescription_497.zip` (decoded: 801271 has the `<br>`), keyed by the skill id, not from the MST's English.
- The correction was removed from the tool; a code comment records why. The v751 root stays as evidence only.

**Decision for Evan:** accept (all text is shown), or re-issue the dictionary: `sgtext_leaderSkillDescription_498.zip` with `<br>` → ". " for 801271, plus an `F_RESOURCE_MST` naming it, then a client check. The client's refresh path for a cached dictionary has to be proven first.

## 5. Phase 3: `docs/REMAINING_CONTENT_SCOPE.md`

Re-verified on October 30:

- **#12:** 0 of 41 Vortex Trials missions are archived (all 41 still in `mission_mst`; 15 have client battle scripts).
- **#15:** 32 UnitMst units have no archive record (the 31 JP-only by intent, plus 30646). 135 archive units have no `unit_anime` in deploy content: a candidate list, not proven absence.
- **#17:** no battle fields or drops Fire Mecha God 10354; the Lv. 6 Sanctums 100320–100325 end on Totem/Idol bosses.
- **#18:** 12 AI records; 10 story bosses still use "Attack random", most of them at 10,000 HP; Xie Jing has no art.

The document gives identifiers, current state, missing pieces, evidence sources, dependencies, the acceptance route and unknowns per item. It ends with 7 ordered batches, starting with Juggernaut reusing its existing AI for mission 85, and two decisions for Evan (the #15 roster; marked authored values where no source exists).

## 6. Not done this round: exact remaining targets

- **#22-1 Lukroar 51247 gate.**
  - Summon Lukroar (normal and special gate) beside a known-good 7★ gate on the S19 client.
  - Record the scripts and SAM/plist/sound requests through the relay (`--relay-dump` covers only GME handlers; asset requests are in `relay.log`), and the animation sequence.
  - Distinguish absent gate art from the fixed #24 overlay.
  - Prerequisite: a deterministic way to pull 51247. A selector ticket (`project_selector_summons`) or a fixture gacha table on a copied save.
- **#26 Roglizer/Fuu.**
  - Fetch the exact wiki revisions for Roglizer 61286/61287 and Fuu.
  - Map each leader skill's process types (EXP 97 is already modelled) to the server reward code.
  - Run a deterministic mission with no leader, each leader and the helper on copies, comparing displayed amounts with stored deltas.
- **#25 Omni frog SP.**
  - Set up a migrated copy with an Omni unit below and at level cap, plus SP-granting fodder (identify it from the client fusion rules and `test_enhancement_sp_wire.py`).
  - Screenshot the fusion selection: greyed or selectable, and why.
- **#40 evolved UBB.**
  - Pick a documented evolution chain into a UBB-capable unit.
  - Check BB/SBB/UBB ids and levels, UI visibility, the unlock (SBB 10, UBB per `reference_unit_skill_columns`), relog, and no prior-form leakage.
  - Keep the Berdette controls.

## 7. Files (SHA-256 prefixes)

Maintained:

- `scripts/unit_mst_corrections.py` `4f0a2960a4fd82d8`
- `scripts/test_unit_mst_corrections.py` `1388ab4127aee971`
- `docs/REMAINING_CONTENT_SCOPE.md` (new) `dba537134c3cda1d`
- `docs/BUG_ACCEPTANCE_2026-10-27.md` `c66e2a7c149b6531` (backup `R5/BUG_ACCEPTANCE_before_round5.md`)

QA artifacts:

- `R5/tools/prep_units.py` `7a27e69ee2ac4fa7`
- withdrawn evidence `R5/content_root_ls/content/F_LEADER_SKILL_MST/Ver751_4dE8UKcw.dat` `5bb98ba4…`
- `R5/unit_mst_corrections_before_spec.py` (pre-generalization copy)

Identities: candidate `c28a8db9…`, S19 DLL `9633061d…`, live save `239f9e69…`. S19 LocalState: `F_UNIT_MST` v1086 and `F_LEADER_SKILL_MST` v751 (from `u2_ls`; the next v750 root makes it fetch v750 again).

Source inclusion: the maintained files above; exclude `out/`, saves, binaries, credentials and these coordination notes.

## 8. Next bounded round

1. Evan's decisions:
   - #22-2 (accept or approve a client header change);
   - #22-3 (accept or re-issue the dictionary);
   - #15 roster;
   - authored values policy;
   - plus the pending round-4 deploys (capture candidate, portrait, Super boss).
2. #40 UBB and #25 frog eligibility. Both are client observations on copies with data already present, so they are the cheapest of the remaining reports.
3. #26 with sourced revisions, then #22-1 once a deterministic Lukroar pull exists.
4. Content batch 1 (Juggernaut, mission 85) per the scope document.

Release gates are unchanged: startup completeness, tuto8 packaging enforcement, missing content, and a disposition for every original report.

## 9. Addendum: Evan chose "Fix them" (#22-2, #22-3)

### #22-3 Galtier: fixed in an isolated root and verified in the client

- **Tool.** `scripts/sgtext_corrections.py` (new). The fix re-issues the client's own dictionary under a new name, because a changed zip under the old name is never re-fetched (`existsLocalFile` has no hash check).
  - `sgtext_leaderSkillDescription_498.zip` holds the shipped CSV (decoded, base64 in 64-character lines) with only `MST_LEADERSKILLS_801271_DESCRIPTION`'s English field changed: `<br>` → ". ", every word kept. The other languages and every other row are byte-identical.
  - `F_RESOURCE_MST` v3276 has the same 49,342 rows in the same order and the same two-part split; only `/sgtext/sgtext_leaderSkillDescription_497.zip` becomes `_498`. The manifest is staged, so the server offers v3276.
  - Unknown source values are refused, re-runs are idempotent, files are written by exclusive temp + rename, a different file under a target name is a refused collision, and deploy is hashed before and after.
- **Test.** `scripts/test_sgtext_corrections.py`: **12 passed, 0 failed** (`R5/test_sgtext_corrections_1.log`); registered in the full regression.
- **Staged root.** `R5/content_root_sg` (mirror of the round-1 root). Hashes: `_498.zip` `c42d235d…`, `Ver3276_hdt2N6mr_1.dat` `8718c12e…`, `_2.dat` `5257d165…`, `manifest.json` `6b15fbbc…`; log `R5/sgtext_stage.log`.
- **Client run `u3_sg`** (server `c28a8db9`, the same squad save).
  - The client fetched `Ver3276_hdt2N6mr_1/2.dat` and `sgtext_leaderSkillDescription_498.zip`.
  - Galtier's ticker now scrolls the whole text **on one line**: "…ABP and CBP gain. 25% boost to all parameters of Dark types & 20% damage reduction from Dark, Light types in Guild Raid", then restarts (`R5/visual/run/U3/galtier_498_timeline.png`, `galtier_before_after.png`).
  - A slightly wider space remains after "reduction" at the ticker's internal piece boundary.
- **Deploy (Evan).** Put `content/sgtext/sgtext_leaderSkillDescription_498.zip` and `content/F_RESOURCE_MST/` from the staged root into `deploy/game_content` (or run the tool against a deploy copy), then restart and do a player check.
  - The S19 client now caches `F_RESOURCE_MST` v3276 and the `_498` dictionary.

### #22-2 Amadream: not started (usage window)

- Next: find the unit-detail header's name label, at x86 0xCA52F0 or 0x183B70 (both reference `layout_unit_detail_userunit.csv`; "Unit No." is a localized string, not a literal).
- Fix its width/wrap in the offline-proxy DLL the same way the #24 gate-flash patch was done (`OfflineMod.win32/content/cp_gate_flash*.cpp` pattern).
- Build a candidate DLL, then compare Amadream against the Durumn/Lukroar/Galtier controls on the S19 package. Swapping the S19 test DLL is the install step, so get Evan's go-ahead first.

Updated files: `docs/BUG_ACCEPTANCE_2026-10-27.md` `4af8bdb849e40c96` (backup `R5/BUG_ACCEPTANCE_before_round5b.md`), `scripts/run_bugfix_regressions.py` `879462c0a1eac013`.

## 10. #22-2 Amadream header fix: started, stopped at the usage limit (October 30)

Evan asked to start immediately rather than after the reset. Progress before stopping at about 88 % of the usage window. Nothing was built or installed, no DLL was swapped, and S19 still runs DLL `9633061d`.

- **0xCA52F0** loads `layout_unit_detail_userunit.csv` (0x7EC470), then calls **0xCAEC20**, the scene build (5,917 instructions; `R5/x86_0xCAEC20.txt`). That function builds only the stats, sphere, cost, BB and favourite elements (`type_frame`, `lv_frame`, `hp_frame` … `sphere_frame1/2`, `WHITE_LABEL`/`INFO_LABEL` font keys). **The header is not built there.**
- **0x183B70** (the second layout user) calls 0x18B7B0 after loading. Not examined yet.
- "Unit No." is not a literal in the EXE, not in `Assets/Resources/text_local.csv` (base64-decoded), and not in any sgtext dictionary. The header text is assembled some other way: a format string with a separate "No." piece, an image, or a shared title-bar routine.
- **Next round:**
  1. Find the title bar the unit screens share (the "Back" button, the stars and element icons, the name label, "Add Favorite"). Search by its image names (e.g. the favourite button) and by callers common to the unit-detail scenes.
  2. In it, find the name label's creation (dimensions, font) and patch it to a single line that shrinks to fit, through an offline-proxy content patch with a site check (the `cp_gate_flash` pattern).
  3. Build a candidate DLL, back up `9633061d`, swap it into S19 only, run the `u1_text` squad fixture, compare Amadream against the Durumn/Lukroar/Galtier controls, then restore the DLL.

## 11. #22-2 Amadream header: fixed in a candidate proxy DLL, verified in the S19 client (October 30)

Evan approved the S19 test-DLL swap and restore. The S19 package was returned to its original DLL afterwards (`9633061d…`, re-hashed). The live client and deploy/ were not touched, and nothing was signed or committed.

**Cause (x86 fb4f3c25, `tools/bin/pe_x86.py`; RTTI resolver `R5/tools/rtti_vtables.py`).**

- `UnitDetailUserUnitScene` (vtable 0x154E2B4) builds its title in slot 154, **0xCA5E70**: `<size=…>Unit</size>` plus "No.", the rarity and element icons, then the name label.
- The name label comes from the call at **0xCA71A5** to the label helper's shortest overload, 0x387100. That forwards through 0x3871E0, 0x3872C0 and 0x3873B0 to the full overload **0x3874A0**, each layer appending one default: 1, **1 (wrap)**, 1, **0 (shrink)**.
- 0x3874A0 runs the helper's own shrink-to-fit, **0x2370B0** (scale × text width > width → scale down), only when wrap is 0 and shrink is set. The label stores the wrap flag at +0x164, and its text update (0x38C3B0) re-applies the same rule.
- So the header name always wrapped at its width. "Intact Champion Amadream" is wider than that width, so it broke onto a second line that grew up over the number row.

**Fix (offline-proxy, uncommitted).**

- New `OfflineMod.win32/content/cp_name_fit.cpp`/`.h`, attached and detached in `main.cpp` next to the gate-flash patch and added to `OfflineMod.win32/CMakeLists.txt`.
- The 5-byte `call 0x387100` at 0xCA71A5 becomes `call NameFitThunk`. The thunk calls 0x3874A0 directly with the same text, colour, size, registers and width (`xmm3`), but wrap 0 and shrink 1. It moves the by-value string into the new argument slot (0x3874A0 frees it, as 0x387100 would) and cleans its own 34h bytes.
- The patch declines unless the client identity and three signatures match. They come from a new `NAME_FIT_SITES` group in `tests/fixtures/make_signatures.py`; the regenerated `cp_signatures.h` and JSON fixture differ only by that table.
- The stock bytes are restored on detach.
- `tests/fixtures/check_name_fit_site.py` (new) proves every fact the thunk relies on from the binary: the site's arguments and cleanup, each layer's appended default, the flag reads and the shrink gate, label+0x164, and that 0x2370B0 only scales down. Result: **0 failures** (`R5/check_name_fit_site.log`). `check_gate_flash_site.py` still passes.

**Build.** `R5/tools/build_proxy_namefit.sh`: the Session 19 recipe, 29960 only, with the cache guard. The cache and `build/bin` were restored identically (`e24234c1…`).

- Candidate `R5/proxy_29960_namefit/libcurl.dll` **`9ba20838210737a53c1bb6f6a40dfc389302e00b32e95ced79ac02e6e79092fb`** (pdb `93f4449d…`).
- `content_provider_tests` was not rebuilt or run this time.

**Client run `n1_namefit`** (candidate in S19, server `c28a8db9`, root `R5/content_root_sg`, the five-unit squad save).

- The client's own log shows `unit name fit installed at exe 00DA0000 + CA71A5`.
- **Amadream: "Unit No.1747" on its own line and "Intact Champion Amadream" on one line below it, scaled slightly to fit inside the title bar, with nothing overlapping** (`R5/visual/run/N1/header_before_after.png`, `info_sheet.png`).
- Controls, title-text region compared pixel by pixel before and after: Durumn and Lukroar are **identical** (0 differing pixels). Galtier's name is about 1% narrower (`galtier_name_zoom.png`): it was marginally wider than the width, and the same rule now fits it exactly. Selena (a short name) is unchanged.
- The S19 DLL was backed up to `R5/dll_backup/libcurl_9633061d.dll` before the swap and restored after the run; it is `9633061d…` again.

**To ship (Evan).** Build the 9960 twin with this source (the `build_proxy_consolefix.sh` steps 2–3), then repack the live client with the new `libcurl.dll`, sign it and install. The live client still runs the May proxy; see the round-1 notes on the staged client update.

**offline-proxy files (SHA-256 prefixes):**

- `content/cp_name_fit.cpp` `4a18cc3a8f127765`
- `content/cp_name_fit.h` `5d40bc55651cec75`
- `tests/fixtures/check_name_fit_site.py` `51a405b04fcfc00e`
- `tests/fixtures/make_signatures.py` `67e46f8f19974506`
- `content/cp_signatures.h` `54e44f177c48d57d`
- `tests/fixtures/bf_2.19.6.0_signatures.json` `032f1302cc331289`
- `main.cpp` `561ca054ce8a2f40`
- `CMakeLists.txt` `347616088f9a637b`

Pre-edit copies are in `R5/proxy_before/`.

## 12. After the reboot (October 30, evening)

**Clean full regression.** The reboot cleared the old pid 11336. `python scripts/run_bugfix_regressions.py out/qa/current/fresh_round4_20261030/bin/gimuserverw.exe` gave **67 entries, 2,755 checks passed, 0 failed, no failed entries** on candidate `c28a8db9`.

- That includes `test_mission_capture_wire`, `test_unit_mst_corrections`, `test_gen_metal_parades` and `test_sgtext_corrections`.
- It replaces round 4's 66-entry run that had the fixture-lock error.
- The previous reports were backed up to `R5/regression_before/` first.

**#40 evolved UBB: started, not concluded.**

- **Server.** It stores no UBB. `user_units` has `bb_id`, `bb_lvl`, `sbb_id` and `sbb_lvl` only. `UnitEvo.cpp` halves the BB and SBB levels on evolution; a locked SBB (0) stays locked. Its comment says "the client derives [UBB] from the SBB level and its own UnitMst". UnitMst carries UBB in `cb0P4mp1`, e.g. Tartarus Blaze Berdette 10826 → 210826.
- **Client strings.** `UNIT_MIX_EFFECT_UBB_GET1/2/NAME`, drawn by 0xC77DF0, is a fusion-result message. That points to the UBB unlocking when a fusion raises SBB to its maximum, so a 7★ evolved with SBB halved to 5 would lack the UBB until its SBB is maxed again. The deciding condition, in the caller of 0xC77DF0, has not been read yet.
- **Client run `b1_bursts`** (`R5/tools/prep_bursts.py`). The squad was 10826 at SBB 10, 5 and 0, 10825 (6★) at SBB 10, and 10824 (5★).
  - The unit info screen shows only the **Brave Burst** row for every form. Tapping it opens the BB detail ("Ruse Eclipse Lv.10" for the 7★), not SBB or UBB (`R5/visual/run/B1/bursts_sheet.png`).
  - So that screen is not where SBB/UBB visibility is judged. The `ubb_label`/`ubb_frame` strings are used by x86 0x930AE4 and 0xC566F9, whose scenes are not identified yet; the battle UI has `ubb_btn` and `wakeup_UBB_btn`.
- **Next for #40:**
  1. Identify the scenes at 0x930AE4/0xC566F9 (RTTI vtables, `R5/tools/rtti_vtables.py`).
  2. Read the UBB-get condition in the caller of 0xC77DF0.
  3. Observe UBB availability in a battle (OD gauge / `wakeup_UBB_btn`) with the 10826 SBB 10 and SBB 5 fixtures.

**Still not started:** #25 frog SP eligibility, #26 Roglizer/Fuu, #22-1 Lukroar (targets in §6).

**New report (Evan, October 30): skill text not wrapping inside its box.** "While you were clicking on the skills, some of the words weren't wrapping properly, almost like the bounding boxes were offset."

- Visible in run `b1_bursts` (`R5/visual/run/B1/u1_bb1.png`, `u1_bb3.png`): the Brave Burst detail popup on the unit info screen, opened by tapping the Brave Burst row.
- The description wraps at a width wider than the popup frame: "…boosts Def relative to Atk for 3 turns & raises normal h" runs past the frame's right edge, then "amount for 3 turns" wraps.
- **Next:**
  1. Find the popup's label creation, most likely the same label helper as #22-2 (0x387100 → 0x3874A0), with its wrap width and position.
  2. Compare them with the popup frame's geometry. Decide between a wrap-width or position fix (proxy patch, as for #22-2) and a layout-CSV fix.
  3. Verify in S19 against BB descriptions of several lengths.

## 13. Skill popup text overflow (Evan's report): diagnosed, candidate built, client check blocked (October 30, evening)

Working notes with every address: `R5/notes_round5_popup.md`.

**Diagnosis (x86 fb4f3c25).**

- The unit-info touch popup is built in **0xCB6750**: frames `Frame1`…`Frame8` on `list_frame1.png`. Frame1 holds the BB (`nameData`, `lvData`, `descData`), Frame2 the SBB (`…Ex`), and the next frame the UBB (`descDataUl`). The popup refresh routines are 0xCBC610, 0xCBB9E0, 0xCBD370, 0xCBE3E0 and 0xCBFF80.
- The three descriptions come from the label helper at 0xCB98C7, 0xCB993E and 0xCB99B7, with x 26, y 157/222/287 and wrap width `movss xmm3, [0x16E56B0]` = **274.0**. The frame rows nearby use x 24 / width 272.
- Captured (`R5/visual/run/B1/u1_bb1_popup.png`): a wrapped line runs past the frame's right border ("…& raises normal hi"), while the left margin is normal.
- An open question, not needed for this fix: in screen pixels the drawn line is wider than 274 units at this scale. The layouter may measure at a different size than it draws (0x38F0D0, not read).

**Candidate fix (offline-proxy, uncommitted; same module as #22-2).**

- `content/cp_name_fit.cpp` points the three width loads' operands at a DLL constant, **258.0**, chosen so the right margin matches the left (about 526 px of text at about 2.03 px per unit). It patches only after checking each operand still names the stock 274.0, and restores all three on detach. 274.0 itself, which other code shares, is untouched.
- `make_signatures.py` gains `popupDescWidthBB/Ex/Ul` (8 bytes each, relocation masked) in the same group; the regenerated tables differ only by those entries.
- `check_name_fit_site.py` gains 8 checks (the constant is 274.0, each load feeds a label-helper call, the tags are descData/Ex/Ul). Result: **24 passed, 0 failures**.
- Candidate DLL `R5/proxy_29960_popupfit/libcurl.dll` **`466ce6fcf29beb0c43d02167aa93f2d0a069f7d3ff0d039959e532f2e1422ec9`**. In S19 its log showed both `skill popup width installed: … wrap at 258` and `unit name fit installed`.

**Client check: not done.** Run `b2_popup` (squad: 10824 long BB, 10826 at SBB 10/5/0, 10011 short control) started with the candidate in S19.

- The driver refused input because a `Windows.UI.Core.CoreWindow` (Windows Input Experience) covered the client's title bar while Evan was using the desktop. No input was sent.
- The run was stopped by `run_ctl` and **S19 was restored to `9633061d`**.

**To finish:**

1. With the desktop free, swap in `466ce6fc…` (back up `9633061d` first), rerun `b2_popup`, and tap each unit's Brave Burst row.
2. Accept if every description line ends inside the frame with a margin like the left one and the short control is unchanged. Also check the 7★ popup for SBB/UBB frames, which is #40 evidence.
3. Adjust 258 if needed, then restore `9633061d`.

**offline-proxy hashes:**

- `content/cp_name_fit.cpp` `45fd2efe35dc8d8c`
- `tests/fixtures/check_name_fit_site.py` `6ad44070f4c01e0e`
- `tests/fixtures/make_signatures.py` `1824bbe008cd4099`
- `content/cp_signatures.h` `1bdc91aa250ce76f`
- `tests/fixtures/bf_2.19.6.0_signatures.json` `20fe9fc5aa9147a1`

Pre-edit copies are in `R5/proxy_before_popup/`.

**Still open:** #40 (§12), #25, #26, #22-1 (§6).

## 14. Skill popup fix and #40 verified in the S19 client (October 30, evening)

**Client run `b2_popup`.** Candidate `466ce6fc…` in S19 (both patches logged as installed), server `c28a8db9`, root `R5/content_root_sg`. Squad: 10011 Vargas (short control), 10824 Inferno Berdette (long BB), 10826 Tartarus Blaze Berdette at SBB 10/5/0. Captures: `R5/visual/run/B2/popups_sheet.png`, `sbb_ubb_frames.png`, `ubb_by_sbb.png`, `popup_before_after.png`.

**Skill popup (Evan's report): fixed.**

- Inferno Berdette's BB description now wraps "…for 3 turns & raises / normal hit amount for 3 turns" inside the frame; before, it was "…raises normal hi" past the border.
- The 7★ BB, SBB and UBB descriptions all end inside their frames.
- The Vargas control's single line is unchanged.
- S19 was restored to `9633061d` afterwards.

**#40 evolved UBB: established in the client.** The unit-info touch popup shows:

| Form | Frames shown |
|---|---|
| 7★ 10826 at SBB 10 | Brave Burst + Super Brave Burst (Lv.10) + **Ultimate Brave Burst "Promethea"** |
| 7★ 10826 at SBB 5 | Brave Burst + Super Brave Burst (Lv.5); **no UBB frame** |
| 7★ 10826 at SBB 0 | Brave Burst only |
| 5★ 10824 | Brave Burst only |

- So the client shows a UBB only when the SBB is at its maximum. That matches its own fusion-result message (`UNIT_MIX_EFFECT_UBB_GET`) and the server's design: no UBB column, derived from the SBB level plus UnitMst `cb0P4mp1`.
- An evolved 7★ whose SBB was halved shows its UBB again once fusion maxes the SBB. **Not a defect**, given that halving rule (Global wiki, Unit Skills, as `UnitEvo.cpp` cites).
- Not checked: UBB activation in battle (overdrive gauge); this round observed visibility and availability only.

**Still open:** #25 frog SP eligibility, #26 Roglizer/Fuu and #22-1 Lukroar (targets in §6). To ship the popup and header fixes: build the 9960 twin from this offline-proxy source, then repack the live client, sign and install (Evan).
