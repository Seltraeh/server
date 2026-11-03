# Claude Session 10 — handoff (stopped at the usage limit; partial)

> October 5 independent QC completed the full regression run (1,972 checks,
> no failures; existing mission baseline unchanged) and final C1 live proxy
> integration (341/0, driver 5/0). See [QC_SESSION_10_REVIEW.md](QC_SESSION_10_REVIEW.md)
> and [CLIENT_SESSION_10_STEPS.md](CLIENT_SESSION_10_STEPS.md). The C1 package
> is now prepared and registered, with account-state files only; its own
> loopback exemption and visual acceptance remain user steps. The observations
> below describe Claude's original stopping point.

Assignment: [CLAUDE_SESSION_10.md](CLAUDE_SESSION_10.md), after
[QC_SESSION_9_ROUND2_REVIEW.md](QC_SESSION_9_ROUND2_REVIEW.md). October 4, 2026.
**Nothing committed, pushed or deployed.** The live save was only read
(unchanged, `239f9e69…`, 01:07:05); neither client nor its settings was touched;
the in-tree server EXE is still `6ee7d454…`. All evidence: `out/qa/current/session10/`.

## 1. Live-provider CSV finding — resolved as test synchronization

- `PollOnce` keeps any delta whose head generation moved, including a
  generation changed only by an unowned file. `SafePoint` adopts only what was
  polled, and `replaceWith` waited only for `HasPending()`. So an unrelated
  generation could be adopted before the CSV edit was published. That explains QC's
  "digest changed on read" generation followed by 409s.
- The old test binary `c40131c9` passed 20 of 20 runs on `47fe25fc`; the race
  never occurred naturally on this machine. Each run's server log, test output
  and provider log are kept in `live_runs/<tag>/runNN_*` (`live_repro.py`).
- **Fix (test layer only; the production provider is unchanged)**, in
  `offline-proxy/tests/test_main.cpp`:
  - An edit counts as ready once the server publishes the new digest *and*
    a poll after that is pending.
  - The live case now reproduces the race deterministically: an unowned
    file's generation is polled first. The old wait's condition is then met,
    but the adopted generation lacks the CSV, so its read is refused; the edit
    is adopted only after a later poll.
  - New `TestPendingReadinessQC`, using `FakeBroker::Bump` in
    `tests/fake_broker.h`.
  - Byte-exact and pinned-version assertions are kept.
- New test binary `9235c88a…`: 20 of 20 live runs pass (324 / 0 each), with the
  same generation sequence every run.
- Client checklist sections 3 and 4 are therefore unblocked as far as this
  finding goes. Note that the server and proxy candidates have changed since (§4).

## 2. Database residual — fixed in the candidate

- **The guard.** `gme/common/Transactions.hpp`: `beginTransaction` returns a
  handle whose release sends ROLLBACK plus an empty BEGIN. Anything not
  explicitly committed is undone, and Drogon's destructor commits only the
  empty BEGIN. The guard uses statements, not Drogon's `rollback()`, which
  avoids a "Transaction roll back error" line after every refusal.
- **The fault hook.** The debug console command `dbfault` (`gme::armCommitFault`)
  makes the next commit throw a non-database exception, simulating a handler
  that fails after all its writes.
- **Negative control** `651b85c1` (fault hook, no guard): `FixGiftInfo`'s failed
  request kept its `user_info` write (39 / 2).
- **Autocommit conversions.** These multi-statement flows now run in one
  transaction each:
  - MissionStart: energy, run record and open battle;
  - GuildCreate: fee and guild, replacing the manual refund;
  - GuildMemberUpdate: dissolve;
  - CampaignReceipt: claim and rewards, with database errors no longer swallowed;
  - DailyLogin spin and MysteryBoxClaim;
  - the two Summoner Journal claims;
  - UserInfo's starter selector and login campaign, and UpdateInfoLight's login campaign.

  The transaction audit now reports 52 sites, 0 unchecked; it is a lint, not
  a proof.
- **Trigger control** on `47fe25fc` (`--triggers-only`):
  - MissionStart kept the spent energy and the run record.
  - CampaignReceipt answered success with the claim and items kept but no Gems.

  On the candidates both cases keep nothing.
- **`scripts/test_db_exception_atomicity_wire.py`** (port 19999, runner entry
  50) passes 70 / 0 on `f088c091` and `ced4f3b4`; all 11 faulted requests rolled back.
- **Not converted:** writes that are independent per item or single
  statements. These are the ShopUse purchases (single guarded UPDATEs),
  UnitFavorite and Scenario, the avatar edit, guild invites, friends, the
  town and dungeon-key state refreshes, and campaign run-state pointers.
  UnitFavorite and Scenario still swallow database errors and reply success.
- **Runner** on `f088c091`: 50 entries, 1,971 checks passed, 0 failed (the
  validator is at its 3,117-finding baseline). On the **final `270832d6`**:
  **50 entries, 1,968 checks passed, 0 failed**, with the validator at the same
  3,117 baseline (`runner_270832d6.log`, `SUMMARY_final_270832d6.md`).

