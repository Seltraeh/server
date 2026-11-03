# Session 16 partial correction QC — October 20, 2026

**Not cleared for client visuals or deployment.** Claude's latest handoff explicitly says the correction pass stopped at a usage limit. Its candidate hashes describe the pre-correction binaries. This is an unfinished implementation, not a completed fix awaiting visual approval.

## Findings

1. **P1: missing dependencies are still silently omitted.** `scripts/startup_coverage.py:247` remains unchanged. Rechecking the QC fixture containing `missing_atlas.plist` without its PNG still produces only the plist and no problem. Category enumeration still starts with existing files rather than proving all expected names. Original QC finding 2 remains open.
2. **P1: offline completeness evaluation still disagrees with the edited server.** `startup_coverage.py:433` excludes absent names from problems; it also ignores the manifest's unknown lists. A controlled evaluator probe with both required sets containing an absent file and an unknown category returned `complete=True`. The edited server correctly treats absent names as problems. Update schema validation and evaluation together, with agreement tests, before trusting regenerated evidence.
3. **P2: the harness still reports completeness from an unvalidated claim.** `content_provider_client_test.py` accepts `bfcp 2 / coverage 1 / complete 1 / generation 1 / end` as complete despite missing the required sets and headers. The edited proxy rejects that report. Its CLI can therefore mislead the tester while claiming parity with the DLL. Update its parser and tests alongside the protocol change.
4. **Delivery and validation unfinished.** The corrected server and DLL candidates have not been rebuilt; negative parser, startup-race and wire cases are still pending. Existing candidate hashes and client layout must not be used as evidence for these source edits.

## Changes inspected and checks run

- Server source now requires nonempty login/background sets, rejects duplicate and contradictory entries, and treats absent names as coverage problems. These are appropriate corrections, but this review did not compile or run that server source.
- Proxy source now validates report structure/counters/build identity and requires an Active provider with matching generation after the fetch. Direction is correct; the missing negative tests still matter.
- Rebuilt **only** `content_provider_tests` in the existing Visual Studio/CMake build tree, configuration RelWithDebInfo. Build succeeded.
- Freshly built proxy tests: **497 passed, 0 failed, 5 skipped**. The skips are live broker, startup coverage, audio, family and visible-swap integrations. No live integration clearance is implied.
- Build/test logs: `out/qa/current/session16/qc_correction_build.log` and `qc_correction_proxy.log`.
- Rebuilt test executable SHA-256: `fe30c3b0dcb11ad504cf62a0aaf381d0c5425b3b649343ca365d1d8b52a056b7`. This supersedes the handoff hash for that test executable only. No deployable DLL or server was rebuilt by this QC pass.
- No client launched, live save modified, cache reset, deployment, commit or push.

## Next pass, in order

1. Finish requirement derivation and preserve every missing dependency. Mark unproven enumeration unknown; do not claim completeness from an existing-file inventory.
2. Align server, Python evaluator and harness validation. Add the original QC probes and the evaluator/harness cases above as negative tests.
3. Finish proxy parser and provider-state/generation negative tests. Rerun against rebuilt binaries, not the previous candidates.
4. Rebuild affected candidates, regenerate policy/manifest/reports, update hashes and the C16 layout, and rerun focused wire/live/harness suites.
5. Return for QC. Only then perform the isolated cold-client visual checklist. A real incomplete-coverage run tests fallback behavior; it does not validate removal of startup screens.

## Plain-language project status

The direct-from-game_content asset reader is established and has previous automated and player evidence. The last C7 policy covered 75,286 names; on one recorded cold route it covered 5,604 of 5,612 downloads. That is strong progress on that route, not a completion percentage for the whole patch.

Removing the download/verification screens safely is still unfinished. First the coverage checks must become trustworthy; then remaining missing assets and unproven categories must be resolved; then the complete-coverage startup path needs real client testing. The existing cold fixture also copies client master-table/account state: it is asset-cold, not proof that a completely fresh installation can start without pre-existing state. A self-contained first-launch setup is a separate remaining acceptance requirement.

Plan for multiple bounded passes: finish correctness, resolve coverage/first-launch gaps, then visual regression and packaging. The unknown part is how many missing assets can be excluded with client evidence versus needing content repair or additional reverse engineering. A reliable hour estimate or percent complete is not available yet.

Gameplay backlog is separate. Accepted sphere, mission-navigation, summon-transition and prompt-removal work is not undone by these findings. The backlog still contains substantial encounter/content work (for example Vortex Trials, Mecha God drops and boss behavior), as well as client acceptance checks. This review does not certify every historical gameplay item or move anything in the Word backlog.
