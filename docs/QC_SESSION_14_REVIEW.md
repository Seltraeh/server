# Session 14 QC — October 8, 2026

October 9 acceptance update: SESSION_14_CLIENT_RESULTS.md supersedes the
candidate's visual expectations. Early whiteout improved, but required final
white is absent for tested Omni/7-star paths. #24 remains open; the prior model
criterion did not guarantee correct final transition. Farm visuals passed.

Verdict: ready for the bounded existing-package client comparison after the
corrections below. #24 remains pending client acceptance. C4 is not a complete
cold-install solution because uncached script sound dependencies remain open.

## Findings

The gate guard is a plausible script-layer fix for the documented global-list
flash trigger. Its model reproduces premature whiteout with current and stock
scripts and prevents it with the candidate under the tested conditions. The
specific second extra animation in the player's run is still inferred. This QC
reviewed model/script/delivery logic but did not independently reproduce all
binary disassembly or run the game. Do not claim deterministic whiteout merely
because the fixture deterministically selects a double gate.

Farm's saved six empty entries and historical balances reconcile. The harvest
wire suite covers payout, exhaustion, upgrade and refresh. There is no evidence
here for changing rates or granting another harvest on upgrade.

Corrected during QC:

- verify_town_harvest.py skipped changed lists and could emit a misleading
  partial comparison. It now exits INCONCLUSIVE (2) on changed lists, missing/
  increased taps, or inputs not representing the same single user. Three new
  regression tests pass. The historical Session 13 comparison still gives four
  OK results. This snapshot checker is not a general ledger for arbitrary play.
- gen_gate_script_policy.py documentation incorrectly promised cached fallback
  on bundle withdrawal. Owned-but-unavailable does not fall back; selecting C3
  or disabling the provider returns scripts to legacy handling.
- Client checklist now qualifies the reproduction and collects after A and B
  before optional C, protecting the comparison against log rotation.

## Independently rerun

| Check | Result |
| --- | --- |
| Gate model with candidate scripts | 56 passed, 0 failed |
| Town harvest wire | 24 passed, 0 failed |
| Historical credits | Four OK results |
| New accounting rejection tests | 3 passed |
| C4 real broker/proxy integration | 5,973 proxy checks; 11 driver checks passed |

The live integration verified 36,335 reads, including 14 script text reads and
nine scripts differing from original content. Two other opt-in proxy modes were
skipped. Server 90e3eff9... and policy 8bdcd08e... hashes match the handoff.
No C++ changed, so no production rebuild was required. Claude's full 54-entry
runner was not independently repeated. Evidence is in session14/qc_gate.log,
qc_town.log, qc_family.log, qc_harvest_accounting.log and qc_harvest_checker.log
under out/qa/current.

## Next

Use CLIENT_SESSION_14_STEPS.md on [ContentTest C1] without resetting caches.
Record actual gate outcomes; do not close #24 from modeled tests. The existing
package has the required legacy sounds, but general cold-client delivery still
needs work. CLAUDE_SESSION_15.md prioritizes those script/audio dependencies,
then coherent UI coverage and the unfinished login-verification transition.
If the player's candidate still whites out, that result takes priority.

No live-save changes, deployment, commits or pushes were performed.
