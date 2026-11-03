# Fresh session, round 7: handoff (October 31, 2026; stopped by Evan, continued in round 8)

Round of [CLAUDE_FRESH_ROUND7_2026-10-31.md](CLAUDE_FRESH_ROUND7_2026-10-31.md) after [QC_FRESH_ROUND6_2026-10-31.md](QC_FRESH_ROUND6_2026-10-31.md). Transient coordination note.

**Status at this writing: §1 (the QC finding) done and automated-tested. The smoke's trade-off (§1a) led Evan to ask for the vertical overflow to be fixed too; that is done and client-verified on S19 (§1b, candidate `23f9a4b0…`). §2: #22-1, #25 and #40 done (client-observed), #26 not started. §3–§4 not started. Evan stopped the round here ("Lets stop there"); the continuation brief is [CLAUDE_FRESH_ROUND8_2026-11-01.md](CLAUDE_FRESH_ROUND8_2026-11-01.md).**

Nothing was staged, committed, pushed, deployed, signed or installed. There was no reboot, and no live save or deploy content was written. **S19 is at baseline `9633061d…`** (verified after the last run; backup `R7/dll_backup/libcurl_9633061d.dll`). QA root: `out/qa/current/fresh_round7_20261031/` (`R7/`).

## 1. QC P2: popup patch write failures (fixed, automated-tested)

**Change (offline-proxy, uncommitted):**

- New `OfflineMod.win32/content/cp_name_fit_internal.h` and `cp_name_fit_logic.cpp`: per-site `PatchSite` bookkeeping. Every write goes through a `WriteFn`; each site records the stock bytes, the patched bytes, and whether our bytes are in place.
  - `InstallSites` writes sites in order. On a failed write it rolls back the completed writes, newest first, and returns `RolledBack`. If a rollback write fails, it returns `RollbackFailed` and keeps that site marked installed.
  - `RestoreSites` restores every installed site; a failed restore keeps its site installed, so it can be retried.
- `cp_name_fit.cpp` now keeps two independent site sets: the name-label call (1 site) and the popup widths (3 operands).
  - Each install and restore result is logged accurately: installed / NOT installed (rolled back) / PARTIAL / could not be restored.
  - Detach restores each set on its own, so a failure in one never discards the other's state.
  - The guarded identity and signatures are unchanged.
- `OfflineMod.win32/CMakeLists.txt` and `tests/CMakeLists.txt` add the logic file.
- `tests/test_main.cpp` gains `TestNameFitSites`:
  - a successful lifecycle, plus an idempotent second restore;
  - an install failure at each of the 3 positions, rolled back to all-stock;
  - an install failure plus a failed rollback (`RollbackFailed`, one site kept), then a successful retry;
  - a restore failure at each of the 3 positions, keeping that site, then a successful retry;
  - two independent sets, where one's failure leaves the other intact;
  - a re-install after a rollback.

**Tests:**

- `build/tests/content_provider_tests.exe`: **738 passed, 0 failed, 5 skipped**. The skips are the broker integration suites, gated by environment variables (`R7/content_provider_tests.log`).
- `tests/fixtures/check_name_fit_site.py`: **0 failures** (`R7/check_name_fit_site.log`).
- The proxy CMake cache still matches the saved baseline; `build/bin` is restored to `e24234c1…`.

**Candidate DLL** (29960; `R7/tools/build_proxy_r7.sh`): `R7/proxy_29960_r7/libcurl.dll` **`4fd996827e3d9008a0a6497aa0317a890375be24d151cfd6daff87feab3f758e`**. In S19 the client logged `unit name fit installed` and `skill popup width installed`. **Superseded by the §1b candidate `23f9a4b0…`**, which contains this fix plus the vertical fit.

**Source hashes** (`R7/proxy_hashes.txt`):

