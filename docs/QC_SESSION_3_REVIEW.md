# Independent QC of Session 3 and unfinished FE work

Reviewed in the September 30, 2026 continuation. **Not ready for release.**
The current source builds, the original mission defects are repaired in the
tested cases, but SP purchase/reset is unfinished and a delayed Continue retry
still mutates state incorrectly. No client playthrough was performed.

This review leaves product code unchanged. Two acceptance scripts and review
documents were added. All runtime requests used copied saves, isolated ports
and explicit configs. Nothing was deployed, committed or pushed; the live
server/save/client cache was not modified.

## Main results and findings

### Original mission blockers: fixed in tested cases

The unchanged `test_mission_run_edges_wire.py` now passes both checks. Legacy
MissionEnd is limited to an open pre-serial record; a stale Continue from a
superseded run no longer spends gems or replaces current resume state. The
Continue handler now validates and writes inside one transaction. Existing
tests were updated to echo actual issued serials rather than relying on the
old mission-ID loophole; this is consistent with the intended protocol.

### P1: FE purchase remains an unsupported request

`FeSkillGet` has generated request/response definitions but no handler or
registration. Independent request for owned Vargas 10017, enhancement 510000
(category 1, cost 10) returns `Unsupported request: nSQxNOeL` with error command
4. SP stays 100/0, no skill is acquired and no roster refresh is sent. Command
4 is the existing close path; actual client exit was not played in this review.

Implement ownership/species/tree membership, eligibility, prerequisites,
already-owned/retry handling, trusted cost and SP-limit checks in one
transaction, then return the complete roster. The helper alone does not
validate positive IDs, uniqueness, tree membership or SP accounting.

### P1: type-9 reset acknowledges without doing anything

`ShopUse.cpp` has no type-9 branch. With a seeded acquired skill `1@510000`,
90 unspent/10 spent SP and 10 gems, the request returns `{}` and all state
remains unchanged. No updated roster or gem count is returned. Implement
atomic refund/clear/payment and full roster + team refresh, preserving the
other ShopUse types' omission of optional roster replacement.

The new `scripts/test_fe_skill_qc_wire.py` has **2 passes / 4 failures**:
purchase state and refresh fail; reset state and refresh fail. The two passes
confirm UserInfo serializes the new acquired-skill column and its seeded value
survives restart. These are integration successes, not a working FE feature.
The test uses an independently seeded skill for reset, avoiding a vacuous test
when purchase is missing. Its first development run used the wrong fixture
column (`lv`); corrected to the existing `unit_lvl` before the reported run.

### P2: nonconsecutive Continue retry charges again and rewinds resume

The replay check in `MissionBreak.cpp` compares against only the latest resume
blob. Independent sequence in the same open battle:

1. Revival A: 10 → 9 gems, blob A saved.
2. Genuine later revival B: 9 → 8 gems, blob B saved.
3. Delayed retry A: **8 → 7 gems and blob A replaces B**.

`scripts/test_continue_delayed_retry_wire.py` reports **1 pass / 1 failure**.
Immediate duplicate detection works in the supplied ownership suite; it does
not establish protection against this out-of-order retry. A real delayed
network request was not captured; this is a reproduced server sequence under
the retry semantics the new code claims. Track processed revival identity per
run durably, or establish another consumer-supported mechanism, while allowing
legitimate multiple revivals. Preserve transaction and ownership protections.

## What the unfinished additions already affect

The statement that none of the new pieces are used until the handler exists
is inaccurate. `ServerCache.cpp` already loads `fe_skill_mst.json` at startup;
`30092026_UnitFeSkillInfo` already adds a database column; the packet mapping
already sends it with owned-unit data. Only purchase/reset behavior and the
new FeSkills helper's use are absent.

`gen_fe_skill_mst.py --check` passes: **2,899 skill rows**, **5,124 tree nodes**,
tree identical to served client data, no unresolved skill references. This
checks local table fidelity, not every gameplay rule. The new JSON, generator,
helper, migration and KDL changes must travel together in a reproducible patch.
The generator depends on locally served encrypted MST assets; do not assume
those ignored assets exist in a fresh checkout without documenting prerequisites.

