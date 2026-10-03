# QC review against mine/main — September 30, 2026

## Decision

**Not ready for release.** The isolated build and all supplied feature suites
pass, but independent edge tests reproduce two mission-run defects. Client
visuals, client frame rate and actual Windows resume behavior remain untested.
No live server/save was modified; no commit, push or deployment was performed.
Product code was left unchanged by this review. A failing regression test and
review documents were added so the next implementation session has concrete work.

`mine/main` was fetched successfully and resolves to
`7851dba8ebcc97b4d4793aae358b3d18b47a6ad2`, also current HEAD. This review covers
the **whole working tree against that baseline**, including relevant untracked
files and 11 modified packet-generator files. There are 33 modified parent
paths (including the dirty submodule entry), 29 relevant untracked files at
inventory time, and 72 untracked backup/staging paths excluded from gameplay
review. See [the exact inventory](DIFF_INVENTORY_2026-09-30.md).

There is no committed boundary between Claude Session 2 and previous Codex work,
so attribution below describes areas added/extended since the prior report,
not a provable per-line authorship claim. `docs/CLAUDE_SESSION_2_HANDOFF.md` was
not found. Older September 29 status documents are historical and are superseded
by this review where they conflict.

## Reproduced findings

### P1 — legacy MissionEnd bypasses a new run serial

`gimuserver/gme/handlers/Mission.cpp:576` accepts a low mission-ID serial whenever
the open mission matches, even when `open_mission_serial` identifies a new run.
Reproduction: start mission 10 twice; the current serial is 1000000002. Submit
MissionEnd with serial **10**, not that issued serial, and 123 reported Zel.
The server pays **423 Zel** (including clear reward) and closes the current run.
A delayed pre-upgrade result can therefore settle the wrong newly started run;
the genuine result then cannot settle it.

Restrict legacy acceptance to a genuinely legacy open record, preserving the
intended migration behavior. Do not remove the new-run ownership check or make
all historical NULL records invalid without a migration policy. Add mixed-era
tests, not only issued-serial versus issued-serial tests.

### P1 — stale MissionContinue spends currency and corrupts resume state

`gimuserver/gme/handlers/MissionBreak.cpp:58` resolves historical serials but does
not require them to be the currently open battle. The subsequent charge and
break write still execute. Reproduction: issue runs 1000000003 and 1000000004,
then Continue the superseded 1000000003. The request **charges one gem** and
sets `mission_break_serial` to **1000000003**, while the open run is 1000000004.
It also records a continue against the mission shared by the current run.

Validate current run ownership before any mutation, handle unknown/mismatched
serials consistently, and make validation/payment/continue marker/resume writes
atomic. Define retry behavior separately: a repeated request must not be
confused with a legitimate second revival in the same battle. The unchecked
fallback for unknown serials also needs negative coverage.

Both defects are reproduced by the newly added
`scripts/test_mission_run_edges_wire.py`: **0 passes, 2 failures**. Run it with
the isolated executable; details are in `out/qc_2026-09-30/mission_edges.log`.
These are independent negative cases missing from the 22 passing settlement
checks. They have not been repaired in this review.

## What changed, grouped by system

| System | Combined diff and latest extension | Server QC / remaining limits |
|---|---|---|
| Inventory, #34 | Persistent warehouse rows; exact-copy sales/equipment/favorites; unseen-row marker and provisional `INT_MAX-itemId` support; Merit delivery by row; favorite snapshots on reward paths | 35 sphere + 35 placeholder + 18 integration checks pass. Immediate requests are now covered server-side. Actual Windows list behavior, multiple provisional copies with different locks, interrupted response/seen-marker synchronization and capacity still need client/integration coverage. |
| Unit sale | Sphere return, unit deletion, currency and response now in one transaction | Injected failure rollback and returned locks covered by integration tests. Check client inventory refresh. |
| Missions, #19 | Owned-bar transfers and consumption; per-run serial table and open serial; Continue maps serial to mission | 41 core + 22 settlement checks pass, but both independent failures above block acceptance. Raid/Grand Quest and all suspend modes are not certified. |
| Progression, #23/#30 | Scenario marker preservation; story prerequisites broadened from any-of to all-of; direct entry follows story checks | 23 Lizeria + 32 story checks pass. This is broader than the previous bounded Lizeria patch. Locked errors still close sessions; verify stale tile UX and actual story unlock scenes. |
| Level/energy, #33 | Level-1000 sentinel row, level-999 cap, energy parity/timer correction, debug clock | Core checks pass. Result animation, edited saves and timer display unplayed. |
| Evolution, #40 | Destination BB/SBB/leader skill fields, transactional recipe/material/payment handling, eligibility checks and returned inventory locks | 23 checks pass on representative recipes. Full chains, upgrades, Omni paths and UBB presentation remain unplayed. |
| Omni SP, #25 | New fe_sp/fe_used_sp/fe_max_sp columns/defaults and packet mapping; material SP gains in UnitMix; parsed UnitExt fields | 36 checks pass. This implements fusion gains, not complete Omni+ or enhancement spending. Ordinary-fodder random SP chance is explicitly not modeled. |
| Leader bonuses, #26 | Quest EXP passive 97 from owned leader and resolved helper, additive percent and rounding | 21 checks pass. Roglizer variants tested; Fuu drops are credited from client reports, not newly simulated. Real client drop effects, other multipliers and helper behavior need gameplay evidence. |
| Slots, #27 | Prize table grows from 9 to 54 entries; reel-strip data; SlotAction charge/grants/response transaction | 247 checks pass. Rates/consolation include authored policy; multi-roll artwork and frame pacing remain unplayed. |
| Simulator, #11 | Mission 6000000 and six dummies; generator and tests | 72 checks pass. Actual entry/battle/exit remains unplayed. |
| Parades, #39 | Super 100601 and Mega 100614 waves upgraded; generator/test | 18 checks pass. Tier/HP evidence is documented by Claude; exact wave layout, 999999 DEF and capture rate include existing/authored policy. Mega EXP stays 5000 to match this MST despite cited wiki 7000. Battle pacing/minimum damage must be checked. |
| Text/data cleanup | Mission names 20002/20003/20022 and Natalamé in 8301143 corrected; full text/summon asset audits added | 8 text + 273 summon checks pass. No new header-layout or flash-animation fix is established by these tests. |
| Trials/boss inventory | New inventory scripts; findings under out/claude_session2_2026-09-29 | Investigation only. Boss inventory reports 10 placeholder bosses and one mixed implementation; do not call #12/#18 complete. |
| Schemas/tooling | 11 KDL files, packet mappings, mission validator, fixture helpers, generators and debug clock | Build/startup/handler audit pass. Parent patch alone omits submodule modifications and untracked source; preserve both. |

