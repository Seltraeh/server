# September 29 bug fixes and independent QC

## Decision

**Do not release the combined patch yet.** The isolated build and the tested
server behaviors pass, but #34 still needs immediate-crafting client compatibility
work and client acceptance. Its two original inventory-identity failures are repaired.
No Windows-client playthrough was performed during this QC pass. Server tests
therefore do not establish that the reported client crashes, cutscenes, animations,
and screens are fixed.

The user asked for QC of Claude's interrupted work and a few additional fixes.
This pass reviewed the existing changes, added isolated regression coverage,
repaired evolution eligibility and energy recovery, and implemented the bounded
Lizeria prerequisite fix. It did not attempt the entire original content backlog.

Nothing was committed or pushed. The live server, live configuration, and live
save were not modified by this pass. Builds used `out/build/bugfix-0929`;
tests used explicit isolated configs and SQLite backup copies under `out/`.

## Source state and change ownership

Parent HEAD at start/end: `7851dba8ebcc97b4d4793aae358b3d18b47a6ad2`.
Packet-generator HEAD at start/end: `2efdaa5a297ae0dd6ed5240e2f66ac762b08aa40`.
Both have uncommitted changes. Numerous pre-existing untracked `.bak` files,
the staging directory, and local tools were preserved.

Claude's pre-existing changes include the simulator archive/generator/test,
BattleItems helper and ItemEdit transfers, MissionEnd settlement guard, level
1000 table row, debug clock, scenario marker handling, UnitEvo rewrite, sphere
warehouse splitting, three KDL edits, and the mission validator adjustments.
Claude's working evidence remains in
`out/bugfix_2026-09-29/NOTES_findings.md`.

This QC pass added:

- `Energy.cpp`: preserve the stored-energy remainder when a regeneration tick
  supplies two energy. Previously, spending three at level 999 stored 428 but
  immediately displayed 427; the next tick displayed 429 instead of 430.
  Also prevent unsigned underflow for an inconsistent far-future refill timer.
- `UnitEvo.cpp`: require the base to reach its source-form maximum level and
  reject favorited material units before mutation. The existing rewrite accepted
  both cases. The recipe, payment, material deletion, and failure rollback paths
  remain transactional.
- `PermitPlace.cpp/.hpp` and `Mission.cpp`: require both mission 666 and 20067
  for land 4 (Lizeria), its areas/dungeons/mission tiles, and direct MissionStart.
  Existing clear history is preserved. Other lands' authored prerequisite rules
  are unchanged; a full audit of their compound requirements remains outstanding.
- `bf_testkit.py`: preserve scenario-upload fields while fixing request identity
  to the copied account.
- Four new QC test scripts and an existing-suite runner, listed below.
- `Warehouse.cpp`, `ItemSell.cpp`, `ItemFavorite.cpp`, `ItemSphereEqp.cpp`,
  `Common.hpp`, and migration `29092026_PersistentWarehouseRows`: persistent
  per-copy warehouse rows, independent favorites, exact-ID sales, atomic sphere
  equipment transfers, and preservation of equipped copy identity. Aggregate
  `user_items` quantities remain the interface for existing reward/recipe code.

## Outstanding QC findings

### #34 Persistent identities repaired; immediate crafting remains blocked

The earlier suite recorded 2 passes / 2 failures: favorite flags spread to all
copies and real ID 2097123 was incorrectly reduced modulo 1048576. The new
persistent-row implementation removes that alias arithmetic. The expanded suite
now passes **35/35** checks: migration counts/high IDs/old locks, independent
favorites, stable refresh, exact sales, ownership, stale IDs, mixed-cart rollback,
equipment transfer/unequip, locked second slot, injected write failures,
concurrent sale, crafting, regular stacks, and restart persistence.

