# Session 18 corrective pass: bind server response tables

Independent QC, October 22, 2026. This is a bounded correction before candidate visuals, not a new broad reverse-engineering session.

Independent reruns passed: manifest 71/0, proxy 677/0 (five integration skips), mocked harness 86/0, binary rules exit 0, live startup 17/0. Candidate server/DLL/policy/manifest hashes match the handoff. No client was swapped or launched by QC.

## Finding

`scripts/startup_coverage.py:source_rows` derives required names from six response MST files and records their hashes in `inputs.server_files`. `ContentBroker.cpp` parses and binds only `inputs.client_files`; it does not reference `server_files`. `check_client_inputs.py` validates server hashes offline, but that optional command is not runtime enforcement. Changing a response table without regenerating coverage can therefore leave the broker's requirements stale. Today's unknown lists force fallback, but that does not close the binding defect once coverage becomes complete.

## Work

1. Reproduce with an isolated, otherwise-complete fixture: generate coverage from a response table, change a resource-bearing field to require another asset, then start the server with the old manifest. Demonstrate the missing rejection in the current candidate; preserve the test outside `out`.
2. Bind coverage to the actual response-table snapshot used by Initialize/UserInfo. Validate schema, roots, paths and digests. Missing, mismatched or malformed required server input must prevent a complete report with a useful reason. Checking unrelated files or only the generator's source directory is insufficient. Explain whether tables are immutable after startup; handle reload/change semantics consistently with the served snapshot and broker generation.
3. Add regression cases for matching inputs, changed/deleted inputs, malformed or duplicate entries, and restored inputs. Retain compatibility by falling back for old/unbound manifests, never trusting them as complete. Ensure real coverage remains incomplete for the existing six unknown lists and content gaps.
4. Reuse existing VS build trees and Session 18 QA folder. Run the focused coverage/live/manifest/harness tests and affected server checks. Preserve previous QC logs and dirty work; no production deploy, client swap, save mutation, staging, commit or push.
5. Append the correction and final source/binary hashes/results to the Session 18 handoff. Clearly identify completion so Codex can independently QC and run the isolated visual route. Do not expand into the remaining lists, consent screen or unrelated gameplay in this corrective pass.

Session documents and `out` are coordination/evidence only and are excluded from the proposed release commit. Preserve them on disk.
