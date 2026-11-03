# Efficient evidence-driven bug-fix batches

Applies to the offline client/server work. User authorization and current session scope take precedence. Claude owns screen interaction; Codex performs independent source/test review. This workflow reduces repeated setup and output, not evidence quality.

## One contract, several related fixes

Group 2–4 defects sharing a data consumer, screen or reward path. Before coding, write one compact contract table: report; expected behavior/source revision; client RVA or parser; required field/type/order/default; producer; acceptance case. Recover the consumer's actual shape first. Distinguish data omission, encoder mismatch, runtime logic and presentation. A plausible packet shape is not proof until consumed successfully.

For gameplay, use precise wiki revisions and client/MST evidence. For pixels, use layout geometry, binary anchoring and a matched visual control. Label missing values as unknown; do not invent parity. Stop research when the bounded contract is sufficient. Do not reverse unrelated systems.

Prefer correcting the existing producer or maintained generator over a second runtime override. A necessary binary hook must have a supported-build guard, verified ABI/ownership, failure policy, and bounded lifecycle. Share helpers only where there is actual reuse or a test boundary. Keep experiments under the round's QA root, successful reusable logic/tests in maintained sources. Do not accumulate fallback implementations, per-unit hacks or permanent verbose logs. Never delete unrelated work; remove only your own superseded implementation after confirming its replacement and preserving evidence.

## Test by dependency and risk

1. Validate copied fixtures before launch: unit_lvl/total_exp consistency, skills, ownership, materials, content versions, candidate hashes and isolated ports. Reuse established harness commands. Promote one reusable validator if needed; do not create a framework for one fixture.
2. Make the related batch, then run focused contract/refusal/persistence tests and build once. For native hooks, test write/restore failure states and ownership as well as normal arithmetic. Log commands, exit status and artifact hashes in one batch manifest.
3. Launch once for a connected route covering the changed behavior and a representative unchanged control. Reuse an existing warm fixture honestly. Use a fresh-cache run only when bootstrap/cache invalidation changes require it. Save setup checkpoints for reuse.
4. Reopen an accepted route only when its runtime/data dependencies changed, a new report contradicts it, or final integration needs it. SP and UBB need not be replayed every round for a text-only change. Keep their evidence tied to the candidate and data hashes.
5. Run the full applicable regression suite at the batch's integration/release-candidate boundary, rather than after every small edit. A source change after that pass requires affected tests again. Do not count skipped tests as passing integration coverage.

## Opportunistic visual review without extra runs

At each existing stable checkpoint, briefly inspect the whole visible scene: text clipping/overlap, icon anchors, enemy/player alignment, bars/counters, missing art, touch feedback, stuck effects and transitions. During attacks, inspect status placement after the effect settles and when it clears. This is a short adjacent check, not a complete game audit.

For a new anomaly, record one useful frame, candidate/data identity, scene/unit, expected versus observed geometry/behavior, reproducibility and severity. The training-arena enemy status icons reported by Evan are an explicit round-8 target. Compare ordinary enemies and different scales before changing a shared offset. Do not dismiss an obvious defect because it was outside the immediate test; do not silently declare a fix based only on a screenshot either.

- Crash, corruption or regression caused by this batch: repair before continuing.
- Reproducible small local presentation defect: trace and fix within the current related batch, with a control.
- Uncertain/broad defect: save actionable evidence and put it in the next bounded batch; avoid an unbounded detour.

No automatic screenshot can prove audio, every transient frame or all resolutions. Record those limits. Human review remains valuable; the goal is to reduce avoidable misses, not promise exhaustive autonomous visual detection.

## Output and context budget

- Read the current QC, current handoff and current scope first; consult history selectively. Keep a compact current-state block instead of repeatedly rewriting narrative history.
- Use rg with bounded context. Parse JSON into selected fields; never dump entire archives, disassemblies, git diffs or test logs into the chat.
- Redirect verbose builds/tests to files. Print exit status, pass/fail/skip totals and a small relevant failure excerpt. Expand only to diagnose a failure.
- Capture at meaningful checkpoints and use small labeled contact sheets. Avoid frame-by-frame tool output or re-opening unchanged screens. Use short bounded waits with state checks rather than repeated blind clicks.
- Keep one row per defect with evidence links and changed-file hashes. Handoff: outcomes, unresolved findings, exact next action, candidate/restore state. Retain detailed logs on disk.
- No repeated permission requests for already-authorized isolated tests. Pause only for real user-only actions, ambiguous destructive scope, missing essential data or an unrecoverable blocker. No commits/deployments or unattended extra sessions.

## Completion boundary

Execute the current session's ordered batches autonomously; checkpoint between them. Do not stop after proposing a fix when implementation and validation are authorized. Stop at the specified review boundary, with a runnable candidate and honest open-item list. Release preparation must reconcile current hashes, maintained source/generator changes and content versions across all three repositories; historical deployment commands are not a substitute for that reconciliation.
