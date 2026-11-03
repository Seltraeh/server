# Session 13 client results — October 8, 2026

Partial visual acceptance; automated QC did not close issue #24.

- Home renders correctly with Selena leading.
- Summon entry briefly downloads. Single gate has no whiteout; double gate
  whites out the screen after the first gate. #24 is reproduced and open.
- Sidonie opens without downloads and animates correctly.
- Quest entry briefly downloads. Brave Bursts, roster and helper work;
  end-of-mission screens look correct.
- Town buildings animate. Mountain, forest and river show drops; Farm shows
  no visible drops. Farm upgrade does not produce more harvest sparkle.
  Missing visuals are reported; lost rewards are not yet established.

Codex collected the available C1 logs without changing the save. Last observed
counters: 906 provider fetches, zero read failures, zero tripwire writes and
4,082 suppressed downloads. These do not prove gate-script correctness or
Farm reward delivery. The server received taps for all four locations:
`1:3,2:8,3:6,4:10`; receipt alone does not prove the resulting credits.

Town.hpp sets a three-hour harvest period. TownFacilityUpdate persists upgrades
without promising immediate replenishment. No sparkle immediately after upgrade
is therefore not independently a proven bug. Inspect Farm's pre-rolled payload,
tap index, inventory/currency changes and client effect to separate visual,
reward and timing issues.

Next: trace #24's actually loaded legacy gWait/gOpen scripts and double-gate
input/white-overlay lifecycle. Compare C3 with the prior policy on copies before
calling it a C3 regression or blaming stale scripts. Preserve logs, prepare a
deterministic sequence, and do not reset caches as a substitute for diagnosis.
Session 12 Archive replacement acceptance remains pending. This run provides
positive evidence for normal squad use on the repaired roster, but does not
explicitly repeat each formerly crashing empty Home-pane action.
