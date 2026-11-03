# Fresh session, round 4: handoff (October 30, 2026)

The round of [CLAUDE_FRESH_ROUND4_2026-10-30.md](CLAUDE_FRESH_ROUND4_2026-10-30.md), after the round-3 QC ([QC_FRESH_ROUND3_2026-10-30.md](QC_FRESH_ROUND3_2026-10-30.md)). This is a transient coordination note.

Nothing was staged, committed, pushed or deployed. No package was installed, signed or reset, and the machine was not rebooted. No live save or deploy content was written. The live save `239f9e69…` and the deploy files `deploy/mst/unit_mst.json` `193d3b9f…`, `deploy/archive/mission.json` `c7002b84…` and the staged `F_UNIT_MST` folder were all re-hashed unchanged. The Word tracker was not touched; it still carries its October 28 notes, and the matrix ([BUG_ACCEPTANCE_2026-10-27.md](BUG_ACCEPTANCE_2026-10-27.md), new Round 4 column) is the latest status.

At the start, no `gimuserverw.exe` was running and ports 9960/29960/29961 had no listener. The round-3 zombie pid 16196 was gone, so there was no blocker and no reboot. QA output: `out/qa/current/fresh_round4_20261030/` (`R4/` below).

## 1. Results

| Target | Result | Runtime/data change |
|---|---|---|
| T1 containment (QC P2) | **Fixed.** `run_dir` refuses a link at `visual`, `saves` or the run, and requires the resolved saves root to be `<qa>/visual/saves`. Every file a helper or its server writes in a run is checked first. `run_ctl start` refuses a config naming any database but the run's own. Helper tests: 57 passed, 0 failed (14 new), run twice. | none (QA tooling) |
| T2 portrait reproducibility (QC P2) | **Done.** `scripts/unit_mst_corrections.py` regenerates the accepted proposal, part and manifest byte for byte from the pristine source, with idempotence, refusals and hard-link safety tested (27/0, in the full regression). Same bytes, so round 3's client acceptance stands. | none to deploy/; proposal and staging in disposable roots |
| T3 capture wire | **Mismatch confirmed on the supported x86 client and fixed in an isolated candidate.** One capture per wave is the client's parse; the server's `-`-joined extra drops were read into the first drop's fields. The candidate sends only the first successful roll, which is exactly what the client already kept, with an unchanged random sequence. Client rerun on the same route: same captures, clean claim. | **runtime**: `gimuserver/archive/MissionArchiver.cpp` (candidate `c28a8db9…`) |
| Boundaries | Mega EXP and capture odds were **not changed**. The wiki revision histories were checked and recommendations made (§5). Evan then chose Super option A and keeping Mega EXP at 5000: **Super's boss is now Metal God + Water King + Thunder King** in a disposable archive, tested and played (§9). | data: Super battle 5, disposable archive only |

Full regression on the final candidate `c28a8db9…`: **65 suites, 2,726 checks passed, 0 failed, no failed entries** (`out/qa/current/SUMMARY.md`, 2026-10-30 07:40–08:03).

- The earlier reports (round 1, `03101975`, 63 suites / 2,692 checks) were copied to `R4/regression_before/` first.
- The two new suites account for the difference.
- `test_brave_slots_wire.py` passed 250 checks against 251 before. That check runs only when a random roll awards a unit (`if want_u:`), so it is roll variance, not a regression.
- `validate_missions.py` has the same baseline digest, `7bae35bbe120`.

## 2. Target 1: helper containment (`scripts/qa/`)

