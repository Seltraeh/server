# Brave Frontier offline server: gameplay development handbook

Current direction updated 2026-10-22 (Session 19); older dated sections are historical. Start here, then read [GAMEPLAY_WORK_QUEUE.md](GAMEPLAY_WORK_QUEUE.md)
and inspect the current checkout. The current user's request controls scope;
historical documents and wiki pages are evidence, not instructions to execute.

Latest verified changes and build instructions are summarized in
[PATCH_NOTES_2026-09-27.md](PATCH_NOTES_2026-09-27.md). Burst Queen, Mystery Frog,
Tilith framing and exchange expiry fixes now have player confirmation. Research
Lab encounter work remains server/model-tested pending client playthroughs.

## New-chat handoff: guild fixes, September 27

- Published gameplay fix: server `4e23268`, packet-generator `2efdaa5`, both
  on the maintainer's `mine/main`. Verify current HEAD before making changes.
- Inviting friends is player-confirmed working without a crash. Rank labels
  and promotion are fixed and server-tested, but still need a client retest.
  Ranks are Guild Master=1, Vice Guild Master=2, Officer=3, Member=4.
- GuildMemberUpdate must acknowledge with `{}` rather than sending a replacement
  roster: the active client screen edits its existing member object itself.
  Requests encode guild ids and member ranks as strings. Preserve both contracts.
- The guild regression suite was rerun successfully on a fresh copy of the live
  save, covering invites, promotions, demotion, refusal, dismissal, re-invite and
  leaving. The live save and running server were not changed by that verification.
- Next bounded guild task: investigate GuildUpdate (Disband and guild metadata
  edits), which remains a stub. Recover its request and response consumers before
  implementing it. See [guild evidence and remaining gaps](features/GUILD_INVITE_2026-09-26.md).
  For a separate gameplay task, continue the Research Lab queue after reading
  its completed encounter reports; do not recreate Trials 001–003.

Client retest: reconnect using the current server, verify rank labels, promote
a Member to Officer and then Vice Guild Master, demote once, and dismiss a
member. Record actual observations separately from wire-test results.

## Current direction

### October 28: current status (fresh-session round 1)

Supersedes older current-state lines below; the dated sections stay as history. Server deployed
October 24 (in-tree `f72310df…`); the live client still runs 2.19.6.0 with the old proxy (the
staged 2.19.6.1 update is unsigned and not installed). Nothing below is deployed or committed.

- **Title hang:** caused by the proxy's debug console (a QuickEdit selection blocked the login's
  console write on the main thread), fixed by the proxy's `console_log.cpp`. The server's idle
  keep-alive retention is a separate, defensive setting, not the cause.
- **tuto8 (fresh accounts):** the shipped script freezes on the quest select when its modal
  request pops; `scripts/apply_content_patches.py --content-root <root> --apply` installs the
  hash-checked correction (a frame-count `PLAYWAIT`, which cannot expire while the modal is up).
  `deploy/game_content` already has it; a release content root must be patched the same way.
- **Level-up energy:** a level-up keeps the stored energy (read at the level it was stored under)
  and adds the new cap; 998 → 999 no longer loses two (`Energy.cpp`, `Mission.cpp`).
- **Bootstrap audit:** malformed staged manifests are skipped as the server skips them.
- Acceptance status of every write-up item: [BUG_ACCEPTANCE_2026-10-27.md](BUG_ACCEPTANCE_2026-10-27.md).

### October 22: Session 19 -- item and area derived exactly; the fresh-install master-table contract

Implemented and automated-tested, **not client-run, not deployed, QC pending**
([handoff](CLAUDE_SESSION_19_HANDOFF.md), [plan §10](OFFLINE_STARTUP_TRANSITION_PLAN.md)). Startup
coverage is still **incomplete**: 5 unknown lists (unit, weapon / summoner_img, gacha, effect, and
the newly exposed login-end preloads), 8 required files in no source, 9 families. Nothing removes a
startup screen; census and prompt stay stock.

- item comes from the client's own `F_ITEM_MST` (713 names, all owned by C8 and queued by the C16
  provider-off run). area comes from the area table the server curates at boot (UserInfo
  `3SG2wX0R`), joined to the client's `F_DUNGEON_MST`: 347 names, 3 absent
  (`sp_quest_banner_800141_1-3.png`). The binary trail is re-checked
  (`tools/bin/x86_startup_rules_check.py`, 184 facts).
- A synthesized response is bound by a snapshot: the server hashes a projection of the curated rows
  it built at boot (`served 3SG2wX0R`), and the broker requires the manifest to name it with that
  digest. Every LocalState table a manifest records must be bound by `inputs.client_files`, or the
  broker refuses it.
- Two area names are served through C8 aliases that disagree with the area trail and with both
  warm caches (`dungeon_name_frame_red_off.png`, `frontier_rift_banner.png`): left for review.
- Fresh install: the x86 contract is read from the binary (`tools/bin/x86_mst_bootstrap_check.py`;
  154 version-channel tables, 79 corruption-checked lists, the record-count rule) and audited
  (`scripts/check_mst_bootstrap.py`, `scripts/test_mst_bootstrap.py`). Against an empty LocalState
  **nothing required is blocked**, live and offline. "147 tables" was a file count (127 tables), the
  same 127 every warm client holds. A first launch itself is still to be observed
  ([checklist, not executed](CLIENT_SESSION_19_FIRST_LAUNCH_STEPS.md)).