This does **not** close #34. Local ARM64 disassembly shows
`GameUtils::incWarehouseItem` assigning newly created local rows
`INT_MAX - itemId` (0x118A304–0x118A328). Sell and favorite request builders read
`getItemIndex`. `ItemMix` still returns an empty response. The test refreshes
UserInfo before operating on crafted copies, so it does not exercise a client's
immediate temporary IDs. Exact-row handlers currently cannot resolve those IDs.
Confirm the Windows consumer and implement a tested identity handoff/refresh
contract before approval; do not blindly decode arbitrary high IDs or replace
warehouse objects during a scene that may retain references. Disassembly logs:
`sphere_client_add.txt` and `sphere_request_disasm.txt` under the evidence folder.

Also audit species-level consumers: aggregate favorite flags conservatively
protect a species if any copy is locked, which can block Merit delivery of an
otherwise unlocked duplicate. Capacity, every equipment-return pathway, and
client favorite refresh outside UserInfo need additional coverage.

### Scope limits requiring explicit follow-up

- #19's `open_mission_id` prevents duplicate settlement of a closed battle and
  survives restart, but it is not a unique run token. A delayed result from an
  older run of the same mission cannot be distinguished after another run starts.
  Do not describe this as general exactly-once processing. Legacy NULL rows are
  intentionally accepted once by Claude's migration; this compatibility policy
  remains to be reviewed alongside an actual suspended client battle.
- #19 coverage establishes ordinary owned bars and simulator-supplied items.
  It does not establish every raid/Grand Quest/challenge/bonus-bar lifecycle.
- #23 coverage establishes marker upload, omission in MissionEnd, and persistence.
  It does not prove the Morgan/Farm cutscene selection or timing in-client.
- #40 coverage establishes three representative recipes, skill IDs, item/unit
  separation, eligibility checks, and rollback. Full evolution-chain fidelity,
  all retained stats and collection behavior, and SBB/UBB unlock presentation
  remain to be assessed. The BB-halving policy is inherited from Claude's change.
- Locked Lizeria direct-entry requests use the server's existing error response,
  which closes the session. Normal clients should not have those tiles. Graceful
  handling of a stale client tile needs a consumer-verified refusal contract.
- The restored player-level row and simulator content are source data changes;
  an executable alone is not the complete deployment. No downloadable-client
  cache behavior or portable package was validated by this pass.

## Reproduction and test results

Run from the repository root with Python and pycryptodome available. Each new
wire script creates and stops its own server, refuses an occupied test port,
and uses a fixture below `out/`. Do not point existing wire scripts directly
at a live server/save. `run_bugfix_regressions.py` provisions their servers.

The tested executable is:
`out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe`.

| Test | Final result | Evidence under out/bugfix_2026-09-29 |
|---|---|---|
| Isolated Debug compile/link | Pass, including persistent warehouse migration | sphere_build_2.log |
| test_battle_simulator_wire.py | 71 passed, 0 failed after sphere changes | sphere_simulator.log |
| test_bugfix_qc_wire.py | 41 passed, 0 failed after sphere changes | sphere_core_qc.log |
| test_evolution_qc_wire.py | 23 passed, 0 failed after sphere changes | sphere_evolution_qc.log |
| test_lizeria_qc_wire.py | 23 passed, 0 failed after sphere changes | sphere_lizeria_qc.log |
| test_sphere_qc_wire.py | 35 passed, 0 failed; immediate client crafting not covered | sphere_after.log |
| test_fusion_merit.py | Pass | regression_test_fusion_merit/test.log |
| test_exchange_ui_wire.py | Pass after fixture correction | regression_test_exchange_ui_wire/test.log; qc_exchange_rerun.log |
| test_synthesis_wire.py | Pass | regression_test_synthesis_wire/test.log |
| test_feature_visits_wire.py | Pass | regression_test_feature_visits_wire/test.log |
| test_research_lab_wire.py | Pass | regression_test_research_lab_wire/test.log |
| test_guild_invite_wire.py | Pass | regression_test_guild_invite_wire/test.log |
| test_research_lab_ai.py | 0 failures | qc_ai.log |
| test_debug_startup.py | Fresh migration, HTTP, symbols, console pass after sphere changes | sphere_startup.log |
| audit_handlers.py | 152 registrations, 0 structural errors | qc_handlers.log |
| validate_missions.py | NOT clean: 3,117 findings, unchanged category counts | qc_missions.log/json |
| git diff --check, parent and generator | Pass | Executed during QC |

