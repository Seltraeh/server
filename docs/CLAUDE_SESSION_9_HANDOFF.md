# Claude Session 9 — handoff: harness readiness, an isolated-port provider candidate, stored leader skills, a database-wedge fix

> Latest independent QC: the second-round `47fe25fc` candidate passes the full
> runner (1,900 checks, no failures; existing mission baseline unchanged),
> including the long-lock and broker replacement regressions. However, the
> separate live proxy integration run failed two CSV adoption assertions;
> a repeat passed, so provider acceptance remains incomplete. Ordinary isolated
> gameplay persistence checks may proceed; see [QC_SESSION_9_ROUND2_REVIEW.md](QC_SESSION_9_ROUND2_REVIEW.md)
> and [CLIENT_SESSION_9_ROUND2_STEPS.md](CLIENT_SESSION_9_ROUND2_STEPS.md).
> [QC_SESSION_9_REVIEW.md](QC_SESSION_9_REVIEW.md) records the superseded
> first-round failures. Use an explicit `--exe`; the harness default is Session 8.

Assignment: [CLAUDE_SESSION_9.md](CLAUDE_SESSION_9.md). October 4, 2026 (local
times). **Nothing is committed, pushed or deployed.** The live save was only read
(SQLite backup API); the installed client, its LocalState and the in-tree server
EXE were only read; no package was registered, no loopback exemption changed.

Evidence labels as in the handbook: **arm64** (libgame.so, names and semantics),
**wire-tested** (isolated server + copied save), **mocked** (fake OS, no process),
**client-pending**.

**Outcome.** (1) The acceptance harness can no longer announce a server that is
not ready: readiness now means the started process is the port's only listener
and its fps_cap and broker status answer as configured, before and after which
the OS still lists it alone; every failure exits nonzero with the log path after
stopping only what the harness started. One routine serves the harness and all
suites. (2) The test package no longer needs the live port: a proxy build that
differs from the reviewed DLL only in its port (byte diff below) talks to 29960,
so the player's server and installed client can keep running. (3) Archive grants
stored no Leader Skill; the server now stores the UnitMst value, a dry-run-first
repair script fixes old rows, and the packets each affected mode uses were
checked. (4) The runner exposed a pre-existing server defect: one outside read
of the save overlapping a request made the server lose that acknowledged action
and fail every later request until restarted; the first round's busy timeout
bounded it, the second round's explicit commits (§R1) close it. Built and
wire-tested in a QA candidate; **not deployed, the live save not repaired** —
both are documented maintainer steps (§8).

**Second round (after [QC_SESSION_9_REVIEW.md](QC_SESSION_9_REVIEW.md), October 4
09:00–14:15).** Both QC corrections are fixed in the final QA candidate
**`47fe25fc…`** (§R): (1) a commit that cannot complete is refused and rolled
back on its own connection, so **a success reply now means the change is
durable**, and a connection left inside a transaction heals at the next
request; (2) the broker's intermittent 404 had two causes, both the
delete-pending state the old file passes through during an atomic replace
(Windows `ERROR_ACCESS_DENIED`): reads now retry it and answer a bounded 503,
and the watcher no longer withdraws a name it cannot examine. (3) The next
offline-client patch is prepared, not started:
[OFFLINE_CONTENT_PHASE_C1_PLAN.md](OFFLINE_CONTENT_PHASE_C1_PLAN.md). Still
**not deployed, nothing committed**; the live save, the installed client, both
packages' data and the in-tree EXE were only read.

## R. Second round: QC corrections

### R1. Failed commits (QC correction 1) — fixed in the candidate

The first round's busy timeout only made short readers wait; past 5 s, Drogon's
destructor COMMIT still failed after the reply and left the connection inside
the transaction. Now every handler **commits explicitly, before replying**
(`gimuserver/gme/common/Transactions.hpp`, rewritten):

- `gme::beginTransaction(db)` opens the transaction and runs `SELECT 1` in it.
  If the connection is still inside an abandoned transaction
  (`TransactionRollback`), it logs `beginTransaction: the connection was still
  inside a transaction …`, rolls that back and retries once.
- `gme::commitTransaction(tx)` sends `COMMIT` through the transaction while it
  still owns its connection. A failed COMMIT (busy beyond the timeout) is
  rolled back there by Drogon's in-transaction failure path and the exception
  reaches the handler: the request is answered with an error, nothing kept.
  After a durable COMMIT it sends `BEGIN`, so the destructor's own commit has
  an empty transaction to finish.
- `gme::tryCommitTransaction(tx)` is the same returning false, for the three
  handlers that answer a refusal themselves (FeSkillGet, MissionBreak, ShopUse
  FE reset).

All 41 transaction sites use them (`session9/transaction_sites.txt`): the
handlers, the two `Warehouse.cpp` helpers (which commit only a transaction they
opened) and the CLI grant. **`scripts/audit_transactions.py`** (new, runner entry
49) checks structurally that every `co_return HandleResult::success` after a
`beginTransaction` is preceded on its path — catch blocks excluded — by a
commit, or by a rollback / refusal with no rethrow: **41 sites, 0 unchecked**;
7 / 7 deliberate mutations caught (`session9/audit_mutations.log`).
`standalone_frontend/DebugCli.cpp`: grants commit the same way; new **`dbwedge`**
leaves the connection inside an open transaction (what a failed destructor
commit used to leave), for the regression. The busy timeout stays. Handbook:
one paragraph under "Save safety and acceptance".

Residual risk, stated: a non-database exception thrown inside a transaction by
a handler without a catch-all still falls back to Drogon's destructor commit;
if that commit fails, `beginTransaction` heals the connection at the next
request.