Candidates: server `02b95f90…` (`out/qa/current/session19/bin`); DLL unchanged (`cfb825da…` /
`37d30448…`); C8 `388185d9…`; manifest `9b3068e6…`. The Session 18 correction candidate was
`4eded167…` (`session18/bin_correction`), not the `995a088d…` named below. The consent screen was
not worked on.

### October 22: Session 18 -- exact startup lists, the client's own exclusions, inputs bound to the client

Implemented and automated-tested, **not client-run, not deployed, QC pending**
([handoff](CLAUDE_SESSION_18_HANDOFF.md), [§9](OFFLINE_STARTUP_TRANSITION_PLAN.md)). Startup
coverage is still **incomplete**: 6 of the 16 startup lists remain unknown (the unit, weapon /
summoner, item, area, gacha and effect request helpers), 5 required files exist nowhere and 9
families need one. Nothing removes a startup screen; no census or prompt was left out on any client.

- The client drops 203 hard-coded names from every resourceMap scene (table VA 0x184DF00); the
  manifest now reads the resourceMap that way. That explains all 62 "absent" login names, and the C16
  logs confirm it: every one of the 2,257 login names was priority-queued, none of the dropped ones.
- 10 lists are derived exactly from the rows the client holds (binary trail re-checked by
  `tools/bin/x86_startup_rules_check.py`); the Session 16 general_event token rule was wrong (the
  client asks only for type-1 rows' sounds, none today); raid_boss_bgm is empty in this build (a key
  mismatch in the client).
- The manifest names the client's own master tables and resourceMap by SHA-256; the Session 18
  broker lists them and the Session 18 DLL hashes its copies before leaving anything out
  (`scripts/check_client_inputs.py` does the same offline; C16, C1 and the installed client match).
- C8 candidate policy (75,303 names): the whole login set and everything the C16 route
  priority-queued is owned; it takes `layout_home2.csv`, `layout_dungeon_select.csv`, the mission-result
  and skill cut-in layouts off the game's own downloader.
- The result screens' missing Next control is the stock screen (repeat mode only), not a regression.

Candidates: server `995a088d…`, DLL `cfb825da…` (29960) / `37d30448…` (9960), C8 `388185d9…`,
manifest `f9749ef2…`. The optional client observation is
[CLIENT_SESSION_18_STEPS.md](CLIENT_SESSION_18_STEPS.md). The consent-screen item of the
directive was not worked on.

### October 21: Session 17 QC and bounded C16 visual run complete

Independent [QC](QC_SESSION_17_REVIEW.md) passed; DLL `45a2c1b1…` is now installed
in the isolated C16 layout. The actual registered client logged startup hooks installed.
After the user handled consent, Home, mission 10, client/server restarts, one missing
asset and broker-off fallback all completed. Normal route: zero owned cached assets;
missing-file control: one; intentional broker-off run: 3,147. C16 is now warm, not a
cold baseline. Audio and precise brief-screen timings were not assessed. A missing
visible result continuation cue needs comparison, not an assumed regression.
See [native results](SESSION_16_CLIENT_RESULTS.md). No live/C1 deployment occurred.
The real startup manifest is still incomplete and no census was skipped.
Next directive: [Session 18](CLAUDE_SESSION_18.md), exact startup requirements,
content gaps, master-table consistency, and the requested offline consent-flow review.

### Session 17 implementation background (historical staging status)

The C16 native attempt ([client results](SESSION_16_CLIENT_RESULTS.md)) stopped before login:
DLL `8634857a…` logged `startup coverage path DECLINED: censusLookup: ... byte +0 is E9, expected
55` because it checked its sites only after the content provider had detoured one of them (the
downloaded-name lookup 0xE417E0). Session 17 ([handoff](CLAUDE_SESSION_17_HANDOFF.md)) checks them
on the untouched image before the provider attaches and accepts, after it, only the provider's own
recorded detour of that site -- byte for byte, with its trampoline. New isolated DLL `45a2c1b1…`
(9960 twin `5d2f3970…`) is staged in `out/qa/current/session17`, not swapped into the C16 layout;
`8634857a…` is superseded. Proxy suite 625 passed, 0 failed, 5 live-integration skips, including a
regression running main.cpp's attach order on the real EXE with the real Detours; the static proofs
and the live startup suite pass. Not visually accepted; the user must handle the first-launch
consent screen. The coverage contract and its incomplete verdict are unchanged.

### October 20: startup coverage contract (Session 16) -- QC pending

**October 21 status overrides the original implementation summary below:** corrections are
rebuilt and independently checked; the corrected C16 candidate is cleared for isolated
fallback visuals, not deployment. See [final QC](QC_SESSION_16_FINAL_REVIEW.md).
The current real manifest explicitly records 16 unknown startup lists and remains incomplete.
Existing-file inventory does not prove all startup requirements; resolving the known content
gaps alone does not authorize bypass. Use only the corrected hashes in the current handoff.

