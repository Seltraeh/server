# Session 12 QC — October 8, 2026

Verdict: suitable for the two scoped client acceptance runs after a minor
collection correction. No blocking defect found in the reviewed clearunits
repair or visible-swap implementation. Visual acceptance remains outstanding.

Reviewed the Session 12 changes in DebugCli, fixture validation/building,
test helpers, harness collection and the live visible-swap driver, alongside
the handoff and client checklist. Earlier dirty server/submodule changes are
preserved and are not represented as new Session 12 changes. The reverse-
engineering findings are supported by the documented chains and prior player
observations; all disassembly was not independently reproduced during this QC.

## Finding corrected

The collection archive used second-resolution timestamps with exist_ok=True.
Two collections in the same second could overwrite the earlier evidence.
Directories are now reserved exclusively, with numeric suffixes on collision.
The preservation test now runs both collections at the same mocked timestamp.
Five command tests pass after the correction. No production binary changed.

## Independent tests

| Test | Result |
| --- | --- |
| Actual clearunits command, roster payload/DB, repeat/empty/two-user cases | 24 passed, 0 failed |
| Actual broker plus proxy hooks: visible swap/restore/park/restore | 340 proxy checks and 10 driver checks passed; 2 other opt-in modes skipped |
| Harness startup/isolation mocks | 49 passed, 0 failed |
| Harness command and collection tests after QC correction | 5 passed |

Candidate SHA-256: e30e2adff2f016393de962e8229066d111f2503c2418f3a648f633ea01ea97a4.
Proxy tests SHA-256: 8536442e16b8cec51f25aada31a06f9c65988a4dd5e93faf66ff6cd861c88ff3.
Both match the handoff. Evidence is in out/qa/current/session12/qc_clearunits.log,
qc_visible_swap.log, qc_harness_startup.log and qc_harness_commands.log.
Claude's reported full 51-suite run was not independently repeated; targeted
tests above were rerun against its matching candidate. No C++ correction or
production rebuild was necessary during this QC.

The visible-swap test verified all four publications, exact drawn-key handling
through the hook test doubles, restored all 17,445 disposable files, confirmed
the 16 involved real content files and live save unchanged, and freed port
19963. Publication took 18.6/13.1/4.9/13.3 seconds: latency remains open.

## Limits and next steps

Run CLIENT_SESSION_12_STEPS.md for the repaired small roster and actual visible
Archive thumbnail. Neither fake game hooks nor valid wire references certify
real rendering. Do not repeat the withdrawn leader A/B test. clearunits repairs
the documented dependencies; it is not a general repair of all malformed saves,
and an empty roster remains a no-op. The debug pipe helper's read is blocking;
tests should remain bounded by the suite runner's outer timeout.

The handbook now replaces automatic deployment with candidate acceptance,
corrects success-shaped refusal advice, documents actual current coverage and
fixture/log semantics, and points to Session 13. The next directive advances
remaining local asset coverage and startup/verification mapping, plus a backlog
evidence reconciliation, independently of the two pending player checks.

No deployment, live-save edit, commit or push was performed.