**Regression** — `scripts/test_db_commit_contention_wire.py` (rewritten, port
19997, in the runner; timings and counts below are `3edffa88…`'s):

| Case | Proves |
|---|---|
| A | a 1 s reader across a craft: the reply waits for the commit (1.07 s), then the craft is acknowledged and in the save |
| B | three more crafts; another reader sees all four |
| C | 15 crafts during 178 concurrent backup copies: all acknowledged, every copy complete |
| E | **QC's 6.5 s reader**: the craft is refused (after 5.6 s) and nothing of it is kept (product, materials, Karma); a read-only request answers while the lock is held; the next craft succeeds and costs exactly 5 materials + 500 Karma; no `Transaction submission failed`, no `Error occurred in transaction begin` |
| F | `dbwedge` through the debug pipe: the next craft heals the connection and succeeds; the recovery is logged |
| G | **restart persistence**: after shutdown exactly the acknowledged crafts (21) are in the save; a server restarted on it lists all of them and crafts again, and that craft persists |

| Server | Result |
|---|---|
| Commit-fix candidate `3edffa88…` | **21 / 0** (`session9/db_contention_candidate.log`) |
| Final candidate `47fe25fc…` | §R3 |
| Deployed `6ee7d454…` (negative control) | **5 / 16**: the first reply came before its commit (0.10 s), every later craft "Unable to run database query", backups blocked, `dbwedge` unknown, the acknowledged craft gone after shutdown (`session9/db_contention_deployed_control.log`) |

**QC's own reproduction** (`session9/qc_long_reader.py`, not edited): "no failed
COMMIT" and "next craft usable" pass; "acknowledged first craft persisted" fails
with detail `1`. The first craft is now *refused* instead of acknowledged, and
the save holds exactly one Famous Blade — the later, acknowledged craft. That is
the behavior the correction asks for (successful replies correspond to durable
mutations); the check presumes an acknowledgement for a commit that cannot
complete within the timeout. Results per candidate in §R3.

### R2. Broker replacement race (QC correction 2) — fixed in the candidate

**The Windows errors.** A diagnostic build (`9dc64c63…`: the old read logic plus
a log line for every failed open with its Win32 error) ran the suite 10× and a
stress driver (8 readers × 200 reads per round, 8 rounds: 12,800 reads against
10,910 replaces) without one failed open — the race is rare on an idle machine.
So it was reproduced deterministically: a held handle marks the file for
deletion (`SetFileInformationByHandle`, `FileDispositionInfo`), the state the
old file passes through during an atomic replace. Then:

- **read path:** `CreateFileW` fails with **Win32 error 5** (`ERROR_ACCESS_DENIED`)
  while the file is delete-pending and **2** once it is gone; `read()` retried
  only sharing violations and answered **404 `missing`** — the first round's
  runner failure (`session9/broker_control_9dc64c63.log`, server log lines
  `open sam/Junk/ok.png failed with Win32 error 5`);
- **index path:** the watcher's existence check fails with **"Access is
  denied."**, and `applyChanged` treated any failure as absent: it **withdrew
  the name** — `404 not-owned` until the next quiet period, which under a
  writer that keeps replacing is the rest of the burst. One of 10 suite runs on
  the read-path fix alone (`486470bd…`) hit exactly that: 113 reads answered
  `404 not-owned` (`session9/broker_race_runs_486470bd.log`).

**Fixes** (`gimuserver/content/ContentBroker.cpp`, `.hpp`):

- `read()`: sharing / lock violations and access denied are the replace window
  and are retried for up to 0.28 s (8 attempts 40 ms apart; the old loop retried
  sharing violations only, 6 attempts); not-found is retried briefly, and for
  the whole window once the file was seen blocked (deleted, its new copy not yet
  in place). Still blocked → **503 `busy`** (the
  provider retries a 503, not a 404); still absent → **404 `missing`**, as
  before. Every failed open is logged with its Win32 error.
- `applyChanged()`: only a definite not-found, or a path that is no longer a
  regular file, withdraws a name. A name that cannot be examined keeps its
  record (`cannot examine … ; keeping its record`) and the watcher looks at it
  again after each quiet period, up to 25 times; a failed re-hash, which already
  kept its identity, is looked at again the same way.

Known limit, unchanged in kind: `read()` runs on the HTTP loop thread, so a
file that stays blocked holds that loop for up to 0.28 s before the 503 (the
old sharing-violation retry: 0.24 s).

**Tests** — `scripts/test_content_broker_wire.py`: every existing assertion
unchanged; 8 new checks (78 total):

| New check | `9dc64c63` (old) | `486470bd` (read fix) | **`47fe25fc`** |
|---|---|---|---|
| a read that meets a delete-pending file is served the new copy once it lands | FAIL `404 missing` | pass | pass |
| a file still delete-pending after the window is `503 busy`, within 2 s | FAIL `404 missing` | pass | pass |
| a file really removed is still a 404 (`missing` or `not-owned`) | pass | pass | pass |
| once put back it is served again | pass | pass | pass |
| the watcher examining a delete-pending name keeps it offered, identity unchanged | — | FAIL (withdrawn) | pass |
| a read meanwhile is `503 busy`, not `not-owned` | — | FAIL `404 not-owned` | pass |
| once the deletion is taken back it is served again | — | pass | pass |

(The `9dc64c63` control ran before the watcher checks existed: 73 / 2.)
Repeats on **`47fe25fc…`: 15 / 15 suite runs clean** (78 / 0 each, no failed
open, no index event, 301 bounded `503 changed-during-read`;
`session9/broker_race_runs_47fe25fc.log`). Stress: **12,800 reads — 12,149 ×
200, 651 × 503, 0 torn, 0 × 404, 0 failed opens, 0 index events** against
11,605 replaces (`session9/broker_race_stress_47fe25fc.log`). On `486470bd…`
it was 9 / 10 (the not-owned run above).

### R3. Reruns on the final candidate `47fe25fc…`

| Run | Result |
|---|---|
| `run_bugfix_regressions.py`, 49 entries, 10:36–10:45 (`session9/runner_47fe25fc.log`, `session9/SUMMARY_final_47fe25fc.md`) | **48 passed, the validator at its recorded baseline** (3,117: H15 842, M2 20, M3 921, M4 1334 — a data baseline, not a clean validation); **1,899 checks passed, 0 failed**. Among them: contention 21 / 0, broker 78 / 0, generation QC 17 / 0, policy 8 / 0, leader skill 48 / 0, synthesis 5 / 5 phases, reward locks 24 / 0, craft row identity 53 / 0, harness (mocked) 49 / 0, `audit_transactions.py` "41 transaction site(s); 0 unchecked success replies" |
| QC's `qc_long_reader.py` (`session9/qc_long_reader_on_47fe25fc.log`) | 2 / 1, as on `3edffa88…`: the first craft is refused, not acknowledged, and exactly the later acknowledged craft persisted (§R1) |
| `test_content_provider_live.py` with proxy tests `c40131c9…` (`session9/content_provider_live_47fe25fc.log`) | driver **5 / 5**, proxy **313 passed, 0 failed, 0 skipped** |
| Broker repeats and stress | 15 / 15 clean, stress 0 × 404 (§R2) |

The first-round runner on `a1b4286e…` (§7) had the broker race as its one
failure; it does not recur here.

### R4. Remaining client checks

1. **Run the candidate explicitly:** `python scripts\content_provider_client_test.py server --exe out/qa/current/session9/bin/gimuserverw.exe`.
   Before READY the harness prints `server candidate … sha256 47fe25fc…`;
   check it. The default is still the Session 8 server (QC item 3; unchanged).
2. **Now unblocked** (QC deferred them until the broker race was fixed):
   deliberate same-size replacement (`edit --height 985`, then `1161`) and
   missing-file recovery. Record what the game shows while an owned file is
   removed and after it is put back.
3. Restart persistence: close and reopen the test client with the harness
   running; account, settings and equipped units/spheres.
4. Broker on versus `--broker off` timing on the same save.
5. Leader skill in Training Grounds on a prepared candidate fixture, not the
   live save.
6. Not testable yet: cache-free startup/login (not implemented; plan below).
   The test package's stored answer to the background-download prompt is
   `"true"` (`Settings\settings.dat`, written 08:39), so the prompt will not
   reappear on it and its background download stays enabled.

The Town/Zeal baseline is client-confirmed and needs no repeat. No claim is made
here for stale-cache precedence, restart persistence or performance.

### R5. Next offline-client patch — prepared

[OFFLINE_CONTENT_PHASE_C1_PLAN.md](OFFLINE_CONTENT_PHASE_C1_PLAN.md) traces what
the client downloads and what decides it, then bounds the next patch:

- Every scene-entry download goes `downloadFiles()` → `requestFile` /
  `requestUnitFiles` / `requestItemFiles` / … → `requestFileWithEncode` →
  **`existsLocalFile`** (already hooked) → priority queue →
  TransitionalDLCScene. A name the provider reports present is never queued.
- Save-dependent demands are large: **every visit to Home** requests the list
  thumbnail of every owned unit (and the unit's whole file set when it is
  missing); the Archive requests every dictionary entry; summons, unit
  screens, missions, active areas and 25 server-data lists add their own.
  So families must be owned for every id the MSTs can name, not per save.
- The background-download prompt (`DLC_WARNING_MENU_FLG`): shown after the
  login file check only while the flag is unset; on Windows it is stored in
  `Settings\settings.dat`, not `LocalState`; it drives
  `FileLoader::getConnectStartFlag` (x86 0x1062D0). Both packages hold
  `"true"`. C1 leaves it, LoginScene and every scene untouched.
- C1 = server family policy (`unit/img|cgg|cgs`, then `item`), a proxy enqueue
  filter (owned names never queued), x86 confirmation of the transitional
  handlers; acceptance on different saves, the Archive, new units and first
  visits with a **second, empty test package** (never resetting the existing
  ones).

### R6. Builds (second round)

| Build | SHA-256 | Content |
|---|---|---|
| `out/qa/current/session9/gimuserverw_3edffa88_commitfix.exe` (copy) | `3edffa882851d41b046884cd547293948c0266fa5e10baf7dea3058a558839c8` | explicit commits in every handler, `dbwedge`, audit |
| `out/qa/current/session9/control_broker_9dc64c63/` (with DLLs) | `9dc64c635f371bb0d8bc0511bca2dcbdc472b51f326a421346d6fb4e9730ecbe` | + a log line per failed broker open, old read logic (negative control) |
| (overwritten) | `486470bd855d90865285c259d9727cfde3f54fec3886d3e98e73d01618ae6576` | + read-path retry |
| **Final QA server `out/qa/current/session9/bin/gimuserverw.exe`** | **`47fe25fcc8187699ecb28a9d14df55d56238d077c1166d79c6a55f087418e398`** | + the watcher keeps names it cannot examine; 10:16:16, 32,560,128 B |

All with `build_qa.sh` (shared tree, temporary runtime-output override removed
each time, cache verified clean). In-tree normal server still `6ee7d454…`.

### R7. Files changed in this round

| Group | Path | Change |
|---|---|---|
| DB | `gimuserver/gme/common/Transactions.hpp` | `beginTransaction` / `commitTransaction` / `tryCommitTransaction` (replaces the destructor-commit awaiter) |
| DB | 33 handler files, `gme/common/Warehouse.cpp` (and the include in `gme/common/Common.hpp`) | explicit commit before every success reply |
| DB | `standalone_frontend/DebugCli.cpp` | grants commit explicitly; `dbwedge` |
| DB | `scripts/audit_transactions.py` (new), `scripts/run_bugfix_regressions.py` | structural audit, runner entry 49 |
| DB | `scripts/test_db_commit_contention_wire.py` | rewritten (A–G above) |
| broker | `gimuserver/content/ContentBroker.cpp`, `.hpp` | read retry; keep unexaminable names; second looks |
| broker | `scripts/test_content_broker_wire.py` | 8 new checks; existing assertions unchanged |
| docs | `docs/BF_OFFLINE_SERVER_HANDBOOK.md` | durable-reply paragraph |
| docs | `docs/OFFLINE_CONTENT_PHASE_C1_PLAN.md` (new) | next patch plan |
| docs | this file | §R, banner kept |

Codex's harness fixes (Start-menu label `[ContentTest] Brave Frontier`, version
2.19.6.1, PowerShell-quoted `CheckNetIsolation` commands) are untouched.

