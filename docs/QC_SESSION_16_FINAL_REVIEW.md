# Session 16 corrected candidate QC — October 21, 2026

**Later native launch found a runtime blocker:** startup-hook installation declines after
the provider detours censusLookup. Earlier automated clearance below does not constitute
visual acceptance. See [SESSION_16_CLIENT_RESULTS.md](SESSION_16_CLIENT_RESULTS.md) and
the corrective [Session 17 directive](CLAUDE_SESSION_17.md).

Review scope: the corrected Session 16 startup contract, its asset policy and the previous two QC reviews. This is not a fresh certification of every historical gameplay change.

**Verdict: cleared for the isolated C16 fallback visual checklist. Not cleared for normal-use deployment or complete startup-screen removal.**

## Findings

No new source-level blocker identified for an isolated **fallback** client run. The original completeness defects are addressed: required sets are validated, missing dependencies remain required, absent/unknown requirements prevent completeness, the harness applies the stricter report rule, and startup bypass requires an Active provider on the matching adopted generation. Unknown category enumeration remains explicitly unknown instead of pretending to be complete.

This does **not** approve release of a cache-free startup. The actual manifest remains incomplete (16 unknown lists; login 62 absent and one unowned name; background five absent and 26 unowned names). Full-tree unowned names appear missing in the harness's policy-only content copy. The bypass has automated decision-level evidence with a complete small fixture, not visual evidence on complete real content.

Documentation corrections made during QC: preserve the post-quest area navigation requirement; add a client-and-harness restart and pre-control cache snapshot; remove an unsupported attribution for the refresh-and-close notice; do not promise a maximum download duration on unproven coverage.

## Independent checks

- Manifest/evaluator regression suite: 45 passed, zero failed.
- Harness suite: 78 passed, zero failed, including shared parser corpus.
- Proxy unit suite: 577 passed, zero failed, five live-integration skips.
- Live startup suite, including real content: 11 passed, zero failed. Complete small fixture bypasses; real content preserves census/prompt.
- Live C7 family suite: 11 passed, zero failed. Its proxy run: 6,270 passed, zero failed, four unrelated integration skips; 36,321 game reads byte-exact.
- Startup static proof and all six negative controls: passed.
- Coverage wire suite, including full-tree broker/evaluator agreement: 56 passed, zero failed.

Logs are `out/qa/current/session16/qc_final_*.log`. The full historical server regression suite was reported by Claude but was not independently rerun in this review.

Verified SHA-256 against the handoff:

| Artifact | SHA-256 |
|---|---|
| Server | b2552da2b531d762f4a09225b41807c9ef34cb136067e71492ca8fc0fab82435 |
| Isolated DLL and C16 layout DLL | 8634857aeb98d1ab2b269d4c92c23f5681ece9a76b633f084631bee8d4ca0095 |
| Startup manifest | 3be2dae4220e4c82736beaa050c15b5e0e16d25258b96e5be770691305795a0c |
| Proxy test executable | 4958210e2aef06565d099192b3a381c1b88216cd442573cfc7492b4287f15d72 |

## Native client test status

Not run. `Get-AppxPackage '*ContentTestC16*'` returned no package, C16 was absent from the loopback exemption list, and the current process is not an Administrator. The isolated package needs registration and an elevated loopback command before it can reach the local test server. No package, exemption, cache or live save was changed by this review.

Use [CLIENT_SESSION_16_STEPS.md](CLIENT_SESSION_16_STEPS.md) in order. It contains exact setup, server, collection, missing-file and broker-off commands. All gameplay is on the named copied save `s16_cold`, not the live save. Broker-off must be last because it intentionally fills the test cache.

Acceptance requires both observations and collected logs: drawable Home/squad/quest, correct quest-area return, persistence across both restart types, no rating/Facebook prompts, no provider-owned cache files before controls, and expected legacy behavior when a file is parked or the broker is off. Absence of one intermittent prompt alone does not prove all paths. No `LEFT OUT` startup decision should appear with this incomplete real manifest.

Passing these visuals clears this fallback candidate only. Exact requirements for the 16 lists, missing-content decisions, a self-contained first-launch setup, and real complete-coverage bypass acceptance remain future work.
