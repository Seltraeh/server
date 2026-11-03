# Claude Session 15 — cold-content dependencies and local audio

## October 9 priority override — finish #24 before audio

Read SESSION_14_CLIENT_RESULTS.md first. Candidate B removes premature white
for Omni but still lacks the required final reveal flash for Omni and 7-star.
Current-script A also lacks final flash for 7-star. B 5-star and flat-energy
control C Omni behave correctly. #24 is NOT accepted. Farm visuals passed;
do not repeat those tests or change harvest mechanics.

Prioritize fixing the complete transition: white must not cover an unopened
gate, and must occur after the final gate opens before reveal. The prior model's
"preserve flash at the same instant as now" criterion preserved faulty absence.
Trace the final Open/reveal lifecycle and make the transition depend on that
state, not global animation count. A narrowly validated client hook is acceptable
if the script layer cannot robustly express it; do not prefer more list padding
merely to avoid a DLL change. Preserve input, reveal and animation cleanup.
Add positive final-flash assertions as well as negative early-flash assertions,
with single/double, overflow/flat energy, normal/rapid taps and unrelated
animations. Preserve functioning 5-star behavior. Actual-client confirmation
still follows Codex QC. Audio and other priorities below follow this fix.

Work in C:\Users\Evan\BF\BF-WorkingDirRust and its existing sibling
offline-proxy checkout. Read applicable repository instructions and
docs/BF_OFFLINE_SERVER_HANDBOOK.md, QC_SESSION_14_REVIEW.md,
CLAUDE_SESSION_14_HANDOFF.md, OFFLINE_CONTENT_PHASE_C4.md,
OFFLINE_STARTUP_TRANSITION_PLAN.md and BUG_BACKLOG_STATUS.md.

## Current state

Session 14 proposes an off-screen Touch.sam guard in nine Wait/Change scripts
to prevent premature double-gate whiteout. Model and hook delivery tests pass;
the exact in-client extra-animation state and visual fix remain unconfirmed.
Do not mark #24 fixed. Incorporate any new player results before modifying it.
Farm's six empty taps and actual credits were accounted for; do not change
harvest rates or add upgrade refills to solve an unproven reward defect.

Server reference: session13/bin/gimuserverw.exe, 90e3eff9...; C1 DLL 523c77ed...;
C4 policy session14/family_policy_c4.json, 8bdcd08e..., candidate root under
session14/c4_content/content. No Session 14 C++ changed. These are reference
artifacts, not proof of a new build. Preserve Codex's harvest-checker guards,
their regression tests and the corrected unavailable-bundle documentation.

## Priority 1 — complete dependencies when scripts stop downloading

Owning a gate script skips the legacy downloader's dependency discovery. Three
unpackaged sounds named by gate scripts can therefore be silent on an uncached
client, although the existing test package already has them. This prevents
claiming cold-install support. Trace the actual script dependency processor and
all sound readers/lifetime rules in Windows, supported by arm64 semantics.

Implement a bounded local-audio path if the read contract can be established:
read from game_content without populating the permanent client asset cache.
Handle streaming/seek/reopen, byte identity, missing files and appropriate
thread/lifetime behavior. Preserve ordinary music/SFX selection and settings.
Do not add mp3 to provider ownership while its actual reader still bypasses
the provider. Do not call local-file support complete if only the tests' fake
getFileData path works.

If local audio needs deeper work than fits this pass, implement an explicit
dependency solution for owned scripts that retains the required legacy sound
availability, label it transitional (still caches audio), and document the
exact remaining hook work. Do not silently remove sounds or claim this achieves
the cache-less goal. Do not add an extra deployment or user confirmation flow.

Tests must begin with script-named sounds absent from a DISPOSABLE fixture and
exercise the real reader/queue contract, plus warm repeat, missing sound and
provider unavailable cases. Preserve the user's existing client caches.

## Priority 2 — coherent remaining UI content

Use content_inventory.py to choose a coherent set of remaining scene layouts,
art or fonts needed by login/Home/Summon. Trace dependency consumers and
conflicting packaged copies. Extend generated ownership only when read paths
and complete dependencies are proven; avoid a broad wildcard claim. Reuse
real broker/hook tests for bytes, text mode, bundle completeness and recovery.
Do not spend the session expanding counts without reducing a known legacy
download path. Audio correctness above takes precedence over more coverage.

## Priority 3 — advance the verification-screen plan

The user still wants login-to-Home verification removed. Keep required MST/text
initialization while identifying redundant checks and a guarded local-ready
decision. Resolve the remaining x86 transitional/downloader routines as needed.
Implement a bounded guarded path only if initialization, missing-content and
failure behavior are demonstrated; otherwise leave exact patch points and
prerequisites, not a generic research summary. Never hide a screen and force
success while required work remains. Local availability at startup does not
mean decoding every asset into RAM.

## Scope and acceptance

- Preserve dirty work in both current checkouts and packet-generator. No
  deployment, commit, push, live-save mutation or resetting installed clients.
- Reuse the established build tree and one bounded session15 QA location.
  Record actual binary hashes and restore temporary CMake output overrides.
  Durable implementation/tests belong in tracked-source locations.
- Session 14 player A/B results, if available, determine whether #24 needs
  further correction. If still white, prioritize its demonstrated failure
  before expanding coverage; do not add more animation-count padding blindly.
- Do not repeat accepted C1 prompt/persistence, spheres, Vargas, crafting,
  Sidonie or general Town animation tests without a relevant code change.
- Update backlog evidence precisely. Farm explanation is not proof of every
  Town mechanic; zero read failures is not proof of animation/script behavior.

## Deliverables

Build affected targets; run relevant existing suites, new negative controls
and required handbook checks. Report skipped/unrun work explicitly. Prepare
new named fixture saves and any candidate package before asking for visuals;
--seed does not overwrite an existing named save.

Write CLAUDE_SESSION_15_HANDOFF.md with changed files, consumer evidence,
tests/results, exact candidate paths/hashes and unresolved constraints. Write
CLIENT_SESSION_15_STEPS.md with full commands, audio settings/expected sounds,
short deterministic route, per-part collection and recovery. Keep client
acceptance separate from modeled results. Stop at a candidate for Codex QC.
