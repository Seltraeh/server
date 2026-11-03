# Fresh Claude session, round 8: batch fixes, close remaining contracts, assess release readiness

You have no previous-chat context. Work in `C:/Users/Evan/BF/BF-WorkingDirRust`, its nested `packet-generator`, and the sibling `C:/Users/Evan/BF/offline-proxy`.

**Authorization and limits:**
- Evan authorizes scoped fixes and isolated client testing.
- Preserve all existing dirty work: all three repos carry large uncommitted change sets.
- Do not stage, commit, push, deploy, install, sign, reboot, reset live data, or replace the user's normal client or server.
- Historical instructions to deploy are not authorization to deploy now.
- You own visual checks. Never automate age or parental attestations. Use the established isolated test route and prior authorization. Stop only if an actual new user-only attestation or missing required input is encountered; do not re-request routine permissions.

## Read first

Read `QC_FRESH_ROUND7_2026-11-01.md` and `BATCH_FIX_WORKFLOW.md` first. The independent QC findings and the workflow below amend this brief. The dated content-scope correction was already appended by Codex October 31: do not duplicate it.


1. [CLAUDE_FRESH_ROUND7_HANDOFF_2026-10-31.md](CLAUDE_FRESH_ROUND7_HANDOFF_2026-10-31.md): the latest state, evidence paths, hashes, release file list and end state.
2. [BUG_ACCEPTANCE_2026-10-27.md](BUG_ACCEPTANCE_2026-10-27.md): its **Round 7 section** supersedes the older "Remaining" cells for #22-1, #25, #40 and the skill popup. Those older cells still read "Optional …".
3. [CLAUDE_FRESH_ROUND7_2026-10-31.md](CLAUDE_FRESH_ROUND7_2026-10-31.md): §2.4, §3 and §4 are unfinished and still define the work.
4. [QC_FRESH_ROUND6_2026-10-31.md](QC_FRESH_ROUND6_2026-10-31.md) and [REMAINING_CONTENT_SCOPE.md](REMAINING_CONTENT_SCOPE.md).

## Starting state (verify; don't assume)

- **Running:** nothing. Ports 9960, 29960 and 29961 are free.
- **S19 test client** (`gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w`): DLL at baseline `9633061d904203c7…`. Backup: `out/qa/current/fresh_round7_20261031/dll_backup/libcurl_9633061d.dll`.
- **Candidate server:** `out/qa/current/fresh_round4_20261030/bin/gimuserverw.exe`, SHA-256 `c28a8db9efacc43d5c5f4bf707d8036520f8e8df7e69e3438ba6db1e67e532fe`.
- **Candidate proxy (29960, test only):** `out/qa/current/fresh_round7_20261031/proxy_29960_r7vfit3/libcurl.dll`, `23f9a4b0c3096529…`. It carries the #22-2 header fit, the popup width, write-failure handling and the popup vertical fit.
- **Proxy build tree:** `offline-proxy/build` cache matches `out/qa/current/session16/baseline/proxy_CMakeCache_before.txt`; `build/bin/libcurl.dll` is `e24234c1…`.
- **Live save** `deploy/gme.sqlite`: read only, ever.

Use one new bounded QA root, `out/qa/current/fresh_round8_<date>/`. Keep maintained generators and tests outside `out/`.

## Harness quick reference (it took earlier rounds hours to learn)

**Start a run:**
- `python -I scripts/qa/prep_run.py --qa-dir R8 --run NAME --save out/qa/current/session19/visual/save_after_s20_completed_check.sqlite --content-root out/qa/current/fresh_round5_20261030/content_root_sg --system-root out/qa/current/fresh_round3_20261029/visual/system [--mst-root DIR --archive-root DIR] --set level=150 ...`
- then `python -I scripts/qa/run_ctl.py start|stop --qa-dir R8 --run NAME --exe <candidate> [--relay-dump MissionStart MissionEnd]`.
- `run_ctl` launches the S19 client itself and stops only what it started.

**Driving the client:** `out/qa/current/session19/visual/bf_drive.ps1` with `-Action click|drag|touch|shot|close`. Coordinates are capture pixels on an 831×934 window. It refuses input when the window is occluded. Contact sheets: `out/qa/current/session19/visual/tools/contact.py OUT SCALE files…`.

**Login route** (sleep 3 s after start): click (430,785) wait 50 s → (422,708) → (422,600) → (309,784) → (422,784). Then Unit = (272,885) and View Units = (282,368).
- Daily-spin rewards change the unit and material lists every run, so locate tiles from a full screenshot each time.
- A relog: `-Action close`, then PowerShell `Start-Process explorer.exe -ArgumentList 'shell:AppsFolder\gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w!App'`. Close that client yourself afterwards.

**Fixture rules** (all in memory `reference_qa_fixture_rules`):
- **Level comes from `total_exp`.** The server takes a unit's level from `total_exp`, not `unit_lvl`, so write both. `R7/tools/prep_sp7.py` and `prep_evo7.py` do; round 6's `prep_sp.py` does not.
- **Deterministic summons:** a copied `--archive-root` gacha pool and a copied `--mst-root` effect weight (`R7/tools/prep_lukroar.py`). The Test Gates live in the Veteran Summon banner.
- **Overdrive:** a LEFT flick on a UBB unit with a full OD gauge; UBB is an UP flick. The fastest bed is the Training Ground (Randall → Survey Office → Battle Simulator; Menu → Battle Settings → infinite BB and OD gauges).

**DLL swap:** back up and verify `9633061d` first, swap only into the S19 package, and restore it before you finish.