The mission-validator comparison is against Claude's saved
`validate_after_sim.json`. It establishes no change in category counts, not
that the pre-existing findings are acceptable or that every finding is identical.
The audit's empty-handler candidates are informational output, not a gameplay
completion claim.

The exchange suite's initial failure was its invitation-candidate precondition:
all friends in the copied save were already guild members. The runner now clears
non-founder membership only in that disposable fixture, following the guild
suite's own setup. No product behavior was changed to make the test pass.

All six existing wire suites were rerun after the persistent sphere changes and
passed; the runner summary is `sphere_regressions.log`. These regressions use
fresh copied fixtures and do not certify immediate client-side inventory refresh.

The before-fix Lizeria suite had 14 failures; all 23 checks pass after the gate.
The before-fix energy tests exposed both odd-cost failures. The evolution suite
exposed accepted under-level and favorited-material requests. Initial test
development also corrected fixture item/field mappings; those development logs
are not product defect counts.

Exact principal commands executed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File out/bugfix_2026-09-29/build_isolated.ps1 -Jobs 4
python scripts/test_battle_simulator_wire.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe --port 19971
python scripts/test_bugfix_qc_wire.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe
python scripts/test_evolution_qc_wire.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe
python scripts/test_lizeria_qc_wire.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe
python scripts/test_sphere_qc_wire.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe
python scripts/run_bugfix_regressions.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe
python scripts/run_bugfix_regressions.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe --suite test_exchange_ui_wire.py
python scripts/test_research_lab_ai.py
python scripts/test_debug_startup.py out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe
python scripts/audit_handlers.py
python scripts/validate_missions.py --json out/bugfix_2026-09-29/qc_missions.json
git diff --check
git -C packet-generator diff --check
```

The build helper is Claude's local ignored script, not a portable repository
entry point. It reuses the already-installed vcpkg dependencies and loads this
machine's MSVC environment. For another machine follow `docs/DEVELOPMENT.md` and
configure a separate build directory rather than relying on this local path.

## Issue ledger and client acceptance

| Issue | Current evidence and next step |
|---|---|
| V1 Legend Stone | Merit delivery/payment/stock rollback passes existing tests. Independent Guild Legend Stone presence and actual shop visuals are not established here. |
| #11 simulator | 71 server checks pass for mission 6000000 and dummies 10000000–10000005. In-client: enter, select all elements, fight, exit, repeat; no crash, dummy captures, or rewards. |
| #12 Vortex Trials | Existing Research Lab suites pass; larger content implementation not undertaken. Do not treat three existing trials as complete coverage. |
| #15 EU/special units | Not undertaken. Missing-unit and asset inventory still required. |
| #17 Fire Mecha God | Not undertaken. Needs exact stage, probability/capture evidence, deterministic success/failure tests. |
| #18 large bosses | Existing AI/model and Research Lab wire tests pass. No additional bosses authored or played. |
| #19 item duplication | Ordinary transfer/use/loot/retry/rollback tests pass, simulator does not consume owned items, restart preserves settlement. In-client: equip 5 Cures, use 2, complete, check bar and storage, refill, repeat; separately test supplied modes and suspended/retried missions. Limits above apply. |
| #20 mission 234 | Claude's investigation retained; no crash reproduced or fixed in this pass. In-client: first clear and replay of A Flash of Lightning; inspect Cordelica unlock transition and collect crash/log evidence. |
| #22A Lukroar gate | Not undertaken; assets and actual reveal still need inspection. |
| #22B Omni Amadream header | Not undertaken; actual client layout still needs inspection. |
| #22C Omni Galtier description | Not undertaken; full source/served/cached text comparison still required. |
| #23 cutscene repeats | Upload/omit/reload tests pass. In-client: Tower of Mistral intermediate versus final stage 85, then replay; Farm after mission 11 once, then replay and relog. |
| #24 double summon flash | Not undertaken; requires repeatable single/double-gate visual tests. |
| #25 frog SP eligibility | Not undertaken; verify the material/target eligibility matrix before changing mechanics. |
| #26 leader bonuses | Not undertaken; resolve variants, affected reward types, and client/server ownership. |
| #27 slots | Not undertaken; current prior fixes are not re-certified by this pass. |
| #30 Lizeria | Implemented bounded land-4 gate; 23 truth-table/direct-entry checks pass. In-client verify absent with only 666 or 20067 cleared, present with both. Existing earned history is retained; access requires both. Other areas still need the requested audit. |
| #33 level 999 | Cap-level and 998→999 wire tests pass, sentinel row present, recovery boundary/odd-cost/phase tests pass. In-client verify result screen finishes for natural and edited level-999 saves and displays correct energy. |
| #34 crafted spheres | Persistent-copy server suite passes 35/35. Original favorite/high-ID failures repaired. Immediate craft temporary-ID handoff and visual refresh remain blocked; see above. |
| #39 Mega Metal Parade | Not undertaken; stage composition/stat research still required. |
| #40 evolution bursts | Three recipe families tested (10011, 10015, 850637), correct destination skills, unrelated unit preserved, payment failure/retry rollback. Under-level and favorite-material holes repaired. In-client inspect new form/BB/SBB, applicable UBB eligibility, and retained upgrades. |
| Fixed #8 | Catalog, payment, expiry wire regressions pass. No new shop client check. |
| Fixed #13 | Burst Queen and BB/SBB boundary regression tests pass. |
| Fixed #14 | Source document says Seria; existing notes identify Tilith 50253. Discrepancy unresolved; no new portrait check. |
| Fixed #16 | Fixed/random Mystery Frogs, reset, preserved upgrades, compensation and growth tests pass. |

## Migration and deployment notes

`29092026_PersistentWarehouseRows` adds the sidecar `user_warehouse_rows` and
imports original aggregate IDs/counts/favorites. Snapshot reconciliation splits
stacks according to MST caps and records equipped copies separately. The new
suite exercises existing-save migration and restart. Keep this schema paired
with this executable; backwards deployment/write compatibility is untested.
The first migration startup attempt failed because several SQL statements were
submitted together; these were separated and migration/restart then passed.

The actionable coding backlog and client evidence worksheet are in
`docs/BUGFIX_REMAINING_AND_CLIENT_QA_2026-09-29.md`.

Claude's `29092026_AddUserInfoOpenMission` adds nullable
`user_info.open_mission_id`. QC exercised both migration of copied existing saves
and fresh-save creation, and restarted a migrated save without reapplying the
migration. Closed-run duplicate suppression persists. Migration handling of a
real in-flight client mission has not been played.

The KDL edits remain uncommitted inside `packet-generator`; do not lose them by
updating the submodule checkout. Generated packet headers are ignored and must
be regenerated by the build. Required source data includes
`deploy/archive/mission.json` and `deploy/mst/user_level_mst.json`.
Simulator generation is in `scripts/gen_battle_simulator.py` and uses shipped
monster assets. No new artwork or downloadable MST bundle was produced here.
Keep logs, copied saves, extracted assets, and local build products out of commits.

## Evidence sources

- Local code, KDL schemas, source MST rows, Claude's binary-address notes, and
  actual encrypted requests to the isolated C++ server.
- [Lizeria](https://bravefrontierglobal.fandom.com/wiki/Lizeria), checked
  September 29, 2026: both Cordelica and Palmyna bosses are required. Local
  area/dungeon 700 rows independently list `666,20067`.
- [Unit Skills](https://bravefrontierglobal.fandom.com/wiki/Unit_Skills) and
  [Units](https://bravefrontierglobal.fandom.com/wiki/Units), checked September
  29, 2026, are context for evolution; the test oracle for skill identity is the
  destination MST. Do not interpret these tests as a complete proof of every
  BB/SBB progression rule across regions.
- Claude cites Summoners' Training Ground revision 599590 and Player Level
  revision 638743. Those claims are preserved as Claude's research evidence,
  not newly reproduced binary/client observations from this QC pass.
