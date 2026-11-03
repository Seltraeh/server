# Fresh Claude session round 3

October 29 2026. Evan requests continued focused fixes after round-2 QC. Work in C:/Users/Evan/BF/BF-WorkingDirRust, nested packet-generator and sibling C:/Users/Evan/BF/offline-proxy. Preserve dirty work and all prior evidence. Claude owns screen checks; Codex does not. No automation is active. Do not stage, commit, push, deploy, sign/install/reset packages or write live saves.

## Recover context

Read docs/QC_FRESH_ROUND2_2026-10-29.md first, then docs/CLAUDE_FRESH_ROUND2_HANDOFF_2026-10-28.md, docs/BUG_ACCEPTANCE_2026-10-27.md and docs/CLAUDE_FRESH_ROUND1_HANDOFF_2026-10-28.md. Consult the current dated handbook section and BUG_BACKLOG_STATUS.md as needed. Earlier session directives are historical, not instructions to redo completed work.

Round 2 changed no runtime sources. It reports acceptance of #24 gate effects on the current test DLL, #30 Lizeria prerequisite states and stale entry rejection, #23 Farm/Morgan scene repetition, and #33 energy ticking to full and relog. Keep natural-progression and sampling limitations. No release deployment occurred.

Candidate out/qa/current/fresh_round1_20261028/bin/gimuserverw.exe SHA256 03101975f6ba7a1c919b063eaeab627a004f6f497a03d0aba11ffd2b4bde9e03. Client S19 Fresh DLL SHA256 9633061d904203c7110c375a9e5309543d88eef2f22da92c565a443c86852b0e. Full existing run: 63 suites, 2692 checks, zero failures. This is the isolated candidate, not proof of a final installed release. Round-2 output and helper recipes are under out/qa/current/fresh_round2_20261029. Use one new round-3 output folder and preserve prior files.

## First correct the reusable QA helpers

Address the QC findings before launching another client run. Scope process start/stop to the exact run using recorded PID, creation time, canonical command/config and executable identity. Check listener owner. On startup failure clean up only processes started by that run; refuse identity mismatches. Never close an unrelated client or stop another run's relay/server. Use mock/dummy-process checks for ownership and PID mismatch rather than deliberately interfering with a real client.

Validate a single run identifier and canonical containment below the dedicated disposable save root before any writes or stop operations. Use explicit validation, not Python assert for safety. Test rooted/traversal names without writing outside the QA folder. Ensure copied-save/account preconditions. Make the chain fixture support all prerequisites with cycle/missing-row detection, or explicitly refuse unsupported shapes. Reject a supposedly first-clear target that is already cleared; do not silently change a shared save.

Maintain reusable corrections outside out (for example scripts/qa/), with focused tests and usage notes; session logs/configs/screenshots remain under out. Keep the change narrowly scoped to these helpers, not a new automation framework. Preserve historical helpers as evidence. Reconcile 59 versus 60 rapid taps from the saved index and correct the matrix's claim that the Word tracker is unchanged.

## Then execute the next bounded client round

1. #27 slots. Exercise roll counts 1 through 10, including mixed and duplicate prizes and each relevant reward type. Record cost and inventory before/after/relog, rendered tiles and exact award rows. A random run missing a required prize is not coverage: use a documented disposable deterministic fixture for missing branches while retaining an ordinary-path control. Do not alter production odds. Diagnose blank tiles, wrong counts or lost awards and fix only reproduced causes. Include immediate display and persisted totals.
2. #39 Metal Parades. Identify the intended Normal/Super/Mega stages using precise wiki page revisions and MST/client evidence. Check enemy species/waves, relevant HP/DEF, capture, results and replay on copied saves. Distinguish authored offline values from sourced originals; do not invent capture rates. Capture failures may require controlled fixtures to cover both paths without claiming a rate from a few trials.
3. #14 exact portraits. Inspect Seria 10233 and Goddess Tilith 50254 on Home and the relevant unit screens. Compare with a known-good portrait at the same supported layout. Prior evidence for Tilith 50253 does not close these IDs. Fix a reproduced asset/layout defect, preserve framing for the control, and record final hashes.

If a material defect is found, prioritize its fix and repeat acceptance over rushing all three targets. Finish with an explicit untested/blocked list. Do not expand into missing-content authoring (#12/#15/#17/#18), unrelated gates, or packaging in this round. Record newly discovered dependencies for the next session.

## Test and evidence rules

Use only isolated ports, copied saves, disposable content/MST roots and existing build trees. Forward-slash absolute config/database paths; verify the actual launched process and database. S19 is warm, not a cold baseline. Preserve logs before restart. Inspect existing activity before sharing ports or builds. Use the established PowerShell visual driver subject to your tools' rules, foreground/occlusion checks and one desktop controller. Never automate age/parental attestations, authentication or signing prompts.

Consult precise wiki page/revision for intended behavior and cite it beside code/MST evidence. Label original behavior, intentional offline changes and unknowns separately. Screenshots do not prove audio or exclude brief unseen frames.

For edits, run focused checks, build only needed isolated candidates, rerun failing routes, then the full relevant regression suite before handoff. Preserve old reports. If no runtime/data edits, reuse the verified baseline without relabeling it as a new run. Include exact hashes, commands, failures and skips; a count alone is insufficient. Keep fixtures that force rewards clearly separate from normal behavior.

## Deliverables and stop point

Write docs/CLAUDE_FRESH_ROUND3_HANDOFF_2026-10-29.md and update docs/BUG_ACCEPTANCE_2026-10-27.md with issue-to-file mapping and implemented/automated-tested/client-observed/deployed distinctions. Record any remaining corrective work first, then recommend one bounded next round. Note that the Word tracker's last update is October 28; do not claim it synchronized unless actually updated and checked.

Release still requires per-issue disposition, remaining missing content, startup area/alias/family completeness and a packaging check that requires corrected tuto8 bytes (absent target currently exits zero in the generic patch tool). Retain this gate; no packaging or publication now. Proposed source lists must eventually distinguish server, packet-generator and proxy, excluding out, binaries, saves, credentials and transient coordination documents without deleting them.