- `cp_name_fit.cpp` `3f98928a28cfb9d9`
- `cp_name_fit_internal.h` `092601116bb80037`
- `cp_name_fit_logic.cpp` `d1c711df3e004fae`
- `tests/test_main.cpp` `9afb6e873cb1ebe1`
- `tests/CMakeLists.txt` `cdff5e5c19024461`
- `OfflineMod.win32/CMakeLists.txt` `e58ae3bcb50c4f8b`

Pre-edit copies are in `R7/proxy_before/`.

## 1a. Visual smoke: trade-off found (decided: fix vertical too, §1b)

Run `v1_smoke` (candidate) and run `v2_base` (baseline), on the same squad: Amadream 51337 SBB10, Tartarus Blaze Berdette 10826 SBB10, Inferno Berdette 10824, Vargas 10011, Durumn 840278 SBB10. Captures: `R7/visual/run/V1/smoke_sheet.png`, `V1/omni_popup_compare.png`.

- **Long name:** Amadream's header is on one line (#22-2 holds).
- **Short and medium descriptions** (Vargas, Inferno, Tartarus BB/SBB/UBB): every line is inside its frame.
- **Very long Omni descriptions** (Durumn, Amadream BB/SBB/UBB):
  - On the **baseline**, they already overflow both sideways (past the right edge) and **downward**: 4–5 lines spill over the next frame's title.
  - The candidate removes the sideways overflow, but the narrower width adds about one line to the longest texts (Durumn SBB/UBB), so the **downward overlap is slightly worse**.
  - The vertical overflow itself predates this work.
- **Decision needed:** whether to keep 258, or tackle the vertical overflow too. Shrinking the font of long descriptions would mean changing the label helper's per-label shrink/scale, or the frame heights in 0xCB6750. That needs a researched design (font scale vs frame layout), not a guess.
- **Resolved:** Evan chose "Fix vertical too". The fix is in §1b.

## 2. Acceptance routes (Evan: "Start §2")

Status: **#22-1, #25 and #40 done** (client-observed on isolated copies). **#26 not started.** Every run below:
- S19 Fresh package with the **stock DLL `9633061d…`**, so the #22-2 header overlap is visible on Amadream, as expected;
- server `c28a8db9…` on 29961, relay on 29960;
- a copy of `session19/visual/save_after_s20_completed_check.sqlite`, content root `R5/content_root_sg`.

No runtime source changed in §2.

### #22-1 Lukroar 51247 gate: not reproduced; no fix (no reproduced cause)

**Fixture** (`R7/tools/prep_lukroar.py` → `R7/fx22/`; copies of `deploy/archive` and `deploy/mst`, deploy only read):
- Test Gate: Omni (99008) holds only Lukroar.
- Test Gate: 7 Star (99007) holds only Durumn 840278, the control. It is the same rarity 8, so it gets the same effects.
- The server picks the gate effect by rarity alone (`Gacha.cpp` `getGachaEffect`). So run `l900` sets effect 901's weight to 0, and run `l901` sets effect 900's weight to 0.
- Gems: 500. Each pull costs 1 gem, as expected.

**Observed: four pulls (both units × both effects), all with the full sequence and the right unit.**

| Run | Effect | Lukroar (user_unit) | Durumn control (user_unit) | Frames |
| --- | --- | --- | --- | --- |
| `l900` | 900 | 1012 | 1013 | `R7/visual/run/L900/s_anim1–3.png`, `L900/ctl/s_ctl.png` |
| `l901` | 901 | 1011 | 1012 | `L901/s_luk.png`, `L901/ctl/s_ctl.png` |

- **Effect 900:** rainbow gate (`gWaitRainbow.txt`) with the TOUCH prompt. Tapping it flashes it into the black Omni gate (`gChange(Rainbow-black).txt`), which also shows TOUCH. Tapping that opens it (`gOpenGold(Rare7).txt`): a light burst, one white transition frame, the "OMNI EVOLUTION!!" reveal with the unit's art and quote, then the result screen ("Sacred Purpose Lukroar", Unit No. 1669).
- **Effect 901:** the black gate (`gWaitBlack.txt`) with TOUCH from the first frame, then the same open and reveal.
- **The control** matches frame for frame.
- **Never seen:** a black void, or a white overlay that stays. The white frame lasts one capture (gone by the next), so it is not the #24 overlay.

