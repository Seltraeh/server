# Independent QC of fresh round 4

October 30 2026. Reviewed capture encoder delta, maintained portrait generator, QA containment helpers, Super parade generator and test evidence. No screen interaction, deployment or runtime edits. All new checks used separate disposable fixtures under out/qa/current/review_round4_20261030; prior evidence preserved.

## Results

- Candidate SHA256 independently matches c28a8db9efacc43d5c5f4bf707d8036520f8e8df7e69e3438ba6db1e67e532fe.
- Independent helper tests: 57 passed, zero failed.
- Independent portrait correction tests: 27 passed, zero failed.
- Independent parade generator tests: 11 passed, zero failed.
- Independent capture wire tests on that candidate: 8 passed, zero failed.
- Independent Metal Parade wire tests on that candidate with the new Super boss archive: 18 passed, zero failed.
- Logs: helpers.log, portraits.log, parade_generator.log, capture.log, parades.log in this review's QA folder. No full suite/build rerun was performed.

The capture change keeps the first nonempty roll but still calls encodeUnitDrop for every monster in its previous position in the loop. This preserves subsequent random calls by code inspection and matches the tested client-format contract. Settlement trust was not tightened; that is a separate offline policy, not an inadvertent change here. The supported x86 trace and client observations are Claude evidence, not independent reverse-engineering or visual verification in this review.

The ancestor-junction gap from round 3 is addressed: visual/saves and run redirections are refused, and mutable files/configured database receive explicit validation. The new tests passed independently.

Historical suite status must remain accurate: the first round-4 full run passed 65 entries. The later 66-entry aggregate has a fixture-lock error despite zero failed assertions; the affected suite passed separately in a fresh fixture. Do not relabel the aggregate as a clean pass. The leftover pid 11336 was present at review; it was not touched. Use fresh fixture names; no reboot is required for this review.

## P2 Portrait proposal temporary-file overwrite

scripts/unit_mst_corrections.py propose() writes to a predictable <output>.tmp using write_bytes without checking whether it already exists. Unlike replace_into(), this path does not refuse a pre-existing temporary file. A previous attempt's data can be overwritten, and a hard-linked temporary file writes through to its other name.

Independently reproduced in temp_probe.py under the review QA folder: create sentinel.txt, hard-link proposal.json.tmp to it, call propose() with a pristine read-only source. sentinel.txt changes to proposal bytes. All affected files were disposable; no live or source file was modified. Existing tests cover linked staging outputs but miss the proposal temporary name.

Required correction: create temporary files exclusively in the destination directory, refuse or safely avoid existing names (including dangling links), close successfully before rename, and clean up only the temporary file this call created. Add a regression preserving both bytes and link targets for a pre-existing ordinary/hard-linked temporary file. Retain identical-output idempotence and source/deploy preservation. Review the staging temporary-write path for the same no-follow/exclusive-creation invariant; don't rely solely on exists() before writing.

## Disposition

Runtime capture and Super candidate checks passed. No new runtime defect established. Correct the proposal writer before next use; this is not a reason to revert the accepted portrait or capture fixes. Round 5 then proceeds through the requested #22 presentation reports, #26 resource boosts, #25 SP eligibility and #40 UBB, plus a bounded missing-content inventory. No live deployment or release approval implied. Word tracker remains at October 28; current status is the acceptance matrix.
