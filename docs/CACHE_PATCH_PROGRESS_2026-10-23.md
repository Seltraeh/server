# Cache/download patch progress — October 23, 2026

The asset provider works on tested routes. The complete replacement for startup caching/download checks is **not finished**. The work has moved from building the provider to proving that it supplies everything the real client will request, with the correct versions and initialization intact. Nothing in this report means the normal client has been deployed with these candidates.

| Session | What it accomplished | What that means |
|---|---|---|
| 7 | Introduced the authoritative server content provider and its client-side reader integration. | Foundation for reading assets from the local content tree. |
| 8 | Added an isolated provider proof; also repaired repeated sphere-row handling. | Safer testing without modifying the normal client; separate gameplay fix. |
| 9 | Prepared isolated-port candidates and harness readiness; addressed stored leader skills and a database wedge. | More reliable candidate testing and server state handling. |
| 10 | Fixed synchronization in live asset-swap tests and strengthened transaction rollback behavior. | Asset edits are tested against the correct generation; failures should not leave partial database writes. |
| 11 | Traced first-visit downloads to specific unit artwork, layouts, effects and sounds. | Replaced guesses about residual downloads with a concrete asset inventory. |
| 12 | Repaired test fixtures and verified client test targets. | Subsequent visual tests exercise the intended content and identities. |
| 13 | Expanded effect-family coverage and documented startup/content boundaries. | More effects can be served by the provider; remaining startup work became explicit. |
| 14 | Diagnosed the summon whiteout mechanism, prepared gate-script coverage and investigated Farm behavior. | Supplied evidence for the later summon fix; the script-only approach was not the final solution. |
| 15 | Implemented the final-gate reveal-flash hook, memory-backed audio reading and Home/Summon UI coverage. | Broadened visible and audio asset delivery. Later user checks also confirmed the obsolete rating/Facebook prompts were absent. |
| 16 | Added a strict startup coverage contract and missing-dependency safeguards, plus isolated C16 testing. | The client may skip work only when completeness is proved; missing/unknown content keeps the original path. |
| 17 | Fixed hook installation order: validate original code before provider detours alter it. | The startup hooks can install correctly instead of declining on their own changes. |
| 18 | Derived ten startup groups from client/table evidence, matched the client's exclusion list, expanded layout coverage and bound requirements to actual served tables. | Warm C16 Home → quest → rewards → map worked; stale response-table coverage is rejected. |
| 19 | Added item and area derivation, binding for the synthesized area table, and a fresh-install master-table audit. | The audit delivers all required startup tables; QC still requires an area-ordering proof, two alias decisions and actual fresh-client observation. |

## Remaining work

- Five reported unknown groups: units/initial units, weapons/summoner images, summons, effects, and login-end preloads. Area's source-ordering proof is an additional prerequisite before calling that rule exact.
- Eight required filenames absent from the available sources and nine unresolved content families. The count increased because exact derivation exposed three missing area banners; it does not mean previously working assets were removed.
- Two area image names whose current aliases select a different copy from the area consumer's requested folder.
- Complete fresh-client observation, then final cache/download acceptance with a complete real-content report. Audio and very brief screens require appropriate observation beyond screenshots.

Session 20 is deliberately limited to area ordering, the two aliases, login-end preloads, audit robustness and release dependencies. It is not the final release session. The remaining work is several bounded proof/content steps; there is not yet enough evidence for a reliable hour estimate or a promise that one more session will finish it.

## Release hygiene

Include necessary runtime changes, generators, tests, test fixtures and maintained technical documentation after review. Keep unrelated gameplay changes separately reviewable. Exclude build/output trees and transient `CLAUDE_SESSION*`, `CLIENT_SESSION*`, `QC_SESSION*`, `SESSION_*_CLIENT_RESULTS`, async coordination notes and this progress note from the proposed commit. Preserve them on disk. No staging, deletion, commit, push or deployment was performed.

Current independent QC details are in `QC_SESSION_19_REVIEW.md`; visual observations are in `SESSION_19_CLIENT_RESULTS.md` when complete. Historical statements above summarize their corresponding handoffs and user observations, rather than claiming those historical tests were rerun today.
