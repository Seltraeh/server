# Claude Session 9 — reliable client acceptance and the next gameplay fix

Read the handbook, DEVELOPMENT.md, complete Session 8 handoff,
QC_SESSION_8_REVIEW.md, CLIENT_SESSION_8_CHECKLIST.md and the offline content
loading plan. Inspect both existing checkouts and packet-generator before edits.
Preserve all uncommitted Session 7/8 work; no broad cleanup, new build trees,
bulk content copies, live save edits, commits or pushes.

## Current acceptance evidence

Final October 4 sphere acceptance: player confirms the selected favorite and
equipment persist, and sphere sales remove only the selected copies. Combined
with the fresh-craft display/equip pass, #34 is player-confirmed fixed for the
reported workflow. Earlier pending sphere checks below are superseded; do not
repeat them without a new reproduction. Continue with harness/provider work.

Latest October 4 update supersedes the earlier fresh-craft uncertainty below:
the player tested the candidate, crafted several identical spheres, could equip
all copies, and favorited one. Record fresh-craft display/equip and the favorite
action as passed; do not ask for the same fresh-batch reproduction again.
Relaunch persistence, favorite isolation and selected-copy sale remain separate
regression checks. Proceed with the harness/provider priorities.

October 4 player confirms all previously crafted spheres are visible and mission
completion no longer returns them to Home. Record both as player passes. They
do not prove first-clear next-area animation or freshly crafted row identity.
Normal server remains fb77bf6f... at the latest QC inspection; ItemMix changes
are in QA candidate a2022e91... (Session 8). A login refresh on the old build
also repairs existing rows. Do not present the player's existing-inventory pass
as a verified fresh-batch pass on the candidate without checking the binary.

Sphere tests independently rerun by QC are documented in QC_SESSION_8_REVIEW.md.
Proxy e24234c1... remains the reviewed candidate, not the installed DLL.
The provider is still disabled for normal play. No app reset is required.

## 1. Correct the acceptance harness before asking the player to use it

scripts/content_provider_client_test.py cmd_server currently exits its startup
wait after 180 seconds and prints "listening" without checking whether the port
ever opened. A TCP connection also does not prove the listener is its child.

Require the launched child to own the test port exclusively AND the expected
server endpoint to respond before announcing readiness. On timeout, child exit,
ambiguous/foreign ownership or endpoint failure: return nonzero with log path,
clean up only the processes the harness launched, and never report success.
Reuse the tested child/ownership handling in bf_testkit where practical.
Add mocked tests for timeout with a live non-listening child, premature exit,
foreign/duplicate listeners, normal startup, and interruption/cleanup. These
tests must not stop the player's server or register packages.

Document the test port arrangement clearly. Prefer an explicitly configured
isolated-port proxy candidate if straightforward; if using 9960, provide a
concrete switch-over procedure and do not silently stop the player's session.
Report how much legacy content the test package still downloads and how to
observe readiness; do not call the current proof a finished cache-free client.

## 2. Make fresh-craft and provider acceptance reproducible

Use the prepared side-by-side package/fixture and copied save; preserve the
installed app, keys and LocalState. Run read-only preflight. Document any actual
registration/loopback/launch steps that require the maintainer; do not claim an
in-game pass from desktop mocks or request the same observations repeatedly.

Test sphere identity against the identified Session 8 candidate: craft enough
Famous Blades (recipe 2005, product 30000) to span multiple screens, then navigate
directly to Equip Sphere without reconnecting. Expect all rows immediately.
Check first/middle/last copy locks, both slots, transfer and sale of an unlocked
spare, materials/Karma counts and persistence. Use test supplies in the copy,
not unrequested grants to the live save. Relink the normal build only as part
of an explicit documented deployment step; record the new hash and rollback.

Provider proof: confirm active hooks/21 owned names; stale-cache Town uses the
fixture; Zeal first battle has portrait/sprite/animations; same-size fixture
edit appears at safe reload; unavailable versions/missing dependencies recover;
account and equipment survive restart; no owned-asset writes; frame pacing
compared under consistent conditions. Keep digest binding and the five original
QC regression assertions intact. Missing-file recovery is still unproven inside
the actual game, and must be recorded honestly if it fails.

Do not advance to broad Phase C/D hooks just to fill time while the player is
testing. Once the proof passes, propose the next bounded Phase C implementation:
scene dependency preflight/background prefetch and suppression of owned-name
legacy downloads, then expand asset families. Obsolete-screen removal follows
verified readiness, not blanket success responses.

## 3. Independent gameplay work while client acceptance is pending

One bounded backlog task: archive-granted units can have stored leader_skill_id
zero. Trace every affected grant and consumer (ordinary battle, Training,
arena/campaign/Frontier Gate) and compare the unit's actual form/master skill.
Fix proven incorrect grant paths and plan an idempotent existing-save repair
that preserves legitimate overrides and upgrades. Do not assign guessed skills
to unknown units. Test on copies: new grant, old zero-value unit, evolved form,
nonzero override preserved, unknown/master-missing unit, restart and repeated
repair. Verify the actual packets for each affected mode, not only DB values.
If the stored zero is intentional in a path, document the evidence instead of
mass-updating it. Keep this change separately reviewable from the client harness.

## Completion

Build in the documented normal VS/CMake trees; use out/qa/current/session9 for
small reusable logs/fixtures only. Rerun affected sphere/warehouse tests if
touched, provider tests if touched, and relevant grant/evolution/mission suites
for the leader-skill change. Run the shared runner on the final candidate once;
investigate unexpected failures rather than weakening assertions. The mission
validator's 3,117 baseline is not a clean result.

Write docs/CLAUDE_SESSION_9_HANDOFF.md: actual changes, tests, binary hashes,
deployed versus candidate state, client-pending steps, and next bounded task.
Keep full backlog visible: #34 copy actions/fresh craft; #23 final-stage scene;
mission 234 and compound gates; level 999; leader EXP/Fuu; slots/parades; text,
portraits/summon gates; EU units; Trials/bosses/Fire Mecha; town refresh;
GuildUpdate. Do not close these from unrelated client passes. No Word issue-list
edits in this implementation run.