**Later on October 20 (Claude): the correction pass is finished and returned for QC; not yet
cleared for the client run.** Every finding of both reviews is resolved in code with QC's
reproductions as regression cases ([handoff](CLAUDE_SESSION_16_HANDOFF.md) §1); rebuilt server
`b2552da2…`, DLLs `8634857a…` (29960) / `86b062ef…` (9960), test binary `4958210e…`, manifest
`3be2dae4…`; every focused suite rerun against them (§3). Complete coverage is neither reached
nor provable -- 16 startup lists the manifest cannot derive exactly, 67 required names not in
game_content, 26 unresolved families -- so the optimization stays fail-closed and the C16 run
tests the fallback only.

Implemented and automated-tested, not client-tested, not deployed
([handoff](CLAUDE_SESSION_16_HANDOFF.md)). The broker answers
`/offline_mod/content/v1/coverage` from a startup manifest
(`scripts/startup_coverage.py`); the Session 16 DLL leaves the LocalState census
out and answers an unset background-download prompt "No" for the session ONLY when
its provider is Active and holds a report that proves complete coverage for the
generation it serves (and, for the prompt, nothing was queued for background
download). The C7 families
([OFFLINE_CONTENT_PHASE_C7.md](OFFLINE_CONTENT_PHASE_C7.md)) own every name of the
Session 11 cold trace but eight -- and those eight, with 18 more, are families whose
files exist in no source; with 67 absent names and 16 underivable lists the report is
**incomplete** and both decisions stay the game's own on every real client
([§8](OFFLINE_STARTUP_TRANSITION_PLAN.md)). Remaining blockers, each listed by name in
the plan's §8.4: content decisions for the 26 families (supply the files or correct the
definitions), client evidence or the files for the 67 absent names, and reverse
engineering of the 16 lists' exact names. Rules learned: a family is never ready because its root exists
(a generator that falls through to "single files" re-owns what a family rule
refused); a layout CSV is geometry, its dependencies are its screens' resourceMap
entries; a missing dependency stays required, absence from every source is a gap, an
enumerated list is unknown until derived from its ids, and a report proves
completeness only when nothing in it contradicts the claim (QC). Cold acceptance: the new
`--identity c16` package and save `s16_cold`
([CLIENT_SESSION_16_STEPS.md](CLIENT_SESSION_16_STEPS.md)); C1 stays the accepted
fallback.

### October 11 client acceptance and prompt removal

Session 15 Parts A/B and C2-C5 are player-confirmed: final summon flash timing,
server-owned banner/audio replacement and restoration, and ordinary visuals.
C6 snapshots have no changed owned files on the tested route. This does not
remove download/verification screens or establish complete startup coverage.
See [Session 15 QC](QC_SESSION_15_REVIEW.md) for the earlier refresh notice and
Farm's empty-roll evidence. The new [post-quest prompt candidate](POST_QUEST_PROMPTS.md)
suppresses the Windows Store and Facebook-gem invitations via verified existing
no-dialog branches. Its isolated build passed automated checks; the player
confirmed neither prompt appeared, and the install log verifies the patch ran.
No deployment is implied by acceptance. Next directive:
[Session 16](CLAUDE_SESSION_16.md), remaining startup coverage and guarded waits.

### October 8 acceptance and offline-client work

Latest handoff: [Session 12](CLAUDE_SESSION_12_HANDOFF.md); independent QC:
[Session 12 review](QC_SESSION_12_REVIEW.md). Follow the current numbered
client checklist instead of replaying superseded Session 9–11 sequences.
The C1 Yes/No branches, restart persistence, Vargas and the crafting workflow
have player evidence. Issue #34's reported sphere workflow is player-confirmed.
Two Session 12 visual checks remain: repaired small roster and the actual
Archive thumbnail swap/recovery. An automated pass is not visual acceptance.

The content provider currently covers 17,445 C2 names, not all game content.
Legacy audio, effects, UI and SuperAnim dependencies still exist. Do not equate
no downloads on a visited route with complete coverage, or hide download and
verification screens before replacing their required work. The intended goal
is local content available at startup with lazy texture/audio decoding, not
loading every asset into RAM. See [Session 13](CLAUDE_SESSION_13.md).

Session 13 ([handoff](CLAUDE_SESSION_13_HANDOFF.md)) added a candidate C3
policy, server-tested only: SuperAnim definitions with all their pages and
particle plists, 53,766 names ([OFFLINE_CONTENT_PHASE_C3.md](OFFLINE_CONTENT_PHASE_C3.md),
with the reproducible boundary inventory `scripts/content_inventory.py`). It
fixed a broker defect: bundles share members, so all-or-nothing must hold
whichever bundle is withdrawn first. Startup and recurring verification are
mapped, with a guarded plan and no startup change
([OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md)):
never own `.mp3` names before the audio reader is hooked. Bug-list status:
[BUG_BACKLOG_STATUS.md](BUG_BACKLOG_STATUS.md).

Session 14 ([handoff](CLAUDE_SESSION_14_HANDOFF.md)) found #24's cause: the
summon scene's reveal flash fires when the client's GLOBAL animation list holds
exactly three animations, so animations other screens leave in it (the
energy-overflow glow, the download icon) move the flash onto a double gate's
second gate. The candidate fix guards each Wait/Change script and reaches the
client through provider policy C4 ([OFFLINE_CONTENT_PHASE_C4.md](OFFLINE_CONTENT_PHASE_C4.md));
judge gate scripts with `scripts/test_gate_overlay_model.py`, not by their
text. The Farm report was six empty pre-rolled taps (no lost reward). Client
check: [CLIENT_SESSION_14_STEPS.md](CLIENT_SESSION_14_STEPS.md), after QC.

