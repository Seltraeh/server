# Claude Session 6 — handoff

Assignment: [CLAUDE_SESSION_6.md](CLAUDE_SESSION_6.md). One agent, October 2,
14:10–15:30. Nothing is committed or pushed. Labels follow the handbook's
evidence model: **binary-confirmed** (arm64 `libgame.so`, addresses for that
build), **server-tested** (isolated server on a copied save), **player-confirmed**
(only what the player reported), **unknown**.

State found at the start: the live server was **down** — its 14:00 start (PID
7236, Session 5's reticle deploy) ended with the clean-shutdown line
`DebugCli: pipe server exiting.` at 14:02:28 and no crash dump; cause not
recorded. No other build was running and the CMake cache had no runtime-output
override. A peer "Claude session 5" window was open and idle.

## 1. P0 — Continue retry protection: FIXED (server-tested)

### 1.1 What the client actually sends and does (binary-confirmed)

- **No nonce.** `MissionContinueRequest::createBody` @0x13A7528 writes the user
  tag, the signal key, the version tag, `Kz7qfSs5 {k9cxD7Ba serial, j3g5P4cq 4|6}`
  and `5PR2VmH1 {k9cxD7Ba serial, K2gIYm0h createSuspendData(true)}`.
- **A retry is byte-identical.** The body is built once (`BaseRequest::create` →
  `createBody`, vtable slot +0x30 → +0x60). `BaseRequest::getSendData` @0x139FDB8
  only serialises the stored groups and AES-ECB-encrypts them (deterministic).
  Both retry paths re-send the same object: `HttpConnector::retry` →
  `WrapAsyncHttpConnector::retry` @0x1005ED8 → `AsyncHttpConnector::start`, which
  calls `getSendData` (slot +0x38) on the same `RequestData`; and
  `NetworkManager::OnErrorRetryPressed` @0xF5DCCC re-sends the same `CCHttpRequest`.
- **A later genuine revival carries a new blob**: `MissionScene::createSuspendData`
  @0x18197AC writes `BattleManager::getTurnCnt`, `BattleParty::getTurnCount`, every
  unit's HP/buffs/gauges and the battle, skill and item logs. It has no
  continue counter.
- **Candidates rejected as identity.** `6FrKacq7.Kn51uR4Y` is the server's own
  signal key echoed back (`SignalKeyResponse::readParam` @0x13EF150 is the only
  `setSignalKey` caller, `createSignalKeyTag` @0x13A0E94 the only reader) and this
  server echoes a constant. The envelope's `F4q6i9xe.aV6cLn3v` is
  `IntToString(this+8)` = `CCObject::m_uID`, a process-wide creation counter
  (`CCObject::CCObject` @0x1F01E9C): unique per request object, the same on a
  retry, restarted every launch, never passed to handlers, and unchecked on the
  Windows build. Documented in `net/gme.kdl`, not used.
- **The reply decides the revival.** When a request finishes, `GameScene::update`
  @0x1601470 calls the scene's `checkConnectResult` (slot +0x310) before clearing
  the in-flight bit. `MissionGameOverScene` inherits `GameScene`'s, which shows
  any error through `checkResponseMessage` @0x1615E4C. With no error,
  `loopContinue` @0x17FB474 calls `MissionScene::requestContinue`: **an ordinary
  reply IS the revival**. Error commands: 4 Close → app exit; 6 ReturnToGame →
  Home; **2 Retry → notice -4000**, which `GameScene::noticeOK` @0x16098AC ignores
  (its table covers -3999..-3989) and `MissionGameOverScene::noticeOK` @0x17FB62C
  turns into state 2 = `initContinueConfirm` (4 in Frontier Gate): the Continue
  prompt again, no revival. *Inferred, not traced:* that `loopContinue` does not
  run under the notice (the scene-manager code that pauses a parent was not
  found; the -4000 handler only makes sense if it does not).

The earlier handler comment ("loopContinue resumes regardless of what came
back", hence "charge, but never refuse") was an incomplete reading; corrected
in the code and `net/mission.kdl`.

### 1.2 Policy (gimuserver/gme/handlers/MissionBreak.cpp)

- **Identity** = (user, run serial, SHA-256 of `"<status>:<blob>"`). The run is the
  serial: issued serials are unique (`user_mission_runs` autoincrement); a
  pre-serial battle uses its mission id, and no new pre-serial run can exist once
  any serial has been issued (MissionEnd writes 0, never NULL).
- **Durable receipts**, migration `02102026_MissionContinueReceipts`:
  `user_mission_continue_receipts (user_id, run_serial, fingerprint, seq, accepted,
  gems_charged, first_seen, last_seen, deliveries)`, PK on the first three. Every
  revival the run has judged is remembered, not just the latest.
- **Accepted** (owned, consistent, affordable, unseen or a re-tried refusal that is
  still the newest): receipt (`accepted 1`), guarded charge (`gems >= cost`,
  affected must be 1), "continued" mark, resume record — one transaction, and
  the ordinary reply goes out only after `gme::CommitTransaction` reports the
  commit.
- **Repeated** (fingerprint already accepted in this run): ordinary reply, no
  charge, resume record untouched (`deliveries` +1). This is A/A and A/B/A.
- **Refused** — not the open battle (superseded, settled, foreign, never issued,
  two serials, status 0), **no gems**, or a late copy of a refused try after the
  run has seen a newer revival — gets `HandleResult::refuseToRetry` (cmd 2) and
  changes nothing chargeable: no gem, no resume update, no "continued" mark. A
  no-gems try leaves only an `accepted 0` receipt, used solely to recognise a
  stale copy; the same revival is accepted later once affordable (not poisoned).
- **Faults**: any exception or failed commit rolls everything back (receipt
  included) and answers cmd 2 ("could not be saved"), so the player's next try is
  judged fresh. A catch-all rollback guards non-`std::exception` throws.
- **Limit (stated, not hidden):** two *genuine* revivals of one run with
  byte-identical status and blob would be taken for one, the second free. The
  blob's turn counters and logs make that unreachable in a real battle; the cost
  of the error is one free revival. Behaviour change: the no-gems case used to
  be a free, recorded revival; it is now refused (the decoded cmd-2 path).

### 1.3 Tests (QA build, isolated servers, copied saves)

| Suite | Result | Notes |
|---|---|---|
| `test_continue_delayed_retry_wire.py` (rewritten) | **34/0** | A/A, A/B/A (+header gems), receipts order, status-6 distinct, 3 concurrent copies, closed run, superseded run, same blob in the next run of the same mission, foreign/never-issued/two-battle/status-0, no gems → funded later → repeat, stale copy of a refused try, injected receipt-insert fault + retry, server restart (repeat + new) |
| `test_mission_run_ownership_wire.py` | **57/0** | Expectations updated to the decoded contract: B1/B5–B8 refused with Retry; B9 no gems refused, not marked; B10 Retry error; B12 accepts either race order (paid xor refused) |
| `test_mission_settlement_wire.py`, `test_mission_run_edges_wire.py` | 22/0, 2/0 | unchanged |
| `test_mission_progression_wire.py` | 50/0 | after a fixture fix — see §3.3 |

Before-fix evidence on the unchanged 11:54 executable (same copied save):
`out/qa/current/test_continue_delayed_retry_wire_before_original.log` — the
original reproducer, **1 passed / 1 failed** (A/B/A: 8 → 7 gems, blob A replaced
B); `…_before_new.log` — the new suite fails at A/B/A, then stops on the missing
receipts table.

## 2. P1 — SP reset (ShopUse type 9): IMPLEMENTED (server-tested)

### 2.1 Client contract (binary-confirmed)

- `UnitDetailVirtuallyInfoScene::resetConnect` @0x1BDEBDC: ShopUse type 9, price
  `DefineMst::getResetFeSkillDiaCnt`, ExtCnt 0, `setUserUnitID(this+0x358)`.
  `ShopUseRequest::createBody` @0x13AC814: `32ibWjFG [{60IsqxDt, 03UGMHxF, 5gXxT7LZ,
  rA9jDCP5, edy7fq3L (only if set), 9i2xhMaJ}]` plus the signal key — no nonce,
  nothing about the unit's state.
- `resetBtnSet` @0x1BE2C88 disables the button while `getFeUsedBP()` is 0;
  `resetDialog` @0x1BDF278 compares `UserTeamInfo::getBraveCoin` with the price
  (`SHOP_SHORTAGE_BRAVECOIN_*` vs `UNIT_VIRTUALLY_RESET_*`).
- After the reply, `updateEvent` state 2 runs `GameScene::updateHeader` (gems from
  team_info) and `drawUP` → `drawUI`/`drawList`/`pointDraw`, which re-resolve the
  unit by id and read `getFeBP`/`getFeUsedBP`/`getFeMaxUsableBP`. The client changes
  none of these itself, so the reply is team_info + the full `4ceMWH6k` roster
  (FeSkillGet's shape, which the player already saw work for purchases).
- **Price**: `5csFoG1G` = **1** in `deploy/mst/defines_mst.json` (the served
  DefineMst the client reads), as is the Continue price `QW3HiNv8`.

### 2.2 Policy (`resetFeSkills` in ShopUse.cpp)

Owned Omni unit (rarity ≥ 8, FeSkillGet's rule) and non-negative SP fields, else
`refuseToHome`. If used SP is 0 and no skills are acquired — the client's
"disabled" state — nothing is charged and the reply resynchronises. If the
balance is short, nothing changes and the reply resynchronises (the client's
next tap then shows its own shortage dialog). Otherwise, in one transaction:
guarded charge of the server's price (a different claimed price is logged and
ignored), `fe_sp = fe_sp + fe_used_sp`, `fe_used_sp = 0`, `fe_skill_info = ''`
(guarded on the values read), cap untouched — total preserved, nothing granted —
and success only after the commit. Failures roll back and go Home.
**Retry limit:** a copy of a reset is harmless (it finds the unit empty), but a
copy delayed until after the player re-bought skills is indistinguishable from
a genuine second reset and would reset and charge again (SP refunded, never
lost). The protocol has no identity to do better; nothing durable is promised.

### 2.3 Tests

`test_fe_skill_qc_wire.py` (expanded) **37/0**: price from DefineMst; purchase
smoke; refund of three skills in two categories, one gem, total preserved,
cap kept, full roster and team_info on the wire, bystander units unchanged;
immediate retry free; already-empty unit (37 SP) free ×4; claimed price 5/0/-3
→ server's 1; no gems → unchanged resync; missing/empty/zero/negative/
malformed/overflowing unit id, malformed price, non-Omni (10016), another
player's unit, corrupt negative SP all refused unchanged; trigger-forced
rollback then retry; 3 concurrent resets charge once; repurchase then a genuine
second reset; ShopUse type 2 unchanged (no roster); restart persistence and
login refresh. Also `test_fe_skill_purchase_wire.py` 27/0 and
`test_enhancement_sp_wire.py` 36/0 (purchase and fusion preserved).
Before-fix: original reproducer on the 11:54 build **4/2**
(`test_fe_skill_qc_wire_before_original.log`); new suite **11/26** (`…_before_new.log`).

## 3. P1 — QA results are trustworthy again

### 3.1 Runner (scripts/run_bugfix_regressions.py)

`classify()` now needs positive proof that a suite finished: exit 0, its own
completion line (Checker `name: N passed, M failed` agreeing with its PASS/FAIL
lines; or a `N failure(s)` trailer; or a pinned number of `PASS:` phases; or a
success marker for `audit_handlers.py` / `gen_battle_simulator.py --check`), at
least one check, no traceback. `KNOWN_OPEN` is gone; `EXPECTED_FAILURES` accepts
a failing suite only for exactly the listed FAIL labels and pass count, and it
is **empty** now. An expected failure that stops happening fails the run until
its entry is removed. A server that will not start for a save-suite is recorded
as a failed entry instead of crashing the runner. Proven on real evidence: the
old 11:49 Continue reproducer log, which the previous runner labelled "known
open", classifies as FAILED.

`validate_missions.py` runs with `--json` and must match
`scripts/qa_baselines/validate_missions.json` (recorded 15:09 and reviewed:
**3,117 = H15 842 + M2 20 + M3 921 + M4 1334**, content digest `7bae35bb…`) by
per-check counts, content digest and exit status. It is reported as a
*baseline*, never as passing. Re-record deliberately with
`--record-validator-baseline`.

### 3.2 Runner tests

New `scripts/test_qa_runner.py` **30/0** (also an entry in the runner): normal
success; exact expected failure; extra, different, missing-pass, stale and
traceback-after expected failures; startup traceback with no summary; missing
summary; empty output; zero checks; summary disagreeing with lines; exit 0 with
a FAIL; ordinary failure; timeout; the `failure(s)`, PASS-phase and marker rules;
validator identity order-independence, match, new finding, same-count content
change, wrong exit, no report, no baseline.

### 3.3 A genuine failure found and explained: fixture drift

`test_mission_progression_wire.py` failed 2 checks ("first 11: … the Farm its
first harvest") on the QA build **and identically on the unchanged 11:54
build** (`test_mission_progression_wire_1154exe.log`,
`…_fixture_drift.log`). Cause: the suite un-clears missions 11/12 in its copy
of the live save but kept the Farm/Mountain rows the player rolled and harvested
at 12:44 (period not yet expired) — a rolled tile behind a closed gate, which
real play cannot produce. The fixture now resets tiles 1 and 3 with the
missions it un-clears (its own stated precondition); 50/0 after. Not a code
regression; the evidence logs are kept.

### 3.4 Complete run (final source and data)

`python scripts/run_bugfix_regressions.py out/qa/current/bin/gimuserverw.exe`,
15:09:42–15:15:41, QA executable 31,536,640 bytes, 14:52:54, sha256
`acae868cf2c3…`; source parent `7851dba` + 183 changed paths (diff sha256
`545876c58fd3`), packet-generator `2efdaa5` + 16 (`e365becec64d`).
**41 entries, 1,612 checks passed, 0 failed, no expected failures; validator
matched its baseline (3,117 findings).** `out/qa/current/SUMMARY.md` / `.json`.
Brave slots ran 249 checks (252 at 11:47): it adds checks only when random pulls
win units/items (`if want_u` / `if want_i`) — legitimate variance, like the
reward-lock suite's 22–24. No code changed after this run; only documentation.

## 4. P1 — evolution test bundle: REVISED, TESTED, DELIVERED (see §6)

Inventory read (read-only) at 14:30: 93 units in a 110-slot box (100 + 10 bought,
**17 free**), 2,242,864 Zel, 5 gems, no unclaimed presents. Changed since
Session 5: a **Goblin 10050 (uu 1228)** and **three Mimics** are back; the
14 presents tagged `Client QA supply bundle 2026-09-30` are that earlier bundle
(claimed Oct 1); the `Client QA evolution bundle 2026-10-02` tag was never granted.

Why a new chain: Goblin→Redcap, Deemo 4★→5★ and Selena→Ice Selena are early forms
with no SBB on either side. The revised bundle uses one **disposable Inferno
Berdette** through two real recipes (`unit_evo_mst`), so both SBB cases occur:

| Step | Unit | Lv | BB (id, lv) | SBB (id, lv) | UBB | Leader skill |
|---|---|---|---|---|---|---|
| start | Inferno Berdette 5★ 10824 | 1/80 | 10824 Ruse Filo, 1 | — | — | 7401 High Ruler's Chakra |
| + Rainbow Crystal, + Burst Queen | same | 80/80 | 10, | — | — | 7401 |
| evolve (Miracle Totem, 2 Fire Totem, Dragon Mimic, Metal Mimic, 500,000 Zel) | Hades Flame Berdette 6★ 10825 | 1/100 | 10825 Ruse Capture, **5** | 110825 Era Cremation, **locked (0)** | — | 7418 Flame Spider's Threads |
| + Rainbow Crystal, + Burst Queen | same | 100/100 | 10 | **10** (unlocked) | — | 7418 |
| evolve (Fire Mecha God, Miracle Totem, Fire Totem, 2 Metal Mimic, 1,500,000 Zel) | Tartarus Blaze Berdette 7★ 10826 | 1/120 | 10826 Ruse Eclipse, **5** | 110826 Terrible Cremation, **5** | 210826 Promethea (client-derived) | 7419 Flame Spider's Silk |

Rules (UnitEvo.cpp): base at max level; BB → max(1, BB/2); an unlocked SBB →
max(1, SBB/2); a locked SBB stays locked until BB 10. A Rainbow Crystal gives
2,005,024 fusion EXP (≥ 1,137,652 for 5★ lv 80 and ≥ 1,999,997 for 6★ lv 100) —
the only way to level both within the box. One Burst Queen took the 6★ from
BB 5/SBB locked to BB 10/SBB 10. Every unit resolves in `deploy/archive/unit.json`
and ships its art. Whether the 7★ shows its UBB button at SBB 5 is the client's
own rule — unknown.

Optional, the player's choice: their Selena (uu 1000, 12/12, BB 10, favourite,
Muramasa) → Ice Selena 20012 with the bundled Water Nymph: BB 20011 → 20012 lv 5,
LS 110 unchanged. Their Goblin (uu 1228) → Redcap 10051 needs nothing new (owned
Mimic + a Metal King; BB → Red Strike, LS none → 1400 Fighting Light).

Manifest `scripts/test_bundles/evolution_2026-10-02.json`: 16 units (Berdette,
2 Rainbow Crystal, 2 Burst Queen, 2 Miracle Totem, 3 Fire Totem, Dragon Mimic,
3 Metal Mimic, Fire Mecha God, Water Nymph) + 2,000,000 Zel (both evolutions, so
the player's own Zel is untouched). `test_grant_bundle_wire.py` (rewritten)
**55/0** on a copy: grant, exact rows, receipt + backup, retry refused
("Already delivered"), every present claimed through PresentReceipt, the whole
chain through UnitMix/UnitEvo with the table above checked at each step,
recipe Zel exact, materials consumed, the login roster shows the 7★, the
optional Selena path, and none of the player's own units touched.

Side finding, deferred: `UnitArchiver::populatePacket` never sets
`leader_skill_id`, so archive-granted units (presents, exchanges) store 0 —
22 of the player's units, Vargas and Selena included. It does **not** change
what the unit screen or ordinary battles show: those read the client's own
`UnitMst::getLeaderSkillID` (BattleParty::setPartyPassiveList,
UnitDetailUserUnitScene::setLeaderSkill, …). `UserUnitInfo::getLeaderSkillID`
has 10 callers: Training Grounds (`PlayerParty::entrySandbag`), arena copies and
campaign/Frontier Gate save-data echoes — so a Training Grounds leader skill may
be missing there. Not fixed in this bounded run.

### 4.1 Sphere House

Cleared: 1, 2, 10, 11, 12, 20 (+ special ids). Mission 21 "Wielder of the Fire"
needs 20 (cleared), and the Sphere House (`town_facility_mst` 1) needs 21. The
next legitimate step is simply to play and clear mission 21; nothing was
unlocked or rewritten.

## 5. Files changed (this session)

Pre-edit copies of every touched file: `out/backups/2026-10-02_session6/source_before/`.

Parent (`C:\Users\Evan\BF\BF-WorkingDirRust`):
- `gimuserver/gme/handlers/MissionBreak.cpp` — receipts, verdicts, Retry refusals, commit await
- `gimuserver/gme/handlers/ShopUse.cpp` — type 9 `resetFeSkills`
- `gimuserver/gme/handlers/FeSkillGet.cpp` — uses the shared awaiter (logic unchanged)
- `gimuserver/gme/common/Transactions.hpp` — new: `gme::CommitTransaction`
- `gimuserver/gme/handlers/Handlers.hpp` — `HandleResult::retry` / `refuseToRetry`
- `gimuserver/gme/handlers/GmeControllerHandlers.cpp` — Retry → `GmeErrorCommand::Retry`
- `gimuserver/db/MigrationManager.cpp` — `02102026_MissionContinueReceipts`
- `scripts/test_continue_delayed_retry_wire.py`, `scripts/test_fe_skill_qc_wire.py`,
  `scripts/test_grant_bundle_wire.py` — rewritten/expanded
- `scripts/test_mission_run_ownership_wire.py` — refusal expectations
- `scripts/test_mission_progression_wire.py` — fixture precondition (§3.3)
- `scripts/run_bugfix_regressions.py` — classification, baseline, bundle suite added
- `scripts/test_qa_runner.py`, `scripts/qa_baselines/validate_missions.json` — new
- `scripts/test_bundles/evolution_2026-10-02.json` — revised
- `docs/CLAUDE_SESSION_6_HANDOFF.md` (this), `docs/CLIENT_REMAINING_2026-10-02.md`,
  `docs/BF_OFFLINE_SERVER_HANDBOOK.md` (refusal contracts, runner rules),
  `docs/FE_SKILL_PURCHASE_2026-10-01.md` (dated update appended)

Submodule (`packet-generator`), documentation only — generated code identical
apart from comments (checked by generating into a scratch dir and diffing):
- `assets/net/mission.kdl` — MissionContinue reply semantics, retry identity
- `assets/net/gme.kdl` — `aV6cLn3v` is the request object's `m_uID`; Retry's meaning
- `assets/net/signal_key.kdl` — server-issued echo token
- `assets/net/handlers.kdl` — type-9 request gating and identity
- `assets/mst/define.kdl` — `reset_fe_skill_dia_count`

`scripts/grant_test_bundle.py` is unchanged (its tag check is durable: claimed
presents keep their rows with `is_receipt = 1`; nothing deletes them).

## 6. Build, deployment, backups and the supply receipt

- **Build.** One tree, `out/build/debug-win64` (Ninja Multi-Config, VS MSVC
  14.51.36231) via `rebuild.bat -Jobs 6`. The live server was already down, but
  the QA build still went to the handbook's override
  `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=out/qa/current/bin` (14:49–14:52, the KDL
  documentation edits regenerated headers: full 209-step build) so the deployed
  EXE stayed the tested 11:54 one until deployment. KDL was pre-validated by
  generating into a scratch directory: generator exit 0 (only the long-standing
  `dbb.kdl` import-cycle advisories) and code identical to the old header except
  comments. Override removed with `-U` at 15:16; the deployment build was a
  relink only (`[3/4] Linking … standalone_frontend\Debug\gimuserverw.exe`,
  nothing recompiled); the cache holds no override. Log: `out/qa/current/build.log`
  (Session 5's moved to `out/backups/2026-10-02_session5/build_session5.log`).
- **Deployed** `out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe`,
  **2026-10-02 15:16:30, 31,536,640 bytes**, sha256 `a1413a12b157…`. It differs
  byte-wise from the QA EXE (`/INCREMENTAL` relink of the in-tree image) but
  links the same `gimuserver.lib` (14:52:42); every new code string
  (`02102026_MissionContinueReceipts`, `ShopUse: reset user unit`, …) is present
  in it and absent from the 11:54 build.
- **Started** 15:17:24 with `tools/bin/bf_ctl.ps1 -Target server -Action start`:
  **PID 23040 owns 127.0.0.1:9960**, console child 22904, stdout
  `deploy/log/server_stdout.log`. Log: 1,339 mission and 12 AI archive records,
  79 migrations found, `Execute migration 02102026_MissionContinueReceipts`,
  listening at 15:17:28. `GET /offline_mod/fps_cap` → 200. Save
  `PRAGMA integrity_check` ok; receipts table present, 0 rows; 80 migrations
  recorded. No gameplay request was sent to the live server.
- **Backups** (`out/backups/2026-10-02_session6/`): `gme_before_session6.sqlite`
  (backup API, 15:16, integrity ok, sha256 `09ad3508c752…`; 67 presents, 93 units,
  5 gems, 2,242,864 Zel); `gimuserverw_1154_before_session6.exe` (rollback EXE,
  sha256 `6ac7b8be0ef3…`); `server_stdout_1400_run.log` (the only trace of the
  14:02 stop); `source_before/` (pre-edit copies of the 22 touched files);
  `worktree_identity_before.txt`.
- **Supply receipt**: `scripts/grant_test_bundle.py` with the server stopped,
  15:17:11 — **presents 68–77** queued for `CLz1ad9x` (unit box 93/110 before,
  16 incoming), its own backup `before_grant_20261002_151711.sqlite` (identical
  to the pre-deploy backup) and `grant_receipt_20261002_151711.json`. Re-running
  it against the live save was refused ("Already delivered: 10 present(s) …;
  nothing granted"), and the tag check is durable because claimed presents keep
  their rows. Nothing else in the save was edited.

| Present | Type | Target | Count |
|---|---|---|---|
| 68 | unit | 10824 Inferno Berdette 5★ | 1 |
| 69 | unit | 750006 Rainbow Crystal | 2 |
| 70 | unit | 750004 Burst Queen | 2 |
| 71 | unit | 50354 Miracle Totem | 2 |
| 72 | unit | 10133 Fire Totem | 3 |
| 73 | unit | 60144 Dragon Mimic | 1 |
| 74 | unit | 60224 Metal Mimic | 3 |
| 75 | unit | 10354 Fire Mecha God | 1 |
| 76 | unit | 20130 Water Nymph | 1 |
| 77 | Zel | — | 2,000,000 |

Claim: Home → Present Box → claim all (16 units fit the 17 free slots).

## 7. Player-confirmed vs server-tested vs unknown

- **Player-confirmed (earlier, not re-tested here):** everything Session 5 lists
  (Farm/Mountain, Farm scene once, spheres through Home/battle/relaunch, unequip,
  slot-2 edit, direct move, mission-11 repeat, Training Grounds entry, dummies
  passive, Training Menu settings, HP/element reload, Vargas SP purchase
  persistence, three Deemo forms, two potions = two consumed).
- **Server-tested this session:** Continue receipts and refusals (§1), SP reset
  (§2), runner rules and baseline (§3), the evolution chain and bundle delivery
  on a copy (§4); the full run (§3.4).
- **Unknown / inferred:** that the game-over scene does not revive underneath a
  cmd-2 notice (inferred from `noticeOK`); whether the 7★'s UBB button shows at
  SBB 5; the Windows client's `aV6cLn3v` behaviour (arm64 only); the cause of
  the 14:02 server stop.

## 8. Deferred, with current limits (not expanded this run)

- **Crafted-copy temporary-ID favorite/sale retries** — unchanged from Sessions 3
  and 5: a crafted copy is named by the client's shared placeholder id until the
  next warehouse list, so a favorite or sale retried across that window cannot
  say which indistinguishable copy was meant; the server does not guess.
- **Town refresh without a mission completion** — tiles refresh at login and at
  each settled MissionEnd only; any later entry-time refresh must respect a
  pending TownUpdate tap flush.
- **Reticle "0,70"** — empirical, dummies only; awaiting the player's check. The
  22 special bosses keep their untested offsets.
- **Archive-granted units store `leader_skill_id = 0`** (§4) — display and
  ordinary battles unaffected; Training Grounds, arena copies and campaign/FG
  save echoes read the stored value.
- Missing Trials/boss/Fire Mecha content, Tilith crop, other unit gaps,
  performance work, GuildUpdate — backlog as before.

## 9. Client tests, in order

The checklist with steps and expected values is in
[CLIENT_REMAINING_2026-10-02.md](CLIENT_REMAINING_2026-10-02.md) ("Session 6"
section). Order: (1) Continue/resume with real gems on mission 20 — 5 gems
cover two Continues and one Reset; (2) SP reset and repurchase on the unit the
player chooses; (3) claim the bundle, run the Berdette chain, compare the table
in §4; (4) Merit 400 Ignis Shards for 20,000 points and the remaining stock;
(5) first clear of mission 23 (Cave of Flames' last stage) and its next-area
presentation; (6) every dummy's reticle and Save/Load Conditions; (7) mission 21,
then the Sphere House: craft/equip/favorite/sell/reconnect. Record counts and
immediate versus relaunch behaviour. Retry and fault injection stay in the
automated copied-save suites.

## 10. Artifacts

| Path | What | State |
|---|---|---|
| `out/qa/current/SUMMARY.md`, `.json`, `runner.log`, `<suite>.log`, `<suite>/` | The complete run | Current |
| `out/qa/current/*_before_original.log`, `*_before_new.log` | Original and new Continue / SP-reset suites on the 11:54 build | Evidence — keep |
| `out/qa/current/test_mission_progression_wire_fixture_drift.log`, `…_1154exe.log` | The fixture-drift failure on both builds | Evidence — keep |
| `out/qa/current/validate_missions.json` | Full validator report behind the baseline | Regenerated each run |
| `out/qa/current/bin/` | QA EXE (the one the full run tested) + PDB + DLLs | 286 MB |
| `out/backups/2026-10-02_session6/` | Pre-deploy save, rollback EXE, 14:00 stdout, grant backup + receipt, pre-edit sources | Recovery points — keep |

Cleanup: `out/qa/current/bin/gimuserverw.ilk` (345 MB incremental-link state of
the QA EXE, regenerated by any QA link, not in use) and
`out/qa/current/mission_progression_1154exe/` (this session's diagnostic fixture;
its log is kept). Nothing else under `out/` was touched.

(Follow-up, 21:00: this folder as it stood is kept in
`out/backups/2026-10-02_followup/qa_current_session6/`; `out/qa/current` now
holds the follow-up runs — see §11.)

## 11. Evening follow-up — the player's report on the 15:17 build

Same session, ~19:50–21:10. Report: Sphere House unlocked, two Continues, SP
reset worked, evolution worked, Merit 400 shards worked; plus "no friends could
be found" now and then, the reticle now BELOW the dummy, and Town labels/tiles
above their buildings. Times are local (the server log is UTC, +4 h).

### 11.1 Player results (player-confirmed; cross-checked in the save and log)

- **Continue:** one run of mission 20 "Cave of Dancing Flames" (serial
  1000000036), revived at 19:01:48 and 19:02:07, 1 Gem each ("3 left", "2
  left"), two accepted receipts (seq 1, 2). The resume-after-relaunch step was
  not exercised (the 19:06 relaunch came after the run had ended).
- **Sphere House:** opened by the first clear of mission 21 "Wielder of the
  Fire" at 16:58 (facility 1, need_mission 21; reported as "mission #20").
- **SP reset:** ShopUse type 9 at 17:09 on Vargas (uu 1034): SP 100 available,
  0 used, no skills; Gems 5 → 4. Repurchase not reported.
- **Evolution:** done on the player's OWN Berdette uu 1014 (the bundle's uu 1239
  is untouched, lv 1): 17:12 Rainbow Crystal lv 5→80; 17:36 10824→10825 BB 10→5,
  no SBB; 18:10 Burst +20 → BB 10, SBB 0→10; 18:14 Rainbow Crystal → lv 100;
  18:15 10825→10826 BB 10→5, SBB 10→5; 18:16 Burst +20 → BB 10, SBB 10. Now 7★
  Tartarus Blaze Berdette lv 5, LS 7419. Every step equals §4's table. The UBB
  button at SBB 5 was not reported.
- **Merit:** 18:30:47, offer 91000059 Ignis Shard ×400 for 20,000; balance
  18,340.
- Save at 20:17 (backup below): Gems 2, Zel 2,242,412, Karma 709,802.

### 11.2 "No friends could be found" — FIXED (cause binary-confirmed, server-tested)

`xZH6EIQ7` is the helper picker list, not a fusion payload: it dispatches to
`ReinforcementInfoResponse`, whose readParam @0x13EB29C calls
`ReinforcementInfoList::removeAllObjects` on row 0 and, at each row's last
field, adds the row only when its user id (`h7eY3sAK`) is non-empty — otherwise
it autoreleases it. UnitMix and UnitEvo sent one `"DecompDev"` row with no user
id there, so every fusion or evolution emptied the picker until the next
FriendGet (sent after a battle result and at login). The log matches: FriendGet
at 16:58 after the mission-21 result, six fusion/evolution replies 17:12–18:16,
then the mission-22 start at 18:31 with no refresh in between — the
`NO_REINFORCEMENT` prompt ("Reinforcements could not be found. Proceed with
Mission?"). Mission 22 "The Thief's Hideout" was cleared 18:32 (reported as
"#23"; mission 23 has not been started).

Fix: `reinforce` is now `optional` in both `UnitMixResp` and `UnitEvoResp`
(net/handlers.kdl, docs corrected; net/unit.kdl marks `UnitReinforceEntry`
retired) and neither handler sets it, so the key is absent. Nothing read the row:
the fusion result screen reads `1ZbHB6Im`, unit cards the `4ceMWH6k` roster, and
the evolution screen `I82p0wCL` — the old comments crediting it with the sphere
and BB display described a row the client had already discarded. Generated code
differs from the previous header only in those two members
(`std::optional<std::vector<UnitReinforceEntry>>`).

### 11.3 Reticle below the dummy — "0,17" (data, server-tested)

The cursor offset is in battle units (the layer is 320 wide), about 1.9 screen
px per unit on the player's 608-px window. "0,-40" sat ~110 px above the body and
the 14:00 "0,70" ~101 px below it; "0,70" had added the 110 px to −40 one for
one. Both screenshots agree on +17. `scripts/gen_battle_simulator.py` regenerated
the six dummies (12 changed lines in deploy/archive/mission.json, nothing else);
`--check` up to date; `test_battle_simulator_wire.py` expects "0,17".

### 11.4 Town labels above their buildings — client layout; 2:3 copy served

Binary-confirmed: `MyTownTopScene::initialize` @0x18F0B34 adds MapVillage.sam at
(160, 202) WITHOUT `GameLayer::getOffsetY`, and pins MapVillageAdd.sam (the river
glints) to the same node Y. `SuperAnimNode::Init` @0xFECD1C sizes the node from
the .sam header (w, h) with anchor (0.5, 0.5) and `draw` @0xFECFBC flips each
sprite with the content height, so only h/2 places the art. The labels and tap
rects (TownFacilityMst / TownLocationMst) hang from the centred 480-point band.
The shipped 640×985 file (the 809-px art under a 176-px sky strip) is aligned on
a 9:16 phone only; on the player's 608×934 window (≈2:3) the art is 44 pt
(~88 px) low. The Appx bundle and the APK ship the identical file (sha256
5bbc14ba…), so this is the client's own behaviour, not a server regression.

New `tools/frame_town_for_2x3.py` writes `content/_dlcbundle/MapVillage.sam`, the
exact path the client fetches (scene 900's resourceMap entry +
`"/_dlcbundle/"`, `CommonUtils::downloadBundlePriority` @0xFB7AF4; until now
that path missed and the name resolver answered with the shipped file). The copy
changes only the header height, 985 → 1161 (= 809 + 2·176): the art top moves
44 pt up to the screen top, and the river glints land on the water. No shipped
file is modified. Because the client never re-downloads a cached file and the
copy has the original's size (so `reset_client_cache.py` cannot see it),
`--clear-client-cache --backup-dir` deleted exactly `LocalState/MapVillage.sam`
(backed up). Trade-off: on a tall (9:16) window the labels would sit 44 pt low;
`--remove` plus the cache step reverts. Tool checks: 10/10 on a temp copy
(apply, idempotence, foreign-file refusals, remove, cache step, source hash).

**⚠ Corrected in §12:** the client does NOT fetch `_dlcbundle/MapVillage.sam`;
after an app reset it fetched `sam/MapVillage/MapVillage.sam`, so this first
deployment changed nothing in the client.

### 11.5 "A feature in the sky later"

The only sky element in this client's Town is the Randall castle
(`ImperialIcon.sam`, top right). It is function release 1, which the server never
sends, and `UserReleaseInfoList::isRelease` @0x129F558 returns true for an absent
id, so it is always shown. Its open-padlock badge is `unlocked_icon.png`, drawn
when `FeatureGatingHandler::setNewForRandall` @0x1C8892C finds one of Randall's
features 11/3/7/13 newly unlocked and not yet entered — "something new inside",
not a lock. The Town's own table has seven facilities, none in the sky: the
Event Bazaar (always), the LS Spheres building after mission 20102 (the wiki:
its materials come "starting from Bectas") and the Music House after mission 33.

### 11.6 Files changed (this follow-up)

- `gimuserver/gme/handlers/UnitMix.cpp`, `UnitEvo.cpp` — no `xZH6EIQ7` row; the
  UnitMix header comment lists what the reply really carries.
- `packet-generator/assets/net/handlers.kdl` — `UnitMixResp`/`UnitEvoResp`
  `reinforce` optional, docs corrected; `assets/net/unit.kdl` —
  `UnitReinforceEntry` marked retired.
- `scripts/gen_battle_simulator.py` — cursor "0,17" with the units note;
  `deploy/archive/mission.json` regenerated (only the six dummies'
  `cursor_disp_pos` changed; ai.json unchanged).
- Tests: `test_fusion_merit.py` (no `xZH6EIQ7`; BB/SBB from `1ZbHB6Im`),
  `test_evolution_qc_wire.py` (+3 checks), `test_battle_simulator_wire.py`
  ("0,17"); fixture fixes in `test_mission_run_ownership_wire.py`,
  `test_continue_delayed_retry_wire.py`, `test_grant_bundle_wire.py`,
  `test_debug_cli.py` (§11.7).
- `tools/frame_town_for_2x3.py` (new, local tool) and its output
  `deploy/game_content/content/_dlcbundle/MapVillage.sam` (git-ignored content).
- Docs: this section; `CLIENT_REMAINING_2026-10-02.md` (evening section);
  handbook runner paragraph (fixture drift).

### 11.7 Tests (QA build, isolated servers, copied saves)

- Targeted: `test_evolution_qc_wire.py` **26/0** (three new "helper picker list
  untouched" checks), `test_fusion_merit.py` **8/8** (no `xZH6EIQ7` on any mix
  or Mystery Frog reply; BB/SBB read from `1ZbHB6Im`),
  `test_battle_simulator_wire.py` **93/0**, `gen_battle_simulator.py --check`
  up to date; town tool **10/10** on a temp copy.
- **First complete run (20:30–20:41): FAILED** — 1,588 checks passed, 7 failed, 4
  suites not ok:
  `test_mission_run_ownership_wire.py` 55/2 (B, B12),
  `test_continue_delayed_retry_wire.py` 33/1, `test_grant_bundle_wire.py` 32/4 +
  traceback, `test_debug_cli.py` 1 PASS + traceback. The same four suites on the
  unchanged 15:16 EXE against the same live-save copy failed **identically**
  (same labels and counts), so this was fixture drift, not this change. Causes,
  all on the live save since Session 6's run: a Continue mark for mission 20 (the
  player's lost 19:01 run keeps it until that mission's next start — by design);
  the evolution bundle already delivered and claimed; the unit box at 104/110
  (the CLI test needs 7 free slots). Fixed in the fixtures, on the copies only:
  clear Continue marks; drop the bundle's tagged presents and give the box room;
  give the CLI copy 7 free slots. Rerun: 57/0, 34/0, 55/0, 2/2 PASS. Logs kept
  (`*_followup_qaexe_drift.log`, `*_followup_1516exe_drift.log`,
  `SUMMARY_followup_first_run.*`). Handbook runner paragraph now says how to
  tell drift from a regression.
- **Final complete run (20:48–21:00, final source and data): 41/41 entries,
  1,615 checks passed, 0 failed**; validator at its 3,117-finding baseline.

### 11.8 Build, deployment, backups

- QA build ~20:19–20:28 into `out/qa/current/bin` (runtime override; KDL changed,
  full 209-step build; KDL pre-validated by generating into a scratch folder:
  exit 0, only the long-standing dbb.kdl import-cycle advisories, code identical
  to the previous header except the two optional members). Override removed with
  `-U`; deployment build was a relink only (`[3/4] Linking …gimuserverw.exe`).
- **Deployed** `out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe`
  **21:01:46, 31,520,256 bytes, sha256 0a3d6718705c…**, linking the tested
  `gimuserver.lib` (20:27:58). `DecompDev` occurs 0 times in it (and in the QA
  EXE), twice in the rollback EXE.
- **Started** 21:02 with `bf_ctl.ps1`: **PID 22992 owns 127.0.0.1:9960**, child
  4392; 1,339 mission and 12 AI archive records; listening 21:02:16;
  `fps_cap` 200; no new migration (80 recorded); save `integrity_check` ok.
  `GET /content/_dlcbundle/MapVillage.sam` returns the 640×1161 copy (sha256
  39f3e427fc6c…). Then the client's cached `LocalState/MapVillage.sam` (640×985,
  from Sep 23) was backed up and deleted. The client (PID 6760) stayed running;
  the Town shows the new framing after a game restart.
- **Backups** (`out/backups/2026-10-02_followup/`): `gme_before_followup_2019.sqlite`
  and `gme_before_deploy_2101.sqlite` (identical, sha256 fcfa3c99ef69…, integrity
  ok: 2 Gems, 2,242,412 Zel, 104 units, 77 presents);
  `gimuserverw_1516_before_followup.exe` (rollback, sha256 a1413a12b157… = the
  Session 6 deploy); `archive_before/` (mission.json and ai.json before the
  reticle regeneration); `LocalState_MapVillage.sam` (the client's cached town
  file); `qa_current_session6/` (Session 6's whole `out/qa/current` as it stood,
  348 MB, before this follow-up's runs overwrote the shared paths).

### 11.9 Next client checks

In [CLIENT_REMAINING_2026-10-02.md](CLIENT_REMAINING_2026-10-02.md) ("Evening
follow-up"): F1 helpers right after a fusion/evolution; F2 reticle on every
dummy; F3 Town after a game restart (labels, tiles, glints; a tall window would
now put labels low); F4 the Session 6 items still open (resume after relaunch,
SP repurchase, mission 23's first clear and the Egor Snowfield presentation,
whether the 7★ Berdette shows its UBB).

Still unknown / not done: the special bosses' untested cursor offsets (same
units lesson applies); whether the client keeps a parsed copy of the old Town
file in memory until restart (assumed — hence "restart first"); the Windows
build's own Town placement (arm64 decoded; behaviour matches the player's
screenshot).

## 12. Night follow-up — the F1–F4 results (22:00–22:20)

The player reset the app (Windows' reset utility) and reported: F1 friends shown
after a fusion; F2 reticle correct but a helper drawn with no sprite; F3 Town
labels unchanged; F4 the double-gate summon white bleed back. Times local.

### 12.1 Results

- **F1 PASS (player-confirmed).** UnitMix 21:42:10, then Training Grounds
  21:44:18 with a helper picked from the list.
- **F2 reticle PASS (player-confirmed)** at "0,17". **New, open:** in that 21:44
  battle the helper Mauve Tenebrosity Zeal (60956) had no sprite or portrait.
  The server has all 8 of its files (cgg, three cgs, anime, ills full/thum/
  battle); the client requested only `unit_ills_thum_60956.png` (21:43:47) and,
  at the battle start, only the six dummies' monster art. The 21:50 battle with
  helper 20837 fetched that unit's 7 files at the start (21:50:04) and drew it.
  Both ran during the post-reset re-download (§12.2); cause not established.
- **F3: no change — the 21:02 fix was on the wrong path.** The log shows the
  client fetching `GET /content/sam/MapVillage/MapVillage.sam` at 21:36:13 and
  21:40:15 (exact path, the shipped 985 file); the only request ever made for
  `/content/_dlcbundle/MapVillage.sam` was my own check at 21:02:29. The
  diagnosis itself is confirmed: image-matching the new screenshot (505×935
  window; the Windows client letterboxes to a 502×751 game area, so offsetY is
  0) puts the art top 86.7 design px down against the model's 87.5.
- **F4 (QC note, not investigated further at the player's request):** summon at
  21:56:03; the client fetched the gate set on demand at 21:56:05 —
  GachaGateChange5_6 (gold → rainbow), Open3/4/6, Open6Add, OpenAdd,
  Close3–5. Every gWait/gChange/gOpen script in LocalState is byte-identical to
  the patched server copy (re-downloaded 21:46:42), so the reset did not restore
  stock scripts. Server gate files equal the Appx bundle's where both exist;
  Change5_6, Open6, Open6Add and Close5 are not in the bundle, so after a reset
  they come only from the server. Old cache contents are unrecoverable.

### 12.2 The post-reset re-download

After the reset the client re-downloads its whole cache in the background:
~1,000 files a minute from 21:38, LocalState 0 → 34,141 files by 22:14, still
running. It works in passes over a list that is fixed when a pass starts — it
fetched `sam/MapVillage/MapVillage_320x200.png` and the whole `MapVillage_iph5`
set at 22:14 but not the `MapVillage.sam` deleted at 22:11. F2 and F4 both
happened inside it.

### 12.3 Town fix, second deployment (22:11, content only, no restart)

`tools/frame_town_for_2x3.py` rewritten: frames BOTH paths
(`sam/MapVillage/MapVillage.sam`, observed; `_dlcbundle/MapVillage.sam`,
binary-derived), takes the pristine source from the Appx bundle (falling back
to the APK; sha256 5bbc14ba…), restores it on `--remove`, refuses all-or-nothing
when any target holds foreign bytes, and timestamps cache backups. 10/10 on a
temp tree. Applied 22:11: both paths return the 640×1161 copy over HTTP; the
shipped `sam/MapVillage/MapVillage.sam` is kept in
`out/backups/2026-10-02_followup/content_sam_MapVillage_MapVillage.sam`; the
client's cached 985 file (downloaded 21:40) is in
`LocalState_MapVillage_20261002_221108.sam` and was deleted. Because the running
pass skips it, the framed copy arrives at the next launch (the login fetch seen
at 21:36/21:40) — or via the Town preflight path, also framed.

**Delivered 22:22:36:** after a relaunch (login 22:22:11) the client fetched
`sam/MapVillage/MapVillage.sam`; its LocalState copy is now the framed file
(640×1161, sha256 39f3e427fc6c…, identical to the served one). Server-side
evidence only; the player has not yet looked at the Town.

### 12.4 Next client checks

G1 Town after the download finishes and a game restart; G2 Zeal as a helper;
G3 a colour-changing summon (screenshot the white frame if it recurs). In
[CLIENT_REMAINING_2026-10-02.md](CLIENT_REMAINING_2026-10-02.md) ("Night
follow-up").

## 13. October 3 — results and the client-cache brief

- **G1 Town: PASS (player-confirmed)** with the framed `MapVillage.sam`
  delivered at 22:22:36 (§12.3).
- **G3 summon gate: PASS (player-confirmed)** — the 21:56 white flash did not
  recur once the post-reset re-download had finished. Cause not isolated further.
- **G2 Zeal: explained, retest pending.** The 22:24:44 Training Grounds battle
  drew Zeal as a white blob: `unit_cgg_60956.csv` was cached at 22:18:35 but the
  re-download's sequential pass reached `unit_anime_60956.png` only at 22:25:59
  (between 60955 and 60964) and `unit_ills_battle_60956.png` at 22:27:47; the
  battle start fetched nothing for Zeal on demand. All four PNGs now decode
  (`c[i] − i²`) to byte-identical copies of the served files.
- **New: [CLIENT_CACHE_PATCH_BRIEF.md](CLIENT_CACHE_PATCH_BRIEF.md)** — the
  write-up for the offline-proxy (`Seltraeh/offline-proxy`, branch `fps-cap`)
  explaining why the client needs a cache patch, with the cache internals now
  traced in the arm64 binary: `existsLocalFile` → `FileLoader::isFileAlreadyDownloaded`
  (in-memory map at `FileLoader+0xb0`), `WrapAsyncFileLoad::connectionDidFinishLoading`
  → `FileCrypt::encode` → `writeFile` (`fopen "wb"` under `getWriteablePath`),
  `CommonUtils::getTexture` → `FileCrypt::decode`; the x86 cocos2d DLL's
  `CCFileUtils`/`CCFileUtilsWinRT` exports; a recommended startup cache
  reconciliation against a new `GET /offline_mod/content_manifest`, and the
  maintainer's preferred direct-read design as phase 2. Write-up only: neither
  repository was changed for it.
