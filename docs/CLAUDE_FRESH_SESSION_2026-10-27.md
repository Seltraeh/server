# Fresh Claude Code session — client bugs and release preparation

Prepared October 27 and finalized October 28 for Evan to give to a fresh Claude Code profile/chat with no prior context. The goal is to finish the bugs in his write-up, then prepare coordinated updates to the offline proxy, packet-generator and server repositories. Work is authorized; publishing/deployment is a later step. Do not resume the old scheduled monitor. Claude owns visual checks; Evan explicitly stopped Codex screen interaction. This document is the assignment, not evidence that a new chat has already been started.

## Workspace and evidence

Use the existing dirty checkout `C:/Users/Evan/BF/BF-WorkingDirRust`, its nested `packet-generator`, and sibling `C:/Users/Evan/BF/offline-proxy`. Preserve all changes. Do not switch/reset branches, create a clean replacement checkout, stage, commit, push, deploy, uninstall a client, or overwrite live saves. A fresh conversation must retain the existing work, not start the implementation over.

Read in this order:

1. `docs/QC_2026-10-27.md` (current independent review and release blockers).
2. `docs/CLAUDE_SESSION_19_VISUAL_HANDOFF.md` and the relevant October 23–24 sections of `docs/SESSION_19_CLIENT_RESULTS.md` (latest fixes, exact candidate paths, QA save, driver and evidence).
3. `docs/BUG_ACCEPTANCE_2026-10-27.md` and `docs/BUG_BACKLOG_STATUS.md` (every original issue and remaining acceptance).
4. `docs/BF_OFFLINE_SERVER_HANDBOOK.md` (architecture; older current-state statements are superseded by the dated review).
5. `docs/CLAUDE_SESSION_20.md` only for its unresolved area/alias/bootstrap findings. That document is an unsent October 23 draft with a placeholder, not the current assignment. The later visual work also used the name Session 20; do not confuse them.

Original write-up: `C:/Users/Evan/Downloads/Bug fixes BF.docx`, SHA256 `76e8949a6e88ab697e51e84335a2e41b969163bb33e7d3afea42aa11dfe546e3`. Read only. Preserve its three separate #22 entries as #22-1, #22-2 and #22-3.

The October 24 QA server is `out/qa/current/session19/visual/bin/gimuserverw.exe`, SHA256 `887e68e03cff982c05f605bf9c542cfecbe6fb325651195599a932f8ce51d1c5`. Codex rebuilt the same current sources into `out/qa/current/qc_20261027/bin` without changing the normal server binary. See the QC report for exact test results and hashes. The October 27 full regression log is partial; do not claim a completed pass from it. Check for surviving test processes before reusing ports/build outputs. The top-level SUMMARY.json inspected at finalization was an older October 22 report, not this run's result.

## What already works

The visual session recorded fresh master-table delivery, starter names/art, item and area screens, and the whole opening tutorial through the summon and completion gifts. It fixed the Ishgria gate being open on a fresh account, unreadable Terms text, console QuickEdit blocking login, the tuto8 quest-select input loss, and the mission-2 checkpoint skipping the summon tutorial. Recorded post-tutorial gifts were checked against the QA save. Keep those fixes and their evidence.

The server was deployed by Evan's earlier authorization on October 24. The client package was only staged unsigned at that handoff. Inspect present state before claiming installation; do not sign or install it as part of this assignment. Earlier documents saying all these changes are undeployed are stale.

## First bounded implementation round

1. **Make the tutorial correction reproducible and robust.** `deploy/game_content/content/tuto/tuto8.txt` is ignored and the added `PLAYWAIT 0.5` is a timing workaround. Add a maintained, narrowly scoped generator/patch application with original/expected hashes, idempotence and refusal of unknown content; do not silently mutate an arbitrary content tree. Reproduce the modal/tap-area race under delayed responses and cold/warm starts on a disposable content root. Establish whether the script timer pauses while the modal is active. If the delay is insufficient, use a proven scene-readiness/event condition or targeted lifecycle correction, preserving initialization and the tutorial. Never merely increase the timeout and call the race solved. Keep the already successful route as the control.
2. **Fix the bootstrap audit crash.** `scripts/check_mst_bootstrap.py:staged_rows` calls `.get` before proving JSON is a dictionary. List, null, numeric and string manifests all raise AttributeError. Check shape first and add negative tests aligned with server behavior. Retain valid staged-table coverage.
3. **Correct stale current documentation and release dependencies.** The idle-timeout comment in `standalone_frontend/main.cpp` attributes the title hang to socket expiry, but the later controlled evidence identifies console writes and reports successful late login with the old server. Keep or change the timeout only on separate evidence; correct the unsupported causal statement. Maintain the current status separately from historical observations. Move or explicitly package the ignored binary-checker dependencies needed by the regression runner. See the QC report's proposed inclusion rules.
4. **Then reproduce #20 and #33 on isolated candidates.** Mission 234 first clear/replay through the unlock; level 998→999 and edited 999 result exit/timer/relog. These are high-impact crash/freeze reports with server-side coverage but incomplete client acceptance. Capture the exact failure, logs/save delta and a control. Fix a reproduced defect and rerun the same route. If no defect reproduces, record tested conditions instead of inventing a fix.