Session 15 ([handoff](CLAUDE_SESSION_15_HANDOFF.md)): the player's run showed
the guard cannot give the final white either, because the trigger counts every
animation on screen. The proxy DLL now replaces only that decision (x86 patch at
0x8760EF, `OfflineMod.win32/content/cp_gate_flash.cpp`): white at frame 45 of the
final gate's opening animation, armed only when seen starting, verified build
only. A patch that replaces a block by a jump needs a binary proof that nothing
after it reads what the block computed (`tests/fixtures/check_gate_flash_site.py`).
Acceptance criteria must assert the desired visual (white present after the
final opening) as well as the absence of the bad one; matching today's timing
is not a criterion when today's behaviour is the defect. Swap a test package's
DLL with the harness `dll` command, never by re-registering. The same DLL now
hooks the audio reader (x86 0xEC0030, the EXE's only `fopen_s` user; `fopen_s` /
`fread` / `fclose` imports): sounds may be owned only with such a DLL, and the
harness refuses a sound-owning policy otherwise
([OFFLINE_CONTENT_PHASE_C5.md](OFFLINE_CONTENT_PHASE_C5.md)). A linear capstone
pass over all of `.text` stops at the first undecodable byte (under 10% of this
EXE): whole-image claims need byte-level scans.

Named harness saves are seeded once; --seed does not refresh an existing save.
Use a new name for a revised fixture. Validate emitted squad references and
equipment as well as database counts: login success alone missed the deleted
squad-unit crash. clearunits is a destructive test command, not a live-save
repair tool; use the reviewed implementation on copies only. Its empty-roster
case does not manufacture a unit or repair every possible historic corruption.

The Archive draws the plain unit thumbnail for the tested units; _100 requests
do not prove that variant is displayed. Trace the draw consumer, not only the
download queue. Training's own-unit helper leader comparison using stored
leader_skill_id was withdrawn: the relevant path uses the master skill.

The harness collect command preserves separate collections (including same-
second calls). One log can contain multiple launches; do not demand a .log.1
when both runs already exist in .log. File-count overlap with an installed
client is not download completion. Keep bounded evidence for acceptance runs;
do not reset clients or repeat passed tests merely to obtain another baseline.

Players are now chiefly finding gameplay problems. Prioritize correct encounters,
progression, fusion and feature rules over a general campaign to emit every known
packet. Missing response blocks still matter when they explain a specific stale
screen, missing reward, lock, or crash. A populated UI is not proof that its
mechanic works, and a successful HTTP response is not proof that a boss behaves
correctly.

Seek gaps beyond the latest bug list. Research the Global wiki by feature family,
compare its mechanics with actual code/data, and implement bounded, testable
slices in the correct layer. Do not mass-populate content with invented stats or
turn wiki tables directly into packets before understanding the client contract.

## Read this checkout, not a remembered snapshot

```powershell
git status --short
git log -5 --oneline
git submodule status
git -C packet-generator status --short
```

This is C++/Drogon/SQLite/Glaze with a Rust KDL generator submodule. The standalone
Windows Debug server normally listens on 127.0.0.1:9960. The APPX proxy is a
separate frontend; Android and Windows client code are not interchangeable proof.
Use [DEVELOPMENT.md](DEVELOPMENT.md) for a fresh build. Do not copy Ninja/PCH/cache
files from another computer or depend on ignored maintainer tools.

Working trees may contain substantial unrelated mission/data changes. Preserve
them. Publish schema commits in packet-generator before updating the parent
pointer. Do not include live saves, extracted clients, multi-gigabyte assets, or
reverse-engineering dumps in a source commit.

## Working in Claude Code

Procedure for an AI coding session on the maintainer's Windows machine. The
maintainer plays the Windows client against the live server and reports what
they see; the session builds, tests on copies, deploys and documents.

**Start of a session.** Read this handbook, the work queue and the feature note
for the area you touch. Then check for parallel work before building:
`git status` in both repositories, other running sessions (Claude Code's agent
list), and whether `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG` is set in
`out/build/debug-win64/CMakeCache.txt` (another session may be testing). The
server state is `tools/bin/bf_ctl.ps1 -Target server -Action status`.

**Local-only tools.** Everything under `tools/` is git-ignored and exists only on
the maintainer's machine; a fresh checkout has none of it. `pwsh` is not
installed: run scripts as `powershell -NoProfile -ExecutionPolicy Bypass -File …`.

| Need | Tool (tools/…) |
|---|---|
| Start/stop/status of the live server (handles its forked child and the port check) | `bin/bf_ctl.ps1` |
| arm64 client: symbols, disassembly, callers through the PLT, string-literal users | `bin/so_symbols.py`, `bin/so_disasm.py`, `bin/so_xref.py`, `bin/so_strref.py` |
| Request GroupId + AES key for every Request class | `bin/so_groupids.py` |
| Every key a `…Response::readParam` accepts, with its setter | `readparam_map.py <Response> <vtable symbol>` |
| Windows crash dumps (`%LOCALAPPDATA%\CrashDumps`): genuine return addresses; x86 code and strings near an RVA | `bin/dmp_stack.py`, `bin/pe_disasm.py <rva> [n] --strings` |
| Screenshot the client window | `bin/client_shot.ps1` |