| File | Change |
|---|---|
| `qa_run.py` | `run_dir` refuses a link (symlink, junction or other reparse point) at `<qa>/visual`, `<qa>/visual/saves` or the run folder, and requires the resolved root and run to equal their lexical paths. New `run_file`: a written file must be absent, or a regular file with one link that resolves to itself; symlinks, junctions, redirects and shared hard links are refused. New `check_run_config`: exactly one database, the run's own `gme.sqlite`. `save_record` checks its temporary and final names. |
| `run_ctl.py` | Before anything is written or launched, start checks the config's database, every log (`server.log`, `server.err.log`, `relay.stdout.log`, `relay.log`, `relay.log.dump.jsonl`), `run.json` and its temporary file, and the database's journal, WAL and SHM files. New `--relay-dump HANDLER…` (§4). |
| `prep_run.py`, `prep_chain.py` | Re-check the run after creating it; the database (and its journal and WAL files) and the config are written only as validated `run_file`s. |
| `http_relay_delay.py` | Optional decrypting dump (§4). Forwarding is unchanged. |
| `test_qa_run.py` | New `--work` folder, so earlier evidence is kept. 14 new checks: junctions at `visual` and at `saves` (run_dir refuses them; prep_run exits 2 and writes nothing at the targets); a config naming another save, and the live save; a hard-linked database (start and prep_chain both refuse); a hard-linked config and server log; a hard-linked temporary record; a symlinked database (ran, not skipped); unchanged link targets; bad file names. Every link target is a disposable file or folder inside the work folder. |
| `QA_RUN_HELPERS.md` | Documents the new refusals, `--work` and `--relay-dump`. |

Logs: `R4/test_qa_run_1.log` and `_2.log` (rerun after the relay and run_ctl edits), each 57 passed, 0 failed, no skips. Run ownership (pid + creation time + image + command line) is unchanged; no process is ever stopped by name.

## 3. Target 2: portrait correction (`scripts/unit_mst_corrections.py`)

- **`propose --source deploy/mst/unit_mst.json --out DIR/unit_mst.json`** applies the declarative `CORRECTIONS` table at byte level, so CRLF and every other byte are kept.
  - The table holds one entry, with its evidence: 50254 `1W9CxaFK` `245,-5,135,369` → `298,48,180,492`.
  - A current value equal to OLD is applied; equal to NEW is already applied (idempotent); anything else is refused as an unknown source.
  - The parsed result must differ in exactly the corrected (unit, key) pairs.
  - It never writes into deploy/ or onto its source, and an existing output is only ever confirmed identical, never overwritten.
- **`stage --rows … --content-root ROOT --version N [--mirror-from SRC]`** writes `F_UNIT_MST` version N and its manifest.
  - Uses the same encoding and row split as `tools/gen_mst_download.py`. That tool is git-ignored, so the encoder is reimplemented in maintained code.
  - New files and the manifest are written by rename, so a hard-linked source inode is never written through.
  - An identical existing part is kept. Different bytes under the same version are a refused collision, as is a version below the staged manifest's. The version is always explicit.
  - The deploy staging folder and any mirror source are hashed before and after.
- **Round-4 output** (logs `R4/portrait_propose.log`, `R4/portrait_stage.log`):
  - `R4/portrait/unit_mst.json` `0c38f831…` = the round-3 accepted proposal.
  - `R4/portrait/stage_root/content/F_UNIT_MST/Ver1087_2r9cNSdt.dat` `2a5d342e…` and `manifest.json` `74f51627…` = the client-accepted candidate.
  - The bytes match, so the Home and unit-screen checks were not repeated.
- **Tests** `scripts/test_unit_mst_corrections.py`, 27/0 (`R4/test_unit_mst_corrections_2.log`; also in the full regression).
  - Synthetic cases: another row holding the same old value is left alone; CRLF is kept; idempotence; unknown, missing and duplicate units are refused.
  - Real data: the pristine source gives `0c38f831…`; exactly one parsed difference; identical re-proposal; refusals for an existing output, the source itself and deploy/.
  - Staging through a hard-link mirror reproduces the accepted part and manifest, and leaves the source's v1086 and manifest untouched.
  - A mirror whose source already holds v1087 is kept, with no write-through.
  - Refused: a collision under the same version, a lower version, uncorrected rows, a root inside deploy/.
  - An explicit v1088 stages beside a linked v1087.
  - The first test run (`_1.log`) had one wrong expectation in the test itself (link count 3; it is 2), corrected.

