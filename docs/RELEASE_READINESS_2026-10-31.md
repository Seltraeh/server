# Release readiness — October 31, 2026 (a checklist, not a deployment)

Written at the end of fresh round 8 ([handoff](CLAUDE_FRESH_ROUND8_HANDOFF_2026-10-31.md)), for Evan/Codex review.

## 0. Status, November 3 (supersedes the October 31 verdict below)

**Live (`deploy/`, server `4ab8af5b` on 9960):** every round-8 server and content item, the 224 EU units, the 29 JP
units, the 107 JP-exclusive units and the 22 authored Home crops (F_UNIT_MST 1093, F_RESOURCE_MST 3281). Save
`239f9e69` untouched. Regression on the live exe after the last apply: 79 entries, 2,950 checks, 0 failed.
**Verdict: the server side is releasable; a full release is not done.**

Release steps still open (in order):
1. **Live client update (Evan):** pack the 9960 release proxy `R8/proxy_9960_release/libcurl.dll` `44dedefd…` into the
   2.19.6.1 package (the staged `live_client_2.19.6.1` appx carries the superseded `bfeaba0b`), sign with `myKey.pfx`,
   `Add-AppxPackage` with the game closed. The live client still runs the May proxy `9ae05b17`. Without this, live
   lacks: the title-login hang fix, #22-2, the skill-popup fit and #24.
2. **Commit:** nothing since `e8cdefc` (October 3): 276 paths here, 4 in packet-generator, 12 entries in offline-proxy.
   §4 lists the round 4-8 set; the older work is not reconciled.
