# Session 11 QC — October 5, 2026

Verdict: suitable for targeted C2 client checks after the corrections below.
This is not final client acceptance or approval to remove download screens.
Scope: Session 11 policy, harness, integration tests and instructions. Earlier
uncommitted work in the checkout is not treated as new Session 11 work.

## Corrections made during QC

- `await_broker` returned success when an index request failed. A timeout
  during a rescan could look like successful publication. It now retries and
  fails if publication cannot be observed within the deadline.
- `restore NAME` required the entire unavailable index to be empty. An unrelated
  parked asset made successful selective restoration time out. It now checks
  the identities of the requested restored names.
- Added three focused regressions: unavailable broker, transient failure, and
  selective restore with an unrelated parked asset.
- Corrected the client checklist: a parked asset's visual response is not a
  guaranteed blank thumbnail; cache counts do not prove a performance route is
  warm; the older test DLL comparison is not full C1 performance acceptance.
  Added collection after B and a combat-HP observation alongside leader text.

## Independent validation

All completed with zero failures:

| Check | Result |
| --- | --- |
| Real C2 broker/proxy integration | 5,948 proxy checks; 7 driver checks |
| Baseline broker/proxy integration | 341 proxy checks; 5 driver checks |
| C2 policy integrity, collisions, completeness and hashes | 8 checks |
| Harness startup and isolation mocks | 49 checks |
| Live disposable swap/park/restore | 13 checks |
| New harness error-path regressions | 3 tests |

Each proxy run skips the other opt-in integration mode; both were exercised
separately. C2 served all 27 traced owned names byte-for-byte from intended
paths and indexed 17,445 names. C1 regeneration is byte-identical to Session
10 (SHA-256 `69f0ecd3...`). The server hash remains `270832d6...`; the proxy
test executable matches the handoff's `646f8786...`.

No production rebuild was needed for Python/document corrections. Claude's
full gameplay regression run was not independently repeated: Session 11 and
these corrections do not change gameplay/server implementation.

Evidence in `out/qa/current/session11/`: `qc_family_live.log`,
`qc_provider_live.log`, `qc_policy.log`, `qc_harness.log`, `qc_disposable.log`.
Focused tests: `python scripts/test_content_disposable_commands.py`.

The disposable smoke confirmed involved real content files retained bytes and
modification times, the live save retained its hash, and port 29960 was released.
All test asset modifications were restored. No deployment, commit, push or
live-save modification was performed.

## Remaining acceptance and next work

Follow [CLIENT_SESSION_11_STEPS.md](CLIENT_SESSION_11_STEPS.md): C2 art and
replacement/recovery, crafting kit, leader behavior before/after, small roster,
and the qualified performance comparison. Accepted C1 Yes/No and restart
sequences need not be repeated.

Publication is still slow at family scale: QC observed 18 seconds for swap,
2 seconds for withdrawal and 21 seconds for restore. Optimize this while
retaining same-size change detection and atomic bundle withdrawal/recovery.

First-visit file attribution uses filesystem/log timestamp correlation, not
direct tracing of every legacy enqueue. Legacy particles, audio, UI and
SuperAnim content still download. Do not disable verification or the DLC prompt
until remaining dependencies and unavailable-content behavior are validated.