Deploying is Evan's decision:

1. Copy the proposal over `deploy/mst/unit_mst.json`.
2. Run `python tools/gen_mst_download.py F_UNIT_MST 1087` (same bytes).
3. Restart, then do a player check after the client's download.

## 4. Target 3: captures

### Supported-client trace (x86 `BraveFrontier.Windows.exe` fb4f3c25…, `tools/bin/pe_x86.py`; RVAs)

| Step | x86 | arm64 (corroboration) |
|---|---|---|
| `MissionNumResponse::readParam` matches `LE6JkUp7`, builds the string and calls `shared()` then `parse` | 0x4F5FD7, 0x57FB30, 0x57FCB0 | 0x13E39C8 → 0x12671C8 |
| `MissionDropInfoList::parse`: split on literal 0x1689A94 `"@"`, each record on 0x169BB44 `"|"`; `StrToInt` battle order; field [1] → `parseUnitDropInfo` **once**; field [2] → `parseTreasureBoxDropInfo` 0x57F570 | 0x57FCB0 | 0x12671C8 |
| `parseUnitDropInfo`: skip a lone `" "` (0x168AE98), construct ONE drop (0x581660), split on 0x168A218 `":"`: [0] slot, [2] unit, [3] level, [4] type, [5] extra skill only if there are at least 6 tokens (`cmp eax, 6`); retain and append once; no loop | 0x57F340 | 0x126617C |
| `CommonUtils::StrToInt`: stream read, stops at a non-digit (`"2-1"` → 2) | 0x65E760 | — |
| `getObject(battleOrder)`: first record whose +0x14 equals the order | 0x20D090 | 0x1267150 |
| `getDropUnit(slot)`: first drop whose slot matches | 0x20B280 | 0x1266548 |
| Battle use: battle order and slot → capture effect (0x42E520) | 0x9EB0B7..0x9EB0EA | `MonsterParty::entryMonsters` |

So `-` has **no consumer** in the unit-drop field. A multi-drop wave becomes one drop whose type token is `"<type>-<next slot>"` (prefix-parsed) and, with 6 or more tokens, an extra skill equal to the next entry's placeholder `"0"`.

### Reproduction on the S19 client (DLL 9633061d)

- **Fixture.** `R4/archive_wave_fixture` (`R4/tools/make_wave_capture_fixture.py`, mission.json `5679784c…`): 100600's waves capture 0 / slot 2 / slots 0–2 / slot 0 / slots 1–2 at 0 or 100 %, flagged `authored_capture`.
  - The wire check `R4/tools/parade_capture_rate.py` showed exactly 7 captures pre-rolled on every start.
- **Relay dump.** New: `run_ctl --relay-dump MissionStart MissionEnd` decrypts the exact fields into `relay.log.dump.jsonl`.

| | Baseline `w1_base` (server `03101975`) | Candidate `w2_cand` (server `c28a8db9`) |
|---|---|---|
| Sent `LE6JkUp7` | `1\| \| @2\|2:0:60132:1:1\| @3\|0:0:20202:1:2-1:0:30202:1:3-2:0:40203:1:3\| @4\|0:0:50203:1:2\| @5\|1:0:20202:1:5-2:0:40202:1:4\| ` | `1\| \| @2\|2:0:60132:1:1\| @3\|0:0:20202:1:3\| @4\|0:0:50203:1:3\| @5\|1:0:20202:1:3\| ` |
| Capture counter by wave | ×00, ×01, ×02, ×03, ×04 | ×00, ×01, ×02, ×03, ×04 |
| Units Acquired | Metal Ghost, Water Ghost, Light King, Water Ghost | the same four |
| Claimed `3MAT6quo` | `60132:1:1,20202:1:2:0,50203:1:2,20202:1:5:0` (stray `:0` on both multi-drop waves) | `60132:1:1,20202:1:3,50203:1:3,20202:1:3` |
| Stored rows (unit, type, level, extra skill) | (60132,1,1,0) (20202,2,1,0) (50203,2,1,0) (20202,5,1,0) | (60132,1,1,0) (20202,3,1,0) (50203,3,1,0) (20202,3,1,0) |

