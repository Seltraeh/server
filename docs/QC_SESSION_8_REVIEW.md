# Session 8 QC — October 4, 2026

Latest player acceptance: fresh identical spheres display and equip correctly;
the selected favorite and equipment persist; sale removes only selected copies.
Issue #34 is player-confirmed fixed for this workflow. This supersedes the
sphere acceptance uncertainties recorded below. The harness readiness finding
and separate client-provider acceptance remain outstanding.

## Scope and conclusion

Reviewed ItemMix's authoritative warehouse reply, KDL response fields, updated
consumer tests, candidate provenance and client-acceptance harness. No new
blocker identified in the sphere response change. The actual Windows rendering
proof remains separate from the modeled consumer tests.

Player confirms existing crafted spheres are visible and missions no longer
return to Home. Normal server is still SHA-256 fb77bf6fcf6a09329c087a8f92e2ae89fe108bf681ca371a633c448a8de70db5;
the Session 8 candidate is a2022e914a0146a749c81a5ddcc9e1401df181bf846dc11d6468cd1ab2ab9024.
Both were independently hashed. An old-build login refresh also gives existing
copies unique IDs, so do not infer a fresh-craft candidate pass from the report.
The mission navigation confirmation is accepted as reported; first-clear
next-area presentation remains its own check.

## P2 — harness can announce a server that never became ready

In scripts/content_provider_client_test.py, cmd_server exits its 180-second
startup loop when time expires, then unconditionally prints that the child is
listening. The child can still be alive without a listening socket. Moreover,
port_busy() accepts any listener; it does not establish child ownership before
printing success. The later duplicate-listener warning does not correct the
initial false readiness announcement.

Before using the harness, require exclusive child ownership and a successful
expected endpoint response. Timeout/foreign listener must fail with the log path
and cleanup of only owned processes. Add mocked startup/cleanup regression cases.
This is a static control-flow finding; no intentional live-port failure was
injected into the player's environment. Assigned first in CLAUDE_SESSION_9.md.

## Validation

Targeted tests use the Session 8 QA candidate and isolated saves. See the
verification results appended below. Parent/submodule whitespace checks passed.
The full 45-entry/1,773-check runner and unchanged provider integration results
remain Claude's Session 8 evidence, not independently repeated in this review.
No production source, live save, client package, content or runtime policy was
changed. No commit, push or deployment performed.

## Remaining limits

Fresh-batch display before reconnect; independent copy favorite/equip/sale;
candidate identification during client play; actual provider hooks/rendering,
missing-file recovery and frame pacing remain acceptance gates. ItemMix has no
retry nonce and a lost reply can cause another paid craft; the handoff documents
this rather than claiming deduplication. The provider still covers Town/Zeal
only and leaves legacy downloads/synchronous reads for Phase C.

## Independently rerun results

| Suite | Passed | Failed |
|---|---:|---:|
| test_craft_row_identity_wire.py | 53 | 0 |
| test_sphere_equip_format_wire.py | 37 | 0 |
| test_warehouse_consumer_model_wire.py | 25 | 0 |
| test_warehouse_placeholder_wire.py | 41 | 0 |
| test_warehouse_integration_wire.py | 19 | 0 |

175 passing assertions across these runs. Logs are
out/qa/current/session8/qc_{craft,equip,consumer,placeholder,integration}.log.
This does not substitute for actual rendering or a full gameplay runner.
