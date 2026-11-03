# Claude Session 11 — handoff

Post-session QC: [QC_SESSION_11_REVIEW.md](QC_SESSION_11_REVIEW.md) records
independent passing checks, two corrected harness edge cases and revised client
instructions. Its qualifications supersede the original acceptance claims below.

October 5, 2026. Assignment: trace the remaining Vargas / Archive-Touka /
Training downloads, implement the next bounded asset-coverage expansion, a
capacity-valid crafting fixture, the hook-contract and VA/RVA work, and the
remaining targeted checks; regressions and final proxy-to-server integration.
Read after [QC_SESSION_10_REVIEW.md](QC_SESSION_10_REVIEW.md) (which supersedes
earlier acceptance statuses) and [CLAUDE_SESSION_10_HANDOFF.md](CLAUDE_SESSION_10_HANDOFF.md).

**Nothing was committed, pushed or deployed.** No server binary or proxy DLL
was rebuilt -- only the proxy's test executable. No production source was
edited. The live save was only read through the backup API (`239f9e69…`,
mtime 2026-10-04 01:07:05, unchanged). The three test packages, their
settings, the Session 10 test saves (`c1_visual`, `yes_visual`) and Session
10 fixtures were not modified (fixture hashes checked before and after). No
client was launched. Evidence: `out/qa/current/session11/`.

## 0. State checked at the start

- Server repo `main` at `e8cdefc`, packet-generator `a46b7df`
  (`audit-campaign`, `assets/net/items.kdl` modified), offline-proxy `fps-cap`
  at `d3aa843`; all pre-existing uncommitted changes preserved (the handbook's
  existing edits included; my paragraph is appended).
- No `gimuserverw` or client running (only unrelated Codex runtime processes).
- Candidates re-hashed: server `270832d6…`, C1 DLLs `523c77ed…` (29960) /
  `00df5a39…` (9960), `build/bin` = reviewed `e24234c1…`, proxy tests
  `3e1df7bb…`. No production source newer than its binary.

## 1. The remaining downloads, traced to files (priority 1)

The proxy logs suppressed queue entries only, so the downloads were recovered
from the packages themselves: LocalState write times attributed to the
provider log's scene changes, each process anchored by a value the game
timestamps -- `keyDLC_WARNING_MENU_FLG` in `Settings\settings.dat` is written
when Yes/No pops the prompt (No branch start 16:12:30.397, Yes 17:01:37.369),
and the last process ends at the log's mtime. Five other settings writes
corroborate both anchors (an initial curve fit was 6 s off; the settings
anchors caught it). Method and tool: [OFFLINE_CONTENT_PHASE_C2.md](OFFLINE_CONTENT_PHASE_C2.md) §1,
`trace_first_visits.py` -> `first_visit_downloads.json` (No branch: 5,752
writes, 0 unattributed).

| Screen | What downloaded | Loader / family |
|---|---|---|
| Vargas detail (Ignis Halcyon Vargas, 10017) | `unit_ills_{battle,full,thum}_10017_2.png` | alternate art, `unit/img`, `getTexture` |
| Archive entry | `layout_menu_unit_dict.csv` + 12 unit images: `_2` alternate art of 61097 and 860798, DBB-marked thumbnails `unit_ills_thum_<id>_100.png` (and `860798_2_100`) | `_dlcbundle` layout; `unit/img` |
| Archive, Touka (840767) — the "seven assets" | 5 particle plists (`effect_prt_1011`, `_101_2`, `_1020R_black`, `_1087`, `_1133_red`) + `20015BattleEffectBgBlackBgF60.sam` and its page | effects; the provider served Touka's own anime, cgg and cgs in that scene |
| Archive, first descriptions | description layout + 20 buttons, placeholder unit `00000`, particle plists, `BattleEffect` SAMs, SE mp3s | `_dlcbundle`, `effect/plist`, `sam/`, `sound` |
| Training | `VillageIcon.sam`; friend select `unit_ills_thum_850266.png`; 2 buttons; `BattleDungeon80066SandFront.sam` + 86 SE/voice mp3s; `Win.sam` + DbbMst combined art `unit_anime/cgg/atk_cgs` of 3810838 and 3840768 (`xbb/`); `LevelUp.sam` | SAM effects, sounds, xbb art, a SuperAnim unit's thumbnail |