`FeSkills.hpp` has no production caller yet. A separate MSVC C++20 smoke test
compiled and passed empty/zero parsing, grouped round trip, membership, append,
numeric rejection and a large valid ID. Source/command/log are
`out/qc_session3/fe_helper.cpp`, `check_fe_helper.ps1` and `fe_helper.log`.
Malformed/duplicate input and business-rule handling still need fuller tests
when integrated; this does not exercise purchasing.

## Session 3 changes beyond enhancements

| Area | Review status and client limits |
|---|---|
| Mission settlement/Continue | Original two failures fixed; ownership/retry/rollback tests rerun. New delayed-retry defect above remains. |
| Placeholder favorites | Multiple duplicate keys now protect multiple unsent copies, matching documented Windows request/list behavior. Lock intent for only the second or later provisional copy still cannot be distinguished before real-ID refresh. |
| Lost inventory replies/sell retries | Consumer-model tests cover safe refusal after lost list. Selling a provisional ID twice after a lost sale reply remains ambiguous; do not claim universal exactly-once inventory mutations. |
| Warehouse performance | Partial live-row index and queries skip tombstones while retaining history/dictionary/seen mark. Functional suites rerun. Claude's timings in the handoff are retained as Claude's measurements; this review did not rerun that performance benchmark or measure client FPS. |
| Locked story entry | ReturnToGame (6) replaces Close (4) for locked MissionStart. Wire checks cover command; actual notice-to-Home behavior is still unplayed. |
| Unit archive | 114 added Global units (2,145 → 2,259 unique rows versus mine/main), including Light/Dark Deemo forms. Roster grant/evolution/restart tests rerun. Missing art is still possible; 850977 is a specifically documented new gap. |
| Reproducibility | The unit archive generator edit is in ignored `tools/gen_unit_archive.py`; the handoff includes a fragment, but a normal tracked patch will not carry that generator change. Make it reviewable/reproducible before packaging. |
| Stale documentation | Session 3 handoff predates FE groundwork: its 37-path/11-KDL inventory and “no stored skill column” statements no longer describe the current source. KDL ShopUse docs already say type 9 implemented although the handler is absent. Update after implementation. |

## Scope and source inventory

The combined parent working tree still sits on the prior baseline
`7851dba8ebcc97b4d4793aae358b3d18b47a6ad2`. This pass primarily compares with the
previous independent QC SHA-256 inventory rather than inventing a new commit
boundary between agents. Full current inventory and hashes:
`out/qc_session3/inventory.json`. New report files and the additional delayed
retry test were created after that snapshot.

Changed since prior hashed QC: MigrationManager, PacketInterfaceSchemas,
ItemFavorite, Mission, MissionBreak, Warehouse, four existing QC scripts
(core/story/integration/placeholder), and KDL unit_ext/handlers/user.
Newly differing paths versus that older inventory include unit archive,
ServerCache.cpp/.hpp/Mst.hpp, controller handler registration/error handling,
Handlers.hpp, research-lab test, FE JSON/helper/generator, and new ownership,
consumer-model, reward-lock and roster tests. “New versus inventory” is not
synonymous with a newly created file or proof of Claude authorship.

## Reproduction

