# Claude Session 12 handoff — repaired fixtures and verified client test targets

October 8 QC: see [QC_SESSION_12_REVIEW.md](QC_SESSION_12_REVIEW.md).
Independent targeted checks passed. Collection now preserves same-second
calls as well; this supersedes the harness hash below. Client acceptance is
still pending. Next directive: [CLAUDE_SESSION_13.md](CLAUDE_SESSION_13.md).

October 6, 2026. Directive: [CLAUDE_SESSION_12.md](CLAUDE_SESSION_12.md).
State: a reviewable candidate for Codex QC. Nothing deployed, committed or
pushed; no client was launched; the live save is byte-identical
(`239f9e69…`); the player's in-tree server EXE (`6ee7d454…`) was not relinked.

| | Result |
|---|---|
| P1 small roster | Crash cause proven from the three Run C dumps (null squad unit, not art). `clearunits` repaired transactionally; regression 24/0 (the Session 11 server: 9/15); repaired fixture `small_roster_repaired` built and validated through the client payload. |
| P2 Archive thumbnail | The Session 11 target `_100` is never drawn by the Archive; the tile is `unit_ills_thum_<id>.png`. No provider invalidation defect. New target `unit_ills_thum_810048.png`, tested end to end with the harness's own commands (driver 10/0, proxy 340/0). |
| P3 leader | "High Ruler's Chakra" in both runs is correct: the Training party screen AND battle use the master leader skill for the player's own unit as helper, in both client binaries. The stored value cannot change text or combat; the A/B request is withdrawn. |
| Touka | Not needed: ownership identical in C1 and C2; Session 10 showed it. |
| Candidates | Server **e30e2adf** (new). C1 DLL **523c77ed** unchanged (provider sources unchanged since it was built). Proxy tests **8536442e** (new test case only). |

## 1. Priority 1 — small roster and `clearunits`

### Root cause

- **Data.** Session 11's `small_roster_kit` kept only user unit 1000 (unit
  20011, Selena) while deck 0 still named deleted 1034 and 1113, and the three
  spheres worn by deleted units (warehouse rows 21, 28, 519) were lost with
  them. `DoClearUnits` (`standalone_frontend/DebugCli.cpp`) deleted unit rows
  and nothing else.
- **Client.** All three Run C dumps (`BraveFrontier.Windows.exe.2464 / .18024
  / .16744.dmp`; WER 14:33:27, 14:36:22, 14:37:35) fault with an access
  violation at `+0xA6ECFC` reading `0x00000210`: `+0xA6ECE1` calls
  `UserUnitInfoList::shared` (RVA 0x5BDBC0, a dispatcher that also asks
  RaidInfo / CampaignInfo / FrontierGate first), `+0xA6ECEF` calls
  `getObjectWithUserUnitID` (0x5BEBD0) for the squad slot's user unit, which
  returns NULL for a deleted unit, and `+0xA6ECFC` is the inlined
  `isMySummoner` read `mov al, [esi+0x210]` with no null check. The callers on
  the stack (`+0x660BE0`, `+0xA6BC5E`, `+0xA6A086`, `+0x63D430`, `+0x7EAA11`)
  reference `ARENA_COLOSSEUM_NOT_PT_SETTING_MSG2` and
  `leader_skill_name_frame`: `PartyEditTopScene2::initialize` /
  `createPopupLsf`. arm64 twin: `PartyEditTopScene2::setUnitAnime`. **Any
  squad slot naming a non-owned unit crashes the squad panes and Manage
  Squad**; missing art plays no part.

### Fix (`DebugCli.cpp`, one transaction)

`DoClearUnits` keeps the lowest `user_unit_id` and, inside one
`gme::beginTransaction` (rolled back on any exception):

1. returns the doomed units' worn spheres (`gme::returnEquippedSpheres`, the
   same helper UnitSell uses: they come back as live warehouse rows);