None is explained by enhancements or unfinished background work. Producers
read in libgame.so: `_<n>` from `getUnitIllsImage` 0x117D084 /
`getUnitCutinImage` 0x117D694 / `getUnitListThum` 0x117CBC4 when the image
type is >= 2; `_100` from `getUnitListThum` when its DBB flag is set and the
unit is on either side of a DbbMst pair; the xbb names are DbbMst columns.

## 2. Phase C2: the unit art those visits still downloaded (priority 2)

A policy-only expansion: `scripts/gen_unit_family_policy.py` now defaults to
`--phase c2`; `--phase c1` reproduces the Session 10 policy and report byte
for byte (`69f0ecd3…`), and C2 only appends.

| Family | Files | Producer |
|---|---|---|
| alternate art `_2` (60 units) | 180 | image type 2 = the dictionary alt-art bit |
| DBB thumbnails `_100`, `_2_100` | 215 | DbbMst pairs (182 units, all with `_100`; 33 with `_2_100`) |
| DbbMst combined-unit art (`xbb/`) | 6 | 2 DbbMst rows, one bundle each |
| illustrations of the SuperAnim units C1 left incomplete | 405 | the units' own ids; their `unit_sam/` animation stays legacy |

C2 = **17,445** owned names (C1 16,639). Of the traced downloads, **27
distinct names are now owned**: all of the Vargas detail, 12 of the 13 Archive
entry files, 5 DBB thumbnails Home's first visit fetched, the Training
friend-select thumbnail and the 6 DbbMst art files of Training and missions.
The other 5,603 stay legacy, with their download screens and the DLC prompt.
Unit `1` (a placeholder) and Summoner parts, guild guardians and 860126 stay
legacy and are reported.

## 3. Capacity-valid crafting fixture (priority 3)

The live account holds 469 material stacks x50 (the debug console's
`additem mat` default): 485 live warehouse rows against a cap of 100. The
client counts `getActiveCount()` (rows with item_num > 0) against
`getMaxWarehouseCnt()`; `isOverItemBox` refuses quest/arena/trial entries over
it, and the crafting scene's own refusals are material shortage and the craft
cap -- so the exact message the player hit is not identified, only that the
copied saves were over capacity.

`out/qa/current/session11/fixtures/crafting_kit.sqlite` (`e0efedf8…`): a
backup-API copy of the live save with every non-material item kept and the
materials replaced by exactly three crafts each of five recipes the server
permits this account (Lunar Treat, Nian Gao, Rainbow Cupcake, Adorned Egg at
Synthesis; Mystic Lantern, a sphere, at Spheres). 24/100 live rows. Proved
through the server on a scratch copy: one ItemMix batch crafts all fifteen,
every material and output moves by exactly the kit, karma spent exactly,
craft counts advanced, the reply's warehouse is 23/100
(`prepare_crafting_fixture.py`, log beside it).

## 4. Hook contract and VA/RVA corrections (priority 4)

- `offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md`: the two FileLoader
  hook sites (§2), the download queues and the hook rule, its counter and its
  observation gap (§3.4), the prompt flag and its storage (§3.5), the C1/C2
  scopes (§6), and the obsolete "queue not intercepted" limit replaced (§7).
- Every x86 address in the C1 plan re-classified against the binary
  (`verify_x86_addresses.py`): **three were VAs** -- SaveUtils save/load
  (RVA 0x23FFD0 / 0x240300), the UserInfo singleton (RVA 0x159782C) and, in
  the contract itself, the downloaded-name map global (RVA 0x1597E20; the
  instruction at 0xE417E5 encodes its VA). Corrected in the plan and contract.
  `addLoadFile`: `ret 1Ch`, one caller (0x63E3A9 in `requestFileWithEncode`
  0x63E270); `addLoadFilePriority`: `ret 20h`, 33 call sites in 13 functions
  (the handoff's "13 callers").
- **Corrected a Session 10 claim:** "every enqueue path is gated by
  existsLocalFile" holds for `requestFileWithEncode` (its call at 0x63E2ED
  precedes both queues) but not for `addLoadFilePriority`'s twelve direct
  callers, and the function itself makes no call to it. Those direct callers
  are what the C1 suppression counter caught in the client runs
  (`MapVillage.sam`, `MapVillageAdd.sam`, `unit_ills_thum_50132.png`). The
  `cp_hooks_internal.h` comment "this should stay 0" is therefore stale; it is
  left until the next DLL change, so the reviewed source stays older than its
  binary.