**Assets** (server.log content-broker lines; `R7/visual/run/L900/broker_served.txt`):

| Effect | Wait gate | Change | Open | Then |
| --- | --- | --- | --- | --- |
| 900 | `GachaGateClose5` | `GachaGateChange5_6` | `GachaGateOpen6` (+Add) | `GachaEndCircleSuperGekiReaBlack` (+Add), `GensouShinka` (+Add), `unit_ills_full_51247` / `_840278` |
| 901 | `GachaGateClose6` (+Add) | — | `GachaGateOpen6` (+Add) | same |

- Every one of these is served by the broker, 200, with nothing refused.
- The `g*.txt` scripts and the sounds come from the S19 cache, not the broker.
- The only 404s in either relay log are the login-time `maintenanceClient.php` and `accounts/game_user/update`, present in every run.

**Disposition:**
- The reported black void does not occur while the gate SuperAnims are served. They have been in the provider policy since Session 13.
- The report ("no gate but the touch icon present") is what a missing wait-gate SuperAnim would look like, because the TOUCH prompt is a separate SAM (`sam/Touch`). That is consistent, **not proven**: the negative control (the same 901 pull with `GachaGateClose6` removed from a copied provider root) was skipped, because the root is 9.1 GB and the usage budget is tight.
- Limits: the S19 cache is warm, so scripts and sounds were already cached.

### #25 Omni Frog SP: works end to end (client fusion, stored gain, cap, ineligible, relog)

**Starting SP** (checked apart from any authored fixture):
- **Migrated save:** every `user_units` row in the S19 save is `fe_sp 10 / fe_used_sp 0 / fe_max_sp 100`. That is the migration default (`MigrationManager.cpp:1574`, `DEFAULT 10`) and the wiki's initial 10 SP.
- **Fresh summons:** units the server created in run `l901` hold the same values.
- **Live save** (read only): 108 rows at 10, one at 100.
- **Displayed** for the untouched-default base: unit info "SP 10"; the Omni-only Enhancements screen shows Current SP 10, Used SP 0, Total SP 10/100.

**Fixture** (`R7/tools/prep_sp7.py`; run `sp7b`, before-copy `R7/visual/saves/sp7b_before.sqlite`): three Intact Champion Amadream 51337 squad units, plus Omni Frog 750003 ×2 and Burst Frog 10312 ×1.

| Unit | Level / SBB | SP |
| --- | --- | --- |
| 1010 | Lv 150 / SBB 10 | migrated default 10 |
| 1011 | Lv 150 / SBB 10 | authored 80 |
| 1012 | Lv 100 / SBB 10 | default 10 |

**Observed** (frames in `R7/visual/run/SP7B/`):

| Case | Client | Server log | Stored |
| --- | --- | --- | --- |
| Eligible: 1010 + Omni Frog | "SPUP" tag; result card **SP 10/100 ▶ 40/100** | `enhancement SP 10+30 -> 40 (spent 0, limit 100)` | `fe_sp` 40; frog 1013 deleted; Zel −3,775 |
| Cap: 1011 (80) + Omni Frog | Warns **"The selected material Unit(s) will fuse over the base Unit's current SP limit. Proceed?"**; card **SP 80/100 ▶ 100/100** | `80+30 -> 100` | `fe_sp` 100; frog 1014 deleted |
| Full: 1011 at 100/100 | Burst Frog greyed (its only effect on a max-SBB base is SP) | — | — |
| Ineligible: 1012 (Lv 100) | Tags read "BBUP"; Burst Frog greyed (BB/SBB already 10); Omni Frog greyed (round 6) | — | — |
| Non-SP material (accidental, `sp7b`) | Lv 1 Thunder unit 40030 fused into 1010 | no SP | SP stays 10; EXP only |
| Relog (client closed and relaunched) | 1010 shows SP 40 (Enhancements: Used 0, Total 40/100); 1011 shows SP 100 | — | DB matches |

