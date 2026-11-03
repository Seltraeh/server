# Claude Session 14 — double-gate whiteout and Farm harvest

Work in C:\Users\Evan\BF\BF-WorkingDirRust and the existing sibling
C:\Users\Evan\BF\offline-proxy checkout. Implement and test the priorities
below, then leave a candidate for Codex QC before further player testing.

## Read first

Read repository instructions and docs/BF_OFFLINE_SERVER_HANDBOOK.md, followed
by SESSION_13_CLIENT_RESULTS.md, QC_SESSION_13_REVIEW.md,
CLAUDE_SESSION_13_HANDOFF.md, BUG_BACKLOG_STATUS.md,
OFFLINE_CONTENT_PHASE_C3.md and OFFLINE_STARTUP_TRANSITION_PLAN.md.
Latest player results supersede earlier predictions and automated acceptance.

## Current evidence

Session 13 reference server: out/qa/current/session13/bin/gimuserverw.exe,
SHA-256 90e3eff97739b0d5d5713cae983db9fc4cadaa7e105b8bdf009a81b1444b5ebc.
C1 DLL: 523c77ed... (unchanged). C3 policy: session13/family_policy_c3.json,
53,766 owned names, SHA-256 starts 5b9c5e52. Test identity is [ContentTest C1],
port 29960; named player test save is c3_effects. Inspect current state before
acting: these hashes identify the reviewed reference, not a new build.

Player-confirmed this pass: Home with Selena; Sidonie animation without a
download; party/helper and Brave Bursts in a quest; mission-end screens; Town
building animation. Brief legacy downloads at Summon and quest entry remain
expected. A single gate works. A DOUBLE gate whites out the whole screen after
the first gate. This reproduces #24 and blocks closing it.

Mountain, forest and river showed harvest drops; Farm did not. Farm upgrade
did not replenish sparkle. Reward loss is NOT established. Town.hpp currently
uses a three-hour cycle, and upgrading does not promise an immediate refill.
Do not implement a free refill solely to satisfy that expectation.

Logs were collected into out/qa/current/content_client_c1, including the
collected/20261008-114743 archive. Server log and save are under
content_client_c1/saves/c3_effects. Last reviewed counters: 906 provider fetches,
zero failures, zero tripwire writes, 4,082 suppressed downloads. Harvest report:
1:3,2:8,3:6,4:10. These do not prove correct scripts or reward credits. Verify
the actual process/session before using a log; preserve the collected evidence.

## Priority 1 — reproduce and fix #24

1. Trace the exact gWait/gOpen and related scripts actually read by this test
   package, their source (package, LocalState, server) and hashes. They remain
   legacy even though the gate animations are C3-owned. Check the prior patch
   against the loaded bytes; do not assume the cache is stale or reset it.
2. Trace double-gate input, breakthrough transitions, white-overlay creation
   and removal, animation completion and script state. Compare single/double
   and normal/rapid taps. Establish whether this predates C3 using identical
   assets and a copied fixture with the prior policy. Do not label it a C3
   regression merely because it was observed during this test.
3. Prepare a deterministic double-gate reproduction on a disposable save so
   the player does not spend time summoning until it happens. Keep control
   changes in test fixtures/tooling; do not alter production rates to test it.
4. Fix the proven cause at the appropriate script/client layer. If delivery
   of corrected scripts is required, trace their complete dependencies and
   implement a safe update/local-source path. Do not change real installed
   caches or deployed game_content during candidate preparation. Do not just
   hide white effects globally or bypass gate transitions.
5. Add a regression that fails against the faulty behavior and exercises the
   transition/overlay lifecycle. Existing script-shape assertions passed while
   the client still failed: matching expected text alone is insufficient.
   Distinguish modeled/fake-game checks from actual client acceptance.

## Priority 2 — diagnose Farm before changing rewards

Read the copied save, pre-rolled Town payloads, harvest log and response path.
Identify the actual Farm location ID from master data. Determine whether the
reported taps produced items, Zel, Karma or empty results, and which effects
the client should draw for each. Without a pre-claim balance, do not infer a
historical credit delta from the final save alone.

Reproduce on a new bounded fixture with known drops and recorded balances.
Compare Farm with the other plots; check per-tap indexing, exhausted taps,
period refresh, upgrade persistence and server/client reward agreement. Fix
any demonstrated credit or rendering defect. If exhausted harvests or a
non-item reward explains the observation, document that with evidence rather
than changing mechanics. Test refresh with controlled time on copies, not by
making the player wait three hours. Preserve duplicate/retry protection and
transactional behavior; do not grant rewards twice to make effects appear.

## Priority 3 — continue local-content work if the above is resolved

Advance D1 from the startup plan: a coherent remaining UI/script family whose
read/dependency contracts can be proven, prioritizing any family needed by the
gate fix. If a bounded implementation is not possible, leave exact evidence
and the next code changes; do not claim the download-screen goal is complete.
Do not own audio until its separate reader is supported. Preserve dictionary/
MST initialization while planning removal of redundant FILE_CHECK work. Avoid
a broad startup bypass or a whole-library RAM preload.

## Working rules

- Preserve existing uncommitted server, packet-generator and proxy work.
  Keep edits in current checkouts. No deployment, commit, push, client reset,
  live-save changes or overwriting the player's c3_effects save.
- Reuse the established MSVC/build tree. Keep new QA outputs under one bounded
  out/qa/current/session14 location plus established suite folders; restore
  temporary CMake output overrides. Durable tools/tests belong in scripts or
  the existing proxy suite, not only in ignored out/ or tools/ files.
- Use new named saves for new seeds; --seed never replaces an existing save.
  Keep test ports isolated and stop only processes started by the test.
- Do not repeat passed C1 prompt/persistence, spheres, Vargas or crafting tests
  unless a change touches them. Session 12 Archive replacement remains pending;
  it does not block independent work. Normal squad play now has positive player
  evidence, but the exact empty-pane crash checks were not explicitly repeated.
- Do not move #24 to Fixed based on automation. Do not conflate zero provider
  failures with gameplay correctness. Keep the Word issue list unchanged;
  update the Markdown evidence/status and note defensible moves separately.

## Deliverables and validation

Build changed targets and record source/candidate provenance and exact hashes.
Run targeted regressions, related existing tests and required handbook checks;
rerun real broker/proxy integrations if policy or hooks change. Include negative
controls where feasible. Verify live save and real source content were not
modified by tests. If a defect cannot be fixed in this pass, state the blocker
precisely and preserve a useful reproduction rather than calling it complete.

Write docs/CLAUDE_SESSION_14_HANDOFF.md: root causes, file changes, tests with
commands/results, artifacts/hashes, limitations and remaining work. Update
BUG_BACKLOG_STATUS.md to record the double-gate reproduction and Farm report.
Write docs/CLIENT_SESSION_14_STEPS.md only for new behavior: full commands,
exact package and fixture, deterministic double-gate path, Farm balances/drop
expectations, recovery and collection. Prepare and validate the fixtures first.
Codex reviews the candidate before the player is asked to run those steps.
