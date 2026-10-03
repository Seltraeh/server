# Claude Code Session 6 — finish paid actions and unblock evolution acceptance

Work in C:\Users\Evan\BF\BF-WorkingDirRust and its packet-generator submodule.
Read docs/BF_OFFLINE_SERVER_HANDBOOK.md, docs/DEVELOPMENT.md and the COMPLETE
docs/CLAUDE_SESSION_5_HANDOFF.md, including sections 9–10. Inspect both working
trees before editing. This is a bounded follow-up, not another broad content
rewrite. Preserve unrelated changes. Do not commit or push.

## Current evidence and scope

The latest full QA summary in out/qa/current/SUMMARY.md records 39 suites,
1,457 passed checks and three failed checks: delayed Continue retry (one),
SP reset (two). That run predates the later dummy AI and reticle data edits;
the handoff records targeted simulator/AI tests afterward (simulator now 93/93).
Do not represent that earlier summary as a full rerun of the latest state.

Session 5 records player confirmation of Farm/Mountain harvesting, once-only
Farm scene, Selena's sphere surviving Home/battle/relaunch, unequip, Vargas's
slot-2 change preserving slot 1, direct sphere transfer, mission-11 repeat
returning to its stage list, and Training Grounds entry. Later reports confirm
dummies never attack and Training Grounds menu/HP/element settings work.
Vargas SP purchase persistence, three Deemo units in mission 10, and consuming
two potions were confirmed earlier. Preserve these; do not redo their fixes
without a new reproduction.

Open client acceptance: 400 Ignis Shards; final-stage next-area presentation;
dummy reticle calibration; Save/Load Conditions; crafting after mission 21;
evolution BB/SBB; SP battle effects; broader visual/performance cases.

## 1. P0: durable Continue retry protection

Read scripts/test_continue_delayed_retry_wire.py and MissionBreak.cpp's
MissionContinue handler. Currently only the latest revival blob is compared:
A, then genuine B, then delayed A can charge again and replace B's resume state.

Recover the actual client request/retry identity and document its limitations.
Use durable per-user, per-run deduplication of accepted revival identities.
Do not deduplicate merely by mission id, use only an in-memory cache, suppress
all later revivals, or overwrite the newest battle state on a recognized retry.
If no explicit client nonce exists, explain the chosen fingerprint and the
identical-legitimate-payload ambiguity instead of claiming perfect identity.

Keep the charge, receipt and resume update atomic; verify commit before success.
Do not let a failed/no-funds request poison a later valid attempt. Preserve run
ownership, superseded-run rejection and no-continue-clear accounting. Respect
the decoded client behavior on refusal; do not invent success semantics.

Test A/A, A/B/A, restart between attempts, concurrent retries, distinct runs of
the same mission, foreign/superseded/closed runs, insufficient gems, and a
transaction fault. Genuine B pays once; delayed A pays nothing and leaves B's
resume data intact. Rerun mission ownership/settlement/progression and related
item tests. Use copied saves only for retry/fault injection.

## 2. P1: finish SP reset (ShopUse type 9)

FeSkillGet already works; do not replace it. Read its handler, FeSkills.hpp,
the existing ShopUse request/response KDL and FE_SKILL_PURCHASE_2026-10-01.md.
Decode the reset button's request and reply consumers before implementing.
Known lead: ShopUse xe8tiSf4 / qthMXTQSkz3KfH9R, body 32ibWjFG, type 9,
edy7fq3L owned unit, 03UGMHxF claimed cost; expected reset price is one gem.
Verify the authoritative price source and unit targeting. The current handler
acknowledges type 9 without implementing it.

Atomically validate ownership/eligibility and balance, refund spent SP into
available SP, clear acquired nodes, set used SP to zero and charge once.
Preserve total earned SP and its cap; reset must not grant additional SP.
Return the decoded full unit refresh and gem/team refresh so both screens update.
Keep the transaction-commit confirmation used by FeSkillGet.

Define and test an already-empty reset policy that cannot repeatedly charge
for no change. Investigate retry identity: distinguish an immediate retry from
a new reset after repurchasing. Do not promise durable retry guarantees the
protocol cannot support. Do not reset the player's actual Vargas as a test.

Expand scripts/test_fe_skill_qc_wire.py for refunds, one-gem cost, empty reset,
no gems, wrong/foreign/missing unit, malformed price/IDs, rollback, retry,
repurchase and restart. Preserve purchase and fusion tests and other ShopUse
actions. Client test: note skills/SP/gems, reset, reopen/reconnect, repurchase;
only run on the player's chosen unit with sufficient gems.

## 3. P1: make QA failures trustworthy