## Tests actually run

Build: the inspected `build_isolated.ps1 -Jobs 4` completed successfully and
confirmed the existing isolated Debug target up to date. This was **not a clean
build from an empty dependency tree**. Executable:
`out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe`.

All logs below are under `out/qc_2026-09-30/`.

| Suite / check | Result | Log |
|---|---|---|
| warehouse_placeholder | 35 pass | warehouse_placeholder.log |
| warehouse_integration | 18 pass | warehouse_integration.log |
| mission_settlement | 22 pass | mission_settlement.log |
| enhancement_sp | 36 pass | enhancement_sp.log |
| leader_exp_boost | 21 pass | leader_exp_boost.log |
| brave_slots | 247 pass | brave_slots.log |
| metal_parades | 18 pass | metal_parades.log |
| story_gates | 32 pass | story_gates.log |
| summon_reveal | 273 pass | summon_reveal.log |
| battle_simulator | 72 pass | battle_simulator.log |
| bugfix_qc | 41 pass | bugfix_qc.log |
| evolution_qc | 23 pass | evolution_qc.log |
| lizeria_qc | 23 pass | lizeria_qc.log |
| sphere_qc | 35 pass | sphere_qc.log |
| Independent mission edges | **2 failures** | mission_edges.log |
| Existing regression runner | All six suites pass | run_bugfix_regressions.py.log |
| Unit text data | 8 pass | test_unit_text_data.py.log |
| Research Lab AI | Exit 0 | test_research_lab_ai.py.log |
| Fresh startup/migrations/HTTP/console | Pass | test_debug_startup.py.log |
| Handler audit | 152 registrations, 0 structural errors | audit_handlers.py.log |
| Mission validator | **Exit 1; 3,117 findings** | validate_missions.py.log / missions.json |
| Parent/submodule whitespace checks | No diff errors | diff_check.log / generator_diff_check.log |

There are **896 passing assertions across the 14 feature suites**, plus eight
text assertions. Do not combine that number with the independent failures to
claim a clean QC result. Validator item lists are identical to the saved
September 29 `qc_missions.json`, not merely equal in count. This is a comparison
against that QC checkpoint, not a fresh validator run on mine/main.

The exact sequential runner is `out/qc_2026-09-30/run_qc.py`; it invokes each
`scripts/test_<name>_wire.py` with the executable above through runpy and redirects
Fixture.create to new copied-save directories, preserving previous evidence.
`results.json` stores exits/timings. Existing suites use the runner's
`--out-base out/qc_2026-09-30/regressions`. The independent test is invoked
directly with the same executable. The runpy wrapper does not change handler
or test assertions. Source hashes were unchanged when checked after the run.

## Performance probe and remaining risk

A disposable fixture was given 100, 1,000 and 5,000 copies of sphere 30000, then
four UserInfo calls were timed per size. Debug build, localhost, Python decrypt/
JSON processing included, other QC possibly running. This is a **diagnostic
scaling probe**, not a mine/main benchmark or client FPS measurement.

| Sphere stock | First call (ms) | Subsequent calls (ms) | Decoded JSON bytes |
|---|---:|---|---:|
| 100 | 70.8 | 63.2 / 61.9 / 64.2 | 280,111 |
| 1,000 | 215.4 | 163.4 / 155.1 / 156.1 | 363,822 |
| 5,000 | 711.5 | 498.6 / 490.0 / 511.6 | 739,822 |

5,000 is a synthetic stress case, not a claimed supported capacity. Warehouse
reconciliation scans historical rows and MST entries, keeps tombstones, and
emits separate sphere rows. Mission run history also grows without pruning.
Profile realistic maximum inventory, long-lived saves and repeated reward
screens before deciding whether indexing, caching or retention changes are
needed. Do not recycle IDs or delete resume history just to improve timing.
Probe source/output: `out/qc_2026-09-30/probe_edges.py` and `edge_probe.log`.

## Required next steps

1. Repair the two reproduced mission cases with atomic current-run validation.
2. Finish provisional-inventory identity/lock and interrupted-response coverage.
3. Play the Windows acceptance cases in
   [the client checklist](CLIENT_DIFF_CHECKLIST_2026-09-30.md), recording actual
   visuals/performance instead of inferring them from data tests.
4. Complete remaining content/investigations (#12/#15/#17/#18/#20, V1 and
   portrait identity), and distinguish authored policies from verified game rules.
5. Produce a proper implementation handoff after the next session; keep the
   entire patch unreleased until blockers and client acceptance are resolved.