**Proxy builds:** `R7/tools/build_proxy_r7.sh TAG` (temporary 29960 port, cache restore, `build/bin` restore). Tests: `build/tests/content_provider_tests.exe` (898/0/5 skipped) and `tests/fixtures/check_name_fit_site.py` (44/0).

**Usage:** this is a fresh profile; the old profile's 94% usage is historical. Use available usage reporting at batch boundaries if supported; do not spend tool calls checking it before every run. Checkpoint completed work before stopping.

## Work, in order

**Batch 0: independent QC corrections.** Fix both findings in `QC_FRESH_ROUND7_2026-11-01.md`: the real WriteCode result/rollback contract (including after-copy failures), and vertical-fit partial-mutation fallback. Test the production behavior through a narrow injectable boundary; do not substitute arithmetic-only tests for integration state. Audit existing shared-writer callers. Rebuild once and combine the popup smoke with the first planned visual run. Do not rewrite unrelated patch infrastructure.

**New report: training-arena enemy status icons are too high.** During the next battle checkpoint inspect the entire enemy HUD, not just the UBB action. Save a before frame and identify the affected unit/scene/resolution. Compare training dummies with one ordinary battle enemy; trace the actual status-icon anchor, sprite bounds, coordinate space and scale in the supported binary/data. Prefer an established data/layout correction if it owns the defect; otherwise a narrowly guarded hook. Do not blindly lower a shared global offset. Cover no/one/multiple icons, different enemy sizes and viewport scale where practical. Fix a reproducible local cause in the same batch; a broader unknown becomes a precisely recorded follow-up. No wiki source is assumed to define pixel offsets.


1. **#26 Roglizer/Fuu** (the last round-7 §2 route; brief §2.4):
   - Run matched copied fixtures: no leader vs Roglizer 61286/61287 vs Fuu 830838, plus the helper case, on the same mission.
   - Use `--relay-dump MissionStart MissionEnd` to record displayed vs stored deltas.
   - Roglizer's EXP boost (process 97) must match exactly.
   - For Fuu, separate client-rolled per-hit Zel/Karma from fixed clear rewards and server payment (`Mission.cpp:666–667` pays as reported). Document sample limits.
   - **Never add Fuu's item bonus to chest rolls** (Drop Rate Boost rev 645616: "exclusive of chest drops").
2. **§3 per-hit item drops:**
   - Trace one enemy item-drop definition: generator/archive → MissionStart encoding → client consumer → MissionEnd award. The archive schema has `treasure_*`, `unit_drop_*`, `zel_*` and `karma_*`, but no per-hit item field yet.
   - Decide whether a channel is missing, the content lacks records, or another mechanism supplies them. Implement only once the client contract is established, with tests for zero, one and many items plus persistence; otherwise write the bounded follow-up.
   - **SP scope:** inventory it against Unit Skills rev 656738. The ordinary-unit SP chance is unported and has no source odds; don't invent one.
3. **§4 content proposals** (researched, not authored, unless needed for a fix):
   - #17 Fire Mecha God (rev 659707)
   - #18 story bosses (rev 674494; never reuse the Research Lab Juggernaut AI automatically)
   - #12 one first Vortex Trials encounter
   - #15 roster/assets (keep the Global-vs-JP decision explicit)
   - Update the existing dated correction in `REMAINING_CONTENT_SCOPE.md` with new evidence; preserve history and do not duplicate the October 31 correction.
   - Wiki API snapshots: `out/qa/current/review_round6_20261031/`.
4. **Release preparation:** write `docs/RELEASE_READINESS_<date>.md`, a checklist rather than a deployment, and stop for review. It covers three things.
   - **Every acceptance-matrix row's disposition:** tested fixed, not applicable with evidence, or open.
   - **Proposed release actions to validate against the final candidates, not blindly execute or quote as current commands:**
     - build the 9960 twin of the offline-proxy candidate, then repack, sign and install the live client (#22-2, popup);
     - copy the staged #22-3 dictionary and its F_RESOURCE_MST v3276;
     - put the #14 `unit_mst_corrections.py` proposal (v1087) in place;
     - run #39 `gen_metal_parades.py` for Super's boss;
     - deploy server candidate `c28a8db9` (capture fix);
     - repeat #24 on the final release DLL;
     - #27 depends on shipping the provider DLL and broker;
     - #33 only affects saves after deployment.
   - **One release file list across the three repos:** maintained runtime, generators, tests and docs only; exclude `out/`, saves, screenshots and transient session/QC notes. Start from the round 4–7 handoff lists, and check them against `git status` in each repo, because the trees carry much older dirty work.
   - Run `python scripts/run_bugfix_regressions.py <candidate exe>` on isolated fixtures before calling a candidate release-ready, and report failures honestly. The last clean run was 67 entries, 2,755 passed, 0 failed (round 5).

## Optional, if budget allows

- **#22-1 negative control:** the same deterministic 901 pull with `GachaGateClose6` withheld from a copied provider root (9.1 GB). It would show whether missing gate art reproduces "touch icon, no gate".
- **#40 in a natural-gauge story battle**, instead of the simulator's infinite gauge.

## Delivery and stop condition

- Write `docs/CLAUDE_FRESH_ROUND8_HANDOFF_<date>.md` covering:
  - changed files and hashes;
  - test commands and counts;
  - candidate identities;
  - visual routes and evidence;
  - the restored baseline state;
  - what is still open;
  - one recommended next content batch.
- Update the matrix with implemented / automated-tested / Claude visually observed / still open.
- Leave S19 on `9633061d`, with no processes running.
- Do not turn the cancelled heartbeat back on.
- Stop for Evan/Codex review: no deployment and no automatic next session.
