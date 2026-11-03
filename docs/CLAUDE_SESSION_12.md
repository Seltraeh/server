# Claude Session 12 — repair fixtures and validate client test targets

Work in `C:\Users\Evan\BF\BF-WorkingDirRust`; proxy checkout is
`C:\Users\Evan\BF\offline-proxy`. This directive supports a fresh session.
Implement and test the bounded work below, then leave a reviewable handoff.

## Read first

1. Repository instructions and `docs/BF_OFFLINE_SERVER_HANDBOOK.md`.
2. `docs/SESSION_11_CLIENT_RESULTS.md` — latest player evidence takes precedence
   over earlier predicted visual results.
3. `docs/QC_SESSION_11_REVIEW.md` and `docs/CLAUDE_SESSION_11_HANDOFF.md`.
4. `docs/CLIENT_SESSION_11_STEPS.md`, `docs/OFFLINE_CONTENT_PHASE_C2.md` and
   the relevant proxy contract/source before changing content handling.

## Current state and constraints

The checkout contains earlier uncommitted server and submodule work. Inspect
status/diffs first, preserve it, and keep changes in the current checkouts.
Do not deploy, commit, push, reset clients, overwrite player saves or stage out/.
Follow the handbook's existing Visual Studio/build workflow; use one bounded
QA location under out/qa/current, not another build tree for every attempt.
Promote reusable tests/tools into scripts or the established proxy test suite;
do not leave required implementation only in disposable out/ scripts.

Session 11 server candidate: out/qa/current/session10/bin/gimuserverw.exe,
SHA-256 starts 270832d6. C1 isolated DLL starts 523c77ed; baseline test DLL
01786e64. These are historical evidence, not proof a newly changed source is
built. Build affected targets and record exact hashes of new candidates.
Test copies only; use isolated ports and stop only processes this session owns.

Player-confirmed: C1 Yes/No prompt paths and client/harness restarts; Vargas
art; crafting fifteen kit outputs with separate sphere rows; prior sphere
equip/favorite/sale persistence and post-mission map return. Do not repeat
these without a change that warrants it. Run D was fully collected: processes
7/8 in content_client/offlinemod_content.log show provider on then off using
the same perf save. On: 124 fetches, zero failures/cache-write tripwires. Off:
no downloads reported. Routes differ at login/download scenes, so this is not
a controlled speed verdict. No more player collection is currently needed.

QC already fixed await_broker false success on failed index requests and
selective restore failing on unrelated parked assets. Preserve those fixes and
scripts/test_content_disposable_commands.py.

## Priority 1: repair small-roster generation and clearunits

Run C crashes on Home unit panes and Manage Squad. Confirmed database defect:
only owned unit 1000 (unit type 20011) remains, while deck 0 references deleted
1034 and 1113. DoClearUnits in standalone_frontend/DebugCli.cpp deletes units
without repairing dependent references. Login success did not validate the
fixture. Do not attribute the crash to missing sprites without evidence.

Inspect owned-unit dependencies, active deck, leaders, campaign decks and
equipment. Implement transactional deletion/cleanup with a valid remaining
squad according to the actual client contract. Preserve unrelated state and
do not turn this into a broad deletion refactor. Add regression coverage for
dangling references, valid active squad/leader, repeated clear, and relevant
empty/multiple-user boundaries. Exercise the actual command and inspect the
resulting UserInfo/deck payload, not just the HTTP success status.

Prepare a separately named repaired fixture and harness save. Preserve
session11/fixtures/small_roster_kit.sqlite and content_client_c1_yes/saves/small
as evidence. --seed does not replace an existing named save. Verify fixture
integrity and emitted unit references before requesting a client retest.

## Priority 2: replace the invalid thumbnail test with an evidenced target

Archive No. 8295 is Galea; No. 8318 is Blaze. Swapping
unit_ills_thum_810048_100.png showed no visible change. Parking it caused a
1/1 download but the visible Blaze thumbnail stayed unchanged. This is not
visual acceptance. Trace the exact image requested by the visible tile and
its texture caching/invalidation path. Establish _100/_2 semantics from actual
consumers; neither filename assumptions nor a download request establish the
displayed image. Distinguish alternate-art selection from acquiring a unit.

Select a verified visible asset for disposable swap/park/restore, or correct
the provider invalidation bug if proven. Test replacement bytes and scene
adoption automatically. Keep all mutation confined to the disposable tree;
never modify real game_content. Describe unavailable-asset behavior as pending
client observation until demonstrated. If deeper reversal blocks progress,
document exact addresses, evidence and next investigation rather than inventing
an expected visual response.

## Priority 3: validate the leader comparison and fixture contents

Player sees High Ruler's Chakra in both runs. Berdette owned ID 1239/type 10824
has stored leader_skill_id 0 in c2_kit and 7401 in leader_after. Trace helper
selection, response construction, skill text and combat application. Determine
whether MST fallback, helper data, or another source makes the expected text
difference invalid. Do not force a display change merely to satisfy the old
test. Provide a deterministic before/after combat-stat check with identical
party, helper and equipment where a real behavioral difference should exist.

Touka was absent from this crafting save's gifts. If its preview remains
necessary, explicitly include it in a bounded fixture and verify the manifest
and capacity. Do not assume gifts transfer between independently seeded saves.

## Deferred scope

Do not disable DLC prompts, download/verification screens, or expand content
ownership in this pass. Family-scale update publication remains slow (QC
18–21 seconds; earlier 23–35). Record it for the following pass. Any later
optimization must retain same-size edit detection and atomic bundle behavior.

## Validation and deliverables

Run targeted regressions for changed systems, the affected build, harness
tests and applicable existing suites. Expand to the required handbook checks.
If proxy/provider code changes, rebuild its tests and candidate and rerun the
relevant live integration. Record skips/failures and exact candidate paths.
Never reuse the prior binary's success as evidence for new source.

Write docs/CLAUDE_SESSION_12_HANDOFF.md: changed files, root causes, test
commands/results, build hashes, remaining risks and unfinished work.
Write docs/CLIENT_SESSION_12_STEPS.md with only necessary visual retests:
explicit package name, full commands, verified fixture contents, expected
observations, log collection and recovery instructions. Prepare fixtures
before asking the player to test. No abbreviated "same command plus flag".
Mark automated evidence separately from player acceptance. Update the results
record without erasing previous failures. Stop at a reviewable candidate;
Codex will QC before the next player run.
