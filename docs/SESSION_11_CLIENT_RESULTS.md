# Session 11 player results — October 6, 2026

These observations qualify the automated QC verdict. Client acceptance is
incomplete, and the small-roster fixture is invalid.

- Vargas loads correctly: visual pass.
- Crafting: player confirms success; screenshot shows three each of the four
  consumables, three separate Mystic Lantern rows, and 23/100 inventory.
- Touka: absent from this save's presents. The crafting fixture was copied
  independently of the previous new-unit gift fixture. Do not assume gifts
  carry between named saves. This preview check remains unperformed.
- Archive: player identifies Galea as No. 8295 and Blaze as No. 8318. Swapping
  `unit_ills_thum_810048_100.png` produced no visible change. Parking it caused
  a 1/1 download on Archive entry, but Blaze's visible thumbnail stayed the
  same. This proves neither that the visible tile uses that filename nor that
  replacement is visually accepted. Trace the actual visible tile read and
  texture invalidation before selecting another test. The player's alternate
  art hypothesis remains a hypothesis; suffix semantics alone are insufficient.
- Leader: High Ruler's Chakra appears in both A and B. Read-only inspection
  confirms owned Berdette (1239, unit 10824) stores leader_skill_id 0 in c2_kit
  and 7401 in leader_after. The proposed UI comparison does not distinguish
  these saves. Trace the selected helper/display source and verify actual
  combat modifiers rather than declaring repair proven by text.
- Small roster: empty Home squad panes, crashes on first/second pane and
  Manage Squad across relaunches. Read-only inspection confirms only unit
  1000 (unit type 20011) remains, while deck 0 references deleted owned units
  1034 and 1113. DebugCli.cpp DoClearUnits deletes unit rows without repairing
  user_decks. This is a concrete fixture defect and likely crash cause; do not
  classify it as a missing-art/provider failure without further evidence.
- Performance on run: Menu downloaded 1/1; Archive did not download. Warm the
  route before comparing. The off command is explicit in the updated checklist.
- Run D follow-up: player confirms the provider-off route had no downloads.
  This confirms the exercised route was cached for that run, not performance
  equivalence or coverage of unvisited content. See the completed log review below.

## Collected evidence reviewed

The full PowerShell transcript supplied by the player confirms READY and clean
Ctrl+C shutdown for A, B, C and both D modes, collection after A/B/C and after
D, and reuse of the same `perf` save for broker off. The player completed the
requested collection steps; no further collection request is needed.

The single collected baseline `offlinemod_content.log` contains multiple
launches. Running `perf_compare.py` found both D runs (processes 7 and 8),
provider active then dormant/off. A rotated second file is not required.
Output: `out/qa/current/session11/perf_client_review.txt`.

Observed: provider-on final counters show 124 fetches (the comparison assigns
123 to scene visits), zero failures and zero tripwire writes; off stayed
disabled. Many scene
windows have p95 near 16.2 ms in both. Both have transition hitches. Whole-run
worst frames are 921.7 ms on and 621.8 ms off, but these are NOT a controlled
speed comparison: on includes daily-login/campaign and download scenes absent
from off, different visit durations, and first provider reads. Do not claim a
speed improvement or regression from those maxima. No repeat is requested at
this stage; address the broken fixtures and invalid visual targets first.

## Required follow-up

1. Repair clearunits dependency handling transactionally: inspect squad,
   campaign, equipment and other owned-unit references; preserve valid state.
   Add a regression asserting every emitted squad unit resolves to an owned
   unit and the active deck has a valid leader. Login HTTP success alone was
   insufficient validation of the old fixture.
2. Prepare a separately named repaired small fixture and save. Preserve the
   broken seed and player's save as evidence. Remember --seed does not replace
   an already existing named save. Validate the response before client retest.
3. Trace the visible Archive thumbnail and helper leader consumers, then issue
   corrected visual checks. Add Touka explicitly to a bounded test fixture if
   that preview remains needed.
4. Preserve complete on/off performance logs, compare warmed identical routes,
   and retain the known family-scale publication latency as open work.

No saves or production code were modified while recording these results.

## Session 12 follow-up — automated only; no player run yet (October 6, 2026)

The results above stand as recorded: none is reclassified as passed. Session
12 established the following for each item; the retests are in
[CLIENT_SESSION_12_STEPS.md](CLIENT_SESSION_12_STEPS.md), and the evidence and
addresses are in [CLAUDE_SESSION_12_HANDOFF.md](CLAUDE_SESSION_12_HANDOFF.md).

- **Small roster — cause confirmed, fix pending client acceptance.** Besides
  deck 0 naming deleted units 1034 and 1113, the seed had lost three worn
  spheres with the deleted units. The three Run C crash dumps (WER 14:33:27,
  14:36:22, 14:37:35) fault at `BraveFrontier.Windows.exe+0xA6ECFC` reading
  0x210: the squad pane looks up the squad's user unit, gets NULL for a
  deleted one, and reads it without a null check. It is not missing art.
  `clearunits` now repairs squads, campaign squads, bonds and worn spheres in
  one transaction (server candidate e30e2adf; its regression passes 24/0, the
  Session 11 server fails 15 of the same 24). Repaired fixture:
  `session12/fixtures/small_roster_repaired.sqlite`. The broken `small` save
  and its seed are unchanged.
- **Archive — the Session 11 target could not show a change.** The Archive
  tile draws `unit_ills_thum_<id>.png`; `_100` is drawn only for DBB-eligible
  owned units, and the Archive merely asks for it. Run A's provider log
  agrees: every Archive entry served the plain names, the swapped and parked
  `_100` was never read, so no decoded copy existed to release, and the 1/1
  download on Archive entry was the game's own download of the withdrawn
  name (tripwire write #1). No provider invalidation defect was found. New
  target `unit_ills_thum_810048.png`; the full swap / restore / park / restore
  sequence passes end to end without a client. The visual result is still
  owed.
- **Leader — the observation is correct client behaviour.** The Training
  party screen and the battle both take the leader skill of the player's own
  unit used as helper from its master data (it is not a "guest friend": no
  guest id). The stored value (0 versus 7401) changes neither text nor combat,
  so the A/B comparison cannot distinguish the saves and is withdrawn. The
  leader-skill repair remains validated server-side only.
- **Touka — preview not needed.** Its seven owned names are identical in C1
  and C2, and Session 10 showed the preview; its downloads were effects.
- **Performance — unchanged.** Family-scale publication latency measured again
  by the automated run: swap 21.4 s, restores 13.7 s and 15.8 s, park 6.6 s.
  Still open work.
