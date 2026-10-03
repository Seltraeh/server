# SP skill purchase implementation

The client screenshot reports unsupported request `nSQxNOeL` when confirming
Vargas's 10-SP enhancement. The request and response KDL already existed,
but no handler was declared or registered. `FeSkillGet.cpp` now implements it;
the registration pairs `nSQxNOeL` with encryption key `nZ2bVoWu`.

## Binary cross-reference evidence

Rechecked the local ARM64 libgame.so using tools/bin/so_disasm.py and so_xref.py.
Raw output is in out/fe_purchase_2026-10-01/{disasm,refresh_disasm}.txt and
skill_writer_xrefs.txt. Addresses are for that binary, not the Windows executable.

- FeSkillGetRequest::setParam at 0x13CA750 and createBody at 0x13CA7F4:
  one bx56032l entry, pn16CNah = species, edy7fq3L = owned unit,
  ri6D9yBi = chosen skill. The scene calls this from feSkillGetConnect at
  0x1BDF7AC. Existing packet-generator/assets/net/handlers.kdl matches.
- UnitDetailVirtuallyInfoScene::skillTermCheck at 0x1BE21E8 checks isFeSkill,
  prerequisite type 1 via isFeSkill, available SP against getNeedBP, and
  spent SP plus cost against getFeMaxUsableBP (0x1BE2620–0x1BE2660).
- checkConnectResult at 0x1BDCF1C calls checkResponseMessage; it does not
  apply SP or acquired-skill changes. UserUnitInfoResponse::readParam calls
  setFeSkillInfo at 0x1405E38. The reply therefore sends the entire owned
  roster under 4ceMWH6k, including Fnxab5CN and the SP counters.
- Existing FeSkills.hpp follows setFeSkillInfo at 0x12B7B14:
  category@skill:skill/category@skill. The handler uses that serializer.
- gen_fe_skill_mst.py --check confirmed all 2,899 costs against the served
  Ver618 file and all 5,124 tree nodes against the served Ver598 file.

## Server behavior

Ownership, species, skill-tree membership, max-level Omni/SBB-10 eligibility,
prerequisites, available SP and spending cap are checked before mutation.
Cost and category come from server MST data. Acquiring the skill, updating both
SP counters and preparing the roster reply occur in one SQLite transaction.
Failure rolls back. Invalid or stale selections return a recoverable Home
message. Retrying an already-acquired skill returns the roster without spending
again, including delayed A/B/A requests. No extra request/response fields were
invented; the KDL change records the implementation and reverse evidence.
The handler awaits Drogon's commit callback before returning success; the
first test run exposed a commit/response race when a database reader arrived
between preparing the reply and the asynchronous commit.

## Automated QC

- New scripts/test_fe_skill_purchase_wire.py: 27 passed, 0 failed. Covers exact
  cost/cap, multiple skills/categories, full roster/SP wire fields, duplicate
  and concurrent retries, A/B/A, prerequisites, low SP, level/SBB eligibility,
  foreign/missing units, wrong species, invalid IDs, forced database rollback,
  restart persistence and the next UserInfo refresh.
- Existing fusion/SP suite: 36 passed, 0 failed. Corrected its fixture to
  explicitly replay the SP migration on a copied pre-SP schema. Its previous
  assumption that all live units still had 10 SP failed after real frog fusion;
  the test never rewinds or edits the live save.
- Existing evolution suite: 23 passed, 0 failed.
- Handler registry audit: 153 registrations, 0 structural errors.
- Skill MST consistency check: passed; parent/submodule diff whitespace checks
  passed (Git reports existing line-ending conversion warnings).

Logs: out/fe_purchase_2026-10-01/purchase_tests_final.log,
fusion_tests_final.log, evolution_tests.log and handler_audit.log. All mutation
tests use copied databases and ports distinct from the live server.

## Deployment

Rebuilt the normal checkout target successfully and restarted it through
tools/bin/bf_ctl.ps1. The server on port 9960 runs
out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe. A non-mutating
empty purchase request now receives the handler's recoverable "Invalid
enhancement selection" response (command 6), not Unsupported request.

The SQLite backup is out/fe_purchase_2026-10-01/before_deployment.sqlite;
the previous executable is gimuserverw_before.exe in the same directory.
Every database table compared equal across deployment and quick_check passed.
Vargas 1034 remains level 150, Current SP 100, Used SP 0, limit 100, no acquired
skills. No live purchase was performed. Build log: current_link.log;
verification: deployment_verification.json and live_probe.json in that folder.
Changes remain uncommitted in the current repository and packet-generator.

## Scope and client checks

This implements purchase, not ShopUse type 9 reset. Reset remains unfinished
and must not be marked passed by the earlier combined purchase/reset smoke.

After reconnecting to the updated build:

1. With Vargas at 100 SP, buy the 10-SP Atk/Rec node. Expect Current SP 90,
   Used SP 10, the acquired marker, and no unsupported-handler dialog.
2. Close and reopen Enhancements, open unit details, then reconnect. Confirm
   the same skill and counters remain. Confirm the unit roster stays intact.
3. Buy the 10-SP Def/HP node. Expect Current SP 80 and Used SP 20.
4. Check fusion eligibility: at Current SP 80 + Used SP 20, Vargas still has
   all 100 earned SP, so an SP-only frog should not grant more SP. Use a
   below-cap eligible unit to test earning SP; an ordinary Burst Frog adds 1.
5. Check selected enhancements in a battle, including the stat/effect changes
   appropriate to the purchased node. Automated wire tests cannot certify
   the client's visuals or battle-effect interpretation.

Do not reset the live unit merely to make room for a test. Spending allocates
already-earned SP; it does not lower available-plus-used SP below the cap.

## Update 2026-10-02 (Session 6): the type-9 reset is implemented

The "Scope" note above is historical. ShopUse type 9 now resets a unit's SP
enhancements (`resetFeSkills` in gimuserver/gme/handlers/ShopUse.cpp): every
spent SP returns to available, the acquired skills clear, used SP goes to 0,
the cap and the total are unchanged, and DefineMst reset_fe_skill_dia_count
(1 gem) is charged once in a transaction that must commit before the reply.
The reply is FeSkillGet's shape (team_info + the full 4ceMWH6k roster), which
UnitDetailVirtuallyInfoScene's state 2 redraws from.  An already-empty unit or
a short balance changes nothing and resynchronises.  The decoded client
contract, the retry limit (a copy delayed past a repurchase cannot be told
from a new reset) and the test results are in
[the Session 6 handoff](CLAUDE_SESSION_6_HANDOFF.md).  FeSkillGet itself is
unchanged apart from using the shared commit awaiter (gme/common/Transactions.hpp).
