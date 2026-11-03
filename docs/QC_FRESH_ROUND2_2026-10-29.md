# Independent QC of fresh round 2

October 29 2026. Reviewed docs/CLAUDE_FRESH_ROUND2_HANDOFF_2026-10-28.md, acceptance updates and round-2 QA helpers. No screen interaction, runtime edits or deployments were performed by this review.

## Verdict

Carry the existing runtime candidate forward. Round 2 made no reported runtime fixes; it supplied Claude-observed acceptance for #24, #30, #23 and the remaining #33 cap check. No new runtime defect was established here. Correct the QA isolation weaknesses below before reusing the helpers for another round.

## Evidence checked

- Tracked server diff SHA256 still 88c473d5e9042b0223f7b11f1bcb3201fe2c1e71a1d4b347d50e53383380ebad, matching round-1 QC and the completed suite report.
- Candidate independently hashed to 03101975f6ba7a1c919b063eaeab627a004f6f497a03d0aba11ffd2b4bde9e03.
- Saved full report still records 63 suites, 2692 passed checks, no failed suites/checks. It is round-1 execution evidence, not a new round-2 run.
- Four Python QA helpers passed AST syntax parsing; two PowerShell helpers passed the PowerShell parser with zero errors. No helper was executed against a client or save.
- Read the fixture preparation and process-control code. Read-only source SQLite connections, backup-API copies, parameterized values, forward-slash configs and separate gate MST roots are appropriate practices.
- Tracked diff equality does not prove every untracked source file unchanged. No exhaustive new build/source provenance audit was performed; do not call this a clean-source release certification.

## Findings for correction before helper reuse

### P2 Process ownership is too broad

out/qa/current/fresh_round2_20261029/tools/run_ctl.ps1 selects all python.exe commands containing http_relay_delay.py and all servers with one of several candidate executable paths. The stop branch then force-stops every match. The supplied run name selects a config but does not constrain these selectors. Two runs sharing a candidate, or another relay with the same filename, can be interrupted. No accidental termination was observed in this review.

Persist the PIDs actually started, creation times, canonical executable/script/config paths and ports in the run folder. On stop verify that identity before acting, refuse mismatches/PID reuse, and leave other runs alone. Roll back only owned processes when startup fails. Verify listener ownership, not just the existence of a listener. An exclusive-open failure proves the database is held by something, not necessarily the launched server; label that probe accurately.

### P2 Run-name containment is not enforced

prep_run.py appends the user-provided name directly below visual/saves; run_ctl.ps1 likewise interpolates Name. Traversal or rooted names can escape the intended disposable area. The observed names were ordinary safe names, so this does not invalidate those runs. Before writes or process operations, require a single valid run identifier and resolve/check containment below the dedicated saves root. Use explicit errors, not assert-only safety guards that disappear under Python -O. Add negative checks that do not touch external paths.

### P3 Reusable chain helper overstates its support

prep_chain.py follows only the first comma-separated prerequisite, has no cycle guard, and does not clear or reject an already-cleared NEXT row although its description promises a first-clear fixture. The specific round-2 routes are not shown invalid by this, but reuse for compound requirements can silently prepare an incomplete fixture. Either support all dependencies with cycle/missing-node checks or refuse unsupported shapes; assert first-clear preconditions before mutation. Keep it restricted to the current disposable run and require the expected account count.

## Acceptance limits

Claude reports seven gate pulls, four Lizeria prerequisite states plus stale-entry rejection, mission 84/85 first/replay behavior and energy reaching 431 with relog. These are Claude's observations, not independent visual reruns. Retain synthetic-progression and frame-sampling limits. The acceptance matrix says 60 rapid taps while the handoff says 59; reconcile with the saved tap index instead of copying either number. Avoid claims that snapshots exclude all brief intervening screens or establish an exact transition second.

The Word tracker was not updated in round 2 and was not changed in this review. Its October 28 notes are historical; use the acceptance matrix for latest status. The matrix introduction should stop calling the Word file unchanged, since it received dated notes on October 28.

## Next step

CLAUDE_FRESH_ROUND3_2026-10-29.md provides the correction-first bounded assignment. No full suite was repeated because no runtime change was identified. Missing content, remaining client acceptance and packaging integration still prevent an all-fixes release claim.
