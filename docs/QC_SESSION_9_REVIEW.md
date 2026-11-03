# Session 9 independent QC — October 4, 2026

> This is the first-round review of `a1b4286e`. The later corrections and
> independent verification are tracked in
> [QC_SESSION_9_ROUND2_REVIEW.md](QC_SESSION_9_ROUND2_REVIEW.md).

## Client baseline confirmation — October 4

Player confirms Town background alignment and Zeal's sprites in Training Ground
look correct. A summon was also performed. Both `snapshot_before_repeat.json`
and `snapshot_after_repeat.json` contain `{}`: no provider-owned names were
present in the sampled flat LocalState at either endpoint.

Collected `out/qa/current/content_client/offlinemod_content.log` confirms hooks
installed for port 29960 and an active provider in the tested run. Its final
record reports 24 reads (4 RAM hits), 20 fetches totaling 1,335 KB, zero provider
failures, zero observed tripwire writes and zero refused removes. Server logs
confirm Town SAM/textures and Zeal 60956 thumbnail, animation texture, battle
portrait and idle/move/attack data were served. This supports the baseline
provider path; the zero-file snapshots alone would not prove it.

The log includes TransitionalDLCScene and SummonsAnimeScene2, useful evidence
for future download-flow work. Scene-transition frame spikes are recorded;
without a comparable broker-off run they do not establish provider overhead or
a performance improvement. Earlier launches in the same accumulated log could
not reach the broker; those are distinct from the later active run.

No stale-file precedence claim is made for this interval: the before snapshot
contains no owned cached files. Restart persistence, deliberate replacement and
missing-file recovery, and controlled performance comparison remain pending.
The database and intermittent broker findings below remain open. The player
stopped the harness cleanly with Ctrl+C; no reset or repeat of the confirmed
Town/Zeal baseline is required.

Verdict: partially verified; not a clean acceptance or deployment recommendation.
The leader-skill correction and harness checks pass. The database timeout is a
bounded mitigation, not a complete fix. No live save, installed client, normal
server binary, package registration, or Git publication was changed by this QC.

## Independent checks

Candidate: `out/qa/current/session9/bin/gimuserverw.exe`, SHA-256
`a1b4286edfe79ce1d56d618234353eb493c41ab7c4e3e776379a07aeb9a355d2`.

| Check | Result |
| --- | --- |
| Harness startup unit tests | 49 passed (mocked OS/process behavior) |
| Leader-skill grants wire tests | 48 passed |
| Short database contention wire tests | 10 passed; all 19 acknowledged crafts persisted |
| Content broker wire tests | 70 passed on this run |
| Isolated proxy byte comparison | Four port immediates, three metadata bytes, zero other differences |
| Parent and packet-generator diff whitespace checks | Passed |
| Deliberate 6.5-second database reader | Three failed checks; see below |

Logs and the additional reproduction are in `out/qa/current/session9/qc_*`.
The full regression runner was not independently repeated. Claude's recorded
full run had a broker failure as well as the existing validator baseline; one
successful broker rerun does not resolve an intermittent failure.

## Corrections still required

1. **High priority: failed commits can lose acknowledged gameplay and wedge the
   connection.** `standalone_frontend/main.cpp` sets a 5,000 ms busy timeout.
   Holding an isolated save read transaction for 6.5 seconds still reproduced a
   failed COMMIT, an error on the next craft, and loss of the first acknowledged
   craft after shutdown. The short-reader suite only proves the bounded case.
   Implement commit failure recovery and ensure successful replies correspond
   to durable mutations. Add a permanent regression covering a reader beyond
   the timeout, connection usability afterward, and restart persistence.
   Merely increasing the timeout is insufficient. This is a known pre-existing
   defect that Session 9 mitigates, not a newly introduced regression.
2. **Intermittent broker replacement failure remains unresolved.** Claude
   recorded transient 404 responses during replacement on both old and new
   candidates. Capture the actual Windows error and correct transient handling
   without hiding genuinely missing files or weakening the tests. Repeat the
   replacement test enough to exercise the race. Defer deliberate client asset
   replacement/missing-file acceptance until this is corrected.
3. **Use the intended executable for client tests.** The harness currently
   defaults to the Session 8 candidate. Explicitly supply the Session 9 executable
   when verifying these edits; otherwise a READY message does not prove that
   the new server code is running.

## Client scope

See [CLIENT_SESSION_9_STEPS.md](CLIENT_SESSION_9_STEPS.md). Baseline provider
checks can proceed on the isolated copy while the above work remains open.
They are exploratory checks, not acceptance of the complete offline patch.
The client still downloads its legacy cache; only 21 asset names are routed
through the candidate provider. Cache elimination and login-screen removal
remain future work.

The user's sphere visibility, equip, favorite persistence, selective sale, and
mission return-navigation confirmations stand; no repeat of those tests is
needed for this session. Stored leader-skill repair has not been applied to the
live save. Its in-game effect, especially Training Ground own-helper behavior,
still needs a prepared candidate fixture and a separate client check.