2. deletes the other units;
3. `RepairSquads` on `user_decks` (key deck_type, deck_num) and on
   `user_campaign_decks` (key deck_num): drops every slot naming a non-owned
   unit; a squad left without a leader gets the kept unit as leader --
   promoted in place if it is already a member, else inserted at the old
   leader's slot (fallback order 2, 0, 1, 3, 4, 5); no free slot = error and
   rollback;
4. ensures the active squad (deck_type 1, `user_info.active_deck`) exists,
   with the kept unit as leader at slot 2 (`addDefaultDecks`' shape);
5. deletes DBB bonds naming a removed unit;
6. reads the capacity **through the transaction** (the global client
   deadlocked on the single SQLite connection while the handle was alive --
   found in testing, fixed before the candidate) and commits.

With no unit at all it changes nothing and says so. It reports what it did:
`Cleared 108 unit(s); kept user unit 1000 (unit 20011). Returned 3 worn
sphere(s); squads repaired 2 (dropped 3 slot(s), set 2 leader(s)); campaign
squads 2 (dropped 3, leaders 1); DBB bonds removed 2. Cleared. Now at 1/110.`
(seeded case A). Not a broad refactor: other commands are unchanged.

### Tests

- `scripts/test_clearunits_wire.py` (new, port 19977, in the runner): runs the
  real command through the debug console (new `bf_testkit.DebugConsole`, the
  server's named pipe) and inspects UserInfo (`bf_testkit.roster_problems`:
  every `dX7S2Lc1` slot names a `4ceMWH6k` unit, one leader per squad, no unit
  twice, the active squad exists and is led by an owned unit) and the database
  (`dangling_unit_references`: decks, campaign decks, DBB bonds, worn spheres).
  Cases: A seeded dangling leader/member/campaign/bond references on a live
  copy; B a repeated clear (no change at all); C the old damage already
  present; D no units; E a second user untouched.
  **e30e2adf: 24 passed, 0 failed. 270832d6 (Session 11 server): 9 passed,
  15 failed** -- the same defect.
- `scripts/make_small_roster_fixture.py SERVER SOURCE OUT` (new): runs
  `clearunits` on a scratch copy, validates the UserInfo payload and the
  database, refuses an existing OUT, writes the fixture plus a JSON manifest.

### Repaired fixture

`out/qa/current/session12/fixtures/small_roster_repaired.sqlite`
(`402b2fb072419c32d9e2b1ff7fde0348efe3c6ec3682cf118fbcae124e421b6a`, manifest
`small_roster_repaired.json`), from the Session 11 crafting kit (`e0efedf8…`)
by e30e2adf: one unit (1000 / 20011 Selena), ten squads each led by it at slot
2, active squad 0, 24 live warehouse rows of 100, no roster problem, no
dangling row. Evidence kept unchanged (re-verified):
`session11/fixtures/small_roster_kit.sqlite` `01499b58…` and
`content_client_c1_yes/saves/small/gme.sqlite` `70fb14e7…`. The client run
seeds a NEW save name (`small_repaired`; `--seed` never replaces a save).

## 2. Priority 2 — the Archive thumbnail target

### What Session 11 actually exercised (Run A provider log, preserved in `session12/evidence/session11_runs_c1yes/` with SHA256SUMS)

- 9 Archive entries; Blaze's and Galea's **plain** thumbnails were served 9
  times each through `getTexture` (caller `+2354ED`), always generation 1.
  `unit_ills_thum_810048_100.png` was never read: only existence-checked
  (present at 28.4 s, absent at 1106.1 s).
- The swap/restore generations 2-6 were adopted at scene changes ("changed:
  ..._100.png") with **no** "released decoded copy" line: nothing had been
  drawn from that name, so there was nothing to release, and nothing visible
  could change.
- The park: absent -> `TransitionalDLCScene` -> `TRIPWIRE persistent write #1`
  of `..._100.png` (the player's 1/1 download, written into the package's
  LocalState by the game's own downloader) -> `UnitDictionaryScene`.
  Blaze's tile, the plain name, was untouched.

### What the Archive draws (static; full chain in offline-proxy `docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` §3.6)

- `UnitDictionaryScene::setUnitDictionary` (arm64 0x17B3910) draws
  `getUnitListThum(id, UserUnitDictionary::getUnitImgType(), false)` (call
  0x17B3EBC, DBB flag 0) via `getCacheSpriteForLocal(2, ...)`.
  `downloadFiles` (0x17B361C) calls `requestUnitFiles(id, 0, 0, 0)` for every
  dictionary unit, which requests the `_100` / `_2` names it may need.
  The player's numbers match `UnitMst` `XuJL4pc5`: 8318 = 810048 Blaze,
  8295 = 820278 Galea.
- `_100`: drawn only with the DBB flag: 43 of 124 call sites pass
  `isDbbEligible` (DbbMst pair, rarity >= 8, extra-skill level > 9; owned-unit
  lists, party and battle). Never the Archive.
- `_2`: requested when the dictionary flag (`2pAyFjmZ`, server-derived from
  `user_unit_alt_art`) is 1 **or** UnitMst `jyj6bl9P` (ext_home_img_pos) is
  non-empty. That explains every `_2` the Archive asked for in Run A (61097,
  860798; and 10017, 840257, 860257 elsewhere): the save has no alt art
  unlocked, all five have ext_home_img_pos. Drawn only when the flag is 1 and
  the client-local selection is >= 2. Acquiring a unit = a dictionary row with
  flag 0 = the plain art.
- Caching: the sprite is per scene (`GameScene`+0x210 `UICacheList`); the
  texture is `CCTextureCache` keyed by the LocalState path `getTexture` reads
  (x86 0x2353D0, contract §3.2); `TextureLoader` is a thin wrapper with the
  same key. The provider's safe point `removeTextureForKey(<that path>)` is
  therefore the complete invalidation. No provider change was needed.
- A withdrawn name: the provider answers absent, the game downloads it
  (tripwire), the draw still gets NULL (`getTextureForAll` 0x1CE7A58 never
  falls back while the provider says absent; the thumbnails are not in the
  client bundle) and `GameSprite::init(NULL)` substitutes the 2x2
  `cc_2x2_null_image` (x86 `+0x650F0C`). Predicted: a blank tile, not a
  crash. **Pending client observation.**

### New automated test

`scripts/test_content_visible_swap.py SERVER PROXY_TESTS POLICY` (new) +
`TestLiveVisibleSwap` in `offline-proxy/tests/test_main.cpp` (new, opt-in by
`BF_SWAP_*`). The driver builds the harness's disposable root for the C2
policy in its own folder (`out/qa/current/content_visible_swap`, never a test
package's), starts an isolated server (port 19963, fresh save) whose broker
serves it, and runs the proxy tests; between the proxy case's steps it runs
the harness's **own** `swap`, `restore`, `park`, `restore` (file handshake).
The proxy case is the game's side with the real provider and hook code: it
draws both tiles through `Hook_GetFileData` with the LocalState path, then
for each step checks the broker's publication, that the pinned scene keeps
its version, that `hooks::SafePoint` releases **exactly** the drawn key (not
Galea's), and what the next draw returns; for the park, all 8 bundle names
absent, `existsLocalFile` absent from the provider alone, the read refused
with no cache fallback, the queued download passed to the game's downloader;
after the restore, served again and kept out of the queue. The driver then
checks `disposable` (every file the original), the 16 real files of both
bundles byte-identical with unchanged mtimes, the live save, the port.
**e30e2adf + 8536442e: driver 10 passed, 0 failed; proxy 340 passed, 0
failed, 2 skipped (the other two live cases).** Publication: swap 21.4 s,
restore 13.7 s, park 6.6 s, restore 15.8 s.

## 3. Priority 3 — the leader comparison

Both client binaries agree; the stored `user_units.leader_skill_id` reaches
neither the text nor the combat of Training Grounds:

- **Response.** UserInfo's unit rows (`4ceMWH6k`, `UserUnitInfo` meta) carry
  the stored value as `oS3kTZ2W`: Berdette 1239 is 0 in the player's `c2_kit`
  save and 7401 in `leader_after` (re-read, read-only). The client keeps it as
  `UserUnitInfo::getLeaderSkillID`.
- **Helper selection.** Picking an own unit at the Training friend select
  makes `SandbagCheckScene::setPartyInfo2` (arm64 0x159A9D4) build a
  `FriendInfo` for it (`UserUnitInfoList::getObjectWithUserUnitID`, friend id
  `SANDBAG_FRIEND_ID`, image type, FE skills, summoner info) -- no leader
  skill is copied into it.
- **Text.** `SandbagCheckScroll::addskillInfo` (arm64 0x15A2E54) names the
  leader's and the helper's skill from `UnitMst::getLeaderSkillID`
  (0x15A3160, 0x15A419C) -> `LeaderSkillMst`. Berdette 10824's master skill
  is 7401 "High Ruler's Chakra" ("30% boost to Atk and max HP for all
  Units"), so both saves show it.
- **Combat.** `PlayerParty::entrySandbag` (arm64 0x10C7DA4; x86 at
  `+0x4179F9`..`+0x417E17`), own unit as helper:
  `initializeMyUnitFriend`, friend flag on (arm64 +0x334 / x86 +0x274),
  `UserUnitInfo::getLeaderSkillID()` -- the stored value -- into the guest
  leader-skill string (arm64 +0x918 / x86 +0x764). It never sets the guest
  id (arm64 +0x900 / x86 +0x74C; zeroed by the constructor, untouched by
  `initializeMyUnitFriend` x86 +0x178140). `BattleUnit::isGuestFriend`
  (arm64 0x113C480; x86 +0x46EBD0 and inlined at +0x40153C, +0x403C66,
  +0x407D5A) is "guest id non-empty AND friend flag", so it is false here,
  and `BattleParty::setPartyPassiveList` (arm64 0x10B3870; x86
  +0x401502..+0x401603) keeps `UnitMst::getLeaderSkillID`. The in-battle info
  screen (`MissionBattleUnitInfoScene::setLayout`) gates the same way.
- Every other reader of the stored value (10 in all) is a Challenge Arena copy
  (padlocked) or a campaign / Frontier Gate save-data upload.

So no deterministic before/after check can show a difference between
`c2_kit` (stored 0) and `leader_after` (7401): the expected difference was
invalid, and nothing was changed to force one. If QC wants the static result
confirmed on the client: same squad and equipment, own Berdette as helper,
read each unit's HP number on the battle panel at turn 1 in both saves --
predicted identical (both include the master skill's +30% HP). It is not in
the player checklist. This also corrects the Session 9 note that Training
was the stored value's one real gameplay effect; the leader-skill repair
(`fromArchivedUnit`, `repair_leader_skills.py`) stays validated server-side
only (`test_leader_skill_grants_wire.py`, in the runner).

**Touka.** Its seven owned names are identical under C1 and C2; Session 10
showed its preview with them served by the provider; its seven downloads were
particle / SuperAnim effects, legacy in both phases. No fixture was built.

## 4. Deferred, unchanged

DLC prompts, download/verification screens and content ownership are
unchanged. Family-scale publication stays slow: a swap or restore took
13-21 s and a park 2-7 s across this session's automated runs (QC 18-21 s;
Session 11 23-35 s). A later optimisation must keep same-size edit detection
and atomic bundles.

## 5. Validation

All logs are in `out/qa/current/session12/` unless named otherwise. Every
server result below is from **e30e2adf**, built from the current source (no
source file is newer than the EXE; `DebugCli.cpp` 15:30:51, object 15:31:16,
EXE 15:31:17). No result of an earlier binary is reused as evidence.

| Check | Command (from `BF-WorkingDirRust`) | Result |
|---|---|---|
| Server build (QA, override removed afterwards; only `DebugCli.cpp` recompiled) | `bash out/qa/current/session12/build_qa.sh` | e30e2adf in `session12/bin`; in-tree EXE still `6ee7d454…`; `CMAKE_RUNTIME_OUTPUT_DIRECTORY` absent from the cache |
| clearunits regression | `python scripts/test_clearunits_wire.py out/qa/current/session12/bin/gimuserverw.exe` | **24 passed, 0 failed** (`clearunits_candidate.log`) |
| The same on the Session 11 server (control) | `python scripts/test_clearunits_wire.py out/qa/current/session10/bin/gimuserverw.exe` | 9 passed, 15 failed: the defect (`clearunits_control_270832d6.log`) |
| Repaired fixture | `python scripts/make_small_roster_fixture.py out/qa/current/session12/bin/gimuserverw.exe out/qa/current/session11/fixtures/crafting_kit.sqlite out/qa/current/session12/fixtures/small_roster_repaired.sqlite` | written, no roster problem, no dangling row (`make_small_roster_fixture.log`) |
| Full runner, 51 suites | `python scripts/run_bugfix_regressions.py out/qa/current/session12/bin/gimuserverw.exe` | **1,992 checks passed, 0 failed; no failed suite**; validator at its 3,117-finding baseline (`runner_e30e2adf.log`, `SUMMARY_runner_e30e2adf.*`). Includes clearunits 24/0, harness startup 49/0, runner rules 30/0, broker 78/0, generation 17/0, policy 8/0, atomicity 70/0, contention 21/0, craft rows 53/0, leader-skill grants 48/0 |
| Proxy test build (test target only; the DLL target untouched, `build/bin/libcurl.dll` still `e24234c1…`) | `cmake --build build --config RelWithDebInfo --target content_provider_tests` (in `offline-proxy`) | `8536442e…` (`proxy_tests_build.log`) |
| Proxy tests, standalone | `build\tests\content_provider_tests.exe` (with `BF_APPX_DIR`) | **311 passed, 0 failed, 3 skipped** (the three live cases) |
| **Visible swap end to end (new)** | `python scripts/test_content_visible_swap.py out/qa/current/session12/bin/gimuserverw.exe ../offline-proxy/build/tests/content_provider_tests.exe out/qa/current/session11/family_policy_c2.json` | **driver 10/0; proxy 340 passed, 0 failed, 2 skipped** (`visible_swap_e30e2adf.log`) |
| Proxy against the real broker, fixture root | `python scripts/test_content_provider_live.py out/qa/current/session12/bin/gimuserverw.exe ../offline-proxy/build/tests/content_provider_tests.exe` | driver 5/0; proxy 341 passed, 0 failed, 2 skipped |
| Proxy against the real broker serving C2 on the real tree | `python scripts/test_content_family_live.py out/qa/current/session12/bin/gimuserverw.exe ../offline-proxy/build/tests/content_provider_tests.exe out/qa/current/session11/family_policy_c2.json out/qa/current/session11/first_visit_downloads.json` | driver 7/0; proxy 5,948 passed, 0 failed, 2 skipped |
| Harness commands (QC's three + two new collect cases) | `python -m unittest test_content_disposable_commands -v` (in `scripts`) | 5 passed |
| Harness QA smokes on 29960 (QA-only scripts) | `python out/qa/current/session11/harness_disposable_smoke.py out/qa/current/session12/bin/gimuserverw.exe out/qa/current/session11/family_policy_c2.json out/qa/current/session11/fixtures/crafting_kit.sqlite`; `python out/qa/current/session10/harness_save_policy_smoke.py out/qa/current/session12/bin/gimuserverw.exe out/qa/current/session11/family_policy_c2.json` | disposable smoke (`[ContentTest C1]` harness folder, kept root and save) **13/0**, its publications 13 s / 2 s / 13 s; save/policy smoke (`[ContentTest]` harness folder) **9/0**: READY, seeded, kept across a restart, live save byte-identical (`harness_disposable_smoke_e30e2adf.log`, `harness_save_policy_smoke_e30e2adf.log`) |
| Client-run pre-flight (read-only) | `python scripts/content_provider_client_test.py --identity c1yes check --exe out/qa/current/session12/bin/gimuserverw.exe --policy out/qa/current/session11/family_policy_c2.json` | 0 item(s) outstanding |

Skipped by design: the client runs themselves; the C1 DLL rebuild (no
provider source changed); a deploy.

## 6. Candidates and artifacts

| Artifact | SHA-256 |
|---|---|
| **Server candidate** `out/qa/current/session12/bin/gimuserverw.exe` | `e30e2adff2f016393de962e8229066d111f2503c2418f3a648f633ea01ea97a4` |
| Server, Session 11 (control only) `session10/bin/gimuserverw.exe` | `270832d6d8957f1327f71a3009c7ca4d6ad80142c086d5d3114af5f77924a764` |
| Superseded during this session (deadlocked after commit; never used for results) | `82f96946…` |
| In-tree player server (untouched) | `6ee7d454d8b75f47fc0d6e8be44c5284871ab885b3f6299ac74f25ac6df08a1b` |
| C1 DLL 29960, unchanged (in the C1 Yes package) | `523c77ed23479174f6f9ce2d222331f6209159048f8372987dbdd156aa21ef76` |
| `offline-proxy/build/bin/libcurl.dll`, unchanged | `e24234c11765275f775a093e7c47e12a44e6a6ba05591962474c7b466eef3f7b` |
| **Proxy tests** `offline-proxy/build/tests/content_provider_tests.exe` (copy `session12/proxy_tests_8536442e.exe`) | `8536442e16b8cec51f25aada31a06f9c65988a4dd5e93faf66ff6cd861c88ff3` |
| Proxy tests, Session 11 (preserved `session11/proxy_tests_646f8786.exe`) | `646f8786fb1d5d61b27b88ed3d445cb620a38d848661766304954694283a481d` |
| **Repaired fixture** `session12/fixtures/small_roster_repaired.sqlite` / manifest `.json` | `402b2fb072419c32d9e2b1ff7fde0348efe3c6ec3682cf118fbcae124e421b6a` / `eebf4485fe693760c7ba632ec008b4d6606eccbf25dc930c6213ac5a82c38993` |
| Fixtures used unchanged: crafting kit / C2 policy | `e0efedf84e382e15fb4d1a0ad7eb852b43b070e6f236a9374d8f0547e84d767b` / `6813f2dbb300f461b30e049a4123392e10290ee7977e0f6807740758e7504458` |
| Evidence kept: `small_roster_kit.sqlite` / `content_client_c1_yes/saves/small` | `01499b58…` / `70fb14e7…` (`session12/evidence_small_before.sha256`, re-verified) |
| Session 11 client logs (Runs A-C), preserved | `session12/evidence/session11_runs_c1yes/` + `SHA256SUMS` |
| Live save (read only) | `239f9e6931d1d45634d2a8c8cae8b66724d68bf8ff63f8ba46e19967bbee4666` before and after |

QA-only helpers (not required by any deliverable): `session12/build_qa.sh`,
`session12/logsplit.py` (process splitting of the provider log). Diffs for
review: `session12/DebugCli.vs_HEAD.diff`, `session12/scripts.vs_HEAD.diff`
(both include the earlier uncommitted work), `session12/proxy_test_main.session12.diff`
(this session only).

## 7. Changed files

Nothing committed. Earlier uncommitted work in both checkouts is preserved;
the hunks below are this session's.

| File | Change | SHA-256 now |
|---|---|---|
| `standalone_frontend/DebugCli.cpp` | `#include <set>`; the `clearunits` help line; `DoClearUnits` replaced and `RepairSquads` added (§1). The `InsertUnitFromMst` transaction and `dbwedge` / `dbfault` hunks in its diff are earlier work | `70819ed0…` |
| `scripts/bf_testkit.py` | appended `DebugConsole`, `roster_problems`, `dangling_unit_references` | `4e4e4986…` |
| `scripts/test_clearunits_wire.py` (new) | §1 regression | `1a0ea4c4…` |
| `scripts/make_small_roster_fixture.py` (new) | validated one-unit fixture builder | `6cc08566…` |
| `scripts/run_bugfix_regressions.py` | `test_clearunits_wire.py` added to `SERVER_SUITES` | `9668c8ae…` |
| `scripts/test_content_visible_swap.py` (new) | §2 end-to-end driver | `91253cb4…` |
| `scripts/content_provider_client_test.py` | `collect` also keeps each collect under `collected/<time>` | `8a2b6782…` |
| `scripts/test_content_disposable_commands.py` | two collect cases added; QC's three cases unchanged | `12e3a468…` |
| `offline-proxy/tests/test_main.cpp` | `TestLiveVisibleSwap` (opt-in `BF_SWAP_*`), `<functional>` | `47f064a1…` |
| `offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` | §3.6 drawn versus requested (Archive tile, caches, `_100` / `_2`, withdrawn names); a §7 cross-reference | `06e0cf9e…` |
| `docs/CLAUDE_SESSION_12_HANDOFF.md`, `docs/CLIENT_SESSION_12_STEPS.md` (new) | this handoff, the client checklist | |
| `docs/SESSION_11_CLIENT_RESULTS.md` | Session 12 section appended; earlier text untouched (copy before: `session12/SESSION_11_CLIENT_RESULTS.before_session12.md`) | |

No KDL, migration, deploy data, `deploy/` file or provider (`OfflineMod.win32/content`)
source changed.

## 8. Risks and unfinished work

- Client acceptance is owed for both runs in
  [CLIENT_SESSION_12_STEPS.md](CLIENT_SESSION_12_STEPS.md): the repaired small
  roster, and the visible swap / park / restore. The park's visual response
  is a static prediction (blank tile), not an observation. The visible-swap
  test drives the real provider and hook code against fake game functions:
  that the real `CCTextureCache` drops the key and the Archive redraws from
  it is shown by the binary reading (§2), not yet by a client.
- `clearunits` keeps the oldest unit even if it is a unit the account would
  rather not lead with; it does not touch other unit-referencing tables
  beyond squads, campaign squads, DBB bonds and worn spheres (the four the
  regression checks). Frontier Gate / arena snapshots of deleted units were
  not in scope (the command is a test tool).
- The x86 mapping of §3 covers `entrySandbag`, `isGuestFriend` and the
  passive-list choice; the x86 `SandbagCheckScroll` text path was not mapped
  (arm64 only, and the player observation agrees).
- The proxy tests now contain a third opt-in live case; the C1 DLL was not
  rebuilt because no provider source changed (all `OfflineMod.win32/content`
  files predate it; hashes in `session12/proxy_sources_before.sha256`).
- The harness `collect` now also keeps a timestamped copy; its old behaviour
  is unchanged otherwise.
- Publication latency (§4) and the remaining legacy families are open.

## 9. Client checklist

[CLIENT_SESSION_12_STEPS.md](CLIENT_SESSION_12_STEPS.md): Run 1, repaired
small roster; Run 2, Archive swap / restore / park / restore on Blaze. Both in
[ContentTest C1 Yes], new save names, full commands. The results record
[SESSION_11_CLIENT_RESULTS.md](SESSION_11_CLIENT_RESULTS.md) has a Session 12
section; nothing earlier was erased.