## 0. State found at the start

- **No server or client running**, 9960 free (`bf_ctl status`: 0 / NOT listening / 0).
- **The normal server is no longer `fb77bf6f…`.** In-tree
  `out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe` is
  `6ee7d454d8b75f47fc0d6e8be44c5284871ab885b3f6299ac74f25ac6df08a1b`,
  2026-10-04 00:51:35, 31,954,944 B. Provenance: `.ninja_log`'s last invocation
  ran only the dyndep scans and the link, and no object or source was newer than
  the Session 8 candidate `a2022e91…` (ItemMix.cpp.obj 23:55, its link 23:58). So
  it is **the Session 8 code relinked in-tree — the sphere fix is deployed**
  (handoff 8 §9). `deploy/gme.sqlite.bak` (00:42) is the pre-deployment backup.
- What the player did (read-only, `deploy/log`): 00:28–00:42 on the old build
  (ItemMix replies `{}`: `1009:6`, `2005:1`; one ItemSphereEqp); 00:52–01:07 on
  `6ee7d454…` (server_stdout: start 04:52 UTC, `content broker: dormant`):
  ItemMix 00:58 `2064:2,2062:2,2020:10` answered with 510 warehouse rows + lock
  set + dictionary; ItemSphereEqp 01:00; ItemSell 00:55 (1 row) and 01:05 (16
  rows, incl. Famous Blade rows 42–45); ItemFavorite 01:07 (rows 36, 509). This
  matches the brief's final #34 acceptance. The brief's §2 sphere sequence
  (Famous Blade batch, copy locks, transfer, sale) was therefore not re-run or
  re-asked; #34 stays player-confirmed for the reported workflow.
