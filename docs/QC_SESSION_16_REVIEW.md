# Session 16 QC — October 20, 2026

**Verdict: changes need correction before startup bypass is cleared. Do not run the Session 16 visual checklist yet.** Existing passing content-provider tests establish useful coverage, but do not establish that the startup completeness contract is sound. No production code, installed package, live save or cache was changed during this review.

## Findings and next Claude pass

1. **P1 — invalid manifests can authorize a startup bypass.** `ContentBroker.cpp:372` validates that `sets` is an object with entries, but does not require nonempty login and background sets. Against the actual candidate, a manifest containing only a valid login set returned `complete=1`; a manifest with empty login and background arrays also returned `complete=1`. Reproducer and output: `out/qa/current/session16/qc_manifest_probe.py` and `qc_manifest_probe.log`. Require the expected sets and reject empty/incomplete schemas. Add negative wire tests for omitted, empty and malformed sets. Make the proxy reject contradictory or incomplete coverage reports rather than accepting a bare completeness assertion.

2. **P1 — required-file derivation can silently lose missing dependencies.** `startup_coverage.py:298` enumerates files that already exist in category folders; the category MST names are descriptive, not used to derive every expected filename. This does not prove a superset of required names when an asset is missing. At line 247, a sprite plist's expected PNG is explicitly omitted if it cannot be resolved. An independent fixture containing `missing_atlas.plist` without its PNG returned `({'missing_atlas.plist'}, {})`: no missing dependency and no problem. Derive requirements independently of file existence, preserve missing dependencies in the closure, and leave unproven category enumeration incomplete. Add tests that remove a required file before regeneration and prove coverage cannot become complete by forgetting it.

3. **P1 — absent everywhere is not evidence of optionality.** `ContentBroker.cpp:1706` accepts manifest `absent` entries without making completeness false. The generated manifest includes 62 login and three background absent entries. Their absence from all sources establishes that they cannot be served, not that the startup path does not need them. Default these to incomplete; only exclude a name with a documented client-side reason that it is optional or unreachable. Update evaluator and wire tests together. Do not fill gaps with dummy assets merely to obtain a green contract.

4. **P2 — census bypass does not always require an adopted generation.** `offline-proxy/OfflineMod.win32/content/cp_startup_logic.cpp:119` only checks SameGeneration when the provider is Active, then permits bypass while it is Starting. A complete HTTP report is not proof that the client provider has successfully adopted that generation. Keep stock behavior until Active and generation-matched; test delayed startup, failed initialization and a generation change between report and adoption.

The current real manifest remains incomplete, so these findings do not show that the supplied C16 run already skipped startup incorrectly. They concern the gate that will become active as remaining coverage blockers are removed. Keep the startup optimization disabled/fail-closed while addressing them.

## Independently rerun checks

- Proxy suite: 497 passed, zero failed, five skipped (`qc_proxy.log`).
- Harness suite: 71 passed, zero failed (`qc_harness.log`).
- Coverage wire suite, including real C7: 32 passed, zero failed (`qc_coverage.log`). Its current acceptance tests do not cover the invalid-manifest reproductions above.
- Live C7 family driver: 11 passed, zero failed. Proxy integration: 6,190 passed, zero failed, four skipped; 36,321 byte-exact game reads (`qc_family.log`).
- Startup-site negative controls: six of six passed (`qc_startup_sites.log`).
- Candidate server, proxy, C7 policy and manifest SHA-256 values match the handoff.

This review did not independently rerun all 55 server regression entries claimed in the handoff. Passing counts above are not a replacement for the missing negative tests or client visuals.

## Completion criteria for the correction pass

Fix the four contract gaps, add focused negative tests, rebuild the affected server/proxy candidates using the handbook's existing build paths, rerun these focused suites, and update the manifest/report/hashes and handoff. Keep artifacts under the existing Session 16 QA directory. Preserve the accepted C1 fallback and all live data. Do not deploy, reset packages, commit or push as part of this correction pass.

After QC clears the corrected build, use `CLIENT_SESSION_16_STEPS.md` for the isolated cold-package visual sequence. Record expected stock startup behavior when coverage is incomplete separately from an explicitly validated complete-contract run. The earlier refresh-and-close dialog's cause remains unconfirmed; do not attribute it categorically to the stock client or this patch without evidence.

## Native visual testing

The Computer Use runtime successfully enumerated the native Brave Frontier Windows app in this session. Codex can launch and inspect a native game window and operate it using screenshots; it does not embed the native game in the browser panel. No game was launched during this review. Once the corrected candidate is cleared, use the isolated test package and save for automated visual checks. Audio and subtle animation timing still benefit from player confirmation; screenshots alone cannot certify them.
