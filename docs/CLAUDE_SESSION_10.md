# Claude Session 10: respond to client acceptance, then bounded offline C1

Read the current handbook, `QC_SESSION_9_ROUND2_REVIEW.md`,
`CLIENT_SESSION_9_ROUND2_STEPS.md`, the player's results, and
`OFFLINE_CONTENT_PHASE_C1_PLAN.md`. The latter is research, not proof that the
Windows patch sites or full coverage are already established.

## First: incorporate the player's client results

Latest player feedback is now recorded in the round-2 QC review: one Giant
Shield crafted and equipped on Beast Zegar, Zegar added to the party, Egor
Snowfield "Manipulator of the water" completed, returned to the map, and
state persisted across client relaunch. Both round2 snapshots contain zero
owned cached names. Preserve these as confirmed; do not ask for repetition
unless subsequent changes affect them. The asset fault tests were not run.

Before provider expansion or asking for visual fault tests, resolve the open
integration finding in the round-2 QC review. The full server runner passed
1,900 checks, but the live proxy test failed edited/restored CSV assertions
at `offline-proxy/tests/test_main.cpp:1303,1306` (311/2); one repeat and one
Session 8 control passed 313/0. Preserve server and proxy logs per reproduction.
Instrument target digest/generation readiness around `replaceWith`, PollOnce
and SafePoint. `HasPending()` alone may be satisfied by an unrelated generation.
Prove whether this is test synchronization or production adoption, fix the
correct layer, and add a deterministic regression while retaining byte-exact
and pinned-version assertions. An arbitrary delay or rerun-until-pass is not a
resolution. Only then release client checklist sections 3 and 4.

Record actual outcomes separately from wire tests. Prioritize any new crash,
lost/duplicated gameplay, failure to adopt the Town edit, or failure to recover
after restoring the missing fixture texture. Reproduce on isolated copies and
add a relevant regression before expanding provider coverage. An asset still
visible while missing is inconclusive until its actual source is established.
Preserve the confirmed Town/Zeal baseline and prior sphere/navigation results.

Review the database residual explicitly: a non-database exception after writes
can still enter Drogon's destructor-commit path unless the caller rolls back.
Connection healing alone does not establish exception atomicity. Add a bounded
fault-injection test and fix any path that commits partial gameplay after a
failed request. Do not claim every database write is safe merely because the
41-site textual audit passes; it is a lint, not control-flow proof.

## Then: implement the first C1 slice

1. Confirm x86 enqueue function signatures, callers, ownership/lifetime, and
   masked-signature/PE-identity guards. ARM64 evidence establishes semantics,
   not executable Windows patch sites. Preserve guarded failure behavior.
2. Add measured family coverage for `unit/img`, `unit/cgg`, `unit/cgs`; only add
   item coverage after the unit slice passes. Check generated names against
   authoritative MSTs, content availability, duplicate basenames and aliases.
   Report absent names instead of silently claiming ownership.
3. Filter only ready, provider-owned resources from download enqueue paths.
   Keep legacy behavior for everything outside the verified scope. Exercise
   provider-disabled/startup-failure cases so suppression cannot strand loads.
4. Measure startup indexing, memory, delta size and repeated poll overhead at
   realistic family scale; do not extrapolate the 21-name baseline.
5. Provide isolated fixtures for different rosters, newly acquired units and
   archive first visits. Prepare a clearly named fresh test-package identity
   only when needed for empty-cache acceptance; reuse the source assets and
   existing build tree. Preserve both existing clients and their settings.

Out of scope: hiding LoginScene/DownloadScene, changing the DLC prompt or its
stored answer, MST version flow, audio/voice/scripts/bundles, and deleting
LocalState. Document remaining downloads honestly. The existing test package's
stored background-DLC answer may be Yes; user intent to choose No is not proof
that the stored setting changed. Test both paths when a fresh fixture is ready.

## Remaining client preparation

Prepare leader-skill before/after comparison on a copied save, including an
owned helper with a measurable HP leader skill. Provide a reproducible stale
cache fixture and comparable broker-on/off timing run. The current harness
recopies the normal save every start: do not call that server persistence.
Either preserve a dedicated fixture explicitly or keep the server running
when checking client-only restarts. Avoid asking the player to repeat passed
checks unless a code change affects them.

## Workflow and deliverables

- Inspect dirty state in server, packet-generator and offline-proxy. Preserve
  all existing changes, including test-package label/version, quoted loopback
  commands, and save-dependent progress text.
- Use the established shared build configuration and one current QA directory.
  Restore temporary output overrides; record candidate hashes. Keep all source
  changes in their current repositories. No extra checkouts or artifact sprawl.
- Run targeted tests, the appropriate shared regressions for changed gameplay,
  and proxy/provider integration checks. Keep the 3,117 mission findings labeled
  as unresolved baseline data, not a clean validation. Do not weaken assertions.
- Update one handoff with reviewed files, evidence, limitations, hashes and
  concise client steps. Link the existing C1 plan rather than duplicating it.
- Do not deploy, modify the live save, reset existing clients, commit or push
  during this assignment. Subsequent publication must handle packet-generator
  first, exclude out/, and audit audit-campaign against mine/main.