- Repository handbook: the rollback-on-release transaction guard and the
  `dbfault` hook (Session 10's open item).

## 5. Remaining targeted checks prepared (priority 5)

- **Replacement / recovery on disposable content:** harness `server --policy P
  --disposable` serves a hard-link mirror of exactly P's files; `swap NAME
  --like OTHER`, `park NAME`, `restore [NAME]` change only the mirror (atomic
  link replacement; the real tree is never opened for writing), `disposable`
  verifies every other file is the original. The three commands wait until the
  running broker has published the change and print how long it took. Live
  smoke against the real broker (`harness_disposable_smoke.py`): **13/0** --
  swap republished with the other file's identity (23 s), park withdrew a
  whole DbbMst bundle (3 s), restore brought the original identities back with
  nothing unavailable (23 s); real files and live save byte-identical.
- **Leader comparison:** `crafting_kit.sqlite` stores 0 for Inferno Berdette
  (10824), `leader_after_kit.sqlite` (`82277c49…`) the same save after the
  leader repair, 7401 -- confirmed in the UserInfo reply each sends; both
  24/100.
- **Small roster:** `small_roster_kit.sqlite` (`01499b58…`): the kit plus the
  server's own `clearunits`, one unit, 21/100. (Session 10's roster_small,
  leader_before and leader_after are over capacity, 482-485/100; kept as they
  were.)
- **Performance:** `perf_compare.py` compares two runs of one route from a
  collected provider log, scene by scene. The `[ContentTest]` package already
  caches 17,424 of the 17,445 names, so provider-off needs no downloads there
  (only Town's 21 Phase B names are uncached: the route avoids Town).

## 6. Regressions and final integration

All against the unchanged server `270832d6` (logs `out/qa/current/session11/`):

| Run | Result |
|---|---|
| Full runner, 50 entries (`runner_270832d6.log`, `SUMMARY_runner_270832d6.*`) | **1,967 checks passed, 0 failed**; validator at its 3,117-finding baseline; broker 78/0, generation 17/0, policy 8/0, atomicity 70/0, contention 21/0, craft rows 53/0, harness mocked 49/0 |
| Proxy tests `646f8786`, standalone | 311 passed, 0 failed, 2 skipped (the two live cases, by design) |
| Proxy against the real broker, fixture root (`test_content_provider_live.py`) | 341 passed, 0 failed, 1 skipped (the family case); driver 5/0 |
| Proxy against the real broker serving **C2** on the real tree (`test_content_family_live.py`) | driver 7/0; 5,948 checks inside, 0 failed |
| `check_content_policy.py` shipped / `--policy` C2 | 8/0 / 8/0 |
| Harness: mocked suite on the final harness / Session 10 save+policy smoke / disposable smoke | 49/0 / 9/0 / 13/0 |
| C1 reproduction, scale (C2 vs C1 vs baseline, 2 repeats) | byte-identical; scale in [OFFLINE_CONTENT_PHASE_C2.md](OFFLINE_CONTENT_PHASE_C2.md) §5 |

The runner's total differs from QC's 1,972 only in two suites whose check
counts depend on random prizes (`test_reward_lock_snapshots_wire` adds two
checks per slot pull that carries the warehouse list: 20 / 24 / 26 across
recorded runs; `test_brave_slots_wire` 249-252); no suite failed or lost a
case. The first disposable smoke passed 12/0; its re-run on an existing mirror
failed 2 checks because the broker published the swap and restore after
32-35 s, past the smoke's 30 s waits -- see §9. The commands now wait for
publication and the smoke passes 13/0.

## 7. Candidates and artifacts

| Artifact | SHA-256 |
|---|---|
| Server (unchanged) `session10/bin/gimuserverw.exe` | `270832d6d8957f1327f71a3009c7ca4d6ad80142c086d5d3114af5f77924a764` |
| C1 DLL 29960 / 9960 (unchanged) | `523c77ed23479174f6f9ce2d222331f6209159048f8372987dbdd156aa21ef76` / `00df5a39c674350d8b93b4d07a9cab64c9f1a36f0b07ed220183529bc267034b` |
| `offline-proxy/build/bin/libcurl.dll` (reviewed, unchanged) | `e24234c11765275f775a093e7c47e12a44e6a6ba05591962474c7b466eef3f7b` |
| Proxy tests (new; `build/tests` and `session11/proxy_tests_646f8786.exe`) | `646f8786fb1d5d61b27b88ed3d445cb620a38d848661766304954694283a481d` |
| Proxy tests, first Session 11 build (no existence-gate check; superseded) | `c51ea984594f8042e3ad31fb38b8b453300f87050f3b778d8d70148db6f31f90` |
| Proxy tests, reviewed, preserved (`session11/proxy_tests_3e1df7bb_reviewed.exe`) | `3e1df7bbf192ea8db1113344ecaaa97791469b744108982193effa515a103a40` |
| C1 policy (reproduced) / **C2 policy** `session11/family_policy_c2.json` | `69f0ecd3…` / `6813f2dbb300f461b30e049a4123392e10290ee7977e0f6807740758e7504458` |
| Fixtures: crafting_kit / leader_after_kit / small_roster_kit | `e0efedf8…` / `82277c49…` / `01499b58…` |
| Trace evidence `first_visit_downloads.json` | `86521f94…` |
| `scripts/gen_unit_family_policy.py` | `5ad7cabb0aad7e8ec167617c62da8d9d261d14eb9b3fa30d74caeb8b582a395c` |
| `scripts/check_content_policy.py` | `e97c53c3d6dbd70e50e5aaa92afb0c6f4131e62a93a081fff9b9ccd81b39d2c9` |
| `scripts/content_provider_client_test.py` | `1dc190c3ba4822ecc6077c0c4d76d3b83d9446201c5389cc6ace4685a0d2f5d6` |
| `scripts/test_content_family_live.py` | `59c323853c8012f594f69946a0cfb0cdb80f9894ee8dfdcd3eaf0480fc25cf75` |
| `offline-proxy/tests/test_main.cpp` | `4809f9371cf56e9751383d953fc74acc08558d1220fea90eb6604aa65832cf6e` |

## 8. Changes

Server repo (all uncommitted): `scripts/gen_unit_family_policy.py` (`--phase`,
C2 families), `scripts/check_content_policy.py` (`--policy`),
`scripts/content_provider_client_test.py` (`--disposable`; swap / park /
restore, which wait for the broker to publish; disposable), new
`scripts/test_content_family_live.py`; docs
`OFFLINE_CONTENT_PHASE_C2.md`, `CLIENT_SESSION_11_STEPS.md`, this handoff,
`OFFLINE_CONTENT_PHASE_C1_PLAN.md` (address corrections),
`BF_OFFLINE_SERVER_HANDBOOK.md` (transaction guard). No C++, KDL or deploy
data change. offline-proxy (uncommitted): `tests/test_main.cpp`
(`TestLiveFamilyPolicy`, opt-in), `docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md`.
QA-only scripts live in `out/qa/current/session11/`.

## 9. Limitations

- C2 is server- and integration-tested, not client-confirmed. The existing
  packages already cache the 27 newly owned names, so they cannot show those
  download screens disappearing; the client check is correct art plus the
  provider log serving the names, and the queue suppression is exercised by
  the integration test with the real hook code.
- The account has no alternate art unlocked: the Vargas `_2` names are
  existence checks the detail screen makes; the art itself is not shown.
- Still legacy by design: scene layouts and buttons (`_dlcbundle`), SuperAnim
  effects (`sam/`, `unit_sam/`), particle plists, item and sphere art,
  banners, audio and voice, Summoner parts. Each needs its loader verified
  before ownership ([OFFLINE_CONTENT_PHASE_C2.md](OFFLINE_CONTENT_PHASE_C2.md) §6).
- The provider still does not log pass-through queue entries; tracing relies
  on the forensic method (a DLL logging change would make it direct).
- **Broker latency at family scale (new finding):** a replace inside a folder
  -- a swap, a restore, a real content update -- arrives as a folder-level
  change, and `rescan("directory change")` re-hashes every file with a known
  digest, i.e. every owned file, before publishing: 23-35 s at 17,445 names
  (a plain removal: 1-3 s). The provider keeps serving the pinned version
  meanwhile, so nothing mixes; it is latency. Limiting that rescan to the
  changed folder or to files whose size/time changed is a server change left
  for a later session.
- Single-statement handlers that swallow database errors (UnitFavorite,
  Scenario) are unchanged.
- Park/restore on a client rewrites the parked file into that package's
  LocalState through the game's own download (by design: an owned name
  reported absent goes to the legacy path); the provider never reads it.

## 10. Client checklist

[CLIENT_SESSION_11_STEPS.md](CLIENT_SESSION_11_STEPS.md): four short runs --
A (C2 art, Archive swap/park/restore, Training with leader "before",
crafting kit), B (leader "after"), C (small roster), D (performance, provider
on versus off, on the Session 7-9 package). No prompt branch or restart
sequence is repeated.