- Live save `239f9e69…` (01:07:05). Installed client 2.19.6.0 under WindowsApps,
  DLL `9ae05b17…`. Test package not registered, no exemption, no test LocalState.
- Git as Session 8 left it (§11). Shared CMake cache had no runtime-output
  override. Proxy `fps-cap` @ `d3aa843`, `build/bin/libcurl.dll` = `e24234c1…`.

## 1. Harness readiness (QC_SESSION_8_REVIEW P2) — fixed

**Defect (as QC found):** `cmd_server` left its 180 s wait on timeout and then
printed "listening" unconditionally; `port_busy()` accepted any listener; the
later duplicate warning never retracted the announcement.

**One startup routine.** `scripts/bf_testkit.py` `IsolatedServer.start()` now
serves every suite and the harness:

- Before spawning: refuses the live port 9960 unless `allow_live_port=True`
  (only the harness's switch-over mode passes it) and refuses a port that any
  process already listens on.
- Ready = in order: the child is running; the OS lists it as the **only**
  listener on the port (netstat, IPv4 and IPv6, loopback or any address — a
  dual-stack `[::]` socket also takes 127.0.0.1); every readiness check answers
  as required; the OS **still** lists it alone. The answers are bracketed by two
  exclusive views because Windows lets a second process bind silently, so an
  answer alone may come from another server. In isolated mode it must not also
  hold 9960.
- Anything else — exit before ready, a foreign or duplicate listener, a wrong
  answer that persists, the timeout — raises `ServerStartError` (a
  `RuntimeError`, so suites behave as before) naming the log, after `stop()`.
- `stop()` stops only what `start()` launched: the children the server created
  after it started (creation-time filter against recycled parent pids), each by
  pid, then the server. The `assert`s became explicit errors (`python -O`
  deleted them). OS and clock calls sit behind small methods for the fakes.
- Default check for suites: `/offline_mod/fps_cap` must answer 200 with the
  fixture config's own cap (60), not merely 200.

**Harness (`scripts/content_provider_client_test.py`):**

- `server` takes its port from the layout DLL's SHA-256 (`KNOWN_DLLS`), refuses
  an unknown DLL or a mismatched `--port`, refuses an occupied port, and — for
  9960 only — refuses while any `gimuserverw` or the installed client runs. Then
  it copies the save, starts the server and prints **`READY  pid … is the only
  listener on 127.0.0.1:<port>`** only after `start()` returned. Its checks:
  fps_cap = 60, and `/offline_mod/content/v1/status` = `bfcp 2`, `enabled 1`,
  `ready 1`, `owned 21`, `collisions 0`, `unavailable 0` (`enabled 0` for
  `--broker off`). A failure prints `NOT READY: <reason>; see <log>`.
- While serving it re-checks every 5 s; if the server exits, stops listening, or
  another pid also listens, it prints `FAILED: …`, stops its own server (only)
  and exits 1. Exit codes: 0 = was ready, stopped by Ctrl+C; 1 = never ready or
  lost exclusivity; 130 = interrupted before ready.
- `layout` refuses while the test package is registered (its install location
  is that folder), records `content_client/layout.json`; `check` is port-aware;
  new read-only `progress` reports the legacy download (§3).

**Tests:**

| Run | Result |
|---|---|
| `scripts/test_content_harness_startup.py` (new, mocked, in the runner) | **49 / 0**: normal startup (READY only after the bracketed views), timeout with a live non-listening child, premature exit (its console child still cleaned), foreign listener answering perfectly, duplicate, duplicate appearing between the answers and the second view, duplicate / exit / unbind while serving, dormant / other-config / `bfcp 1` / 404 answers, Ctrl+C before and after ready, live-port guards, port busy, suite defaults, the real recycled-pid filter and netstat parser on fake OS output, `cmd_server` refusals and the isolated run beside the player's server. Every process call other than the faked ones trips an assertion: no real process, port or package is touched. |
| Mutation check (`session9/harness_mutations.log`) | 6 / 6 caught: timeout announces (7 failures), no ownership check (7), no second view (1), cleanup forgets children (5), serving loop ignores ownership (4), READY printed early (9). Sources restored byte-identical (hash-verified). |
| Live smoke (`session9/harness_live_smoke.py`, real server `a2022e91…`, port 29960 only) | **11 / 0**: (A) 29960 occupied → refused, nothing started; (B) READY, then Ctrl+C → exit 0, server gone, port free; (C) after READY the driver bound 29960 too with `SO_REUSEADDR` — **Windows accepted the duplicate bind** — the harness reported `FAILED: pid(s) […] also listen`, stopped its server, exit 1. Live save byte-identical. |

## 2. Test port arrangement: an isolated-port proxy candidate

**Why the port belongs to the DLL.** `OFFLINEMOD_PORT` (CMake, default 9960)
becomes `SERVICE_PORT`; `MyInternetConnectA/W` rewrite **every** WinINet
connection to it whatever host/port the game asked for; the FPS fetch runs on the
first render frame through those hooks; the provider's broker client uses it
directly; and the server advertises its own bind as `dls.game_ip`
(`ServerCache.cpp:206`). The libcurl shim rewrites nothing.

**Built** in the existing tree `offline-proxy/build`: reconfigured with
`-DOFFLINEMOD_PORT=29960`, built the `offlineproxy` target only (3 files that
include `serverconfig.h` recompiled), copied out, reconfigured with
`-UOFFLINEMOD_PORT` (cache diffed identical to the saved copy), rebuilt as a
determinism control, and restored the reviewed DLL + PDB into `build/bin`.

| Build | SHA-256 | Diff vs reviewed `e24234c1…` (`session9/pe_port_diff.py`) |
|---|---|---|
| Isolated `out/qa/current/session9/proxy_29960/libcurl.dll` (PDB `2306bba0…`) | `01786e642ec6afcc2e9b89594d9ecb4a8af29dbb18420e9c9fbd2be4a5c1b48f` | 4 × `push 0x26e8` → `push 0x7508` (RVA 0x17510, 0x175d0, 0x31ad1, 0x31ceb — the four `SERVICE_PORT` uses: main.cpp ×2, cp_hooks.cpp ×2) + 3 link-metadata bytes |
| Control: 9960 rebuild, no change (evidence only) | `e27c1cbb…` | the same 3 metadata bytes (2 in the header timestamp, 1 in the CodeView record) |

So the isolated build is the reviewed code with only the port changed. It has not
had its own QC review; the proxy tests were not rerun on it (no code change, and
the test binary `c40131c9…` does not link the DLL).

Port 29960: outside the suites' 199xx block, Windows' excluded ranges
(50080, 50443) and the dynamic range (49152+). With it the test package reaches
only the harness server; the player's server (9960, live save) and the installed
client are unaffected and may run. **Do not use `bf_ctl.ps1 -Action stop` during
a test**: it stops every `gimuserverw`, the harness's included.

The test package layout now carries the isolated DLL (`layout --dll
out/qa/current/session9/proxy_29960/libcurl.dll`, 1,361 hard links,
`gumi.BraveFrontier.ContentTest` / `CN=BraveFrontier`). Fallback: re-run
`layout` with the reviewed `out/qa/current/session7/candidate/libcurl.dll`; the
harness then uses 9960 and refuses until the normal server and installed client
are closed (handoff 8 §6.4's switch-over; restart the normal server afterwards).

## 3. Maintainer procedure (isolated port) and what the test package still downloads

From the server checkout; nothing needs the normal server stopped.

1. `python scripts\content_provider_client_test.py check` — today only the next
   three items are open (`session9/content_check.log`).
2. Register: `powershell -NoProfile -Command "Add-AppxPackage -Register 'C:\Users\Evan\BF\BF-WorkingDirRust\out\qa\current\content_client\package\AppxManifest.xml'"`
3. **Elevated**: `CheckNetIsolation.exe LoopbackExempt -a '-n=gumi.BraveFrontier.ContentTest_99p3jr0gh0z6w'`
4. Launch "[ContentTest] Brave Frontier" once, close it (creates its LocalState).
5. `python scripts\content_provider_client_test.py state --stale-town`
6. `python scripts\content_provider_client_test.py server` — wait for `READY`;
   `NOT READY`/`FAILED` means stop and read the named log. Leave it running.
   It copies the live save first: until §8 is deployed, keep the installed
   client idle for that moment (§6).
7. Launch the test package. It downloads its asset cache through the **legacy**
   path: the installed LocalState holds **71,149 asset files, 2.54 GB**
   (`progress` census; 71,328 files / 2.64 GB including state files). The
   provider owns 21 names and the download queue is not intercepted (Phase C),
   so expect roughly that volume. Watch `python scripts\content_provider_client_test.py progress`
   (files/bytes against the installed cache) and the game leaving its download
   screen; then `snapshot before`.
8. Acceptance (§4), then `snapshot after` and `collect`.
9. Ctrl+C the harness (exit 0); `powershell -NoProfile -Command "Get-AppxPackage gumi.BraveFrontier.ContentTest | Remove-AppxPackage"`;
   elevated `CheckNetIsolation.exe LoopbackExempt -d '-n=gumi.BraveFrontier.ContentTest_99p3jr0gh0z6w'`.

This is **not a cache-free client**: the test package still builds a full legacy
LocalState; only 21 owned names are served from the broker into RAM.

## 4. Provider acceptance — client-pending, unchanged

Handoff 8 §6.5's table stands (hooks + 21 owned names; stale-cache Town uses
the fixture; Zeal 60956's first battle has portrait/sprite/animations; same-size
`edit --height 985` appears at a safe reload, `1161` restores; an unavailable
pinned version fails recoverably and recovers; account/settings/equipment
survive restart; `snapshot` shows no owned-asset writes; `perf` lines broker on
vs `--broker off`). In the provider log the port now reads `127.0.0.1:29960`.
No proxy or broker source changed this session: protocol-v2 digest binding and
the original QC regression assertions ran unchanged (first runner pass: broker
70/0, generation QC 17/0, policy 8/0; live 313/0 + 5/5). The broker's concurrency
check turned out to be intermittent on the Session 8 build too (§7).
**Missing-file recovery inside the game is unproven**; record what happens. No
Phase C/D hook was started.

## 5. Stored leader skills on archive grants — fixed in the candidate

**Grant paths.** `gme::fromArchivedUnit` → `UnitArchiver::populatePacket` never
set `leader_skill_id` (the archive record has none), and `addUserUnit(UserUnitInfo)`
inserts the packet as is: summons (Gacha), presents (Present), exchanges
(Exchange), achievement trades (AchievementAction), captures (Mission),
tutorial starters (Tutorial), selector picks (UnitSelectorGachaTicket) and CLI
`addunit` all stored 0. The raw-SQL `addUserUnit(UnitMst)` (slots, Journal,
CampaignReceipt) and UnitEvo (target form) already stored the UnitMst value.
All 2,259 archive units have UnitMst rows; 2,099 name a skill, every one in
LeaderSkillMst.

**Consumers (arm64).** `UserUnitInfo::getLeaderSkillID` has 10 callers:

| Mode | Reader | What the stored value does | Packet that carries it | Checked |
|---|---|---|---|---|
| Training Grounds, helper = own unit | `PlayerParty::entrySandbag` +0x6f4 → `BattleUnit::setGuestLeaderSkillID` when `UserState::getMyUserUnitID` names one of the player's units | **the helper's Leader Skill** — 0 dropped it | UserInfo `4ceMWH6k` (full replace, every return to Home) | wire |
| Frontier Gate | `FrontierGateBaseRequest::createBodySaveDataUnit` / `…FGPlusDataUnit` | uploaded in `52xDBRGr` | FrontierGateStart echoes it; Continue/Retry hand back the stored copy | wire |
| Campaign | `Campaign{Start,Save,End}Request::createBodySaveDataUnit`, `CampaignUserUnitInfoList::getSaveData` | uploaded save data | the server keeps only position (CampaignStart); not echoed | static |
| Challenge Arena | `ChallengeArena*::copyUnitInfo` / `clone` (3) | copied between arena structures | none: Arena is padlocked offline (`ArenaInfo`) | static |

Ordinary, Trial, Raid, guild and campaign **battles** and the unit screens read
`UnitMst::getLeaderSkillID` (`BattleParty::setPartyPassiveList`,
`setNewTrialPartyPassiveList`, `CampaignUtils::partyPassiveSet`,
`RaidFieldReady::initParty`, `GameUtils::getGuestLeaderSkillID` for helpers) —
unaffected, which is why nobody saw it.

**Live save (backup copy `session9/evidence/live_save_copy_2026-10-04.sqlite`, `21a26795…`):**
109 units — 37 stored 0 where UnitMst names a skill (Selena, Gun Goddess Serin,
Ignis Halcyon Vargas, Tilith, the three Deemos, …), 71 correctly 0 (no skill),
1 correct nonzero (Tartarus Blaze Berdette, evolved), **no** nonzero values that
differ from UnitMst, no unknown units.

**Fix** (`gimuserver/gme/common/Common.hpp`): `unitLeaderSkillId(unitId)` returns
the UnitMst row's skill (0 and a warning when UnitMst has no row — never a
guess); `fromArchivedUnit` sets it, so every archive path now stores what the
other writers store. `Mission.cpp`: comment only.