## 3. C1 slice ([plan](OFFLINE_CONTENT_PHASE_C1_PLAN.md))

**x86 confirmed** (RVAs; image base 0x400000):

| Function | RVA | Calling convention and arguments | Notes |
|---|---|---|---|
| `FileLoader::addLoadFile` | 0x634FA0 | `__thiscall`(bool encode, `std::string` by value), `ret 1Ch`; callee frees | only caller `requestFileWithEncode` 0x63E270 |
| `FileLoader::addLoadFilePriority` | 0x1049A0 | `__thiscall`(bool, bool, string), `ret 20h` | checks `existsLocalFile` itself; 13 callers, as on arm64 |

Every enqueue path is gated by `existsLocalFile`.

**Corrections to the plan** (two of its addresses were VAs, not RVAs):

| Item | Correct RVA |
|---|---|
| `SaveUtils` load / save | 0x240300 / 0x23FFD0 |
| UserInfo singleton (VA 0x199782C) | 0x159782C |

**Proxy:**
- `Hook_AddLoadFile` / `Hook_AddLoadFilePriority` suppress an enqueue only
  when the provider is active and the name is owned and present. They free
  the by-value string with the game's `delete`; everything else goes to the
  original.
- Signatures regenerated (17 sites); the check against the real binary passes.
- `TestEnqueueFilter` covers startup, provider off, dormant, absent and
  not-owned names.
- Tests `3e1df7bb…`: 311 / 0 standalone; the live case has not yet been rerun.
- DLLs: C1 for port 29960 `523c77ed…`, C1 for 9960 `00df5a39…`. `build/bin` is
  restored to the reviewed `e24234c1…`, and the CMake cache is identical.

**Server:**
- `scripts/gen_unit_family_policy.py` (policy and report in `family_policy.json`
  / `family_report.json`):
  - 2,155 of 2,291 UnitMst units are owned: 16,628 files, one bundle per unit.
  - 136 units are incomplete (mostly SAM-animated) and are reported; they stay legacy.
  - 0 basename collisions; 4,880 variant files stay legacy.
- The broker matches wildcard-free scope entries through a set lookup.

**Measured** (`family_scale_results.json`):

| | 21-name baseline | unit families |
|---|---|---|
| ready from process start | 8 s | 14–17.5 s |
| index build | 3.8 s | 9–13 s |
| server memory | — | about +45 MB |
| full index | — | 2.29 MB, 140 ms |
| empty delta poll | — | 54 B, about 1 ms |

**Harness** (Codex's label, version and quoting fixes kept):
- `--identity c1` selects a fresh `gumi.BraveFrontier.ContentTestC1` package
  labelled "[ContentTest C1]".
- `KNOWN_DLLS` includes the two C1 hashes.
- `server --save NAME [--seed PATH]` uses a persistent fixture save;
  `--policy PATH` serves a generated policy.
- Mocked suite 49 / 0; live smoke 9 / 0 (the family policy served 16,639
  names, the save was kept across restarts, the live save was byte-identical).
- `bf_testkit.Fixture.create(keep=True)` supports the persistent save.

**Fixtures** (`fixtures/`, described in `fixtures.json`):

| Fixture | Contents |
|---|---|
| `roster_live` | 109 units |
| `roster_small` | 1 unit; logs in |
| `roster_new_units` | 10 never-owned family units queued as presents; unit box 130 |
| `leader_before` / `leader_after` | unit 10824, leader skill 0 → 7401, "30% Atk and max HP for all Units" |

## 4. Builds

| Artifact | SHA-256 |
|---|---|
| Server stage A (fault hook, no guard; control in `control_noguard_651b85c1/`) | `651b85c1…` |
| Server stages B, C, D | `e3b5a285…`, `f088c091…`, `ced4f3b4…` |
| **Final server `session10/bin/gimuserverw.exe`** | `270832d6d8957f1327f71a3009c7ca4d6ad80142c086d5d3114af5f77924a764` |
| Proxy tests | `9235c88a…` (CSV fix), `3e1df7bb…` (+ filter) |
| C1 DLLs `proxy_29960_c1/`, `proxy_9960_c1/` | `523c77ed…`, `00df5a39…` |

## 5. Left for the next session

1. Rerun `test_content_provider_live.py` with `3e1df7bb` and the broker
   repeats (the scope matching changed). The runner on `270832d6` is done (§2).
2. Document the new hook sites in
   `offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md`, the guard in the
   handbook, apply the plan corrections from §3, and update the memory files.
3. QC review of the C1 DLL. Client steps:
   - lay out `--identity c1` with `proxy_29960_c1`;
   - run `server --policy family_policy.json --save … --seed fixtures/...`;
   - test both answers to the download prompt, Archive first visits and the
     new-unit presents;
   - check stale-cache precedence via `state --stale-town`;
   - compare broker on and off timing on one `--save`;
   - do the Training Grounds leader comparison (`leader_before` / `leader_after`).
4. Leave the client's existing download and cache flows in place (out of scope).
