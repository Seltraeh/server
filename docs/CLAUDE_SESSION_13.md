# Claude Session 13 — local content coverage and startup transition

Work in C:\Users\Evan\BF\BF-WorkingDirRust and the existing sibling
offline-proxy checkout. Read repository instructions, BF_OFFLINE_SERVER_HANDBOOK.md,
QC_SESSION_12_REVIEW.md, CLAUDE_SESSION_12_HANDOFF.md and
SESSION_11_CLIENT_RESULTS.md first. This is a fresh-session directive.

## Goal and boundaries

Progress toward local game_content supplying every asset and eliminating
redundant download/verification screens. Availability should be established at
startup, with assets decoded on demand; do not preload every texture into RAM.
Keep implementation and test coverage concrete, rather than spending another
session only preparing fixtures. Preserve all existing dirty work. No deploy,
commit, push, live-save edits or client resets. Use established builds, restore
output overrides, and keep outputs in one bounded QA folder under out/qa/current.

Session 12 server candidate e30e2adf and C1 DLL 523c77ed are reference candidates,
not proof of new changes. Codex QC corrected same-second collection collisions.
Preserve that correction. Two client checks await the player: repaired small
roster and visible Archive swap/park/recovery. If new results arrive, incorporate
them. Do not claim they passed or block independent coverage work waiting for
them. Do not repeat C1 prompt branches, persistence, crafting or leader A/B.

## 1. Audit the full local-content boundary

Inspect the current policy generator, broker, client file hooks and downloader
consumers. Produce a reproducible inventory of files available in game_content
versus owned and legacy requests: UI, particles/effects, audio, SuperAnim and
other dependencies. Record basename collisions, aliases, bundles, missing files,
mutable client state and any stream-based reads that bypass existing hooks.
Use actual content and client consumers; avoid wildcard ownership of everything.
The inventory must distinguish unavailable content from existing unowned content.

## 2. Implement the next justified coverage expansion

Choose the largest coherent remaining family whose complete read/dependency
contract can be established this session. Implement generated coverage and any
necessary read-path support. Keep mutable saves/settings outside provider scope.
For multi-file assets require complete coherent bundles. Preserve generation
pinning, safe-point invalidation, same-size edits and missing-file behavior.
Tests must verify actual bytes/paths, collision handling, missing dependencies,
and no persistent cache writes while files are owned and available. Reuse the
existing real broker/proxy integration rather than inventing another harness.

If the audit proves existing hooks can safely cover several families, include
them together with equivalent evidence. If a family needs unresolved reversal,
document exact blockers and implement another justified family instead.

## 3. Map startup and recurring verification, then prepare a guarded transition

Trace first launch, login-to-Home verification, foreground download scenes and
the background-DLC prompt in the Windows client, using arm64 as semantic support.
List required work hidden behind these screens: tables, account initialization,
download flags, completion callbacks and missing-content checks. Identify exact
hooks and prerequisites for replacing this with local availability checks.

Do not simply suppress visuals or force success. Implement a guarded local-only
startup path only where the required content and initialization contract are
proven; otherwise deliver an explicit patch plan with addresses, prerequisites
and acceptance criteria alongside the completed coverage expansion. Default
behavior must remain recoverable if content is missing or the broker is absent.
Do not imply a cold fresh install is supported until tested on an isolated copy.

## 4. Reconcile backlog evidence without overstating closure

Create docs/BUG_BACKLOG_STATUS.md against the current Bug fixes BF.docx in
C:\Users\Evan\Downloads. Preserve original IDs and wording when quoting.
Separate implemented/server-tested, player-confirmed, partial, and unresolved.
#34's reported sphere workflow is player-confirmed; #15 Deemo is only a subset;
#23 farm replay/map return does not establish every first-clear/final-stage scene.
#12/#18 content authoring is still substantive work. Use current source and
evidence for other items rather than extrapolating from unrelated tests.
Do not edit the Word document in this session; list defensible moves and exact
remaining acceptance checks so the maintainer can update it without rewording.

## Validation and deliverables

Build affected server/proxy targets and run targeted and required existing
suites. State exact hashes and paths, positive completion and skips. New source
requires a matching candidate. Keep live save/content unchanged and verify that.
Record coverage counts as inventory metrics, not percentage of project completion.

Write CLAUDE_SESSION_13_HANDOFF.md and a minimal CLIENT_SESSION_13_STEPS.md with
full commands and package/save identities. Prepare any necessary fixtures before
asking for a visual test. Include only new coverage/behavior, not every prior
workflow. Write the full coverage inventory and startup patch plan into durable
docs/scripts, with bulky generated evidence only under the bounded QA directory.
List family-scale rescan latency as an open follow-up unless directly necessary
for this implementation; avoid spending the session on unrelated optimization.
Stop at a reviewable candidate for Codex QC, not a deployed patch.
