# Claude Code Session 5 — player regressions and disciplined build workflow

Work in C:\Users\Evan\BF\BF-WorkingDirRust and its packet-generator submodule.
Read docs/BF_OFFLINE_SERVER_HANDBOOK.md, docs/DEVELOPMENT.md, BUILD.md and
docs/GAMEPLAY_WORK_QUEUE.md first. This assignment's priorities supersede older
session prompts and the queue's generic boss assignment. Inspect both dirty
working trees and preserve existing work. Implement and verify the following
bounded fixes, deploy tested changes to the normal launcher, and leave a concise
handoff and client checklist. Do not commit or push yet.

## 1. Current player evidence — October 2

Confirmed passes; preserve these:
- Vargas purchased SP nodes persist after client relaunch; screenshot shows
  Current 0, Used 100, Total 100/100. Actual battle effects remain unconfirmed.
- Three supplied Deemo forms appear in inventory, can join the squad and fought
  successfully in mission 10. This does not certify evolution or every skill.
- Latest potion test: using two potions consumed two. This is a pass for that
  reported consumption case, not every item-bar/refill/resume path.

Current failures/blockers:
- Buying 400 Ignis Shards in Merit Exchange gives Invalid trade request,
  GroupId m9LiF6P2. Screenshot: 38,340 points, unit price 50, total price 20,000.
- Mission 10 completion still plays the Farm cutscene EVERY time, and the Farm
  remains unavailable. Do not describe this as fixed or merely cosmetic.
- Mission completion sends the player Home. Expected: ordinary stage completion
  stays in its mission area to select the next stage; completing the final stage
  runs the appropriate unlock presentation and exposes the next area/first stage.
  Recover the actual mode-specific transitions rather than forcing one destination.
- Sphere crafting is unavailable at current progression. Its cause is unverified.
  Fix Farm/story progression first, then exercise legitimate feature unlocks.
- Evolution comparison is blocked by missing suitable base units, evolution
  materials and Burst Queens. The player requests a usable test setup.
- Training Grounds crashes after selecting Begin. Previous server wire passes
  did NOT establish a client fix.

## 2. Build, evidence and artifact discipline — apply throughout

- Use ONE development build tree: out/build/debug-win64, with the existing
  Visual Studio x64 MSVC environment, CMake presets and rebuild.bat workflow.
  BUILD.md specifies Ninja Multi-Config and Visual Studio Open Folder support;
  there is no .sln preset. Make the documented build/VS workflow produce the
  executable the normal launcher uses. Do not create another generator/tree or
  another dated build helper to sidestep the problem.
- Check running sessions, server processes and CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG
  before building. Do not run overlapping generators/builds against shared headers.
  Configure only when needed; batch all KDL changes, including documentation,
  because they regenerate shared headers and can force a full rebuild.
- If retaining a running server during compilation, use the handbook's temporary
  runtime-output override within the SAME build tree, pointing to one reusable
  out/qa/current/bin directory. Record it, then clear it before deployment. A
  scratch executable location is not a second CMake/dependency build tree.
- Keep mutable fixture DBs, configs and short logs in out/qa/current/<suite>.
  Use explicit isolated config paths and distinct ports, verify port ownership,
  and stop only the test process tree. Reuse the suite directory only after its
  processes have stopped. Keep durable, reusable tests in scripts/.
- Keep one compact latest result summary with source/build identity and commands.
  Retain evidence for unresolved failures; replace successful transient fixtures
  when safe instead of creating timestamped copies on every run. No full client
  asset copies, duplicate dependency trees, giant dumps or repeated source snapshots.
- Store pre-deployment recovery backups in one clearly identified backup folder;
  never rotate away the only recovery point or a backup for an unresolved issue.
  Do not recursively purge out/. Existing directories may contain unique saves
  and evidence. Inventory candidate cleanup by path/purpose first; remove only
  positively identified regenerable artifacts whose resolved paths stay inside
  the intended workspace and are not in use. Preserve unknown files and report them.
- Test only on SQLite backup copies. Deploy tested changes against the existing
  live save after backing it up; do not replace it with a fixture or reset its
  progress. Stop/rebuild/relink/start via the established workflow, verify the
  normal executable path, listener, migrations and save integrity, and report
  its timestamp. The player should only need to reconnect, not find a QC build.
- Update shared build docs/scripts only where necessary to make this workflow
  reproducible. Do not introduce hardcoded ignored helper dependencies. Source
  and schema changes belong in this checkout and submodule, not only scratch files.

## 3. P0 — Training Grounds Begin crash

Reproduce and correlate the Windows crash dump and last request/response.
Use %LOCALAPPDATA%/CrashDumps, server logs, handbook Windows stack/disassembly
tools, and ARM64 symbols as semantic evidence. Do not substitute an Android
address for a Windows crash location or assume the previous missing-dummy
diagnosis is the complete cause.

Known context: simulator mission 6000000, dummies 10000000–10000005, generator
scripts/gen_battle_simulator.py, existing test_battle_simulator_wire.py. Trace
Begin through actual mission metadata, encounter selection, monster/unit/skill
lookups, downloaded assets and scripts. Determine whether failure is a missing
reference, invalid field/value, asset mismatch or another path. Avoid fallback
mission 10 goblins, invented combat content, broad zero/default filling or an
empty-OK workaround. Correct the owning schema/data/handler layer.

Tests: reproduce the identified bad relationship in a targeted regression;
validate all referenced IDs and expected response consumers; rerun simulator
and affected mission tests. Client acceptance: enter all six elements, fight,
exit/re-enter; no crash, unintended rewards/captures or owned-item loss.

## 4. P0 — Farm progression, one-time scenes and mission return destination