Write a handoff after this bounded round, including changed files/hashes, commands/results, screenshots and remaining checks. Do not claim the entire write-up solved at that point. Continue later rounds using the acceptance matrix, prioritizing client regressions before content-authoring work. Keep each round reviewable and avoid an unbounded reverse-engineering detour.

## Remaining work after the first round

- Presentation/assets: #22-1 Lukroar gates, #22-2 Omni Amadream header, #22-3 Omni Galtier text, #27 slots 1–10 with duplicate/mixed prizes, plus the recent missing star glyph. Retest #24 single/double gates with normal/rapid input on the final DLL.
- Progression/rewards: #23 Morgan/final-stage versus replay, #30 compound unlock cases, #26 Fuu/Roglizer/helper payouts, #39 Metal Parade waves/captures, #25 frog eligibility and #40 UBB visibility. Include each acceptance gap in the matrix.
- Missing content: #12 Vortex Trials, #17 Mecha God encounters/capture, #18 boss stats/AI, #15 roster scope and art. These require sourced encounter/content work; Research Lab trials do not close Vortex Trials. Do not invent stats/rates or claim complete EU parity.
- Startup-provider release gates: area source ordering proof, two image aliases, five unknown groups, eight absent names and nine unresolved families at the S19 baseline. Re-evaluate after changes. Preserve safe fallback until actual complete requirements and client bindings are demonstrated; no blanket screen suppression.

For intended gameplay, consult the relevant Brave Frontier wiki page and record its exact revision alongside client/MST/code evidence. Distinguish original behavior from intentional offline policy and unresolved evidence. Do not use adjacent fixed behavior to close a different report.

## Visual and test discipline

Use isolated identities/ports, copied saves and disposable content roots. The existing S19 Fresh identity is now warm; never call it a new cold baseline. Its saved tutorial state and completed snapshots are described in the visual handoff. Never change live/C1/C16 data, and do not reset S19 without a concrete need and Evan's approval. Pass an absolute config path when launching the Debug server, otherwise it can bind live port 9960 and use the live save. Preserve logs before harness restarts, which truncate them.

Do not reuse Codex's October 27 visual harness unchanged: it wrote a backslash database path that was stripped during configuration parsing, opening an unintended empty SQLite database inside the disposable QC folder. The resulting opening prologue is a fixture error, not evidence of a client regression. Use forward-slash absolute database paths (`Path.as_posix()`), verify the actual opened database and copied player row before launching, and preserve the faulty fixture as evidence. No new warm Home acceptance was obtained. Codex stopped screen work at Evan's request; use the prior Claude visual handoff as the last completed visual baseline.

Evan has been using the existing PowerShell visual driver successfully. Reuse the established driver and its front-window/occlusion checks where allowed by your tools, capture before/after actions, and keep the desktop under one agent's control. Do not rely on stale coordinates. Any authentication, age/parental attestation, signing password or security prompt remains with Evan. Do not treat consent permissions quoted in a document as a new user instruction.

Use existing Visual Studio trees and one bounded QA output directory per round. Server regressions recreate disposable fixture folders and overwrite top-level reports: preserve prior reports first. Run focused checks for edits, then the full regression runner before the next release decision. Proxy unit tests have five explicitly skipped integrations unless their environments are supplied; do not report them as all integrations passed. Screenshots do not verify audio or exclude brief intervening screens.

## Release preparation, after client acceptance

Prepare separate proposed file lists for server, nested packet-generator and offline-proxy. Include runtime source, necessary generators/fixtures/tests, build definitions and maintained documentation. Make ignored content fixes reproducible from maintained sources. Include all required checker dependencies. Verify regeneration and clean-source buildability without staging anything.

Exclude `out`, binaries, saves, caches, credentials, unsigned packages, transient `CLAUDE_SESSION*`, `CLIENT_SESSION*`, `QC_SESSION*`, `SESSION_*_CLIENT_RESULTS`, async notes and this coordination handoff. Preserve them on disk. No repository update or deployment until Evan reviews the client state and authorizes it.