The client binaries sit beside this checkout on the maintainer's machine:
`..\BraveFrontier-APK\lib\arm64-v8a\libgame.so` (symbols; the reference for
semantics) and `..\BraveFrontierAppxClient\BraveFrontier.Windows.exe` (what
actually runs; stripped). Map a Windows crash to arm64 through the string
literals both builds share. Offsets differ between them.

**Build while the live server runs.** From bash, export `LIB`/`INCLUDE` for the
installed MSVC and Windows SDK (or use `rebuild.bat`, which imports them).
The running server locks its EXE, so build into a scratch directory with
`cmake -S . -B out/build/debug-win64 -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=<dir>`.
That variable sits in the shared cache: every session's build goes there
until it is removed with `-UCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG` (`rebuild.bat`
keeps it, and still prints the in-tree path as "Built:"; the link line shows
where the EXE really went). **Any** KDL edit,
documentation included, regenerates `all.hpp` and rebuilds all 205 steps
(about 20 minutes at two or three jobs), so batch schema edits.

**Test on copies, never the live save.** Copy `deploy/gme.sqlite` with SQLite's
backup API into a folder under `out/`, next to a config listening on 19960 or
19962 with `"filename": "./gme.sqlite"`. **Pass that config's absolute path as
the EXE's first argument.** Debug builds otherwise load `deploy/config.json`,
which binds the live port 9960 and opens the live save; on Windows the
second bind succeeds silently. Confirm `starts listening on 127.0.0.1:19960` in
the test server's log before sending anything, and stop test servers by their
own PID plus the children they created (`scripts/bf_testkit.py`), never by name
and never by `taskkill /T` alone: `/T` follows recycled parent PIDs, so a live
server whose launcher has exited can be taken down with a test server.
`bf_testkit.IsolatedServer.start()` is the one readiness check, for suites and
the client-acceptance harness alike: ready only when the started process is the
port's sole listener in the OS table both before and after its endpoint answers
(an answer alone may come from a silent second bind); anything else raises
`ServerStartError` after stopping only what it started.
`bf_ctl.ps1 -Action stop` stops EVERY `gimuserverw`, test servers included.
Encrypted request helpers are in every
`scripts/test_*_wire.py`. Run the existing suites that touch your area too:

| Suite | Port | Covers |
|---|---|---|
| `test_research_lab_wire.py` | 19960 | Trials 001–003, The Creation God: payloads, unlocks, pay rules |
| `test_research_lab_ai.py` (no server) | — | Every authored boss AI against `ai_model.py` |
| `test_guild_invite_wire.py` | 19960 | Invite candidates, GuildJoin, member ranks |
| `test_exchange_ui_wire.py`, `test_fusion_merit.py` | 19960 | Exchanges, fusion, Bazaar |
| `test_feature_visits_wire.py`, `test_synthesis_wire.py` | 19962 | Feature visits, synthesis transactions |
| `validate_missions.py`, `audit_handlers.py` (no server) | — | Mission archive checks, handler registrations |

`scripts/run_bugfix_regressions.py EXE` runs all of them (each on its own
copied save) and writes `out/qa/current/SUMMARY.md`.  A suite passes only with
positive proof it finished (its summary line agreeing with its PASS/FAIL lines,
no traceback); a failure is accepted only when `EXPECTED_FAILURES` lists those
exact cases; `validate_missions.py` must match the recorded data baseline in
`scripts/qa_baselines/validate_missions.json` (3,117 findings: open work, not a
clean result).  `scripts/test_qa_runner.py` tests those rules.

The copies are of the LIVE save, so the player's progress reaches the fixtures.
When a suite that passed before fails, run it on the unchanged deployed EXE
against the same copy first: identical failures mean fixture drift, not a
regression (keep both logs).  The fix belongs in the fixture, on the copy:
reset the state the checks compare (town tiles, Continue marks, an
already-delivered bundle, a full unit box), never relax the check.

**Candidate QC before deployment.** A request for QC or a Claude handoff does
not authorize replacing the running server or live save. Build and test a
candidate from the current checkout; use the named test-package harness and
copied saves for client acceptance. Preserve the existing build tree and keep
QA outputs bounded under out/qa/current. Do not create another checkout or
build tree for each attempt. Record actual output paths and SHA-256 hashes;
do not infer the binary from a script's printed destination or timestamps alone.
Restore any shared CMake output override after building, including generic
and configuration-specific CMAKE_RUNTIME_OUTPUT_DIRECTORY variables.

When deployment is explicitly in scope, back up the live save using SQLite's
backup API, coordinate stopping the identified live server, remove temporary
output overrides and build in-tree using the established MSVC/Visual Studio
workflow. Confirm the new binary and listener after restart. The broad
bf_ctl stop action stops test servers too; do not use it during unrelated QA.

**Client contracts that cost a test round each:**

- The generic close-error path and unsupported GroupIds can close the client's
  session. Do not acknowledge a rejected action with a success-shaped reply:
  the client may apply it locally. Decode the screen's refusal path and use
  the appropriate command, such as `HandleResult::refuseToHome` (cmd 6,
  message then Home) or `refuseToRetry` (cmd 2, message then the scene's own
  noticeOK; for MissionContinue that is the Continue prompt again).
- **An ordinary reply can BE the action.** MissionGameOverScene revives on any
  non-error MissionContinue reply, so a revival the server did not accept must
  be refused with cmd 2, never acknowledged.  A retry re-sends the same
  request object byte for byte (no nonce); see net/mission.kdl.