3. **Player package:** the last `rebuild_release.bat` output is `out/releases/bf-server-20260922-190400-6eb273`. Build a
   new Release (`server.zip` + `game-content-NNN.zip` from today's `deploy/game_content`) and pass `Check-Package.ps1`.
   The Cloudflare `assets.zip` must carry every file added since (EU list `out/release/eu_units_assets_for_zip.txt`;
   no list exists yet for the JP units, the new dictionaries and the F_* parts).
4. **Player smoke on live** after step 1: none of the November 2-3 content has been played on the live client.

**Live since November 3, evening** (Evan: "Deploy it"; server `ee6f920f`; handoff §3i, [MISSING_CONTENT.md](MISSING_CONTENT.md)):
- player package `out/releases/bf-server-20261103-133810-87d212`, rebuilt after this deploy (smoke test passed);
- 187 EU units' real sprites;
- 9 evolutions (F_UNIT_EVO_MST 601);
- all 41 Vortex Trials: candidate server `ee6f920f` with the subtree permits, Winter Paradise topology
  (F_DUNGEON_MST 1004), `gen_vortex_trials.py` output.

The player package is rebuilt from the updated deploy (handoff §3i).

Open content (EOS target), none blocking a release of what exists:
- #12 Vortex Trials: built and live (above); the AI, ATK/DEF and stand-in art are AUTHORED for community tuning.
- Omni+ Boost is a probe (`UnitOmniEvo.cpp`: the boosted unit's node is unknown; one client tap settles it).
- Guild raids, boards and research are stubs; Arena is deferred by decision (locked door).
- Summoner's Journal: 28 of 45 missions have no progress source, 18 rewards are stand-ins.
- Units: 259 static stand-in battle sprites (47 JP, 212 EU) await real sheets; 9 wiki evolutions are missing from
  `unit_evo_mst` (e.g. 10716->10717, 61016->61017); 8 incomplete rows (50876, 9850167, six element guides).

Nothing below §0 was rewritten; it is the October 31 checklist with its later status notes.

Nothing here has been run against `deploy/`, the live client or the live save. The commands in §3 are proposals: validate each one against the final candidates before running it, and do not quote them as current.

**Verdict (October 31): not release-ready.**
- Content items #12, #15, #17 and #18 are scoped but not authored.
- The per-hit item-drop channel exists in server candidate `92f41576` (and its successor `4ab8af5b`), and its content is now generated (`scripts/gen_material_drops.py`, handoff §3c). It is staged in QA archives only: deploy must not carry it before that server (§3 step 7).
- The release proxy (the 9960 twin) has not been built.
- Two regression entries are stale by design until deploy is regenerated (§5).
- **Monster skills (November 1, handoff §3e):** they crash the Windows client at battle start on every server deployed so far, including deploy's Research Lab trials (very likely). The contract is recovered and built into candidate `e37da19b`, client-verified (v16). **Deploy that server (§3 step 7).**

What is ready is a set of isolated candidates, each with client evidence (§1–§2).

## 1. Candidates and data identities

| Kind | Identity | Where | Notes |
|---|---|---|---|
| Server | **`4ab8af5b580c7410…`** | `out/qa/current/fresh_round8_20261031/bin_skills/gimuserverw.exe` | `92f41576` (`c28a8db9` + the per-hit material-drop channel, handoff §3b) plus the monster-skill contract (MissionStart sends `8aiBoHg5`, handoff §3e), rebuilt from the final source (§3f; `e37da19b` = the same code before comment-only corrections, kept as `gimuserverw_e37da19b.exe.keep`). It replaces `92f41576` as the release candidate |
| Proxy (test, port 29960) | `f74efc2b8f0d5bd1…` | `out/qa/current/fresh_round8_20261031/proxy_29960_r8qc2/libcurl.dll` | #22-2 header fit, popup width, vertical fit with the round-8 failure policy, code-writer result model, gate flash. **9960 release twin built: `out/qa/current/fresh_round8_20261031/proxy_9960_release/libcurl.dll` `44dedefd…`** (17 bytes differ, all the port; handoff §3f) |
| Proxy (S19 test-package baseline) | `9633061d904203c7…` | S19 Fresh package (restored) | Isolated test package only |
| `deploy/archive/mission.json` today | `c7002b8480ecedb8…` | deploy (read only) | Holds the invented dummy offset and no Super-boss change |
| Proposed `mission.json` | not final | — | Five generators must all run (§3 steps 4, 5, 5b, 5c and 7). `48e51e31794fdbb7…` is **only** `gen_battle_simulator.py` applied to today's file (six `hp_disp_pos` lines). `3f51875fec688f60…` (`R8/archive_r8_v7`) is that plus `gen_material_drops.py`, without the Super boss. Recompute after all three and record the final hash |
| Material-drop sources | `b95b04af639cd07e…` | `scripts/data/material_drop_sources.json` | Extracted from the wiki caches and the one live capture; `gen_material_drops.py` reads only this and deploy |
| #14 unit MST | proposal `0c38f831…`, part v1087 `2a5d342e…`, manifest `74f51627…` | `scripts/unit_mst_corrections.py` | Round 5 |
| #22-3 text dictionary | `sgtext_leaderSkillDescription_498.zip` plus `F_RESOURCE_MST` v3276 | staged `R5/content_root_sg` | Round 5 |

## 2. Every acceptance-matrix row ([matrix](BUG_ACCEPTANCE_2026-10-27.md))

Dispositions:
- **Tested fixed:** client-observed on an isolated candidate.
- **Not applicable (evidence):** works as designed, with evidence.
- **Open.**

None is deployment acceptance.

| Row | Disposition | Evidence (latest) | Remains before release |
|---|---|---|---|
| Legend Stone question | Open | Server-tested Merit offer only | Client check of the Merit row: price, stock, purchase, reopen. Never invent Guild stock |
| #8 shops | Open (regression) | Expiry player-confirmed earlier | Final client stock/payment regression |
| #11 Randall simulator | Tested fixed (entry, battle); **status-icon defect found and fixed in round 8** | R8 v1–v3: icons on all six dummies at `0,30` | Regenerate deploy/archive (§3 step 4). Settings regression on the final candidate |
| #12 Vortex Trials | Open (content) | Scope plus R8 evidence: first encounter 8301560 | Author waves/AI/rewards; decrypt the client script first |
| #13 BB/SBB spillover | Open (regression) | Player-confirmed earlier | Evolution/fusion regression |
| #14 Seria/Tilith portrait | Tested fixed (isolated v1087) | Round 3 client; round 5 reproducible script | §3 step 6 |
| #15 EU units / Deemo | **224 EU-only units installed into deploy** (Evan, 2026-11-01; handoff §3d) | `test_eu_units.py` 15/0 on `92f41576` and live `f72310df`; run v8: login, Home, list, detail, battle and BB | Player test (Evan: "later"). Add `out/release/eu_units_assets_for_zip.txt` (1,834 files, 211 MiB) to the Cloudflare `assets.zip`. 212 static stand-in battle sprites await real part files. The 31 JP-only units and Deemo checks are not part of this |
| #16 Mystery frog | Not applicable (player-confirmed) | Earlier | Keep in regression |
| #17 Fire Mecha God | Open (content, decision) | R8: archive = old Lv. 6 layout; rev 659707 = Mecha God, HP/capture blank | Evan: which version. Then a sourced or marked HP/capture |
| #18 placeholder bosses | Open (content). #18-a staged | R8 §3e: `gen_story_bosses.py` (Juggernaut resists + Atom Cannon + AI); client v16 on `e37da19b`: cast with name banner, quest cleared | Ship after the server (§3 step 7). Then #18-b/c per boss from the source |
| **Monster skills (new, R8 §3e)** | **Fixed in candidate `e37da19b`** | v13/v14 crash on the old servers; `test_mission_monster_skills_wire.py` 10/0 (5/10 on `92f41576`); v16: Atom Cannon cast, quest cleared, no dump | Deploy the server; the live Lab trials crash until then. Weak landing on the party not confirmed |
| Special-boss status icons (new, R8 §3e) | Tested fixed (QA archive) | v11 calibration, v12 confirmation (3 of 5 bosses) | §3 step 5b |
| #19 battle items | Open (regression) | Ordinary mission player-confirmed | Raid / Grand Quest / campaign item bars |
| #20 mission 234 crash | Open (not reproduced) | Round 1 | A real-progression save |
| #22-1 Lukroar gate | Open (not reproduced) | Round 7: four pulls, gate art served | Optional missing-art negative control; repeat on the release DLL |
| #22-2 Amadream header | Tested fixed (29960 candidate) | Rounds 5, 7, 8 | §3 step 1 (9960 twin, repack/sign = Evan), then a player check |
| #22-3 Galtier leader text | Tested fixed (staged root) | Round 5 `u3_sg` | §3 step 2 |
| #23 Farm/Morgan cutscenes | Tested fixed (reported cases) | Round 1 | Natural-progression save; Morgan entry |
| #24 double-gate white bleed | Tested fixed on `9633061d` | Round 2 | §3 step 8: repeat on the release DLL |
| #25 Omni Frog SP | Tested fixed / as designed | Round 7: fusion, cap, relog | Ordinary-unit SP chance and Omni+ are unported scope, not defects |
| #26 Roglizer/Fuu | Roglizer EXP tested fixed; chests not applicable; **per-hit item channel and its generated content tested fixed (QA archives)** | R8: XP 24 for leader and helper. Run v6: channel. Run v7: generated content on missions 10 and 11, dropped, shown and stored | §3 step 7 (content after the server). The caps are decided: the live server's 3 / 5 / 1, the only limit the client enforces (handoff §3c). Fuu Zel/Karma needs a multi-sample study (not a server matter) |
| #27 slots | Tested fixed with the provider DLL | Round 3 | Depends on shipping the provider DLL and broker (§3 step 9) |
| #30 Lizeria prerequisites | Tested fixed | Round 2 | Natural-progression save |
| #33 level 999 energy | Tested fixed (server candidate) | Rounds 1–2 | Only saves after deployment benefit (§3 step 10) |
| #34 sphere crafting | Not applicable (player-confirmed) | Earlier | Keep in regression |
| #39 Mega Metal Parade | Tested fixed (capture `c28a8db9`; Super boss staged) | Rounds 3–4 | §3 steps 5, 7 |
| #40 evolved UBB | Tested fixed (mechanism, Berdette chain) | Round 7 | Optional natural-gauge story battle |
| Skill popup overflow | Tested fixed (29960 candidate) | R7 v5; R8 v2 matches it | §3 step 1. Other descData popups not patched |
| Training Ground status icons (new) | Tested fixed (QA archive) | R8 v1/v2/v3 sheet | §3 step 4. The special-boss rows: see the row above (§3 step 5b) |

## 2b. Already in deploy (EU units, 2026-11-01)

`scripts/eu_units.py install --root deploy` ran at Evan's instruction. Backups and the written-file list are in `out/backups/2026-11-01_eu_units/`.

**Deploy now stages:** F_UNIT_MST 1089, F_SKILL_MST 1537, F_SKILL_LEVEL_MST 1488, F_LEADER_SKILL_MST 752, F_EXTRA_PASSIVE_SKILL_MST 772, F_UNIT_EVO_MST 598, F_UNIT_CGS_MST 812, F_ITEM_MST 1029, F_UNIT_COMMENT_MST 707, and F_RESOURCE_MST 3277 (ten reissued dictionaries).

**Consequences for §3:**
- **Step 6 (#14 crop):** it must now be staged **on top of the EU unit table**, at a version above 1089. It can no longer be staged as 1087.
- **Step 2 (#22-3 text):** it must reissue `sgtext_leaderSkillDescription_499` (the EU one) and stage F_RESOURCE_MST above 3277. 498 and 3276 are taken.
- **Players:** every client fetches these tables at its next login, and needs the new content files: add them to the Cloudflare `assets.zip` (list above).

## 3. Proposed release actions (validate first; in this order)

> **Status, November 2 (Evan: "Apply it and test"):** steps 2, 3, 4, 5, 5b, 5c, 5d, 5e, 6, 6b and the material drops
> are DONE in deploy, in this order, and the live server (step 7) was restarted on them (handoff §3h).
>
> | Live file | Hash or version |
> |---|---|
> | `mission.json` | `5ce525d6…` |
> | `ai.json` | `ff9714dc…` |
> | `unit.json` | `f955a881…` |
> | `unit_mst.json` | `3e8a4034…` |
> | F_UNIT_MST | 1090 |
> | F_RESOURCE_MST | 3279 |
>
> Backups: `out/backups/2026-11-02_r8_content_apply/`. Full regression: every entry passes. Still open:
> - step 1 (the proxy repack and signing, Evan);
> - the Cloudflare `assets.zip` (add the new dictionaries and per-unit files);
> - steps 8-10.
>
> **6c: the 107 JP-exclusive units: DONE November 2 (Evan: "Install them"; handoff §3h), live F_UNIT_MST 1092 /
> F_RESOURCE_MST 3281, 2,619 archive records; backups `out/backups/2026-11-02_r8_jp_exclusive/`.** As run:
> - Run `python scripts/jp_exclusive_units.py install --root deploy --backup-dir <dir>`, then `check`, then restart the
>   server.
> - It needs the git-ignored wiki art in `tools/jp_art/raw`.
> - It stages F_UNIT_MST 1092 … F_RESOURCE_MST 3281.
> - Tested: `test_jp_exclusive_units.py` 16/0, client run v22.
>
> **6d: the 22 authored Home crops (Harley and the other JP units whose thumbnails are not crops of their full art):
> DONE November 3 (Evan: "Yes apply"; handoff §3h), live F_UNIT_MST 1093; backups
> `out/backups/2026-11-03_r8_home_crops/`.** Same command as 6c; client run v24 verified them on a copy.

1. **Proxy.**
   - Build the **9960 twin** from the exact offline-proxy source of `f74efc2b` (round-8 hashes in the handoff §1). Record its hash.
   - Run `content_provider_tests` (expect 1048/0/5) and `check_name_fit_site.py` (52/0) on that source.
   - Then (Evan): repack the live client with it, sign, `Add-AppxPackage`.
   - Player check: #22-2 header, popup BB/SBB/UBB under their titles, #24 summon.
2. **#22-3 (rebased, handoff §3f):** `python scripts/sgtext_corrections.py stage --content-root <root> --version 3278 --mirror-from deploy/game_content` writes `sgtext_leaderSkillDescription_500.zip` (`7c4308c6…`) and F_RESOURCE_MST 3278 (parts `4fdc8364…`, `5257d165…`) on top of the EU install's 3277. Copy those into `deploy/game_content` (or use `R8/content_rebase`). Client run v17 downloaded and extracted them. The old 498/3276 staging is obsolete.
3. **Back up** `deploy/archive/mission.json` (`c7002b84…`) and `deploy/mst/unit_mst.json`.
4. **Training Ground icons:** `python scripts/gen_battle_simulator.py`, which rewrites `deploy/archive/mission.json` (six dummy rows only). Then `--check` should print "up to date".
5. **#39 Super boss:** `python scripts/gen_metal_parades.py` (mission 100601 battle 5 only). It touches different records than step 4.
   - Run `--check` of both generators afterwards.
   - Record the final `mission.json` hash, and diff it against step 3's backup: 6 + 100601 changes only.
5b. **Special-boss icons:** `python scripts/gen_boss_icon_offsets.py --out deploy/archive/mission.json` (15 `hp_disp_pos` lines). Then run `--check` on it, `gen_research_lab.py --check` and `gen_battle_simulator.py --check`: all three must print "up to date".
5c. **#18 story bosses (handoff §3e/§3f):** the server of step 7 is now live (November 2), so this may run: `python scripts/gen_story_bosses.py --out deploy/archive` (nine story bosses' skills, AI ids, resists, HP and action counts; `ai.json` gains nine story AIs). Then run `--check`. On a server without `8aiBoHg5` these skills crash the client.
5d. **#17 Mecha Gods (handoff §3g):** `python scripts/gen_mecha_gods.py --out deploy/archive` (missions 100320-100325, six AIs 30100320-30100325). Then run `--check`. It needs the `8aiBoHg5` server (live since November 2).
5e. **#12 Vortex Trials (handoff §3g):** `python scripts/gen_vortex_trials.py --out deploy/archive` (today: 83013400 Winter Paradise). Then run `--check`. Run 5c-5e before the material drops of step 7, which must be regenerated after any `mission.json` change. `R8/archive_r8_content` is the three together (`adc18563…` / `ai.json` `ff9714dc…`).
6. **#14 (rebased, handoff §3f):**
   - `scripts/unit_mst_corrections.py propose --out <dir>/unit_mst.json` on the EU-installed table: exactly 50254 `1W9CxaFK` changes (`3e8a4034…`). Put it in place of `deploy/mst/unit_mst.json`.
   - `scripts/unit_mst_corrections.py stage --rows <dir>/unit_mst.json --content-root <root> --version 1090 --mirror-from deploy/game_content` (parts `44337b57…`, `e6de035d…`). `R8/content_rebase` holds it; client run v17 downloaded `Ver1090`.
6b. **#15 JP-only units (handoff §3g), AFTER step 2:** `python scripts/jp_units.py install --root deploy --backup-dir <dir>`, then `check`. It adds 29 archive records, the seven reissued dictionaries and **F_RESOURCE_MST 3279** (built on step 2's 3278; on a root without it, it skips the reserved leaderSkillDescription 500). The server must be restarted to load the records. `R8/jp_root` is a staged copy. Clients fetch the dictionaries at their next login. Add them to the Cloudflare `assets.zip`.
7. **Server: DONE November 2, 2026** (Evan: "Yes, deploy the new server to the live setup"). The in-tree exe is `4ab8af5b…` (copied from the candidate, byte-identical), running on 9960 since 11:47 UTC, with no migrations. Backups and rollback: `out/backups/2026-11-02_r8_server_deploy/` (handoff §3g). The Research Lab trials no longer crash (monster skills, handoff §3e).
   - Any archive that carries `material_drops` must go in **with or after** this server. The old loader rejects the whole `mission.json`: on `c28a8db9` every MissionStart of the generated archive failed with "Archive error".
   - Production `deploy/archive` has none today.
   - **Then, and only then, material drops:** with `4ab8af5b` installed (it is) and the server stopped, run `python scripts/gen_material_drops.py --out deploy/archive/mission.json` (after steps 4–5). Expect "990 mission(s), 20919 monster list(s)" on today's archive.
   - Run `--check` (expect "up to date") and `python scripts/validate_missions.py` (expect the baseline, with no C13/H16 findings). The server log should say "20919 monster(s) with per-hit material drops; 0 invalid list(s) refused".
   - Rerun the generator whenever `mission.json` is replaced, for example from the Mission Builder's `dist/`, which does not carry the lists.
8. **#24:** repeat the double-gate regression on the release DLL from step 1.
9. **#27:** blank-tile acceptance depends on shipping the provider DLL **and** the broker; the live client is otherwise cache-only.
10. **#33:** only saves that level up after deployment benefit. Old saves keep their two lost energy.
11. **Regression after steps 3–7:** `python scripts/run_bugfix_regressions.py <release exe>`. With deploy regenerated, the two stale entries in §5 must pass. Then a player smoke.

## 4. Release file list (maintained runtime, generators, tests, docs; three repos)

Excluded: `out/`, saves, screenshots, binaries, credentials, and transient session/QC/handoff notes. Sources: the round 4–8 handoffs, checked against `git status` on October 31.

**offline-proxy**:
- Every dirty or untracked entry is part of the content provider and belongs to the release: 48 paths. `git status` showed 12 entries, with `OfflineMod.win32/content/`, `tests/` and `docs/` untracked as whole directories.
  - `OfflineMod.win32/content/` — every `cp_*.cpp/.h` (25 files). Round 8 changed:
    - `cp_gate_flash.cpp`, `cp_gate_flash_internal.h`, `cp_gate_flash_logic.cpp`
    - `cp_name_fit.cpp`, `cp_name_fit_internal.h`, `cp_name_fit_logic.cpp`
  - `OfflineMod.win32/console_log.cpp/.h`, `main.cpp`, `curl_exp.cpp`, `CMakeLists.txt`, `serverconfig.h.cmake`
  - root `CMakeLists.txt`, `.gitignore`, `README.MD`
  - `tests/`: `test_main.cpp`, `test_util.h`, `fake_broker.h`, `CMakeLists.txt`, and `fixtures/` (`*.py` checkers, `make_signatures.py`, `bf_2.19.6.0_signatures.json`, `coverage_reports.txt`)
  - `docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md`

**BF-WorkingDirRust** (rounds 4–8 maintained set; `??` = untracked, `M` = modified):
- **Server:**
  - `gimuserver/archive/MissionArchiver.cpp` (M; round 8: `6e2223fc…`, with the §3e skill rows), `gimuserver/archive/MissionArchiver.hpp` (M, `e43a470d…`), `gimuserver/gme/handlers/Mission.cpp` (M, `cd285b58…`)
  - `packet-generator/assets/net/handlers.kdl` (M in the submodule, `47426f7d…`: `MissionStartResp.skills` / `8aiBoHg5`)
  - `scripts/test_mission_monster_skills_wire.py` (??, `58e88b64…`)
  - `scripts/test_mission_material_drops_wire.py` (??, round 8, `6405532b…`: now strips deploy's lists so it tests the channel alone)
  - `gimuserver/packets/all.hpp` (generated, git-ignored, `fbc8d3a4…`): regenerate from packet-generator rather than copying it
- **Generators:**
  - `scripts/gen_metal_parades.py` (M, `8c02de30…`)
  - `scripts/gen_battle_simulator.py` (M, round 8, `6c9c29a1…`)
  - `scripts/unit_mst_corrections.py` (??, `4f0a2960…`)
  - `scripts/sgtext_corrections.py` (??, `66f84861…`)
  - `scripts/gen_material_drops.py` (??, round 8, `c8e186d7…`) and its input `scripts/data/material_drop_sources.json` (??, `b95b04af…`)
  - `scripts/validate_missions.py` (M, round 8, `90adc9ef…`: C13/H16 material-drop checks, `--archive DIR`)
  - EU units (round 8, §3d):
    - `scripts/eu_units.py` (??), its data `scripts/data/eu_units/` (??; rows, text, report) and `scripts/test_eu_units.py` (??, registered).
    - The builder lives in git-ignored `tools/eu_datamine/` (with the downloaded datamine).
  - Round 8 §3e:
    - `scripts/gen_boss_icon_offsets.py` (??, `17b7d50a…`) with `scripts/test_gen_boss_icon_offsets.py` (??, `f8db2887…`).
    - `scripts/gen_story_bosses.py` (??, `61e1e37a…`) with `scripts/test_gen_story_bosses.py` (??, `b440df0c…`).
    - `scripts/gen_research_lab.py` (M, `396444d3…`: re-derives a copied story boss's icon offset for the trial's position).
    - `scripts/qa/check_fixture_units.py` (??, `1b570d6a…`) with `scripts/test_check_fixture_units.py` (??, `760b6c16…`).
    - `scripts/test_eu_units.py` (now `182d3756…`, with EU evolution).
  - Round 8 §3g (November 2):
    - `scripts/gen_mecha_gods.py` (??, `18f5c6fb…`) with `scripts/test_gen_mecha_gods.py` (??, `4ca76f40…`).
    - `scripts/audit_unit_playability.py` (??, `d1052741…`) and its source `scripts/data/wiki_units.json` (??, `f2512e73…`). Its outputs are `docs/UNIT_PLAYABILITY_AUDIT.md` and `docs/unit_playability.csv` (??).
    - `scripts/jp_units.py` (??, `0302023a…`) with `scripts/test_jp_units.py` (??, `6276e995…`) and `scripts/data/jp_units/units.json` / `text.json` (??, `317ad02f…` / `4693c1ea…`).
    - `scripts/gen_vortex_trials.py` (??, `0e11a5e1…`) with `scripts/test_gen_vortex_trials.py` (??, `384746ea…`) and `scripts/data/vortex_trials_wiki.json` (??, `3a72e025…`).
    - `scripts/eu_units.py` (now `db7f8410…`: `reissue_dictionaries` lifted out of `install`) and `scripts/run_bugfix_regressions.py` (now `fba6f080…`, with the four §3g suites registered).
- **Tests:**
  - `scripts/test_gen_metal_parades.py` (??), `scripts/test_metal_parades_wire.py` (M)
  - `scripts/test_mission_capture_wire.py` (??)
  - `scripts/test_unit_mst_corrections.py` (??), `scripts/test_sgtext_corrections.py` (??)
  - `scripts/test_battle_simulator_wire.py` (M, round 8, `b9273fa4…`)
  - `scripts/test_gen_material_drops.py` (??, round 8, `c49e7d8a…`)
  - `scripts/run_bugfix_regressions.py` (M, `babbe3dc…`, with the three §3e suites registered)
- **QA harness:**
  - `scripts/qa/qa_run.py`, `run_ctl.py`, `prep_run.py`, `prep_chain.py`, `http_relay_delay.py`, `test_qa_run.py`
  - `scripts/QA_RUN_HELPERS.md` (all ??)
- **Docs:**
  - `docs/BUG_ACCEPTANCE_2026-10-27.md` (??), `docs/REMAINING_CONTENT_SCOPE.md` (??), this file
  - The workflow docs `docs/BATCH_FIX_WORKFLOW.md` (??) and `docs/BF_OFFLINE_SERVER_HANDBOOK.md` (M)
- **Not in the repo (git-ignored `tools/`), needed by a release action:**
  - `tools/gen_mst_download.py` (`0847a45c…`)
  - `tools/bin/pe_x86.py` (?? `ed4fe9ad…`)

**packet-generator**:
- `assets/net/mission.kdl` (M; round 8: `4e292437…`, after round 4's `b524a34c…`)
- `assets/archive/mission.kdl` (M, round 8, `a0a4231a…`)
- `assets/net/items.kdl` (M; older work)

**Not reconciled here, so review it before committing:**
- BF-WorkingDirRust has **230** dirty entries (160 untracked, 71 modified), most of them older work outside rounds 4–8. That includes the `gimuserver/` handler changes behind the deployed `f72310df` (handoff `project_live_deploy_2026_10_24`).
- This list covers only what rounds 4–8 changed or depend on. It is not a whole-tree commit plan.

## 5. Regression on isolated fixtures

`python scripts/run_bugfix_regressions.py <server exe>`, fixtures copied from the live save (read only).

**Result on `c28a8db9`: 67 entries, 2,754 checks passed, 6 failed.**

**Result on the release candidate `92f41576`** (after the per-hit item channel): **68 entries, 2,772 passed, 6 failed.**
- The new `test_mission_material_drops_wire.py` passed 19/0.
- The failed entries are exactly the two below; everything else passed.
- `validate_missions.py` matched its baseline.
- Summary: `R8/SUMMARY_items.md`.

**After the generated drop content (handoff §3c), same server `92f41576`:** **69 entries, 2,806 passed, 6 failed.**
- The new `test_gen_material_drops.py` passed 30/0.
- `test_mission_material_drops_wire.py` passed 19/0, now independent of deploy's content.
- `validate_missions.py`, with its new C13/H16 checks, matched its baseline.
- The failed entries are the same two below.
- Two suites report two more checks each than before, both still at 0 failed. Their check counts depend on the data: `test_brave_slots_wire.py` 249 → 251 and `test_reward_lock_snapshots_wire.py` 20 → 22.
- Summary: `out/qa/current/SUMMARY.md` (also `R8/SUMMARY_drops.md`). Log: `R8/regressions_r8_drops.log`.

**After handoff §3d/§3e (EU install, boss icons, fixture checker, #18-a), same server:** **73 entries, 2,862 passed, 6 failed**: the same two below. New suites: `test_gen_boss_icon_offsets` 15/0, `test_check_fixture_units` 13/0, `test_gen_story_bosses` 20/0; `test_eu_units` 24/0 (with EU evolution). Log: `R8/regressions_r8_final.log`, summary `R8/SUMMARY_final.md`.

**On the skills candidate `e37da19b` (handoff §3e, monster-skill contract):** **74 entries, 2,865 passed, 6 failed**: the same two staged-data entries. New: `test_mission_monster_skills_wire` 10/0; `test_gen_story_bosses` 20/0 with the skill. `validate_missions.py` matched its baseline. Log: `R8/regressions_r8_skills.log`, summary `R8/SUMMARY_skills.md`.

**Final, on `4ab8af5b` (handoff §3f):** **74 entries, 2,879 passed, 6 failed**: the same two staged-data entries. `test_gen_story_bosses` 23/0 (nine bosses), `test_sgtext_corrections` 14/0 and `test_unit_mst_corrections` 19/0 (rebased), `test_mission_monster_skills_wire` 10/0, `test_gen_boss_icon_offsets` 15/0, `test_eu_units` 24/0. Log: `R8/regressions_r8_final2.log`, summary `R8/SUMMARY_final2.md` (= the current `out/qa/current/SUMMARY.md`).

Expected and deliberate until §3 step 4:
- `gen_battle_simulator.py --check`: "stale: mission.json".
- `test_battle_simulator_wire.py`: its six new `hp_disp_pos "0,30"` checks fail against the un-regenerated deploy archive.

Both checks pass against the round-8 archive copy (`--check --archive R8/archive_r8_icons`: "up to date").

## 6. Stop for review

No deployment, signing, install or commit was done. Evan/Codex: review §2's open rows, decide #15 and #17, and approve or amend §3 before anything touches `deploy/` or the live client.