**Existing saves** — `scripts/repair_leader_skills.py` (new; not a migration, per
the no-one-off-migration rule). Dry run by default; classifies every row:
`repair` (stored 0, UnitMst skill exists in LeaderSkillMst → set it, the current
form's), `ok`, `preserved` (nonzero ≠ UnitMst: never touched), `unknown` (no
UnitMst row: left), `unresolved` (UnitMst skill missing from LeaderSkillMst:
left — only unit 50876, not owned). `--apply` backs up first (backup API, under
`out/backups/leader_skill_repair/`), writes one transaction with each UPDATE
guarded by `leader_skill_id = 0`, and refuses `deploy/gme.sqlite` while a
`gimuserverw` runs. Idempotent. Dry run on the live copy: **repair 37, ok 72,
preserved 0, unknown 0, unresolved 0** (`session9/leader_skill_dry_run_live_copy.log`).
**Not applied to the live save.**

**Tests** — `scripts/test_leader_skill_grants_wire.py` (new, port 19992, in the runner):

| Server | Result |
|---|---|
| Candidate `69bebae7…` | **48 / 0** — repair on a seeded copy (old zero grant; evolved form 10013 at 0 takes its own 101, not base 100; override preserved; unknown unit 99999999 and unresolvable 50876 left at 0; untouched rows identical; backup = pre-repair; second `--apply` changes nothing); login roster sends every stored value; new grants via PresentReceipt (Selena → 110), GachaAction (Lukroar → 10149), MissionEnd capture (Ignis Vargas → 160) and CLI addunit (Tilith → 4400) store and send the skill; FrontierGateStart echoes and FrontierGateContinue returns the uploaded skills; values survive a restart; a final repair finds nothing |
| Pre-fix `a2022e91…` (negative control) | **39 / 9** — exactly the eight new-grant checks (`('20011','0')`, …, stored 0) and the final repair, which finds those four fresh zero grants |

