# Claude Session 16 — complete local startup coverage and remove redundant waits

Work in the current `C:\Users\Evan\BF\BF-WorkingDirRust` checkout and its
existing sibling `C:\Users\Evan\BF\offline-proxy`. This document is the task
directive. Read the current source and applicable repository instructions;
older handoffs are evidence, not competing priorities.

## Read first

1. `docs/BF_OFFLINE_SERVER_HANDBOOK.md`, especially the October 11 updates and
   current build/QA rules.
2. `docs/QC_SESSION_15_REVIEW.md` and `docs/POST_QUEST_PROMPTS.md`.
3. `docs/OFFLINE_STARTUP_TRANSITION_PLAN.md`, especially section 7's Windows
   addresses, dependencies and remaining 1,373 legacy login names.
4. `docs/OFFLINE_CONTENT_PHASE_C5.md`, `OFFLINE_CONTENT_PHASE_C6.md`,
   `BUG_BACKLOG_STATUS.md`, and the proxy's `docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md`.
5. Inspect dirty status in both checkouts and packet-generator before editing.

## Accepted baseline — preserve it

October 11 player evidence supersedes the old "client pending" descriptions:

- Session 15 summon tests passed: single/double gates, overflowing/normal
  energy and normal/rapid taps. No early white hides a waiting gate; the final
  opening has its white reveal transition. #24 is accepted on the test candidate.
- C6 banner replacement, RAM audio replacement, restoration and normal route
  visuals/audio passed. The correct 7-star banner is
  `gacha_rare_bg_img_20161028_5M.png`, NOT `gacha_rare_bg_img_20170524.png`.
  `bf113_zin_title.mp3` is the short voice heard as the gate spawns, not music.
- C6 snapshots: 3,931 existing owned files before/after, zero additions,
  removals or changes. This proves the tested route, not a cold install.
- The subsequent Windows Store rating and Facebook gem promotion removal is
  player-confirmed. Collected log `.log.1` confirms patch installation. Preserve
  `cp_post_quest.cpp` and its static proof; do not restore those prompts.
- Farm is not a proven payout regression: latest batch has five rolls
  `0:0:0,0:0:0,0:0:0,0:0:30,0:0:0`; two empty taps consumed, three remaining.
  Do not change harvest rates or upgrade-refill semantics in this session.

Current C1 installed DLL and candidate:
`out/qa/current/session15/postquest/libcurl.dll`, SHA256
`09056ee55d52936ad584a1ec79e044369c817da0b904901a9e3deb7947ba9c24`, port 29960.
Includes all Session 15 gate/audio changes plus both prompt removals.
Server remains `out/qa/current/session13/bin/gimuserverw.exe`, SHA256
`90e3eff97739b0d5d5713cae983db9fc4cadaa7e105b8bdf009a81b1444b5ebc`.
C6 policy `out/qa/current/session15/policies/family_policy_c6.json`, SHA256
`e3d29e253a4330d1abbb6a49ba5e93bd3d09910a172e46ca05b112f86c2ce6fb`.
Validate these on disk; do not confuse a rebuilt candidate with an older hash.

## Objective

Make first launch and ordinary gameplay use local game_content without permanent
client asset caching or redundant download/verification screens. Local availability
does not mean decoding all art and audio into RAM. Keep required master-table,
text-dictionary, save and login initialization. Deliver tested implementation
this session, with exact remaining blockers if full coverage is not achievable.

### 1. Close the observed startup gaps

Start with a fresh inventory derived from current MSTs and the collected C6
login/Home/quest logs. The historical 1,373 names are a starting point, not a
hard-coded completeness claim. Account for save-dependent inventory/Archive
assets and both background-DLC answers.

Implement coherent remaining families that remove actual first-launch windows:
common/button and common image collisions; Home layouts and their complete
resourceMap dependencies; font files with atlas pages; scripts with their
referenced art/audio. Prioritize the login/Home route before unrelated content.
Record each conflicting basename's chosen source and the consumer evidence.
Never hide a missing dependency with an empty placeholder or a blanket wildcard.
Unresolved layouts must remain explicitly unavailable or legacy; they cannot
be declared ready merely because the layout CSV itself exists.