Isolated build command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File out/bugfix_2026-09-29/build_isolated.ps1 -Jobs 4
python scripts/gen_fe_skill_mst.py --check
python scripts/test_fe_skill_qc_wire.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe
python scripts/test_continue_delayed_retry_wire.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe
```

Packet generation and all 207 reported build steps completed. This was an
incremental Debug build using existing dependencies, not a clean/Release build.
The 19-suite sequential wrapper is `out/qc_session3/run_qc.py`; it waits for
build success, redirects Fixture.create into new isolated directories and
records exits/timings in `results.json`. It preserves prior review logs. It also
runs all six existing regression suites, text/AI/startup/handler/mission checks.
The two new acceptance scripts run separately, so their failures are not hidden
inside that baseline runner's results. Logs are under `out/qc_session3/`.

## Next directive and client acceptance

Use `C:/Users/Evan/Downloads/BF Claude Code Session 4.md` for the next Claude
session. Purchase/reset and the delayed Continue retry are first priorities.
The focused client list is [CLIENT_SESSION_4_CHECKLIST.md](CLIENT_SESSION_4_CHECKLIST.md),
with a link to the full combined-diff checklist. No visual or performance case
should be marked passed from these server tests alone.

## Final test ledger

| Suite | Result | Evidence under out/qc_session3 |
|---|---|---|
| test_mission_run_edges_wire.py | 2 passed / 0 failed | mission_run_edges.log |
| test_mission_run_ownership_wire.py | 57 passed / 0 failed | mission_run_ownership.log |
| test_warehouse_consumer_model_wire.py | 22 passed / 0 failed | warehouse_consumer_model.log |
| test_reward_lock_snapshots_wire.py | 26 passed / 0 failed | reward_lock_snapshots.log |
| test_unit_roster_wire.py | 18 passed / 0 failed | unit_roster_utf8.log |
| test_warehouse_placeholder_wire.py | 39 passed / 0 failed | warehouse_placeholder.log |
| test_warehouse_integration_wire.py | 18 passed / 0 failed | warehouse_integration.log |
| test_mission_settlement_wire.py | 22 passed / 0 failed | mission_settlement.log |
| test_enhancement_sp_wire.py | 36 passed / 0 failed | enhancement_sp.log |
| test_leader_exp_boost_wire.py | 21 passed / 0 failed | leader_exp_boost.log |
| test_brave_slots_wire.py | 249 passed / 0 failed | brave_slots.log |
| test_metal_parades_wire.py | 18 passed / 0 failed | metal_parades.log |
| test_story_gates_wire.py | 35 passed / 0 failed | story_gates.log |
| test_summon_reveal_wire.py | 273 passed / 0 failed | summon_reveal.log |
| test_battle_simulator_wire.py | 72 passed / 0 failed | battle_simulator.log |
| test_bugfix_qc_wire.py | 41 passed / 0 failed | bugfix_qc.log |
| test_evolution_qc_wire.py | 23 passed / 0 failed | evolution_qc.log |
| test_lizeria_qc_wire.py | 23 passed / 0 failed | lizeria_qc.log |
| test_sphere_qc_wire.py | 35 passed / 0 failed | sphere_qc.log |
| run_bugfix_regressions.py | Pass (exit 0) | run_bugfix_regressions.py.log |
| test_unit_text_data.py | 8 passed / 0 failed | test_unit_text_data.py.log |
| test_research_lab_ai.py | Pass (exit 0) | test_research_lab_ai.py.log |
| test_debug_startup.py | Pass (exit 0) | test_debug_startup.py.log |
| audit_handlers.py | Pass (exit 0) | audit_handlers.py.log |
| validate_missions.py | Not clean: 3,117 findings (exit 1) | validate_missions.py.log |
| New FE acceptance smoke | **2 passed / 4 failed** | fe_smoke.log |
| New delayed Continue retry | **1 passed / 1 failed** | continue_delayed.log |
| FE table fidelity | Pass, 2,899 skills / 5,124 nodes | fe_table.log |
| Standalone FE helper C++20 smoke | Pass | fe_helper.log |
| Debug build and packet generation | Pass | build.log |
| Parent and submodule diff whitespace | Pass | diff_check.log / submodule_check.log |

The roster suite initially exited on a Windows cp1252 printing error for the star glyph, before wire coverage. The unchanged test passed with `PYTHONIOENCODING=utf-8`; both logs are retained. This was a harness/output issue, not a product test pass on the initial run.

Numeric pass summaries total 1038 across the baseline logs that report counts (excluding the two new acceptance scripts). Counts are not a release approval; the five failed new assertions represent the two missing FE operations and one retry defect.

Baseline runner process completed; no tests were left running. Production source hashes remained unchanged during this QC.

Mission-validator item lists are identical to the prior independent QC snapshot.
