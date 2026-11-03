# Session 17 independent QC — October 21, 2026

**Independent code QC and the bounded registered C16 fallback visual run are complete. No blocking defect found in the attach-order repair; not approved for deployment or cache-free startup.** See `SESSION_16_CLIENT_RESULTS.md` for actual installed-hook, quest, restart, missing-file and provider-off evidence. Audio and a result-screen continuation cue comparison remain unassessed. Claude completed its handoff and was idle before review.

The repair validates original signatures before the provider installs its detours, then verifies the exact recorded lookup detour, original displaced instructions, trampoline return and unchanged surrounding bytes before installing startup hooks. It does not accept arbitrary jumps or remove the original proof site. Reviewed the production attach order, provider detour records, snapshot verification and startup attach/detach code. The new real-image/Detours regression covers the prior failure; an AppContainer launch is still required to prove the actual DLL initialization path.

## Independent reruns

- Proxy tests: **625 passed, zero failed, five integration skips**. Includes the real EXE attach-order test and negative cases.
- Harness tests: **79 passed, zero failed**.
- Attach-overlap proof and all six negative controls: passed. The expected FAIL lines inside negative models are controls, not suite failures.
- Live startup decisions, including real content: **11 passed, zero failed**. Complete fixture bypasses; incomplete real content preserves census/prompt.
- Logs: `out/qa/current/session17/qc_proxy.log`, `qc_harness.log`, `qc_overlap.log`, `qc_startup_live.log`.

Verified candidate DLL hash `45a2c1b18e3ad1da07db735282636bf62f52003c5552e43347ba05ea1a41c1c8` and test executable hash `b766d8c654a1dff6b769ea5d2dfb5ea2ae952630ae2c3e9c0e979823ccdb440a` against the handoff. No server source change; broad gameplay suites were not rerun.

## Preparation record (completed before the native run)

With the game and harness closed, swapped the verified Session 17 DLL into the registered C16 layout using the harness command. Previous DLL retained in its history. Snapshot `c16_after_attempt1` shows **zero owned and zero other cached assets**. Preflight reports **zero outstanding items**. No re-registration, account-state copy or reset is needed; checklist R1-R3 are complete.

Resume at Part A of `CLIENT_SESSION_16_STEPS.md`: start the named isolated server, launch **[ContentTest C16]** through its registered shortcut, and have the user handle the legal-age/parental-consent screen. Automation must not submit it. Collect the log and require `startup coverage path installed ... Its 9 sites matched ...`; any DECLINED means stop. Then continue Parts A-E, including both restarts and the pre-control snapshot. The real manifest is still incomplete; no startup `LEFT OUT` decision is expected.

No further launch was attempted while the known consent gate awaited the user. After the user personally accepted it, native testing resumed and the repaired hook installed on all subsequent captured launches. Home, mission 10, restart persistence and both fallback controls completed; audio was not assessed. The test client/server are stopped. Live data and C1 are untouched.

The scheduled follow-up is removed now that repair QC and preparation are complete; progress needs the user's consent-screen action rather than repeated polling. Resume this thread when available.