**Client visibility after deployment + repair:** new grants immediately
(`qC2tJs4E` inserts absent units); repaired existing units at the next return to
Home (UserInfo's `4ceMWH6k` replaces the whole list) or a relaunch — `qC2tJs4E`
alone never updates an owned unit.

**Side finding, not changed:** `populatePacket` does not set `element` either:
102 of the 109 live units store `''`. `CampaignBattleStart` maps that to fire for
its synthetic helper. Whether the client reads UserUnitInfo's element was not
traced — a separate bounded task.

## 6. Database commit contention (found by the runner) — fixed in the candidate

> Second round: the busy timeout below stays, but QC reproduced the > 5 s case
> it cannot cover; §R1 commits explicitly before replying and is the fix.

The first runner pass (§7) failed `test_synthesis_wire.py` with
`sqlite3.OperationalError: database is locked`. It reproduced 3/3 on the
**deployed** build too, so it is not a Session 9 regression — but the cause is
a real server defect, not test noise.

**Mechanism (Drogon 1.9.10 source in vcpkg buildtrees):** `Sqlite3Connection`
opens the save with `sqlite3_open` and **no busy timeout**; `TransactionImpl`
queues `commit` on the connection's loop **after** the handler has replied; when
that `commit` returns `SQLITE_BUSY` it only logs `Transaction submission failed`
— no retry, no rollback. SQLite keeps a transaction active after a busy COMMIT,
so the server's only connection stays inside it.

**Proof** (`session9/commit_race_probe.py`, then the suite below): with a read
lock held across one ItemMix, the deployed server **replied ok** to the craft,
its COMMIT failed, every later request failed (`Error occurred in transaction
begin`, client-visible "Unable to run database query"), the save stayed locked
for other readers, and after shutdown the acknowledged craft was **gone**.
Anything that reads `deploy/gme.sqlite` while a request is processed can trigger
it: the runner's and the harness's backup-API copies (which the handbook allows
while the server runs), a suite's direct read, a DB browser.

**Fix** (`standalone_frontend/main.cpp`, one statement): `PRAGMA busy_timeout =
5000` on the server's single connection at start, before the migrations, so a
COMMIT waits for a short reader instead of failing. Remaining limit: a reader
holding the save for more than 5 s still triggers Drogon's no-rollback path
(closed in §R1).

**Tests** — `scripts/test_db_commit_contention_wire.py` (new, port 19997, in the runner):

| Server | Result |
|---|---|
| Final candidate `a1b4286e…` | **10 / 0**: a craft overlapped by a held reader is acknowledged and persisted, no failed COMMIT; three later crafts fine; 15 crafts during **123 concurrent backup copies**, all acknowledged, every copy complete; all 19 crafts persisted after shutdown |
| Deployed `6ee7d454…` (negative control) | **1 / 9**: the one acknowledged craft lost `(1, 0)`, every later craft "Unable to run database query", backups blocked |

**The two runner failures, resolved without touching an assertion:**

- `test_synthesis_wire.py` read the save straight after each reply, i.e. possibly
  before Drogon's queued COMMIT. Before the fix that overlap wedged the server
  (the original error); with the fix it reads the pre-commit state
  (`output() == 3` failed). Its `call()` now ends with a **barrier**: one
  BadgeInfo request, which reads the database without a transaction on the same
  connection and therefore returns only after the pending COMMIT. 3/3 passes on
  the candidate **and** on the deployed build. The same barrier is in the two new
  wire suites. Other suites that read the save right after a reply carry the
  same latent race (no failure observed).
- `test_reward_lock_snapshots_wire.py` "setup: three copies listed" saw
  `[28, 36, 37, 38]`: on Oct 4 the player equipped Famous Blade row 28 on unit
  1113 (ItemSphereEqp 00:31) and locked row 36. Identical failure on the deployed
  build (fixture drift, handbook procedure); the copy is now reset as
  `test_craft_row_identity_wire.py` does (unequip the sphere, clear its row
  locks). 22 / 0 on the deployed build.

**Until the fix is deployed:** do not run the runner, the harness `server`
command or any DB tool against `deploy/gme.sqlite` while the player is
mid-action (an idle client is fine). The harness prints a caution when it copies
the live save beside a running normal server; `repair_leader_skills.py` refuses
the live save while any `gimuserverw` runs.

## 7. Shared runner and other reruns

| Run | Result |
|---|---|
| `run_bugfix_regressions.py` on the final candidate `a1b4286e…` (48 entries, 02:29–02:37) | **46 passed, validator at its baseline (3,117: H15 842, M2 20, M3 921, M4 1334 — not a clean result), 1 failed: `test_content_broker_wire.py` 69 / 1**, "concurrency: failures are only bounded 503s" (`[404]`) — the pre-existing broker race below. 1,880 checks passed, 1 failed (`out/qa/current/SUMMARY.md`, `session9/runner.log`). Synthesis and reward-lock pass. |
| Same, first pass on the intermediate `69bebae7…` (leader-skill fix only; 47 entries) | 45 passed, 2 failed — `test_synthesis_wire.py` and `test_reward_lock_snapshots_wire.py`, both explained in §6; validator at its 3,117 baseline (`session9/runner_intermediate_69bebae7.log`, `SUMMARY_intermediate_69bebae7.md`) |
| `test_content_provider_live.py` (`69bebae7…` + proxy tests `c40131c9…`) | driver **5 / 5**, proxy **313 passed, 0 failed, 0 skipped** (`session9/content_provider_live.log`) — as in Session 8 |
| `check_content_policy.py` (refactored IsolatedServer, `a2022e91…`) | 8 / 0 |
| Leader-skill suite / contention suite on `a1b4286e…` (with barriers) | 48 / 0, 10 / 0 |
| `test_content_broker_wire.py` reruns (`session9/broker_concurrency_reruns.log`) | final `a1b4286e…`: 1 of 3 runs clean; Session 8 `a2022e91…` (identical broker code): 2 of 3 clean — every failure the same check, only 404s |

**Pre-existing broker race (Session 7 code, not changed):** while the suite's
writer replaces `MapVillage_320x200.png` in a tight loop (`os.replace`), a read
occasionally gets `404 missing` instead of a served version or a bounded 503
(server log: `-> 404 missing` between `503 changed-during-read` lines).
`ContentBroker::read` (`ContentBroker.cpp` ~1204–1214) retries `CreateFileW` only
on `ERROR_SHARING_VIOLATION`; any other error — plausibly the delete-pending
`ERROR_ACCESS_DENIED` or a not-found inside the replace window (hypothesis, the
error code is not logged) — becomes 404. For the client that is the
removed-file path for one read; relevant to the same-size-edit acceptance step.
Its first pass on `69bebae7…` was clean (70 / 0), as were Sessions 7–8's recorded
runs. **Fixed in the second round (§R2):** the delete-pending state of the old
file, on the read path (`404 missing`) and on the index path (`404 not-owned`).

## 8. Deploying the Session 9 server changes and repairing the save (maintainer)

One relink deploys every Session 9 server change: the leader skill, the busy
timeout, the explicit commits (§R1) and the broker fixes (§R2; the broker stays
dormant while `deploy/system/content_provider.json` has `"enabled": false`).

1. `powershell -NoProfile -ExecutionPolicy Bypass -File tools\bin\bf_ctl.ps1 -Target server -Action stop`
2. Rollback copy: `copy out\build\debug-win64\standalone_frontend\Debug\gimuserverw.exe out\qa\current\session9\rollback_gimuserverw_6ee7d454.exe` (and the `.pdb`).
3. Confirm `grep RUNTIME_OUTPUT out/build/debug-win64/CMakeCache.txt` is empty; build
   in-tree (`rebuild.bat`, or `cmake --build out/build/debug-win64 --config Debug --target gimuserverw`
   with LIB/INCLUDE). The objects are current, so this is a relink of the Session 9
   code; the hash will differ from `47fe25fc…` (output path) — record it.
4. `python scripts\repair_leader_skills.py` (dry run: expect 37 repairs, nothing
   preserved/unknown), then `python scripts\repair_leader_skills.py --apply`
   (prints the backup path).
5. `bf_ctl.ps1 -Target server -Action start`; confirm 9960.
6. Client: return to Home (or relaunch); Training Grounds with one of your own
   units as the helper — e.g. Inferno Berdette (LS 7401, +30% Atk and max HP
   for all units): the party's max HP at battle start now includes the boost
   (note it before the repair to compare). Ordinary battles look as before.

Rollback: stop, copy the step-2 EXE back, start. The repair needs no rollback
(only `leader_skill_id` changes, to what UnitEvo/new grants write); if wanted,
restore the backup it printed with the server stopped.

**Any in-tree relink now deploys the Session 9 server changes**: the shared
tree's objects are the candidate's.

## 9. Builds and hashes

Second-round builds: §R6. **The final QA server is now `47fe25fc…`**; the table
below is the first round's.

| Artifact | SHA-256 | Notes |
|---|---|---|
| **Final QA server** `out/qa/current/session9/bin/gimuserverw.exe` | `a1b4286edfe79ce1d56d618234353eb493c41ab7c4e3e776379a07aeb9a355d2` | 02:23:48, 31,955,456 B; leader-skill fix + busy timeout; shared tree, temporary `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG`, removed afterwards (cache verified clean); `build_qa.sh` |
| Intermediate QA server (same path, overwritten) | `69bebae7cb0a23a92505b89cb6f7f03471acd2e72ac96430b05f483971d140ce` | 01:56:55; leader-skill fix only (80 objects recompiled: includers of Common.hpp); the final build recompiled only `main.cpp` |
| In-tree normal server (deployed) | `6ee7d454…` | unchanged; Session 8 code |
| Session 8 candidate / pre-fix control | `a2022e91…` | unchanged |
| Proxy reviewed `offline-proxy/build/bin/libcurl.dll` | `e24234c1…` (PDB `31738788…`) | restored byte-identical from `session7/candidate` |
| Proxy isolated `session9/proxy_29960/libcurl.dll` | `01786e64…` (PDB `2306bba0…`) | §2 |
| Proxy tests `offline-proxy/build/tests/content_provider_tests.exe` | `c40131c9…` | unchanged |

Proxy tree note: its intermediate objects are from the control rebuild (same
source and configuration); `build/bin` holds the reviewed bytes, so the next
proxy build relinks a functionally identical DLL with a new hash.

## 10. What is deployed

Unchanged by this session: normal server `6ee7d454…` (Session 8 code, sphere fix
included, **no busy timeout**, not running at hand-off); installed DLL
`9ae05b17…`; provider policy `enabled: false`; live save not written
(`deploy/gme.sqlite` still 01:07:05, `239f9e69…`). The harness change is test
tooling only.

## 11. Repository status

Second-round files: §R7 (in addition to the table below).

Nothing committed or pushed. `out/` stays git-ignored.

**BF-WorkingDirRust** (`main` @ `e8cdefc`) — Session 9 changes, in separately
reviewable groups:

| Group | Path | Change |
|---|---|---|
| harness | `scripts/bf_testkit.py` | `IsolatedServer` readiness/ownership/cleanup (§1), `ServerStartError`, `fps_cap_check`, `LIVE_PORT` |
| harness | `scripts/content_provider_client_test.py` (untracked, Session 7) | port from the DLL, READY/NOT READY/FAILED, `progress`, layout guard, live-copy caution (§1–3, §6) |
| harness | `scripts/test_content_harness_startup.py` (new) | mocked harness suite |
| leader skill | `gimuserver/gme/common/Common.hpp` | `unitLeaderSkillId`, `fromArchivedUnit` sets it (§5) |
| leader skill | `gimuserver/gme/handlers/Mission.cpp` | comment only |
| leader skill | `scripts/repair_leader_skills.py` (new) | existing-save repair |
| leader skill | `scripts/test_leader_skill_grants_wire.py` (new) | wire suite |
| DB contention | `standalone_frontend/main.cpp` | `PRAGMA busy_timeout = 5000` (§6) |
| DB contention | `scripts/test_db_commit_contention_wire.py` (new) | wire suite |
| DB contention | `scripts/test_synthesis_wire.py` | BadgeInfo barrier in `call()`; assertions unchanged |
| fixture | `scripts/test_reward_lock_snapshots_wire.py` | fixture reset for the player's Oct 4 equip/lock; check unchanged |
| all | `scripts/run_bugfix_regressions.py` | + the three new suites (48 entries) |
| docs | `docs/BF_OFFLINE_SERVER_HANDBOOK.md` | two bullets (test-server readiness, stored leader skills) |
| docs | `docs/CLIENT_SESSION_8_CHECKLIST.md` | pointer to the isolated-port procedure and the leader-skill check |
| docs | `docs/CLAUDE_SESSION_9_HANDOFF.md` (new) | this file |

Session 7/8 uncommitted work is preserved as found. **packet-generator**
(`a46b7df`): untouched (Session 8's `items.kdl` still uncommitted).
**offline-proxy** (`fps-cap` @ `d3aa843`): no source change; build tree
reconfigured and restored (§2, §9).

`out/qa/current/session9/`: build script/logs, `bin/` (final QA candidate),
proxy builds and diffs, harness / leader-skill / contention / synthesis logs and
drivers, `evidence/` (live-save backup copy, dry-run report), previous and
intermediate runner summaries.

## 12. Next bounded task and backlog

1. **Maintainer:** deploy §8 — it now carries the explicit commits (§R1), which
   make a concurrent read of the live save safe in every case — then the
   leader-skill repair and the Training Grounds helper check.
2. **Client:** §R4 (explicit `--exe`; replacement and missing-file recovery are
   unblocked; restart persistence; broker on/off timing).
3. **Next offline patch:** C1 as bounded in
   [OFFLINE_CONTENT_PHASE_C1_PLAN.md](OFFLINE_CONTENT_PHASE_C1_PLAN.md) — family
   ownership (`unit/img|cgg|cgs`, then `item`), a proxy enqueue filter, x86
   confirmation of the transitional handlers, acceptance with a second empty
   test package. No download screen, prompt or cache flow is removed in C1.
4. ~~Broker replace race~~ — done (§R2).
5. ~~Commit-failure recovery~~ — done (§R1). Remaining: a non-database
   exception inside a transaction still meets Drogon's destructor commit
   (healed at the next request); other suites that read the save right after a
   reply should use the BadgeInfo barrier.
6. Gameplay: trace UserUnitInfo `element` consumers (§5 side finding) before
   changing grants.

Still open (none closed by unrelated passes): #34 only for copy actions after a
lost ItemMix reply (documented protocol limit; the reported workflow is
player-confirmed fixed); #23 final-stage scene / first-clear next-area
presentation; mission 234 and compound gates; level-999 client results/energy;
leader EXP/Fuu; slots/parades; text, portraits, summon gates; missing EU units;
Trials/bosses/Fire Mecha drops; long-session town refresh; GuildUpdate; stored
leader skills until deployed, repaired and checked in Training Grounds.
Comment-only follow-up from Session 8 (ItemFavorite/ItemSell/ItemSphereEqp
"a craft" placeholder wording) is still pending.
