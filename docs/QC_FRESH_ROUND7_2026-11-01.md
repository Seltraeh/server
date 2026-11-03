# Independent round 7 QC — November 1, 2026

## Verdict

Meaningful acceptance progress; not release-ready. Reviewed round-7 implementation and round-8 brief, rebuilt the maintained proxy test target, reran tests and checked candidate/source hashes. Codex did not control or inspect the screen; visual outcomes below are Claude-reported evidence, not independent visual acceptance. No runtime edits or deployment performed.

## Findings for round 8

1. **P2: low-level write success remains overstated.** `offline-proxy/OfflineMod.win32/content/cp_gate_flash_logic.cpp`, WriteCode, checks the first VirtualProtect but ignores the protection-restoration VirtualProtect and FlushInstructionCache results. The new name-fit WriteFn contract promises false if any step fails, but its actual writer does not implement that contract. The injected writer tests fail before copying only. Correct the result model to distinguish bytes changed from protection/cache completion; simply returning false after memcpy is unsafe because InstallSites would omit the failed current site from rollback. Preserve restoration bookkeeping, test each failure stage, and audit the shared writer's existing callers. This is a failure-path finding, not a reproduced client crash.
2. **P2: vertical-fit failure does not preserve the original label.** FitPopupDesc changes wrap width and calls destructive setString before FirstLine validation, and subsequently changes scales/positions. A null FirstLine returns without restoration; SEH only logs after any intermediate mutation. The handoff's claim that a fault leaves the label as built is unsupported. Validate what can be validated before mutation, define a recoverable fallback with explicit state/ownership, and test early-return and post-mutation failures. Do not promise rollback for arbitrary memory corruption; if recovery is unsafe, state and implement a safe failure policy rather than silently continuing as if unchanged. Keep this bounded to the popup adapter.

The prior unchecked per-site InstallSites/RestoreSites result handling is fixed for the tested pre-write failure model: independent sets retain retry state, rollback is exercised, and restoration is idempotent. Small shared testable logic is appropriate; it is not unnecessary abstraction. The vertical-fit arithmetic tests do not execute the actual x86 thunk, client allocator, node mutation or exception path.

## Independent checks

Evidence: `out/qa/current/review_round7_20261101/`.

- Rebuilt `content_provider_tests` in the existing build tree using **RelWithDebInfo**: success.
- Rebuilt executable: **898 passed, 0 failed, 5 skipped**. Skips require live broker/environment integration; no claim those were tested.
- Original-client disassembly checker: **44 passed, 0 failures**.
- An initial attempt using Release failed because this tree only configures RelWithDebInfo. Corrected; retained both logs. Not a source/build defect.
- Candidate DLL SHA256 matches handoff: `23f9a4b0c3096529bfe2a732b19a5000242bb52266eb9900ae00ec620fdfefd9`.
- `cp_name_fit.cpp` SHA256 matches handoff: `0682f2a8df97f031b0b1fc5ea3cedb78c5f14f0f7603d3682046aa5265f43f51`.
- Did not rebuild/replace the candidate DLL, rerun unchanged server suites, or perform visuals. Full source-to-candidate binary reproducibility was not established by hashing alone.

## Progress

| Area | Current evidence | Remaining limit |
|---|---|---|
| Header / popup | Claude observed one-line names and long descriptions fitting vertically/horizontally; short controls unchanged | Failure paths above; other popup scenes and smaller display readability not covered |
| #22-1 Lukroar | Four successful pulls, two effects, matched control; broker supplied gate art | Not reproduced, not a proven new fix; warm cached scripts/sounds; no missing-art negative control |
| #25 SP | Claude observed 10→40, cap80→100, material consumption, ineligibility and relog | Ordinary-unit chance and Omni+ are separate missing capabilities, not covered by this acceptance |
| #40 UBB | Actual evolution, unlock, Training Ground Overdrive/Promethea use, negative controls and relog | Infinite gauges; exact originally reported unit unknown; natural gauge route untested |
| #26 resources | Previous wire tests support server payment/EXP | Matched client reward runs and per-hit item contract unfinished |
| Content #12/#15/#17/#18 | Inventory and researched starting points exist | Implementation scope/data still unresolved |
| Training-arena status-icon position | New user-reported visual anomaly | Capture, classify, trace enemy anchor/offset/scale and fix if reproduced; not yet independently verified |

Fixture lesson: changing unit_lvl without matching total_exp produces inconsistent client/server levels. Round7 corrected the fixtures. Promote reusable fixture validation, not every one-off QA script.

No credible all-bugs completion percentage/date yet. More completed visual routes mean less repeat testing is needed, but outstanding content and reward contracts still prevent an all-bugs-fixed release claim.