Types are rolled per start, so they differ between the runs; the selection is identical. Captures are in `R4/visual/run/W1/`, `W2/` (`battle_sheet.png`, `results_sheet.png`).

**Settlement.** `MissionEnd` (`Mission.cpp` `parseUnitDrops`) stores the client's claim: comma-separated `id:level:type`, a 4th field ignored. It is checked only for a known unit, level ≤ max and type ≤ 6, and **not against MissionStart's roll**. That stays as it is (offline trust model). It is recorded because "trace settlement validation" asked for it. A repeated settlement of the same serial adds nothing (existing behaviour, re-tested).

### Fix (isolated candidate)

`MissionArchiver::encodeMissionDropInfo` now keeps only the first non-empty `encodeUnitDrop` per stage. It still calls `encodeUnitDrop` for every monster, so `RandomUInt` is consumed exactly as before: types, chests and Mimics see the same random sequence, and the selected capture is the one the client already kept. No odds, rates or client hooks changed. MissionArchiver is the only `drop_info` producer (Town's column is unrelated).

`packet-generator/assets/net/mission.kdl` `drop_info` doc now records the verified parse; `gimuserver/packets/all.hpp` was regenerated by the build and is identical to the hand-mirrored text.

Builds:

- `R4/tools/build_server_round4.sh` is the round-1 recipe: shared tree, temporary output override removed afterwards. The in-tree `f72310df…`, QC `c9405702…`, S19 `887e68e0…` and round-1 `03101975…` EXEs were checked unchanged.
- First build `9fa6f534…` (code only); final `c28a8db9…` after the KDL doc regenerated `all.hpp` (logs `R4/build/`).

New test `scripts/test_mission_capture_wire.py` (in the full regression), on the same fixture built in its own folder:

- every wave sends no capture or exactly the first rolled one, as `slot:0:unit:1:type`;
- no `-`; types 1..5;
- a client-model parse keeps exactly what was sent;
- a claim settles with exact units and types;
- a repeated settlement adds nothing;
- the legacy `:0` claim still settles;
- 100601/100614 production data send at most one complete entry per wave.

Results: baseline `03101975` **4 failed** (`R4/test_mission_capture_baseline.log`); candidate **8/0**. Focused suites on `c28a8db9`: capture 8/0, metal parades 18/0, battle simulator 93/0, mission settlement 23/0.

## 5. Recommendations on the open behaviour questions (nothing changed)

The wiki revision histories are fetched into `R4/wiki/` (API, fandom).

- **Mega EXP (decided: keep 5000).** Parade:MMP was created on 2015-05-19 with exp 4000 and has said 7000 from revision 194288 (2015-05-29) through 630716 (2019-11-22). It never said 5000. The server's own mission MST, which is version-specific data, says 5000, and so does the archive. **Recommend keeping 5000**: a wiki page cannot show which historical value the 2.19.6 data carried, and the MST is the only version-tied source.
- **Super boss (decided: A, §9).** Parade:SMP lists "1 God and 2 Kings" or "1 Crystal" in every checked revision (64415 of 2014-08-07 through 630719 of 2019-11-22). The normal parade's page also allows "1 God". No MST evidence exists, because battle content is server-side. Our lone Dark God matches neither Super formation.
  - **Recommended (A):** Metal God + Water King + Thunder King. That is the wiki's "1 God and 2 Kings", following our normal parade's boss pattern (Dark King + Water Ghost + Thunder Ghost) one tier up.
  - **Alternative (B):** a lone Dark Crystal, the wiki's "1 Crystal".
  - Evan chose A on October 30; done in §9.
- **Capture odds.** Unchanged. The client keeps one capture per wave, so authored per-monster odds reach the player as "the first success in monster order" per wave. More captures could only come from changing per-wave odds, which is a separate decision.

## 6. Untested / not done

- The candidate is not deployed; deploying it, and the portrait, is Evan's call.
- Mimic capture (`encodeMimicDrop`, treasure field) was not exercised by the client this round.
- Elemental, Jewel and Imp parades were not played.
- The arm64 address for `getDropUnit` (0x1266548) comes from the symbol table; I did not re-disassemble it.
- The S19 client now has whichever `F_UNIT_MST` version its last run served (v1086 from the round-1 content root).
- Release gates are unchanged:
  - the tuto8 packaging requirement;
  - startup completeness;
  - missing content #12/#15/#17/#18;
  - #22 presentation;
  - an explicit disposition for every original report.

## 7. Changed files (SHA-256 prefixes)

Maintained sources:

- `scripts/qa/qa_run.py` `152ac7435a40f864`
- `scripts/qa/run_ctl.py` `eb6aeb0c71628261`
- `scripts/qa/prep_run.py` `2fafaa2def69f303`
- `scripts/qa/prep_chain.py` `1280736b138bac06`
- `scripts/qa/http_relay_delay.py` `fce44a4cd9bf3a1e`
- `scripts/qa/test_qa_run.py` `a8e6d66230026268`
- `scripts/QA_RUN_HELPERS.md` `24731d6633fc92e2`
- `scripts/unit_mst_corrections.py` (new) `360c4e8f41a594bb`
- `scripts/test_unit_mst_corrections.py` (new) `76797664c2b4f34f`
- `scripts/test_mission_capture_wire.py` (new) `4919c66787ec6a35`
- `scripts/run_bugfix_regressions.py` (two suites registered) `a643ab09e92be477`
- `gimuserver/archive/MissionArchiver.cpp` `c9e1b979031ed42a`
- `packet-generator/assets/net/mission.kdl` `b524a34cfa38d9d7`
- `gimuserver/packets/all.hpp` (generated, git-ignored) `fbc8d3a4bb8ee0b5`
- `scripts/gen_metal_parades.py` `8c02de30b59f0bdf` (§9)
- `scripts/test_gen_metal_parades.py` (new) `7de727467947c2e2`
- `scripts/test_metal_parades_wire.py` (absolute archive path, `--fixture`) `6801e9b7af5ddcf2`
- `scripts/run_bugfix_regressions.py` is now `8265d9a8121db6e6` (also registers `test_gen_metal_parades.py`)
- `docs/BUG_ACCEPTANCE_2026-10-27.md` `8792e831edb8b123` (backups `R4/BUG_ACCEPTANCE_before_round4.md`, `R4/BUG_ACCEPTANCE_before_super_boss.md`)

QA artifacts:

- `R4/tools/make_wave_capture_fixture.py` `6ddc0910fe706d05`
- `R4/tools/build_server_round4.sh` `6bdae2f38d4328ad`
- `R4/archive_wave_fixture/mission.json` `5679784c468ac433`
- `R4/archive_super_boss/mission.json` `17156ac83164deba` (deploy's is still `c7002b8480ecedb8`)
- candidate `R4/bin/gimuserverw.exe` `c28a8db9efacc43d5c5f4bf707d8036520f8e8df7e69e3438ba6db1e67e532fe`

Tracked diff: `23e9d1e5def5…`, previously `88c473d5…`. The difference is only this round's files.

For an eventual source inclusion list, take the maintained sources above. Exclude `out/`, saves, `.pdb`/`.exe`, the relay dumps, the wiki JSON, and this coordination note.

## 8. Next bounded round

1. Evan decides whether to deploy:
   - the capture candidate (or a rebuild of this source);
   - the portrait correction (§3);
   - Super's boss: `python scripts/gen_metal_parades.py` rewrites `deploy/archive/mission.json` to `17156ac8…`.
2. Clear the leftover pid 11336 at the next normal reboot (§9); until then, run `test_metal_parades_wire.py` with `--fixture <new name>`.
3. Optionally exercise a Mimic capture and one elemental parade on the candidate.
4. Release gates, as listed in §6.

## 9. Addendum: Super Metal Parade boss (Evan chose option A, October 30)

- **Change.** `scripts/gen_metal_parades.py` now raises all five of Super's waves one tier from the normal parade (it kept a lone Dark God finale before).
  - The boss wave is therefore the normal boss wave (Dark King + Water Ghost + Thunder Ghost) one tier up: **Metal God (Dark God) + Water King + Thunder King**.
  - Same positions, wiki HP 100/35/35, DEF 999999, capture 60 %, using the archive's own monster rows 61650 / 21850 / 41850.
  - Mega keeps its lone Dark Crystal; the MMP page lists no boss formation.
  - The generator gained `--source` and `--out` (`--out` refuses deploy/ and existing different bytes), and its docstring records the decision.
- **Output.** `R4/archive_super_boss/` (hard links, plus a new `mission.json` `17156ac8…`). Against deploy's `c7002b84…`, exactly one mission differs (100601), and only its battle 5. deploy/archive/mission.json was not written.
- **Tests.**
  - `scripts/test_gen_metal_parades.py` (new, no server, in the full regression): 11/0. It checks the formation, the one-tier-up rule, the monster rows, the tiers per parade, Mega's finale, the untouched normal parade and other missions, idempotence, and the deploy refusal.
  - `test_metal_parades_wire.py` on the candidate with the new archive: 18/0 (`R4/test_metal_parades_super_boss.log`).
- **Client run `s1_super`** (server `c28a8db9`, archive `R4/archive_super_boss`, 3 Metal keys). Captures: `R4/visual/run/S1/` (`battle_sheet.png`, `boss_wave.png`, `results_sheet.png`).
  - MissionStart sent boss group 61650/21850/41850 with HP 100/35/35.
  - After BATTLE 5/5 and WARNING, the frames show a crowned Metal God, a blue Water King and a yellow Thunder King; the target bars name "Metal God" and "Thunder King".
  - Clear: 3,000 XP (exp 4068 → 7068), Zel 96, Karma 75.
  - Units Acquired: Water King, Thunder King, Water King, Light God, Metal God. That equals the claim `20203:1:4,40203:1:4,20203:1:1,50204:1:4,60134:1:3` and the five stored rows, one capture per wave.
- **Full regression rerun** (old reports kept in `R4/regression_after_capture/`): 66 entries, 2,716 checks passed, 0 failed.
  - One failed entry: `test_metal_parades_wire.py` raised `PermissionError` before any check. Its fixture database `out/qa/current/metal_parades/gme.sqlite` is held by a leftover process (next bullet).
  - The same EXE and suite against deploy's archive in a fresh fixture: **18/0** (`R4/test_metal_parades_rerun.log`).
  - Every other suite matches the first run, except `test_brave_slots_wire.py` at 247 checks against 250. That check runs only when a roll awards a unit; there were 0 failures.
- **Leftover process.** pid 11336 (`R4/bin/gimuserverw.exe out/qa/current/metal_parades/config.json`) came from my first wire run with a relative `--archive` path. The server failed at boot and cannot be stopped.
  - Stop-Process was refused for this verified pid, and nothing else was touched.
  - It listens on no port.
  - It holds only that fixture's database, so it blocks that fixture name until the next reboot. I did not reboot.
  - The suite now resolves `--archive` to an absolute path and accepts `--fixture`.

