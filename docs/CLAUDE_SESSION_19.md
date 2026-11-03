# Claude Session 19 — two exact startup lists and a fresh-install bootstrap contract

October 22, 2026. Work in `C:/Users/Evan/BF/BF-WorkingDirRust` and the existing
`C:/Users/Evan/BF/offline-proxy` checkout. This is a bounded implementation phase,
not authorization to reverse the whole game or declare startup-screen removal.

## Read first and establish the baseline

Read `docs/BF_OFFLINE_SERVER_HANDBOOK.md`, `docs/CLAUDE_SESSION_18_HANDOFF.md`
**including correction §10**, `docs/OFFLINE_STARTUP_TRANSITION_PLAN.md` §9,
`docs/ASYNC_REVERSE_PROGRESS.md`, and the final Session 18 client results if present.
Inspect both checkouts' status, diffs and existing QA outputs before editing.
Preserve all dirty work, live data, C1, and C16's package, registration, save,
consent state and collected evidence. C16 is warm, not a fresh-install baseline.

The corrected server candidate is
`out/qa/current/session18/bin_correction/gimuserverw.exe`, SHA-256
`4eded167851c4b30eb0ae5bf23833a2b08bb6adb873c0d8c35ba2a9ba8e19856`.
Independent QC reviewed hashing of the exact parsed MST bytes and the immutable
response snapshot, and reran the server-table regression (28/0) and manifest suite
(93/0). The unchanged DLL previously passed 677/0 (five integration skips) and
live startup tests 17/0. Verify identities before relying on these results.
The pre-correction server `995a088d…` is retained evidence, not the candidate.

The clean registered warm C16 route completed Home/Selena, mission 10 with Stein,
results and Mistral map return. See SESSION_18_CLIENT_RESULTS.md for collected
evidence and screenshot/audio limitations. Five route layouts were provider-served. Neither that launch nor complete
fallback visuals would establish fresh-install or screen-removal acceptance.

Current real coverage remains incomplete: six unknown lists, five absent names,
nine unresolved families. Census and prompt remain stock. Correction §10 binds
the manifest to the six tables actually parsed and served by the server; do not
replace that with a filesystem hash taken later or an offline-only check.

## Objective and bounded scope

Implement exact derivation for **item and area**, two of the six remaining list
groups. In parallel within your own work, produce the concrete version-table
bootstrap contract needed for a genuinely fresh install. Keep unit/initial units,
weapon/summoner_img, gacha/gacha_effect and effect/effect_group explicitly unknown.
Do not expand into those four implementations during this session. If either
selected group proves blocked, finish the other and document the precise missing
proof; do not substitute folder enumeration or quietly broaden scope.

### 1. Exact item and area requirements

- Trace the supported x86 client (`6253F827-01756000-00ED273D`) from
  `LoginScene::createDownloadCheckList` / `updateEvent` through the actual
  `GameScene::request*Files` consumer helpers. Record executable hash, RVAs,
  category keys, MST identifiers and fields, filters, defaults, splits, ranges,
  formatting, and nested calls. Use arm64 semantics only as corroboration of the
  supported x86 behavior. Extend a reproducible binary-fact check.
- Determine the authoritative source at each call: client LocalState table,
  package data, Initialize/UserInfo response, or another response. Include every
  source that changes the requested set, including lookup tables and overrides.
  A field read from a different table is another input, even if today's rows happen
  to produce the same names. Explain how initial/version-refresh state affects it.
- Derive the names the consumer requests whether or not files exist. Preserve
  dependency closure, collisions, client exclusions and all unresolved groups.
  Trace-backed requirements and existing provider-off queue evidence are distinct
  checks: one observed route cannot prove exhaustive category requirements.
- Bind every newly used client input through the existing DLL-verified contract.
  Extend server binding for every newly used response source to the exact bytes
  parsed into the served snapshot. If that response is synthesized or mutable,
  prove a suitable snapshot/lifetime contract or leave the group unknown; never
  pretend a startup disk digest binds later changing response rows.

### 2. Content-gap decisions, limited to this work

For item/area requirements and any newly exposed dependency, record required,
available, absent, unowned and ambiguous names with the actual consumer trail.
Use verified source assets where a uniquely correct copy is available. A definition
correction needs evidence of the intended filename and compatibility with its
reader; a similar basename or visually plausible substitute is insufficient.
Preserve the known five absent names and nine families unless a specific gap is
resolved with that evidence. Do not remove requirements merely because today's
content tree lacks them, exclude an entire category, fabricate assets, or claim
ownership without dependency closure. Leave uncertain content decisions explicit
and reviewable. Broad content cleanup is outside this phase.

