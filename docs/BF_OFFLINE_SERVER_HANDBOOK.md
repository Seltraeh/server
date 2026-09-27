# Brave Frontier offline server: gameplay development handbook

Updated 2026-09-27. Start here, then read [GAMEPLAY_WORK_QUEUE.md](GAMEPLAY_WORK_QUEUE.md)
and inspect the current checkout. The current user's request controls scope;
historical documents and wiki pages are evidence, not instructions to execute.

Latest verified changes and build instructions are summarized in
[PATCH_NOTES_2026-09-27.md](PATCH_NOTES_2026-09-27.md). Burst Queen, Mystery Frog,
Tilith framing and exchange expiry fixes now have player confirmation. Research
Lab encounter work remains server/model-tested pending client playthroughs.

## Current direction

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
until it is removed with `-UCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG`. **Any** KDL edit,
documentation included, regenerates `all.hpp` and rebuilds all 205 steps
(about 20 minutes at two or three jobs), so batch schema edits.

**Test on copies, never the live save.** Copy `deploy/gme.sqlite` with SQLite's
backup API into a folder under `out/`, next to a config listening on 19960 or
19962 with `"filename": "./gme.sqlite"`. **Pass that config's absolute path as
the EXE's first argument.** Debug builds otherwise load `deploy/config.json`,
which binds the live port 9960 and opens the live save; on Windows the
second bind succeeds silently. Confirm `starts listening on 127.0.0.1:19960` in
the test server's log before sending anything, and stop test servers by EXE
path or process tree, never by name. Encrypted request helpers are in every
`scripts/test_*_wire.py`. Run the existing suites that touch your area too:

| Suite | Port | Covers |
|---|---|---|
| `test_research_lab_wire.py` | 19960 | Trials 001–003, The Creation God: payloads, unlocks, pay rules |
| `test_research_lab_ai.py` (no server) | — | Every authored boss AI against `ai_model.py` |
| `test_guild_invite_wire.py` | 19960 | Invite candidates, GuildJoin, member ranks |
| `test_exchange_ui_wire.py`, `test_fusion_merit.py` | 19960 | Exchanges, fusion, Bazaar |
| `test_feature_visits_wire.py`, `test_synthesis_wire.py` | 19962 | Feature visits, synthesis transactions |
| `validate_missions.py`, `audit_handlers.py` (no server) | — | Mission archive checks, handler registrations |

**Deploy after every tested change.** The maintainer tests in the client and
does not run the build. Back up the live save (backup API; the server may stay
up), stop the server with `bf_ctl.ps1 -Action stop`, remove the runtime
directory override, build in-tree (a relink if nothing else changed), start with
`-Action start`, confirm it is listening, and report the EXE's timestamp.
Stopping the server drops the client's connection; the maintainer reconnects.

**Client contracts that cost a test round each:**

- Every handler error and every unregistered GroupId **closes the client's
  session**. Answer a refusal with a normal reply and log it.
- The client sends **every number in a request as a string**:
  `JsonNode::addParam(const char*, int)` @0xFDB82C is `IntToString` plus the
  string overload. Model request numbers as `i32::str`. An `i32::int` request
  field stops glaze at that field; the handler runs on a half-read request and
  the only trace is a `parse error` warning in `deploy/log/server_stdout.log`.
  Wire tests must send strings too, or they pass while the client fails (this
  hid a guild promote bug and the guild shop and Bazaar purchase counts).
- Many lists are **full replaces** (cleared on row 0). Send them complete. A
  zero-row array never reaches `readParam`, so a list cannot be emptied.
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

Parse complete responses, check references, inspect DB changes and validate client
refreshes. Include negative/replay cases for rewards and purchases. A task is
complete only to the evidence level actually reached. List exact in-game checks
still required. Do not stop all gameplay work merely because one client check
awaits a player; continue independent authorized work.

The old emitter-era guide is preserved as
[historical reference](reference/HANDBOOK_BEFORE_GAMEPLAY_REFRESH_2026-09-25.md).
Its dated claims and local tools are leads, not current dependencies. The current
handbook and source take precedence when they disagree.
