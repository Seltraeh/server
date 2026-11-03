# Session 9 round 2 independent QC — October 4, 2026

**Verdict: gameplay persistence checks may proceed on the isolated candidate;
provider acceptance is still incomplete.** The two earlier reproduced failures
are covered by passing independent regressions, but the final proxy integration
run exposed an intermittent CSV replacement/adoption failure. Do not treat the
full runner alone as clean approval of the provider. See the finding below.

This reviews Claude's second-round corrections, not the older `a1b4286e`
candidate. The candidate hash was independently checked:
`47fe25fcc8187699ecb28a9d14df55d56238d077c1166d79c6a55f087418e398` at
`out/qa/current/session9/bin/gimuserverw.exe`. No C++ source under gimuserver or
standalone_frontend had a modification time later than that binary. This is
provenance support, not a reproducible-build proof; QC tests the existing build.

## Scope reviewed

- `Transactions.hpp`: explicit COMMIT before a success reply, failure rollback
  through Drogon's statement-error path, recovery of an abandoned transaction.
- Call-site migration across gameplay handlers, Warehouse helpers and the debug
  grant command. The `dbwedge` diagnostic and source audit support the regression.
- Broker read retries for sharing/lock/access errors and watcher preservation
  of records it temporarily cannot inspect, with bounded rechecks.
- Updated contention and broker regression cases, handoff and C1 research plan.
- Preserved package-label/version, PowerShell quoting and save-dependent
  download wording from the earlier QC.

No live save repair, normal-server deployment, client reset, commit or push was
performed during this QC. Tests use isolated save copies. The normal installed
client and the candidate package remain separate.

## Verification

The independent full runner completed: **49 entries, 1,900 passed checks, zero
failed checks or failed entries**. Of the entries, 48 pass and the mission
validator matches its recorded **3,117 unresolved findings** (H15 842, M2 20,
M3 921, M4 1334). That line is a data baseline, not clean mission validation.
Evidence: `out/qa/current/session9/qc_round2_runner.log` and
`out/qa/current/SUMMARY.md` / `SUMMARY.json`. Parent and packet-generator
whitespace checks pass. The source audit reports 41 sites, zero unchecked
success replies, subject to its lint limitations below.

Independent decisive checks: database contention **21/0**, including refusal
after the 6.5-second reader, no partial costs/product, later success, recovery
of an intentionally abandoned transaction, and restart persistence; broker
**78/0**, including delete-pending reads and watcher preservation; generation
**17/0** and policy **8/0**. The client preflight also reports **0 outstanding
items**, with the reviewed proxy on port 29960 and all 21 fixture files present.

Claude's separate evidence: 15 broker suite repetitions and a 12,800-read stress
run without unexpected 404s. Those are reported counts, not additional
independent runs by this QC.

### Open finding: live provider CSV replacement test is intermittent

The independent `test_content_provider_live.py` run on `47fe25fc` with proxy
test binary `c40131c9` produced **311 passed, 2 failed** (driver 4/1).
`offline-proxy/tests/test_main.cpp:1303` failed to read the edited
`unit_idle_cgs_60956.csv` after a safe point; line 1306 failed to read its
restored original. An immediate fresh-fixture repeat passed **313/0** (driver
5/0), as did one Session 8 control. A passing repeat does not close the finding.
Logs: `session9/qc_round2_provider_live.log`,
`session9/qc_round2_provider_live_repeat.log`, and
`session9/qc_round2_provider_control.log` under `out/qa/current`.

The failed run's server log showed generation 3 created by "digest changed on
read", followed by two requests for the old CSV digest returning 409; after
restoration it similarly returned 409 for the edited digest at generation 4.
The first server log was replaced by the repeat fixture; the failure output
and these inspected observations are retained here. Future reproductions must
preserve both logs before recreating the fixture.

Likely test synchronization gap, not yet proven: `replaceWith` waits only for
`p.HasPending()`, which can describe any generation, rather than confirming the
target CSV's new digest is in the pending index. `SafePoint` adopts the already
polled snapshot; it does not fetch a newer index itself. Distinguish this from
a production adoption bug using deterministic generation/digest observations.
Keep exact-byte/version-binding assertions. Do not fix by adding an arbitrary
sleep, accepting stale bytes, or rerunning until green. Defer visual fault/
replacement acceptance until this is resolved; ordinary persistence checks
can still give useful client feedback now.

## Interpretation and remaining limits

- The corrected long-lock behavior is refusal plus complete rollback, followed
  by a usable connection. A craft whose COMMIT exceeds the timeout should NOT
  receive success. The old ad-hoc QC script's assumption that its first craft
  must be acknowledged is therefore no longer an appropriate acceptance rule;
  the new suite checks accepted and refused actions separately.
- Explicit commits solve the reviewed success-before-commit path. This does
  not prove universal exception safety. Non-database exceptions still require
  caller rollback; the handoff acknowledges destructor-commit fallback. Carry
  a fault-injection/rollback task into the next pass.
- The 41-site audit is textual lint, not a proof of all branches, all writes,
  or response serialization. Avoid the handoff's blanket "safe in every case"
  conclusion.
- Broker retries still block the serving HTTP thread for up to about 280 ms
  per blocked open. Performance acceptance remains open; do not claim improved
  gameplay speed from functional passes.
- A successful repeated suite is evidence against the reproduced replacement
  race, not proof that all filesystem races are impossible.
- The C1 document is a bounded plan. No family expansion, download enqueue
  patch, cache-free login or background-prompt removal is present yet.

## Client and next assignment

### Player acceptance recorded after round-2 QC

The player crafted one Giant Shield, equipped it on Beast Zegar, added Zegar
to the party, and completed Egor Snowfield's "Manipulator of the water".
The client returned to the map rather than Home. After closing and reopening
the client, the player reports that everything remained as left. Mark this
craft/equip/party persistence and mission-return sequence client-confirmed.
This is client-relaunch evidence, not an additional server-restart test or
an exact materials/currency accounting assertion. No new favorite toggle or
SP/summon check was specifically reported in this round.

The collected server log records recipe 2010 x1 at 19:10:51 UTC and mission 31
first clear at 19:14:06 UTC. It also records recipe 1009 x2 separately. No
failed destructor commit or transaction-begin failure matched the inspected
log. Both round2 snapshots are `{}` (zero owned names in sampled LocalState).
The final relaunched-client provider record is active with zero fetches,
zero failures and zero observed tripwire writes. Thus this run is useful
gameplay persistence evidence, not additional proof of asset replacement or
cache precedence. Collect-before-after-snapshot does not invalidate these
endpoint snapshots; no intervening gameplay was reported.

No repetition of this sequence is needed absent relevant code changes.
The intermittent CSV integration finding remains the next Claude priority;
visual replacement/recovery checks remain on hold.

Use [CLIENT_SESSION_9_ROUND2_STEPS.md](CLIENT_SESSION_9_ROUND2_STEPS.md) for the
candidate run. It includes explicit hash selection, a compact gameplay
persistence check, reversible Town replacement, missing-texture recovery, and
evidence collection. User checks use only the disposable fixture.

After the player reports those observations, use
[CLAUDE_SESSION_10.md](CLAUDE_SESSION_10.md). Resolve any observed failures first,
then the bounded C1 slice, keeping the next client checklist proportional to
what actually changed. Leader-skill repair, stale-cache precedence and controlled
performance comparison remain separately prepared checks.