### 3. Fresh-install version-table bootstrap: executable contract, not a fake first run

Inventory the actual client version/load channel and server delivery path. Reconcile
the earlier count of 147 client-kept tables versus 21 staged tables against current
code/data, rather than treating those historical counts as a specification.
Record each required table's client key, response/download endpoint, version,
encoding/container, authoritative source and destination/part naming. Distinguish
required non-empty tables from legitimately empty tables using consumer evidence.
Include dictionaries, multipart tables, initial-unit initialization, corruption
checks, reload/saveVersion ordering and version changes where relevant.

Add a durable offline/server test or audit command that identifies exactly which
required bootstrap deliveries are available and which are missing, wrong-version,
malformed or incomplete. Test an empty isolated client-state fixture; do not copy
the installed user's versioned tables or account state to make it pass. Include
an interrupted/partial delivery and a version-change case. This session may fix a
narrow, evidenced server mapping/delivery defect using existing authoritative data
if it can be tested without a client install; do not fabricate missing MST rows or
force corruption/version checks to succeed. Record remaining delivery blockers and
the smallest next implementation step. An audit passing is not a client first-run
pass, and true first-launch visual validation remains a later isolated task.

## Verification and negative controls

Keep new tests and reverse-engineering checks in durable `scripts/`, `tools/` or
the existing proxy test suite, registered with the applicable regression runner.
QA-only scripts may orchestrate them but must not be their sole implementation.

- For each derived group: empty and populated rows, alternate branches, malformed
  required input, actual formatted names and dependencies, and a required asset
  removed before regenerating both policy and manifest. The absent requirement
  must survive regeneration and coverage must remain incomplete.
- Test every newly introduced client/server source with a stale binding, omitted
  binding and changed rows that produce an additional required name. Exercise the
  runtime server report and client decision as applicable, not only offline tools.
  Client extra versions/parts and wrong build must still reject. Server edits after
  startup must retain the immutable served snapshot; restart must rebind/reject.
- Negative controls must catch a deliberately wrong naming/filter rule, ignored
  new source digest, and dropped missing dependency. Keep controls isolated and
  clearly marked; restore source byte-for-byte and rebuild the candidate after any
  temporary mutation. Never leave a mutant as the reported candidate.
- Run focused derivation/binary/binding tests, relevant wire and live harness
  suites, and the server regression runner for changed server behavior. If DLL
  code changes, run its unit and relevant integration/site checks. Retain existing
  fail-closed, hook-signature, gate-flash, audio-reader and post-quest guarantees.
  Explain unchanged checks reused from Session 18 and disclose skips/failures.

## Boundaries and stop criteria

No blanket cache/download/verifying-screen suppression, forced queue completion,
scene jumps or fabricated completeness. FILE_CHECK, dictionaries, master-table
versions/corruption checks, login/save/account initialization and legitimate error
paths must continue. Do not enable a real bypass while any proof or content gap
remains. A lower unknown count is progress, not gameplay parity or screen removal.

The consent/online-service screen issue is separately deferred following the
earlier classifier refusal. Do not revisit it, request a bypass, alter consent or
attestation state, or automate its UI in this session.

No UI/client launch, package swap/reset/install, deployment, staging, commit, push,
live-save mutation or unrelated cleanup. Reuse existing build trees and one
`out/qa/current/session19/` folder; preserve existing outputs and Session 18
evidence. Build isolated candidates without overwriting live executables or C1/C16.

Stop this phase after the two selected groups, bootstrap contract/audit, appropriate
checks and handoff. Stop the affected change earlier on unsupported binary evidence,
unbound mutable data, missing authoritative content or a regression that cannot be
resolved within this scope. Preserve a tested partial result and exact blocker;
do not expand into the four other groups or weaken acceptance to finish.

## Deliverables

Write `docs/CLAUDE_SESSION_19_HANDOFF.md` with changed files, binary/MST evidence,
before/after coverage counts, every new bound source, bootstrap inventory/results,
remaining blockers and next bounded action. Distinguish implemented,
automated-tested, client-observed and deployed (none). Give runnable commands,
test outcomes/skips and full final SHA-256 hashes for candidate EXE/PDB, DLL/PDB if
changed, test executables, policy, manifest, changed sources and durable fixtures.
Hash final rebuilt artifacts, and verify live/C1/C16 identities remain unchanged.

Update the technical startup plan with settled findings, preserving historical
results. Prepare a proposed release file list including durable code/tests/tools
and needed technical documentation; exclude `out/` and transient session, QC,
checklist and coordination documents from the proposed commit while preserving
all of them on disk. No actual staging or commit. Supply a later isolated-client
checklist only if new behavior merits one; do not claim it was executed.