scripts/run_bugfix_regressions.py currently treats any nonzero exit for a suite
in KNOWN_OPEN as an accepted known failure. A traceback, startup error or new
failed assertion in that suite can therefore be hidden. Fix this classification:
only exact documented expected assertion failures may be accepted, with valid
completion and expected check counts. All unexpected exceptions, missing
results and extra failures must fail the run. Prefer structured results or
explicit expected-case identifiers over a whole-suite exemption.

Remove Continue and SP-reset exceptions once their fixes pass. Add small runner
tests covering expected failure, extra failure, traceback/startup failure,
missing summary and normal success. Keep validator baseline separate; 3,117
findings is a known baseline, not a clean validation result. Check baseline
identity/content where available, rather than accepting arbitrary exit 1.

Run targeted suites while developing, then the complete existing runner once
against the final source/data. Record genuine failed checks even if deferred;
never claim all tests green by relabeling failures. Preserve evidence for the
old reproducers without accumulating another fixture tree per attempt.

## 4. P1: refresh and deliver the minimal evolution test bundle

The Session 5 bundle was NOT delivered. Its manifest incorrectly assumes owned
Goblin 10050 x4 and Mimic x7; the handoff found no Goblin and only two Mimics.
Read current inventory again; it may have changed further. Inspect the existing
scripts/grant_test_bundle.py, its tests, and scripts/test_bundles/evolution_2026-10-02.json.

Choose supported recipes and suitable disposable base units that permit a real
BB/SBB before/after comparison, not solely early forms that have no SBB. Verify
recipe data, maximum-level prerequisites, skill IDs/levels, all materials,
fusion resources, currency and capacity. If current data cannot support a
particular comparison, state that rather than inventing a recipe or skill.

Revise the minimal manifest to cover missing bases/materials/Burst Queens and
leveling needs. Test on a copy, then deliver via existing presents/admin tooling
with a SQLite backup and durable duplicate-grant protection. This fulfills the
player's outstanding request for test supplies; do not leave it as another
undelivered draft. Do not evolve, fuse, reset or overwrite the player's valued
units yourself. Report exactly what was granted, how to claim it and the expected
BB/SBB/leader-skill values before and after each test evolution.

Sphere House requires mission 21 according to Session 5. Check current progress
and give the next legitimate stage/feature step. Do not globally unlock maps or
rewrite clears to bypass testing the progression fix.

## 5. Build/deploy/artifact rules

Use the existing out/build/debug-win64 tree, VS x64 MSVC, rebuild.bat and CMake
presets. No new full build trees or dated helper scripts. Batch KDL edits (even
comments regenerate headers). Check for other active builds; never race shared
packet generation. Follow the handbook's temporary runtime-output override to
out/qa/current/bin if needed, and remove it before the normal deployment relink.

Use out/qa/current/<suite> for reusable fixture/config/log output and existing
out/backups for identified recovery points. Explicit test configs and verified
isolated ports are mandatory. Use the corrected bf_testkit process ownership
logic; never blanket-kill servers or use taskkill /T alone for tests. Finish test
processes before the normal bf_ctl deployment, since it stops all server instances.

Back up the latest live save, deploy tested changes to the normal executable,
verify its path/timestamp/listener and save integrity, and leave the player ready
to reconnect. Never substitute a fixture save. Preserve all prior backups and
unique failing evidence. Cleanup only proven regenerable, unused files inside
verified workspace paths; do not purge the historical out/ tree blindly.

## 6. Defer these rather than expanding the run

- Crafted-copy temporary-ID favorite/sale retry ambiguity: document current
  limitations; don't guess which indistinguishable copy the user intended.
- Town refresh during a session with no mission completion: remains a separate
  lifecycle issue. Any later entry refresh must respect pending TownUpdate taps.
- Reticle 0,70 is empirical calibration for dummies only. Await the player's
  check; do not mass-apply it to the 22 special bosses.
- Missing Trials/boss/Fire Mecha content, Tilith crop, other unit gaps and broad
  performance work stay in the backlog. Reuse prior investigations, not a broad
  new content pass in this run.

## Required handoff

Write docs/CLAUDE_SESSION_6_HANDOFF.md and update the current client checklist
in place. Include changed files in parent/submodule, binary evidence, migration
and retry policies/limitations, exact test results, deployment identity, backup
and supply receipts, remaining blockers and the ordered client tests below.
Clearly separate player-confirmed results, server-tested fixes and unknowns.
Do not rewrite old historical claims as fresh observations.

Client queue: (1) Continue/resume after its fix using a suitable funded fixture;
(2) SP reset and repurchase after its fix; (3) claim bundle and compare evolution
skills; (4) Merit 400 shards for 20,000 points and correct remaining stock;
(5) first-clear final-stage next-area animation; (6) all dummy reticles and saved
conditions restoration; (7) Sphere House after legitimate mission-21 progress,
craft/equip/favorite/sell/reconnect. Record counts and immediate versus relaunch
behavior. Leave mutation/retry fault injection to automated copied fixtures.