- The client sends **every number in a request as a string**:
  `JsonNode::addParam(const char*, int)` @0xFDB82C is `IntToString` plus the
  string overload. Model request numbers as `i32::str`. An `i32::int` request
  field stops glaze at that field; the handler runs on a half-read request and
  the only trace is a `parse error` warning in `deploy/log/server_stdout.log`.
  Wire tests must send strings too, or they pass while the client fails (this
  hid a guild promote bug and the guild shop and Bazaar purchase counts).
- Many lists are **full replaces** (cleared on row 0). Send them complete. A
  zero-row array never reaches `readParam`, so a list cannot be emptied.
- **Rows the client appends itself share one id** (`INT_MAX - itemId`, one row
  per copy of a max-stack-1 sphere) until a `9wjrh74P` list replaces them, and
  screens key their UI on row ids: Equip Sphere drew nineteen crafted copies as
  one (#34). A reply that follows a local append, and is parsed before the next
  screen exists, should carry the whole list; ItemMix does since Session 8
  (consumer chain in `net/items.kdl` ItemMixResp, `test_craft_row_identity_wire.py`).
  Town crafts are queued and sent by StepScene, not at once.
- A stored unit field can be **read in only some modes**. Ordinary battles and
  unit screens take the Leader Skill from UnitMst; Training Grounds (a helper
  from the player's own units) and the campaign / Frontier Gate save data read
  `UserUnitInfo.oS3kTZ2W`, which archive grants stored as 0 until Session 9
  (`scripts/repair_leader_skills.py` for old saves). A new unit reaches the
  client in `qC2tJs4E`; a changed owned unit only in a full `4ceMWH6k`
  (UserInfo, at every return to Home).
- A reply can **free objects a screen still holds**. After an edit the client
  often updates its own cache (the member screen sets the new rank itself); a
  roster in that reply cleared the list first and the promote crashed.
- Find the request a button **actually sends** (`so_xref` on its Request's
  `setRequestValues`) before building a handler. Inviting is `GuildJoin`, not
  `GuildMemberUpdate`; that assumption survived a "wire-verified" slice.
- A screen reached from a list may need a **second list keyed by the same id**
  (the invite profile's `8lAroepR` beside the cards' `fRaBu6et`).
- Unit art: image variant 2 and up appends `_N` to the file name; most units
  ship no such file, and a missing texture crashes. Guild cards send 1.
- Archive JSON loaders are strict: one unknown key (even a `_doc` note) leaves
  that archive empty while handlers still reply successfully.
- Transforming and multi-boss fights are driven by the client's own
  `F_MISSION_SCRIPT_MST`; decrypt that row first. Boss AI grammar and the test
  model are in `packet-generator/assets/archive/ai.kdl` and `scripts/ai_model.py`.

**Publishing.** Commit only when the maintainer asks. Both repositories work on
`audit-campaign`; `mine` is the maintainer's fork (Seltraeh/server,
Seltraeh/packet-generator) and `origin` is upstream decompfrontier (do not push
there). Commit and push packet-generator first (`git push mine
audit-campaign:main`), then the parent with the updated submodule pointer. Stage
source, authored data and docs by name; never live saves, `*.bak*` backups,
`out/`, `deploy/log/`, `deploy/mst_download_staging/` or client assets.

**Session notes.** Claude Code's per-project memory (outside the repository)
holds the maintainer's standing preferences and longer histories; this handbook
and the feature notes are the shared record. When they disagree, re-check the
source and fix whichever is stale.

## What was established on September 25

| Area | Evidence and remaining limits |
|---|---|
| Manual BB radial input | Earlier bb_timer feature correction was user-confirmed. Do not reopen it from an old handoff alone; independently test BB/SBB/UBB/DBB/bonded variants when their prerequisites change. |
| Burst Queen | Isolated encrypted-request tests reach BB10/SBB10 in one eligible fusion and cover boundary cases. Eligibility still controls ultimate skills. |
| Mystery Frogs | Fixed/random types, level/XP reset, retained upgrades, three Rainbow Crystal presents and subsequent Anima growth passed API/DB tests. Live random odds were unavailable: ordinary weight 1, Rex 0.25 is authored policy. |
| Merit Exchange | 241 supported offers selected from original records, including Legend Stone. Purchase/stock/payment tests passed. |
| Guild Exchange | 121 offers, persistent wallet/stock and present type 8002 payout. Reward earning sources are not thereby complete. |
| Event Bazaar | 51 curated offers for currencies 8, 13 and 61, with source URLs. Not the complete historical event catalog; stock has no automatic daily/monthly reset. |
| 4-star Tilith | Original crop did not fit the stock artwork. Authored rectangle 303,50,180,492 for unit 50253; download MST 1086 verified over HTTP. Actual post-update framing needs player confirmation. |
| Boss AI | deploy/archive/ai.json still has only “Attack random” in this inspection. Schemas can represent more than the archive uses. Real phase/skill behavior is a major work item. |
| Emitters | Many reward/roster/warehouse paths were repaired. Remaining gaps must be evaluated per action; historical “missing” lists can be obsolete. |

Detailed test coverage and offline choices are in
[the September 25 bugfix notes](../scripts/BUGFIX_NOTES_2026-09-25.md).
These are dated observations, not promises about every mechanic or client.

## Evidence model

Label conclusions separately:

- **Gameplay source:** Global wiki page/revision or archived official material,
  URL, access date, mechanic, region and version. Record disagreements.
- **Binary-confirmed:** consuming function/build/address and observed semantics.
  A setter map establishes a field name, not what its values mean.
- **Server-tested:** request and full response, database before/after, isolation,
  regression cases. Serialization success alone is insufficient.
- **Client-confirmed:** client platform/build, reproduction, observed result and
  who confirmed it. Do not promote API tests to this label.
- **Authored policy:** offline availability, timing, odds, simulated opponents or
  balancing not recovered from original behavior.
- **Unknown/historical:** retain uncertainty; replace old claims when rechecked.

Prefer the Global wiki for this client. EU/JP/International pages can differ.
For example, Mystery Frog sources disagree about retaining the original type;
record the selected Global behavior instead of silently mixing regional rules.
If a source is inaccessible, report that and seek another source; a search snippet
is a lead, not a full phase specification. Summarize mechanics rather than copying
whole wiki pages into the repository.

## Gameplay implementation workflow

1. **Reproduce and delimit.** Name the feature, unit/mission IDs, client and action.
   Preserve the reporter's save. Compare normal, tutorial and auto/manual paths
   only where relevant. Identify an observable before/after result.
2. **Research the full lifecycle.** Entry requirements, resources, operations,
   failure/retreat, rewards, repeat clears, persistence and reset/refresh rules.
   Record exact source pages and conflicts in a short feature note.
3. **Audit existing coverage.** Trace handler registration, helpers, generated
   schemas, MST and archives. Find placeholders, fallback waves, skipped effects,
   constant rewards and stored-but-unused values. Distinguish absent data from
   absent implementation and from stale client state.
4. **Choose ownership.** Establish what the server supplies and what the client
   already calculates. For battles, determine which AI/skill/monster tables the
   client consumes before adding a parallel server combat implementation.
5. **Implement one complete slice.** Prefer one faithful encounter or mechanic
   to a broad placeholder pass. Put general rules in helpers, authored instances
   in archives, recovered fields in KDL, and persistence in migrations/handlers.
6. **Verify semantics and edges.** Test costs, limits, boundary values, failure,
   replay, all-or-nothing mutations, save/reload and required client refreshes.
   Use deterministic fixtures for random effects and phase boundaries.
7. **Play-test and record.** Confirm the actual behavior, not merely navigation.
   If client testing is unavailable, finish independent server work and leave a
   precise play-test checklist. Never mark an unobserved result client-confirmed.
8. **Update the work queue.** Record what is complete, evidence, remaining gaps
   and the next bounded slice. Continue to the next prioritized slice within the
   user's authorized scope; do not repeatedly ask which routine fix to do next.

## Put changes in their proper places

| Location | Responsibility |
|---|---|
| packet-generator/assets/net/*.kdl | Wire wrappers, hashed keys, types, packed field descriptions and consumer evidence |
| packet-generator/assets/mst/*.kdl | Original master-data schema; preserve original values unless a documented correction is needed |
| packet-generator/assets/archive/*.kdl | Server-owned encounter/reward vocabulary |
| gimuserver/packets/all.hpp; gimuserver/archive/archive.hpp | Generated outputs; never hand-edit |
| deploy/archive/mission.json; ai.json | Encounter composition, stats, phases/AI records and mission rewards where supported |
| gimuserver/archive/MissionArchiver.cpp | Validated conversion from authored encounters to client data; shared behavior belongs here, not scattered per-mission branches |
| deploy/mst/*.json | Recovered tables and explicitly documented compatibility corrections |
| deploy/archive/*.json | Offline catalogs, pools, calendars and other authored content |
| gimuserver/gme/handlers/ | Request validation, transactions, transitions and replies |
| gimuserver/gme/common/ | Reusable inventory, progression, bonding, reward and refresh helpers |
| gimuserver/db/MigrationManager.cpp | Durable schema evolution; do not require manual player-save edits |
| scripts/ | Shared generators and reproducible tests; parameterize external dump paths |
| docs/ | Current instructions, feature evidence and work queue |
| deploy/game_content/ | Downloadable content, including generated MST manifests; distributed separately from Git |

Avoid ID-specific C++ exceptions when an archive row or a general rule represents
the behavior. Do not change an original MST field to compensate for an unrelated
emitter bug. Conversely, verify that the original value actually fits the assets:
Tilith's source, served MST and client cache agreed, but its crop still did not.
Publish a new downloadable MST version after changing a client-cached table;
editing deploy/mst alone does not refresh that client cache.

## Bosses and trials: first major gameplay track

Start with the [Summoners' Research Lab](https://bravefrontierglobal.fandom.com/wiki/Summoners%27_Research_Lab)
index and one fully documented encounter. Keep Trial Zone, Strategy Zone and
Vortex variants distinct. [Trial No. 003](https://bravefrontierglobal.fandom.com/wiki/Trial_No._003)
is a useful candidate because turn scheduling and threshold precedence are
observable; do not assume it shares another Maxwell variant's mechanics.

For each encounter, inventory stages, identities/forms, HP/ATK/DEF/REC, element,
action count, target rules, skill effects, preemptives, turn cycles, HP triggers,
once-only flags, counters, dispels, shields, mitigation, ailments, resurrection,
dialogue cues, retreat/continue rules, unlocks and first/repeat rewards. Mark
unavailable values unknown. Test simultaneous triggers and multiple thresholds
crossed in one turn, not just a scripted slow fight.

Trace archive AiRecord through MissionArchiver into the client's AI selector and
skill resolver. Existing condition/action strings and unknown action arguments
must be understood before extending them. A valid monster model with only random
attacks is not a completed boss. Asset placement and combat behavior are separate
issues. Missing missions must not silently pass a fidelity audit because a generic
fallback battle starts.

## Fusion and progression invariants

Use [Unit Skills](https://bravefrontierglobal.fandom.com/wiki/Unit_Skills),
[Unit Types](https://bravefrontierglobal.fandom.com/wiki/Unit_Types), and
[Bonding](https://bravefrontierglobal.fandom.com/wiki/Bonding) as starting points.
Verify specific material and unit pages when their exceptions matter.

- BB/SBB gains are a budget across unlock boundaries. Test units without SBB,
  already-maxed skills, mixed materials and all supported rarities.
- Imps affect ext_*; ordinary/stat-growth fields and Omni bonuses are separate.
  Preserve enhancements through resets/evolution according to the specific rule.
- Second sphere slot uses the existing -1 locked / 0 empty / equipped-type sentinel.
- Distinguish DBB unlock, eligible partner, bond level, gauge/resource costs and
  bonded SBB availability. A successful radial gesture proves none of those rules.
- Test fusion costs, material ownership/consumption, favorites/locks if enforced,
  invalid batches, evolution recipes, type growth and experience caps.
- Current Great/Super rates and random-frog odds include authored choices. Do not
  describe them as official values without recovered evidence.

## Focused emitter and wire debugging

GME uses POST /bf/gme/action.php. Header F4q6i9xe contains Hhgi79M1; encrypted
body a3vSYuq2.Kn51uR4Y uses the request's AES-ECB key padded/truncated to 16 bytes,
PKCS#7 and base64. Responses may be gzipped. Inspect current-session logs; do not
reuse a stale log to claim a current test passed.

For a stale UI, compare **what the handler writes**, **what the response carries**,
and **what the client reader does**. Some lists replace the entire cache; partial
snapshots can erase unrelated inventory. Use existing emitGrantedRewards and
replacement-roster paths. Preserve wrappers, string-vs-number conventions, packed
lengths and full dictionaries. Read the entire readParam and its downstream
consumers, including inlined accesses, before changing a field.

Known debugging distinctions: successful transport vs supported request; missing
asset vs malformed data; generated member absent vs stale PCH; stale downloaded
MST vs source table; a field's presence vs consumer semantics. Tutorial success
does not establish normal progression gates. Auto success does not establish
manual input configuration.

## Save safety and acceptance

Use a separate config/database and loopback port for destructive fixtures. SQLite's
backup API produces a consistent copy from a read-only source connection while
the server runs. Do not copy a busy SQLite file without accounting for WAL state.
Never restore an old backup over newer player progress merely to satisfy a test.
Stop only the identified test process; another server/client may be active.

**A success reply must mean the change is durable.** Drogon commits a
transaction from its destructor, after the handler has replied, and on
`SQLITE_BUSY` neither retries nor rolls back: an outside reader of the save
(a backup copy, a DB browser, a suite) once cost an acknowledged action and
wedged the server's only connection. Open transactions with
`gme::beginTransaction(theDb())` and finish every success path with
`co_await gme::commitTransaction(transaction)` before building the reply
(`gme/common/Transactions.hpp`): a failed commit then rolls back on its own
connection and the request is answered with an error, nothing charged.
`scripts/audit_transactions.py` (in the runner) fails a success reply that is
not preceded by a commit; `test_db_commit_contention_wire.py` covers a reader
beyond the 5 s busy timeout and restart persistence. A test that reads the save
right after a reply sends a BadgeInfo request first (it is answered only after
anything queued on that connection).

**What a request did not commit is undone.** The handle `beginTransaction`
returns rolls back on release (`detail::rollbackOnRelease`): a ROLLBACK
statement and an empty BEGIN for Drogon's destructor to commit, not Drogon's
own `rollback()`, which logs an error after every refusal that already rolled
back. So a handler that throws after some writes, or refuses without a
rollback, keeps nothing -- before Session 10 Drogon's destructor committed
those writes while the client was told the request failed. The debug console's
`dbfault` (`gme::armCommitFault`) makes the next commit throw a non-database
exception, simulating exactly that; `scripts/test_db_exception_atomicity_wire.py`
(runner entry 50) drives it through the converted multi-statement flows. Flows
that write several rows belong in ONE transaction; single statements and
per-item independent writes may stay outside one, but must not swallow a
database error and reply success (UnitFavorite and Scenario still do).

Parse complete responses, check references, inspect DB changes and validate client
refreshes. Include negative/replay cases for rewards and purchases. A task is
complete only to the evidence level actually reached. List exact in-game checks
still required. Do not stop all gameplay work merely because one client check
awaits a player; continue independent authorized work.

The old emitter-era guide is preserved as
[historical reference](reference/HANDBOOK_BEFORE_GAMEPLAY_REFRESH_2026-09-25.md).
Its dated claims and local tools are leads, not current dependencies. The current
handbook and source take precedence when they disagree.