Treat these as related lifecycle investigations, not one guessed flag fix.
Start with mission 10, not the older checklist's mission-11 suggestion. Identify
the real Farm unlock milestone and feature-gating condition from scripts/MST/
client consumers; mission 10 is the observed reproduction, not proof it should
itself be the unlock milestone. Check that legitimate prerequisites actually
unlock the Farm even when an intro has already been seen.

Trace mission start/end, first/repeat clear, persisted clear flags, scenario
uploads and replies, deck uploads, facility/feature availability, result screen
completion and next-scene selection. Inspect any error response/command that
could explain Home navigation; do not assume it is solely a visual layout bug.
The current code has a recoverable ReturnToGame/Home refusal path (command 6),
so the handbook's historical statement that every error exits is not universal.
Determine whether a refusal is legitimate before changing it. A successful
mission must not rely on a refusal to choose its destination.

Preserve authored intentional replays. Do not hardcode all scene flags complete,
globally open maps, remove scenario content, suppress the scene while leaving
Farm locked, or route all modes to a single area. If an existing save requires
repair, derive it narrowly from earned progression and make it idempotent.

Tests on copied fixtures: before/at/after the real milestone; first clear,
ordinary repeat, final stage, reconnect/restart and interrupted scene; stage
selection versus next-area unlock; feature access and no reward duplication.
Keep item consumption and mission serial ownership protections intact. Rerun
related scenario, story-gating, settlement/ownership and core wire suites.
Client acceptance must separately show Farm functionality, once-only scene,
ordinary mission-area return, and final-stage unlock presentation.

## 5. P1 — Merit Exchange quantity/schema bug

Concrete evidence already inspected:
- deploy/log/http_log_m9LiF6P2_20261002_080604.log contains
  rX74GNsm:[{Mdgsh04u:"91000059",H6k1LIxC:"400"}].
- Response diagnostic: Invalid trade request / invalid quantity or price.
- Handler exists in gimuserver/gme/handlers/AchievementAction.cpp,
  HANDLEF(AchievementTrade). It rejects count > offer->limit_count or price <= 0.
  This is NOT an unregistered-handler issue.
- deploy/mst/achievement_trade_mst.json offer 91000059 has price key 3EWLm0sA=50,
  reward qBAb07rh="4:800901:1:0", 9Hau45Jj=1 and S8rdp9zk=400.

Trace the request builder and trade MST readParam/setters and consuming code.
Check whether limit/count/stock fields were reversed or misinterpreted. These
values are leads, not permission to relabel keys without binary evidence.
Confirm per-purchase quantity versus lifetime stock and reward multiplier.
Correct KDL and server validation/stock accounting as necessary; do not simply
raise a hardcoded limit or disable validation. Abort malformed requests rather
than continuing with a partially parsed structure. Keep safe refusal behavior
and client refresh semantics consistent with the decoded consumer.

Tests: quantity 1 and 400 (400 must cost 20,000 at this offer's 50-point price
and grant exactly 400); partial stock then remaining stock; 0, negative,
overflow, above-stock, insufficient points, malformed quoted-number request;
no partial debit/grant; reconnect persistence and relevant retry semantics.
Test other Merit offer types so a corrected mapping does not regress units,
spheres, Legend Stone or expiry. Rerun exchange UI, Merit/fusion and affected
inventory suites. Confirm actual grant destination and immediate UI refresh.

## 6. P1 — unblock client tests through legitimate progression and supplies

After Farm/mission lifecycle fixes, establish the real requirements for sphere
crafting and test the normal unlock path. Do not manually unlock every map or
facility. If progression assistance is necessary, explain the narrow intended
milestones and use the game's supported transitions/admin mechanism, preserving
existing clears, rewards and once-only scenes.

Prepare a minimal, documented evolution bundle using existing present/admin
tools after checking current inventory. Choose real supported recipes and exact
IDs, provide eligible base units, required evolution units/items, Burst Queens
and sufficient Zel/Karma; list expected before/after BB/SBB/leader skills and
retained upgrades/equipment. Do not overwrite or evolve the player's Vargas or
Deemo for them, duplicate prior grants blindly, or issue an unrestricted stockpile.
Back up before a live grant, record its receipt/manifest, and make retries avoid
duplicate delivery. Confirm the player can actually access the relevant screen.

## 7. Deferred work and protected changes

- Do not reimplement FeSkillGet. It is now registered, deployed, server-tested
  and purchase persistence is client-confirmed. Read FE_SKILL_PURCHASE_2026-10-01.md;
  preserve full roster refresh and waiting for the commit callback before success.
- SP reset (ShopUse type 9) remains unfinished; take it only after priorities
  above, as a separate complete slice with decoded request/reply and tests.
- Delayed Continue A/B/A can still overcharge/rewind state; temporary sphere-ID
  lock/sale-retry ambiguities remain. Keep them in the handoff, don't silently
  declare the entire diff accepted or run destructive retry tests on the live save.
- Don't reopen previously confirmed Burst Queen/Mystery Frog fixes from old
  checklist text alone. New relevant regressions justify focused retesting.

## 8. Required completion output

Update docs/CLAUDE_SESSION_5_HANDOFF.md and the current client checklist, rather
than creating a document per command. Include: exact files changed in parent
and submodule; cause/evidence for each fixed issue; automated results with real
pass/fail counts; client-confirmed versus pending; unresolved risks; normal
executable path/timestamp; save backup and any live supply grants; one artifact
inventory and any safe cleanup performed. State what has NOT been implemented.

Give the player a short ordered client test list with exact mission/unit/item
IDs and expected results. Stop calling compilation or an HTTP success a client
pass. If a task is blocked on binary/client evidence, preserve the repro and
continue independent work; do not invent a fix to satisfy the checklist.