### 2. Implement a machine-checkable startup coverage contract

Add a bounded broker coverage report for the names the current login and
background-download paths require, derived from the relevant master data and
dependency closure. Report missing/unowned/ambiguous dependencies and the
policy/content generation the result describes. A claimed complete result
must remain valid through startup, or be invalidated/rechecked when content
changes. Treat unknown lists, unsupported build, broker unavailable and partial
coverage as not ready. Reuse existing protocol/version conventions.

Tests must catch false completeness: remove a required file, withdraw a shared
bundle member, introduce a collision, use an unsupported/incomplete report,
and change the content generation during verification. Include recovery.
Do not equate names in a policy or a warm cached route with complete coverage.

### 3. Use the contract to replace redundant startup work

Once coverage is demonstrably complete, implement the narrow verified Windows
path described in OFFLINE_STARTUP_TRANSITION_PLAN.md: avoid the redundant
LocalState asset census and its wait while retaining necessary MST/text/login
work. Resolve remaining callers/ABI before patching. Do not simply force
FILE_CHECK or a scene's download queue to finish.

Handle the background-DLC Yes/No prompt only when its actual required work is
covered. The offline-ready path should not ask the user to download assets
that are already locally available. Preserve the legacy/error path when the
provider is disabled, incomplete or unavailable. A hidden UI with ongoing
unbounded background caching does not satisfy the goal.

Investigate the one-time "Refreshing the latest data files. Application will
close now" notice observed after a C6 image swap. It cleared on ordinary
relaunch, but its exact trigger is unconfirmed. Distinguish legitimate MST
updates from an asset-swap-induced restart; do not suppress all NoticeScene
messages or bypass real corruption checks to make it disappear.

If complete startup coverage cannot be reached, deliver the coverage report
and concrete family implementations, identify the exact remaining names/read
paths, and leave skip logic disabled. Avoid spending the whole session only
rewriting plans or adding ownership counts with no route improvement.

## Validation and operating rules

- Preserve all dirty work and accepted fixes. No commits, pushes, live-save
  mutation, deployment or resetting existing packages/caches. Do not install
  a new DLL into the player's C1 test package during this implementation pass.
- Reuse existing Visual Studio build trees. One bounded
  `out/qa/current/session16` for candidates/evidence; no timestamped build-tree
  sprawl. Restore temporary CMake port/output settings and pre-session bin
  artifacts. Durable code/tests belong outside out; never commit out.
- Keep source changes in the current checkouts. Record the exact server,
  proxy and packet-generator diff scope and candidate hashes. Never claim
  tests exercised the latest build without verifying the executable used.
- Rerun relevant proxy, policy, live broker, dependency and harness suites.
  Retain `check_gate_flash_site.py`, `check_audio_reader.py` and
  `check_post_quest_sites.py`. Include negative/failure and restart tests.
  Run server regressions appropriate to any server changes.
- Prepare a NEW named cold-client identity and copied, usable save for later
  player acceptance; preserve C1 as the accepted fallback. Do not launch or
  reset it on the player's behalf. Avoid over-capacity inventories. Verify
  roster/squad references, helper paths, currencies and fixture prerequisites.
- Cold acceptance needs first launch to Home, first quest, relaunch, missing
  content and provider-off controls, with actual download/verification-screen
  counts. Automated correctness is not visual acceptance.

## Deliverables

Write `docs/CLAUDE_SESSION_16_HANDOFF.md` with implementation, evidence,
commands/results, hashes, limitations and unfinished tasks. Write
`docs/CLIENT_SESSION_16_STEPS.md` with a short sequential cold-run checklist,
explicit package identity and exact commands, expected visuals, rollback and
collection steps. It must be runnable after Codex QC, with no guessed banners,
ambiguous terminal instructions or unnecessary repeats of accepted tests.

Update relevant handbook/coverage documents. Keep gameplay backlog status
honest: separate implemented, automated-tested, player-confirmed and deployed.
Do not edit the Word bug list this session. Do not introduce unrelated gameplay
work while startup remains the session's primary objective.
