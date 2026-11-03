# Session 14 player results — October 9, 2026

Issue #24 remains open. Required behavior: no white overlay covering a gate
still awaiting input; a white transition after the final gate opens, before
unit reveal. Merely preserving the old animation-count-dependent behavior is
not sufficient. Session 14's "may or may not appear" acceptance was too weak.

| Part | Observation |
| --- | --- |
| A Omni, current scripts and overflow fixture | Rainbow reveals black, then white covers black. Clicking white opens the hidden gate and reveals the unit. Screenshot confirms full whiteout. |
| A 7-star | Single black gate, but no final white transition; reveal looks janky. |
| B Omni, guarded candidate | Premature whiteout gone, but required final white transition also absent. |
| B 7-star | Single black gate with touch prompt; final white transition absent. |
| B 5-star | Correct: white transition after the second gate, then unit reveal. |
| B Farm | Items, Karma and Zel visibly drop. |
| B other plots | Passed. |
| B exhausted Farm upgrade | No new sparkle, matching the implemented harvest cycle. |
| C Omni, current scripts and flat-energy fixture | Correct: white AFTER second gate, never covering it before input. |

Player also observed a Home download after daily spin/calendar in B. This is
not by itself a C4 failure; legacy content remains. No specific asset identified.

Part A already lacks final white for 7-star, so do not call that a new candidate
regression. C's success supports state-dependent triggering but does not prove
which animation causes it. Candidate reduces premature triggering without
ensuring the desired final transition. Do not close #24 or ship as complete.

Codex collected the available C1 logs to collected/20261009-083805 under
out/qa/current/content_client_c1. Collection alone does not establish that every
part survived rotation; inspect per-process markers and earlier collections.

Next: trace/implement final Open/reveal state explicitly so flash timing is
independent of unrelated global animations. Require both absence of early white
and presence of final transition in model and actual client acceptance. Compare
single black, rainbow-to-black and gold/red-to-rainbow under overflow and normal
energy with normal/rapid taps. Do not fix this by blindly adding more count guards.
Farm's reported visual concern is now resolved for the tested fixture; no further
harvest repeat needed unless relevant code changes.