**Server-side ineligible guard** (run `sp7`, which came from a fixture mistake):
- The server judged a base the client showed at "Lv 150" to be level 2 and logged `(base not eligible) 10+30 -> 10`.
- SP stayed 10, the Omni Frog was consumed, and the unit levelled 2 → 6.

**Fixture finding:**
- The server takes a unit's level from `total_exp` (`UnitMix.cpp:483`, "the row stores total_exp, not a level"). The client trusts `unit_lvl`.
- Round 6's `R5/tools/prep_sp.py` set `unit_lvl` alone, so its "max level" bases were level 2 to the server. Its eligibility observation still stands, but it cannot drive a fusion.
- `prep_sp7.py` writes the matching `total_exp`.
- No game defect: rows the server writes always carry both values.

**Not covered** (SP scope, §3):
- ordinary-unit probabilistic SP: not ported, and there is no source probability;
- Omni+ limits: no Omni+ levels exist yet;
- spending SP: FeSkillGet, already player-confirmed earlier.

### #40 evolved UBB: works end to end (evolution, unlock at SBB 10, Overdrive, UBB in battle, relog)

**Source.** Unit Skills, revision 656738 (`review_round6_20261031/Unit_Skills.json`). The UBB:
- needs one BB charge **plus the Overdrive gauge**;
- Overdrive mode gives the unit +100% Atk/Def/Rec, and up to four turns, including the activation turn, to use the UBB.

UBB visibility follows the client's own rule: shown only at max SBB (rounds 5–6).

