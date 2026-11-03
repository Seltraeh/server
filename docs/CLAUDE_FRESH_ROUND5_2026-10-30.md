# Fresh Claude session round 5

Prepared October 30 2026 after independent QC of round 4. Evan asks to tackle all three #22 reports and resource/SP/UBB cases, while defining the remaining content scope. Work in C:/Users/Evan/BF/BF-WorkingDirRust, its nested packet-generator, and C:/Users/Evan/BF/offline-proxy. Preserve dirty work and prior evidence. No stage, commit, push, deployment, signing/install/reset, reboot or live save/content writes. Claude owns visuals; no scheduled monitor is active.

## Recover context

Read docs/QC_FRESH_ROUND4_2026-10-30.md first, then docs/CLAUDE_FRESH_ROUND4_HANDOFF_2026-10-30.md (including Super addendum), docs/BUG_ACCEPTANCE_2026-10-27.md, scripts/QA_RUN_HELPERS.md and the handbook's current dated section. Older session directives are history. The Word tracker has October 28 notes; the matrix is newer.

Runtime candidate: out/qa/current/fresh_round4_20261030/bin/gimuserverw.exe, SHA256 c28a8db9efacc43d5c5f4bf707d8036520f8e8df7e69e3438ba6db1e67e532fe. S19 test DLL: 9633061d904203c7110c375a9e5309543d88eef2f22da92c565a443c86852b0e. Round-4 portrait root: fresh_round4_20261030/portrait/stage_root (v1087); Super archive: fresh_round4_20261030/archive_super_boss. Use explicit roots and record their hashes; these are isolated candidates, not deployed state.

Capture wire correction, reproducible portrait and new Super formation are implemented. Mega EXP stays 5000 and capture odds unchanged per recorded decision. Codex independently passed helper 57, portrait 27, parade-generator 11, capture 8 and Super wire 18 checks. The later 66-entry full aggregate had one fixture-lock error, not a clean pass; affected suite passed separately. Preserve that history.

## Phase 0 Correct the QC defect first

Fix unit_mst_corrections.propose's predictable .tmp write-through. See QC and out/qa/current/review_round4_20261030/temp_probe.py. Use exclusive temporary creation, no following existing links, and cleanup only owned files. Test pre-existing ordinary/hard-linked temp targets and dangling links, preserving original bytes. Audit replace_into for the same invariant. Retain source/deploy immutability and deterministic accepted output hashes. Run focused correction tests before using this tool for new candidates.

## Phase 1 Three separate presentation reports

- #22-1 Lukroar: establish the exact affected unit/form and summon path from local evidence. Compare its normal/special gate with a known-good control; record asset requests, missing/loaded assets and animation/input sequence. Distinguish absent gate art from the already-fixed #24 white overlay. Diagnose and fix a reproduced fault, then rerun with a fresh disposable asset state where practical without resetting existing identities.
- #22-2 Omni Amadream 51337: examine the unit overview header, including name, number, rarity and light element icon. Compare a long-name Omni at the same supported resolution. Fix overlapping geometry or data only when the cause is demonstrated; retain normal-name controls and verify supported layouts.
- #22-3 Omni Galtier 860428: verify the complete intended leader text and its wrap/scroll behavior against Durumn as a control. Determine whether truncation is in dictionary/MST data, width, line limit or client rendering. Do not shorten away gameplay information to make it fit. Capture complete text before/after.

Use minimal maintained corrections/generators or narrowly justified client changes; no unexplained binary patch or output-only fix. Regenerate relevant staged content/versioning with collision protection and preserve accepted unrelated crops/strings.

## Phase 2 Resource and skill cases

- #26 Roglizer/Fuu: cite exact relevant wiki page/revision and inspect their current leader-skill data/consumer. Separate EXP, Zel, Karma, item/resource effects and helper effects. Run no-leader, each leader, and applicable helper controls with deterministic rewards on copied saves. Compare displayed amounts, server calculation and stored deltas, including rounding and stacking where relevant. Fix verified omissions; do not assume every resource receives the same bonus.
- #25 Omni frog/SP eligibility: identify actual permitted fodder and requirements from client rules plus sourced behavior. Test migrated and newly prepared copies below/at level cap, eligible/ineligible fodder and SP limits. A greyed-out frog is not automatically a bug. Correct an inconsistency or document an evidence-backed not-applicable disposition; do not force illegal fusion through the server.
- #40 evolved UBB: choose an applicable documented evolution chain with BB/SBB/UBB controls. Verify resulting identifiers and levels, UI visibility, unlock conditions and actual availability where feasible. Preserve prior Berdette BB/SBB acceptance. Verify relog and no leakage of prior-form skills. Mark any unavailable battle activation check explicitly.

Diagnose before modifying. If an issue cannot reproduce, list tested conditions and missing inputs; no invented fix. Prioritize real failures over filling a checklist. If the round grows too large, finish and verify the active fix, then hand off exact remaining targets rather than making unreviewed broad changes.

## Phase 3 Define remaining content scope without authoring it yet

Create maintained docs/REMAINING_CONTENT_SCOPE.md covering #12 Vortex Trials, #15 missing EU/special units, #17 Fire Mecha God and #18 placeholder bosses. Inventory concrete missing entries from the write-up/current data; distinguish present-but-broken from absent and do not infer an unlimited roster requirement. For each item record identifiers, current assets/data/behavior, evidence sources and revision, missing encounter/AI/reward/unlock/art/evolution pieces, dependencies, acceptance route and unknowns. Recommend small ordered implementation batches. Where scope requires Evan's choice, present a specific decision and its consequences; continue independent work meanwhile. No invented stats, rates or original-game parity.

## Tests, isolation and handoff

Use one new round-5 QA folder, existing Visual Studio trees, maintained helpers, copied saves and disposable roots. Inspect activity/ports; stale pid 11336 held the old metal_parades fixture at last review, so choose fresh names and verify identity before any cleanup. Do not reboot or kill by name. Use forward-slash absolute roots and process-owned startup/stop. Preserve logs before restart. S19 is warm; track which MST version it caches. No age/parental attestations, authentication or signing automation. Screen/audio limits must be explicit.

For actual edits, run focused tests, rebuild only needed candidates, repeat failing client routes, then a full relevant regression with fresh fixture paths and old reports preserved. Do not claim a suite with harness errors passed. If unchanged runtime evidence is reused, label it as historical. Maintain exact source/binary/data hashes and control fixtures. Gameplay claims require precise sources plus client/code evidence.

Deliver docs/CLAUDE_FRESH_ROUND5_HANDOFF_2026-10-30.md, updated acceptance matrix and the content-scope document. Include issue-to-file mapping, implemented/automated-tested/client-observed/deployed distinctions, failures/skips, remaining decisions and next bounded recommendation. Do not claim the Word tracker updated unless actually edited and checked.

Release gates remain startup completeness, corrected tuto8 packaging enforcement, missing content and explicit disposition of every original report. No publication/deployment this round. Durable runtime/generator/test/docs work stays outside out; output, saves, binaries, credentials and transient handoffs remain excluded from eventual source inclusion lists without deletion.
