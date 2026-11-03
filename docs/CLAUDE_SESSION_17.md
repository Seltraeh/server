# Claude Session 17 — repair the actual C16 startup-hook installation

Read applicable repository instructions, the handbook, `QC_SESSION_16_FINAL_REVIEW.md`, and **`SESSION_16_CLIENT_RESULTS.md`** first. The user authorized Codex to perform visuals and hand you the next work. Actual launch found a blocker before the visual sequence could be accepted; this session is corrective, not advancement to complete-coverage bypass.

## Priority: real attach-sequence defect

The registered C16 candidate (`8634857a…`) logs content-provider hooks installed, followed by:

`startup coverage path DECLINED: censusLookup: censusLookup: byte +0 is E9, expected 55`

The log path and reproduction are in the client-results file. `ContentProvider_Attach()` precedes `Startup_Attach()`. The latter verifies original-image signatures including censusLookup at RVA 0xE417E0 after the former has installed its detour. Inspect and prove the overlap and lifecycle before changing it.

Design a safe coordinated validation/install sequence: preserve validation against the supported original bytes, ensure the expected provider owns any altered site, and retain fail-closed behavior for unrelated patches or unsupported binaries. Do not accept arbitrary E9 bytes, drop the proof site or globally weaken signatures. Preserve attach/detach rollback correctness and the content provider's startup dependency. Document the chosen approach and why it is safe.

Add a regression that exercises the real ordering/modified bytes, not only standalone decision logic or signatures on an untouched executable. Demonstrate the old behavior fails and the fix passes. Rerun the affected proxy, static proof and live startup suites, and build a new isolated 29960 candidate using the existing build tree/output hygiene. Update hashes and KNOWN_DLLS, marking the previous candidate superseded for startup-hook validation. Do not overwrite evidence logs or create a new build forest.

## Boundaries and handoff

Do not modify the live save, normal client, accepted C1 fallback, current C16 account/cache state, or loosen the incomplete coverage contract. No deployment, commit or push. No age/consent bypass or preference injection: the user must handle the client consent checkbox. Do not claim visuals passed. Leave the candidate staged for Codex to swap and launch through the registered shortcut after automated QC; do not operate the client while Codex is testing it.

After this bounded fix, return a handoff with root cause, exact changed files, tests including negative controls, hashes, and exact resume steps. Broader derivation of 16 unknown lists, missing-content decisions, master-table binding and fresh-install support remain subsequent work. Do not expand this session into those changes before repairing and validating hook installation.