**Fixture** (`R7/tools/prep_evo7.py`; run `evo7`, before-copy `R7/visual/saves/evo7_before.sqlite`; every row carries its level's `total_exp`):

| Unit | Role | BB / SBB |
| --- | --- | --- |
| Inferno Berdette 10825 (6★), Lv 100 / 100 | Evolution base | 10 / 10 |
| Tartarus Blaze Berdette 10826 (7★) | Control | 10 / 5 |
| Tartarus Blaze Berdette 10826 | Control | 10 / 0 (locked) |
| Inferno Berdette 10824 (5★) | Lower-rarity control | 10 / no SBB |

- Materials: the `unit_evo_mst` recipe 10825 → 10826 (10354, 50354, 10133, 60224 ×2, Zel 1,500,000) and one Burst Queen 750004.
- Unit art and skill names are from the client's own data; the evolution chain is the game's own.

**Observed** (frames `R7/visual/run/EVO7/`, battle frames `EVO7/battle/`, sheet `EVO7/ubb_evidence.png`):

| Step | Client | Server / DB |
| --- | --- | --- |
| Evolution screen | Lv 100/100 → Lv 1/120; all 5 materials "Have"; "Can be Evolved", Zel 1,500,000 | — |
| Evolve | Animation, "MEGA RARE!!" reveal, Tartarus Blaze Berdette Unit No. 1264; popup BB Lv 5, SBB Lv 5, **no UBB** | `UnitEvo` log: `bb 10->5 (10826), sbb 10->5 (110826)` (halved, per the wiki); row ids 10826 / 110826, leader skill 7419 (none of 10825's 10825 / 110825 / 7418 kept); 5 materials deleted; Zel −1,500,000 |
| Burst Queen into the evolved unit | Warns "material Unit(s) which will break the BB level limit"; card **SBB Lv 5/10 ▶ 10/10** | `burst +20 (bb 5->10, sbb 5->10)`; queen deleted |
| Popup at SBB 10 | BB **Ruse Eclipse** Lv 10, SBB **Terrible Cremation** Lv 10, UBB **Promethea** listed | — |
| Training Grounds (Randall → Survey Office → Battle Simulator) | Menu → Battle Settings: BB gauge Infinite, **Overdrive gauge Infinite** (the simulator's own options) | — |
| Overdrive | **Left flick** on the leader with a full OD gauge: screen dims, unit charges alone (OD start). Gauge label becomes **ULTIMATE BRAVE BURST** (red), OVER DRIVE buff icon, party OD bar spent | — |
| UBB in battle | **Upward flick**: full cut-in **"Promethea"**, 23-combo all-foe attack (damage 125,613 → 197,004), "UBB" buff icons on the party, "UBB DEF" debuffs on the dummies. Repeated next turn: same left flick, same "Promethea" cut-in | — |
| Controls, same left flick, full gauge, fresh turn | SBB 0 Tartarus, SBB 5 Tartarus and 5★ Inferno each do a **normal attack**: no OD start, gauge not spent | — |
| Quit practice → close and relaunch client → log in | Evolved unit: BB 10, SBB 10, UBB Promethea. SBB 5 control: no UBB. SBB 0 control: SBB locked. 5★: BB only. Daily task "Evolve any unit" 1/1 | DB rows unchanged |

- No crash dumps.
- The Training Ground sends no reward request, as designed.

**Gesture** (from libgame.so, `SandbagBattleManager::touchEndedChild` @0x10962DC): with `isEnableOverDrive`, `isFullOverDriveGage`, not already Overdrive-ready and `isHaveUltimateSkill`, a flick enters Overdrive; once in Overdrive, the unit's skill flick casts `getBattleUnitUltimateSkill`. A plain tap on the OVERDRIVE bar does nothing (tried with both mouse and touch).

**Limits:**
- The battle was the Training Ground with the simulator's infinite-gauge options, not a natural gauge fill in a story battle.
- The fixture's helper was picked by accident during the intro dialogue. Its leader skill fills OD, which is irrelevant with an infinite gauge.
- **The #40 report's exact unit is unknown**, so this proves the mechanism on the Berdette chain, not on every UBB unit.

### #26: not started

Matched fixtures for a plain leader vs Roglizer/Fuu and the helper need battles with rewards. They are left for the next go-ahead (weekly usage window high).

## 3–4. Not started

- §3 per-hit item-drop contract and the SP-scope inventory.
- §4 content-scope corrections (#17 vs revision 659707, #18 vs revision 674494, #12 first encounter, #15 roster/assets).

## Release file list (round 7, maintained files only)

offline-proxy (all uncommitted; the popup and header fixes ship together):

- `OfflineMod.win32/content/cp_name_fit.cpp`, `cp_name_fit.h`, `cp_name_fit_internal.h`, `cp_name_fit_logic.cpp`
- `OfflineMod.win32/main.cpp`, `OfflineMod.win32/CMakeLists.txt`
- `OfflineMod.win32/content/cp_signatures.h`
- `tests/fixtures/make_signatures.py`, `tests/fixtures/bf_2.19.6.0_signatures.json`, `tests/fixtures/check_name_fit_site.py`
- `tests/test_main.cpp`, `tests/CMakeLists.txt`

BF-WorkingDirRust:

- `docs/BUG_ACCEPTANCE_2026-10-27.md` (`d7a54492a7cf3166`; pre-round copy `R7/BUG_ACCEPTANCE_before_round7.md`): the Round 7 section.
- No runtime, generator or test file changed in round 7. The round 4–6 maintained files are listed in those handoffs.

packet-generator: no round-7 change.

Not release files (QA evidence under `R7/tools/`; promote to `scripts/qa/` only if wanted):

- `prep_lukroar.py` `fb03b8996744ceb4`
- `prep_sp7.py` `38a2784f0c62093c`, which also writes the matching `total_exp`; round 6's `R5/tools/prep_sp.py` does not
- `prep_evo7.py` `9754ed0695cad2e0`
- `build_proxy_r7.sh` `fb5ffb756a09dbb3`

Exclude `out/`, copied saves, screenshots and the transient session/QC notes, this one included.

## End state (October 31, evening)

- **Clients and servers:** none running, and ports 9960/29960/29961 are free.
- **S19:** the S19 Fresh package DLL is the baseline `9633061d…`, verified after the last run.
- **Proxy build tree:** the CMake cache matches the saved copy, and `build/bin/libcurl.dll` is `e24234c1…`.
- **Candidates on disk, not installed anywhere:**
  - server `R4/bin/gimuserverw.exe` `c28a8db9…`
  - proxy 29960 DLL `R7/proxy_29960_r7vfit3/libcurl.dll` `23f9a4b0…`, which has the header fit, popup width, write-failure handling and vertical fit
- **Not done in round 7:**
  - #26;
  - §3: the per-hit item-drop contract and the SP-scope inventory;
  - §4: the content proposals, and the dated correction to `REMAINING_CONTENT_SCOPE.md` (replacing the "reuse Research Lab Juggernaut AI" recommendation);
  - the maintained regression runner, not rerun because no runtime changed.

## 1b. Vertical fit of long skill descriptions (Evan: "Fix vertical too"; fixed, client-verified)

**Result.** Descriptions longer than 3 lines (Durumn, Amadream BB/SBB/UBB) now fit their frames:
- They are wrapped again at a wider width, then scaled so the block fits the frame's width and its 3 lines.
- They stay left-aligned, with the first line where the client puts it, under the skill name.
- Nothing reaches the next frame's title.

Descriptions of 3 lines or fewer are untouched: pixel-identical to the width-only build, measured on Vargas, Inferno, Tartarus and Selena.

Comparison image (baseline above, fix below): `R7/visual/run/V5/vertical_fit_compare.png`.

**How the label works** (x86 2.19.6.0; each fact is checked by `tests/fixtures/check_name_fit_site.py` §8–§10):
- **The label.** Each description is gumi's text block (vtable 0x18E8CE0, a `CCNode` from libcocos2d 2.2.5).
  - Its lines are separate `CCLabelTTF` nodes, children of the layer, not of the label. They live in the list at +100h, outline copies included; +168h is the line count.
  - The label places the lines itself, in its `setPosition(x, y)` override 0x38CDC0 (vtable +48h). It scales each line's centre about the block's centre by the label's scale. Align 4, which these labels use, keeps the left edge (`(w·s − w)/2` shift).
- **setString.** 0x38C3B0(force, std::string by value; `ret 1Ch`) stores the text at +14Ch. It removes and releases the old lines, and lays the text out again at the wrap width +114h.
- **Text is final at creation.** The popup builds the text before it calls the helper, and choosing the font size (10/8/7 at 162/309 characters) is the client's own logic. The only other user of these labels in the scene (0xCBC610) sets their opacity.

**The change** (offline-proxy `cp_name_fit.cpp`):
- The three description calls (0xCB98C7 / 0xCB993E / 0xCB99B7, `call 0x387100`, caller-cleaned) now go to `PopupDescThunk`.
  - The thunk parks the return address, calls 0x387100 unchanged, and runs `CP_FitPopupDesc(label)`.
  - It restores eax, ecx, edx and xmm0–7: the client is whole-program optimised, so it is not assumed to treat them as clobbered.
- `CP_FitPopupDesc` does nothing at 3 lines or fewer. For n > 3 lines it:
  1. sets +114h to `width / √(3/n)` and calls the label's own setString with force 1 and a copy of its text. The copy is made by the client's `std::string::assign` 0xDE250, and the callee frees it.
  2. scales by `min(√(3/n), 3/n′)` (n′ = the new line count) on the label and on every line node, then calls the label's `setPosition`.
  3. measures the first line's top (position, anchor, height, scale) before and after. It measures the y slope with a second layout and moves y by the difference.
  - The work runs inside `__try`. The result is logged, and a fault is logged and the label left as built.
- Install and restore use a third `PatchSite` set, with the same rollback rules as §1.
- `NameFit_Attach` declines the vertical fit unless each site is still `call 0x387100`.
- Seven new signatures: the 3 calls, setString, its width read, the layout, and the string assign.

**Two attempts that failed, kept for the record:**
- **v3 (`b11b39dc…`):** a plain `setScale` on the label shrank each line in place. Spacing and positions didn't change and lines looked centred, which proved the lines are separate nodes.
- **v4 (`e3ad466a…`):** replaying the scale and layout without re-wrapping fitted, but the computed top correction overshot (the SBB rose onto the skill name). A 6-line text at scale 0.5 was also barely legible.

**Tests:**
- `content_provider_tests`: **898 passed, 0 failed, 5 skipped** (`R7/content_provider_tests_vfit3.log`). New checks cover `PopupFitScale`, `PopupRewrapScale` and `PopupFinalScale`, including a sweep that the scaled block fits both ways for 4–12 lines, and `PopupTopDelta`.
- `check_name_fit_site.py`: **44 passed, 0 failures** (`R7/check_name_fit_site_vfit3.log`).
- The proxy cache still matches the saved copy; `build/bin` is restored to `e24234c1…`.

**Candidate DLL:** `R7/proxy_29960_r7vfit3/libcurl.dll` **`23f9a4b0c3096529bfe2a732b19a5000242bb52266eb9900ae00ec620fdfefd9`** (29960). It contains §1 and §1b.

**Client run** v5 (S19 Fresh, server `c28a8db9…`, save `R7/visual/saves/v5_vfit3`):
- The client logged all three patches installed.
- Fit log (`R7/visual/run/V5/offlinemod_content.log`):

| Unit | Description | Lines before | Wrap | Lines after | Scale |
| --- | --- | --- | --- | --- | --- |
| Durumn | BB | 4 | 258 → 297.9 | 4 | 0.75 |
| Durumn | SBB | 6 | 258 → 364.9 | 4 | 0.707 |
| Durumn | UBB | 5 | 258 → 333.1 | 4 | 0.75 |
| Amadream | BB, SBB, UBB | 4 each | 258 → 297.9 | 4 each | 0.75 |

- In every case the first line's top was the same before and after (for example 327.5 → 327.5).
- Durumn's popup was reopened from the unit list, closed and reopened: built once per unit screen, no crash, no new dump.
- The header fix (#22-2) holds.
- **S19 restored to `9633061d…`** afterwards; no server or client is running.

**Source hashes** (`R7/proxy_hashes_vfit.txt`; supersede the §1 list):

| File | Hash |
| --- | --- |
| `cp_name_fit.cpp` | `0682f2a8df97f031` |
| `cp_name_fit_internal.h` | `5978d5fe784f64f1` |
| `cp_name_fit_logic.cpp` | `cd4560da808c7b75` |
| `cp_signatures.h` | `32af44bb687fd744` |
| `tests/test_main.cpp` | `d0e47c549c80c2fa` |
| `tests/CMakeLists.txt` | `cdff5e5c19024461` |
| `OfflineMod.win32/CMakeLists.txt` | `e58ae3bcb50c4f8b` |
| `make_signatures.py` | `9fb11a1cb9cdc142` |
| `check_name_fit_site.py` | `19b1dfa40482c3ab` |
| `bf_2.19.6.0_signatures.json` | `e72bad0e4c761948` |

**Not covered:**
- Other skill popups built elsewhere: the same "descData" labels exist in other scenes (0xC23xxx, 0xCBFF80, 0x19xxxx). Only the unit-info popup (0xCB6750) is patched, as with the width fix.
- The capacity of 3 lines is an observation, not read from the layout CSV.
- Readability at scale 0.71–0.75 was judged on Evan's window size; a phone-sized client would make it smaller still.
