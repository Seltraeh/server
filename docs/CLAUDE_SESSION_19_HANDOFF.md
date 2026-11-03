# Claude Session 19 handoff — item and area derived exactly; the fresh-install master-table contract

October 22, 2026, for Codex QC. Directive: [CLAUDE_SESSION_19.md](CLAUDE_SESSION_19.md). Builds on the
Session 18 correction ([CLAUDE_SESSION_18_HANDOFF.md](CLAUDE_SESSION_18_HANDOFF.md) §10) and the warm
C16 route of [SESSION_18_CLIENT_RESULTS.md](SESSION_18_CLIENT_RESULTS.md), which is fallback validation,
not a cacheless start.

**Startup coverage is still incomplete**, and the optimization stays fail-closed:
- 5 lists remain unknown (6 before);
- 8 required files exist in no source;
- 9 families need such a file.

Census and prompt stay stock, and nothing removes a startup screen. **For a fresh install, every master
table the login check requires is delivered.** That holds offline and over the wire, but it is an
audit, not a first run.

**What was not done:**
- No client was launched, reset or installed.
- Nothing was deployed, staged, committed or pushed.
- The live save was opened only through the runner's read-only backup copies.
- C1 (`09056ee5…`) and C16 (`45a2c1b1…`, warm, consent accepted) are as they were. Their packages,
  registration, saves and consent state were not touched. Read only, the installed packages'
  LocalState was listed by name (§4.4), and two cached files were decoded in memory and hashed
  once (§3).
- The DLL was not changed. The Session 18 DLLs and proxy test binary are reused and verified.
- **The consent / online-service screen was not worked on** (deferred).

## 0. Status

| Objective | Implemented | Automated-tested | Client-observed | Deployed |
|---|---|---|---|---|
| 1. item and area derived exactly, bound to their sources | yes | yes, incl. 11 negative controls | consistent with C16 (all 713 item and 340 of 347 area names queued; the rest explained) | no |
| 2. item / area content gaps | recorded (3 absent kept; 2 collisions for review) | yes | warm caches read once (read only) | no |
| 3. fresh-install bootstrap contract and audit | yes: x86 contract, audit, test | yes, offline and live (fresh save) | no (checklist for a later run, not executed) | no |
| consent screen | **not worked on** | — | — | — |

## 1. Changed files

Pre-edit copies: `out/qa/current/session19/pre/` (`SHA256SUMS`). Line counts are against those copies,
CRLF ignored; full hashes are in §7.

| File | Change | sha256 (first 16) |
|---|---|---|
| `scripts/startup_coverage.py` | +429 −78: item and area `DERIVED` (`Field.folder_by` / `computed`, `Join`, `unique`), `item_frame_name`, the area curation port (`served_area_rows`), `served_projection`, `SERVED_SOURCES`, server inputs with root `served`, the `inputs.tables` binding rule in `validate_manifest`, login-end preloads with their evidence, the weapon stand-in's `item/`, `--banner-dir` / `--no-banner-dir` | `ad20a58c5228fe70` |
| `scripts/test_startup_coverage_manifest.py` | +164: frame branches, item rows, area folder / join / curation / projection, served-source binding, omitted bindings, regeneration with removed assets, the weapon stand-in | `07cf60ec5b77a379` |
| `scripts/check_startup_evidence.py` | new: every name C16 queued in its login window must be required or excluded | `c8cdad3c30044861` |
| `scripts/check_client_inputs.py` | +21 −8: served inputs, `--banner-dir` / `--no-banner-dir` | `e2f3701dcbad3f39` |
| `scripts/test_content_server_tables.py` | +207 −74: the served projection against a real UserInfo, translated banner, edited row, restart, partial and omitted cases | `283f8f16408e93de` |
| `scripts/test_content_coverage_wire.py` | +22 −9: isolated served tables, a stale and an unnamed area projection | `35b4911da5fd08c1` |
| `scripts/test_content_startup_live.py` | +55 −1: item / dungeon tables bound in the fixture client: changed, older version beside it, restored, omitted | `1c12f8887d18f6ea` |
| `scripts/check_mst_bootstrap.py` | new: the bootstrap audit (offline or `--server`) | `665d5bd4bca5edb4` |
| `scripts/test_mst_bootstrap.py` | new: synthetic, real and live bootstrap cases | `f3ea1d29aa38e7e1` |
| `scripts/run_bugfix_regressions.py` | +8 −2: `test_mst_bootstrap.py`, `x86_mst_bootstrap_check.py`, `check_startup_evidence.py`, `x86_startup_rules_check.py` | `5242380dca63f5fa` |
| `scripts/qa_baselines/c16_provider_off_queue_20261021.json` | new: the C16 provider-off queue (6,941 names, 42 scenes) | `ef64d968e0a2240d` |
| `scripts/qa_baselines/mst_bootstrap_x86.json` | new: the extracted x86 facts | `2b92c32fa4a43f4b` |
| `scripts/qa_baselines/mst_bootstrap_deploy_20261022.json` | new: the recorded real inventory | `d262c54d15ac8589` |
| `tools/bin/x86_startup_rules_check.py` | +143 −6: the item / area facts and the server-key check (54), the weapon stand-in fact (1): 184 in all | `d218d1d117e30f95` |
| `tools/bin/x86_mst_bootstrap_check.py` | new: the fresh-install contract from the binary (37 rules) | `c9011e4435ccf144` |
| `gimuserver/drogon/ServerCache.cpp` / `.hpp` | +46 −1 / +6 −2: `StartupTable.root`, `ProjectionText`, `StartupProjection`, the `served 3SG2wX0R` entry | `553832b2d370e483` / `e46258fe7a6d2670` |
| `gimuserver/content/ContentBroker.cpp` / `.hpp` | +67 −13 / +11 −6: `ServedTable` / `ServerInput` with a root, `serverInputName`, `server_files` root `mst` or `served`, the `inputs.tables` binding refusal, T rows with the root | `f4548fca94c776e1` / `e56a827dea83d311` |
| `gimuserver/drogon/GimuServer.cpp` | +1 −1: passes the root | `2f0293169dc3d974` |
| docs | this file; [OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) §10 (+117, pointers in §8.4 and §8.5); handbook current direction (+32); [CLIENT_SESSION_19_FIRST_LAUNCH_STEPS.md](CLIENT_SESSION_19_FIRST_LAUNCH_STEPS.md) (new, not executed) | |

Session helpers and evidence, all under `out/qa/current/session19/` (never committed):
- `re/`: the disassembly the facts were read from;
- `build/`: the build script, `run_s19_suites.sh`, `run_mutants.py`, `line_deltas.py`;
- `coverage/`: the manifests, reports and `item_area_gaps.{py,json}`;
- `bootstrap/`: inventories, `reconcile.py`, `inventory_md.py`;
- `mutants/`, `mst_bootstrap/`, `server_tables/`, `bin/`, `bin_mutant/`, `pre/`.

## 2. Objective 1: item and area, exactly

Both are `DERIVED` lists in `scripts/startup_coverage.py`. Each request is rebuilt the way the
supported x86 client (`BraveFrontier.Windows.exe` `fb4f3c25…`, build `6253F827-01756000-00ED273D`)
builds it. `tools/bin/x86_startup_rules_check.py` re-reads every cited address (184 facts in all,
55 of them new). The disassembly the facts were read from is in `out/qa/current/session19/re/`.

### 2.1 item

| Step | x86 evidence |
|---|---|
| the check list holds one entry per ItemMstList row, keyed and valued by the row id | ItemMstList 0x5E4F90 (0x8E0CC7); its array 0x5E5170 (0x8E0CF3); id 0xE9960 (0x8E0D3B, 0x8E0D9F) |
| `updateEvent` finds "item" and calls `requestItemFiles` 0x7F6FA0 | 0xF1140 at 0x8E377C; 0x8E3790 |
| the thumbnail `Mt3Y0bo5` under /item/ | getter 0xF1B80 (0x7F707F), request 0x63E120 (0x7F708F) |
| the frame `getItemFrameName` 0x5E4D20 -> 0x5E4D40 under /common/, when not empty | 0x7F70CF, 0x7F70DB, 0x7F70FC |
| frame branches | `rNmJkH6K` +0x180 = 1 with type 1 -> `item_frame_raid`; `ZKQ1X69a` +0x174 = 1: type 2 -> `item_frame_5`, other types but 3 and 5 -> `item_frame_raid`; 888001 / 888002 -> `item_frame_4`; below 5 -> `item_frame_<type>`; 5 -> `item_frame_6`; 6 -> `item_frame_7`; otherwise `.png`. Every branch appends ".png" (0x5E4DA1-0x5E4F48) |
| the type `h0K7wjeH` is read with `stream >> int` | parser 0x4EFCF0 (0x65E860 at 0x4EFF4B) into +0x5C |
| who fills the list | `loadItemMst` 0x54E310 over the client's own `F_ITEM_MST` (0x54E3F6, through `loadFile` 0x543030). The response key `2C7LDzYk` would also fill it, but no server response carries that key (`gimuserver/packets/all.hpp`) |

Rows: the client's `F_ITEM_MST`. The server stages it as `Ver1028_83JWTCGy.dat`, the copy every
installed client holds; `inputs.tables` records the part. Unique on `kixHbe54`; a repeated id is a
problem and keeps the list unknown. Result: **713 names**, every one in the tree, owned by C8, and
queued by the C16 provider-off run.

### 2.2 area

| Step | x86 evidence |
|---|---|
| one entry per AreaMstList row, keyed and valued by the area id | AreaMstList 0x5C61E0 (0x8E0B4B, 0x8E0B64, 0x8E0B95) |
| `updateEvent` finds "area" and calls `requestAreaFiles` 0x7F7140 | 0xF0FF0 at 0x8E3807; 0x8E381F |
| img `b5VveUd8` under /area/ | 0xF4FF0 (0x7F71B1, 0x7F7228), "/area/" 0x7F71FC |
| plate `H0UB8pqm` under /dungeon/ for types 1 and 3, else /area/ | type +0x44 (0x7F72BF): 1 (0x7F72C2), 3 (0x7F72C7); parser 0x4CBC80 reads `3v1qg7Uj` with `stream >> int` |
| then each of the area's dungeons' `21VKZo0E` under /dungeon/ | `getDungeonListWithAreaID` 0x5DA320: area id at +0x5C (0xEE4C0 at 0x5DA3ED; parser 0x4E3230 stores `VjCY7rX4` there) |
| ... filtered by the permit list | 0x1997730 (0x5DA4C7), 0x582D80 (0x5DA50C). Not modelled: every dungeon is required, a superset over every permit state |
| who fills the list | UserInfo `3SG2wX0R` builds AreaMstResponse (vtable 0x18F2A60, 0x495895), whose readParam 0x4CBC80 REPLACES the list; `loadAreaMst` 0x544C30 loads the client's `F_AREA_MST` |

Rows: the area table the server sends, which **it synthesizes**. `ServerCache` builds
`m_clientAreaMst` once in Setup, and `served_area_rows` ports the curation:
- a Vortex area (land 99, id below 200000) is hidden when it is not a parent and has no dungeon, a
  name that is not displayable, or a banner set already shown (display order, then id);
- Parade Garden (100600) is ordered last;
- plates are localized through `LocalisedBanner`, relative to the server's working folder. Isolated
  and harness servers have no banner folder and serve the original plates.

The join is the client's own `F_DUNGEON_MST`: `VjCY7rX4` compared as a string, an empty `21VKZo0E`
asks for nothing. Result: **347 names** for an isolated server, 350 in the live layout with the `_EN`
plates.

### 2.3 Binding what the rule read

- **Served snapshot** (`ServerCache.cpp` `StartupProjection`). After `m_clientAreaMst` is built, the
  server records `{root "served", "3SG2wX0R", sha256(projection)}`. The projection is a header line
  plus one line per row: area id, type, img and plate, tab-separated, with `\`, tab, CR and LF
  escaped. `startup_coverage.served_projection` writes the same bytes. The broker requires every
  served source named in `inputs.server_files` (root `served`) with an equal digest; `/status` T rows
  carry the root.
  **Lifetime contract:** Setup is the only writer of `m_clientAreaMst`. UserInfo copies the vector,
  nothing reloads it, and the banner probe runs once at Setup. An edit after startup therefore
  changes nothing the broker binds, and a restart re-curates and rebinds or refuses.
  `test_content_server_tables.py` shows the projection of the rows a real UserInfo sends equals the
  T digest and the generator's prediction. It covers row edits and a translated banner added while
  running (the old binding holds), and the restart cases.
- **Client inputs.** `inputs.tables` records the LocalState parts each table was read from (own
  parts, `also`, `joined`). The broker's `loadManifest` refuses a manifest whose recorded part is
  missing from `inputs.client_files` ("inputs.tables records reading X, which inputs.client_files
  does not bind"), and `validate_manifest` agrees. The Session 18 DLL then hashes the client's copies,
  unchanged: `test_content_startup_live.py` covers bound, item table changed, older dungeon version
  beside it, restored, and omitted.

**Initial and version-refresh state.**
- *Item.* A fresh install downloads `F_ITEM_MST` through the version channel before login: the
  staged v1028, one part (§4). When the server raises its version, the client fetches the new part
  and deletes the old one after a successful login check (`saveVersion`). The manifest still names
  `Ver1028_83JWTCGy.dat` by digest, so the DLL's `VerifyClientInputs` sees another version and no
  bound file. Census and prompt then stay stock until a regenerated manifest binds the new part.
- *Area.* Two sources fill the list: loading the client's `F_AREA_MST` (`loadAreaMst` 0x544C30,
  login state 3) and UserInfo `3SG2wX0R`, whose parser replaces the list. The rule uses the curated
  rows UserInfo sends. That is consistent with the C16 provider-off evidence: every area name it
  queued in the login window is required, and the 7 it did not queue are explained in §3. The x86
  ordering of the two fills relative to `createDownloadCheckList` was not traced. The broker binds
  the rows as curated at boot. A server restart re-curates; a different result fails the binding
  until the manifest is regenerated.
- *The join.* It reads the client's `F_DUNGEON_MST` as bound in `inputs.tables`, so a version
  change there is the item case again.

### 2.4 What deriving them exposed

- **"login-end preloads"**, a new unknown list. The S18 area stand-in enumerated `dungeon/`, which
  covered the end of `LoginScene::updateEvent` by accident. That code runs after
  `createDownloadCheckList` (arm64 0x174BDF8-0x174BF44: PermitPlace places, DungeonMst icons by
  type, VortexArenaMst art). The new entry keeps `dungeon/` plus the 12 names the C16 provider-off
  client queued there and no other list requires. `scripts/check_startup_evidence.py` checks that
  every name queued in the login window is required or excluded. The S18 manifest fails that check
  on exactly those 12 names; the S19 manifest passes.
- **The weapon stand-in gains `item/`.** The exact item rule leaves out the 38 `item/` files no
  `F_ITEM_MST` row names (7 are `weapon_thum_*`). The x86 weapon branch 0x7F6CE0 asks for "/item/"
  (0x7F6D50), so the still-unknown weapon / summoner_img stand-in now enumerates `item/` beside
  `unit/img`, `unit/cgg` and `unit/cgs`. Those three are where summoner_img 0x7F2690 -> 0x7F27E0
  asks. The rules check and a manifest test pin this. None of the 38 was queued by C16; they stay
  required.
- **Unknown lists 6 -> 5**: unit / initial units; weapon / summoner_img; gacha; effect /
  effect_group; login-end preloads.

### 2.5 Manifest and coverage, before and after

| | Session 18 manifest `f9749ef2…` | Session 19 final `9b3068e6…` |
|---|---|---|
| login required / owned under C8 | 2,257 / 2,257 | 2,257 / 2,257 |
| background required | 72,203 | 72,218: 38 item/ names move from the item stand-in to the weapon stand-in, +12 login-end names, +3 area names |
| background owned / absent / unowned under C8 | 72,189 / 5 / 9 | 72,201 / 8 / 9 |
| excluded | 254 | 254 |
| unknown lists | 6 | 5 |
| binding | client files (S18); response tables (S18 correction) | + the served area snapshot, + the `inputs.tables` -> `client_files` rule |

The intermediate Session 19 manifest `f7f959b6…` (before the weapon stand-in gained `item/`) is
kept as evidence: background 72,180, 72,163 owned. Every live result in §6 uses the final one.

## 3. Objective 2: content gaps of the item and area lists

Record: `out/qa/current/session19/coverage/item_area_gaps.json`. It is the S19 final manifest
evaluated under C8 with `startup_coverage.evaluate`, every name with its class and trail, joined
with the C16 provider-off queue as positive evidence only.

| List | Required | Owned and available | Absent | Unowned | Ambiguous | Queued by C16 provider-off |
|---|---:|---:|---:|---:|---:|---:|
| item | 713 | 713 | 0 | 0 | 0 | 713 |
| area | 347 | 344 | 3 | 0 | 0 (7 collisions, all resolved: 5 identical copies, 2 by C8 aliases) | 340 |

- **Absent, kept as requirements:** `sp_quest_banner_800141_1.png`, `_2.png` and `_3.png`. They are
  `21VKZo0E` of dungeons 8001421-8001423 of area 800142, required through the area rule's join
  ("category area (M_AREA_MST 21VKZo0E -> /dungeon/)"). None of game_content, the Appx package and
  the APK assets holds them. The C16 save never had the area permitted, so they were never queued.
- **Unqueued but owned:** `sp_quest_banner_smn_training_1.png` ... `_4.png`, permit-filtered on the
  C16 save. They are also login-end preloads.
- **Two collisions to review; no alias was changed.** Each copy pair differs.

  | Name | Copies (sha256) | C8 serves | The area rule asks for | Both warm caches hold, decoded (`b[i] - i*i`) |
  |---|---|---|---|---|
  | `dungeon_name_frame_red_off.png` | dungeon/ `04421788…`, mission_select/ `ffe8aa91…` | mission_select/ | /dungeon/ | the dungeon/ copy |
  | `frontier_rift_banner.png` | challeng/ `6bcda33e…`, dungeon/ `5e5bda74…` | challeng/ | /dungeon/ | the dungeon/ copy (C16 holds none) |

  C7 chose these aliases by "background-downloaded cache" and "F_RESOURCE_MST names only this
  path". The area trail now shows the login check list asks for the /dungeon/ copies, and the warm
  caches hold them. Proposed for the next policy: alias both to `dungeon/`, after review. The warm
  caches were read once, read only; the record holds the observation, so the QA script never opens
  a player's LocalState.
- **The known 5 absent names and 9 families are unchanged** (C8: 9 unowned roots).

## 4. Objective 3: the fresh-install master-table contract

### 4.1 Binary evidence (x86, `tools/bin/x86_mst_bootstrap_check.py`, 37 rules)

`--write` extracts the facts into `scripts/qa_baselines/mst_bootstrap_x86.json` (deterministic). The
default run requires the file to equal a fresh extraction and every rule to hold. Every rule is
anchored to an RVA:

| Fact | Evidence |
|---|---|
| **readParam** 0x52C900 reads `moWQ30GH`, `d2RFtP8T`, `H6k1LIxC`, `5kbnkTp0` and registers **154** tables: `DownloadMstFileList::add` 0x5556D0(table, file key, encode key, version, parts) | file key and encode key are std::string globals filled by static initializers (`push len; push text; mov ecx, global; call 0xDDDC0`). The later branches share one add (`jmp 0x530FB9`) |
| `F_AI_MST` is compared and dropped | 0x52D9EC; the `F_LEADER_SKILL_MST` compare at 0x52D9FA overwrites `al` |
| 5 tables also set VersionInfo +0x3C | `F_RESOURCE_MST`, `F_ITEM_MST`, `F_RECIPE_MST`, `F_EFFECT_MST`, `F_EFFECT_GROUP_MST` (0x52D9DF, 0x52D9E8) |
| request | `requestMstFiles` 0x7F48F0: "/" + "mst" (global 0x188D000) + "/" + part, `requestFile` 0x63E1C0 |
| part name | `getMstFileName` 0x554C80: "Ver" + version + "_" + key (+ "_" + part) + ".dat" |
| **record count** | `loadFile` 0x543030: rows summed per part (0x5432F8). When they differ from the version list's count (0x54339E) and that count is not 0 (0x5433AA), every existing part is deleted (0x543512, 0x54351E), the table is re-added (0x543603) and marked invalid (0x543626). One part when the list does not know the table (0x54308B) |
| response channel | `getResponseObject` 0x493770: 52 tags call a save function writing one fixed LocalState file (`YDv9bJ3s` -> 0x544810 -> `EEPBM76L2G.dat`, ...) |
| **login** | state 3: load list 0x53E2A0 (134 entries), `loadMst` 0x53D340 per entry, `checkCorruptionsOfMsts` 0x540540. False: `REDOWNLOAD_MST_FILES_CHECK_COUNTER` "1", the `REDOWNLOAD_MST_FILES` notice, state 0xC. True: counter "", `saveVersion` 0x558930 (saves each version, deletes lower versions, drops the valid entries), state 4: `setInitialUnits` 0x8DF020. False there: `NETWORK_SERVER_ERROR` -4000 and a `CORRUPT_MST_ERROR` / `DEFINE_MST_INITIAL_UNIT` report |
| **corruption check: 79 lists** | 45 hard version tables (`deleteCorruptFile` 0x542050 through 0x5424E0). `F_RESOURCE_BASE_MST` among them is checked only while 0x199734F and two flags of 0xE13190 are set. 2 soft (`resetFileVersion` 0x542640 through 0x542760: `F_NPC_MESSAGE_MST`, `F_RELOAD_FILE_MST`). 32 named for response files: 29 saved from Initialize tags; 3 in-memory lists filled by loading `F_AREA_MST`, `F_DUNGEON_MST`, `F_LAND_MST` (their response readParam calls the checked getter). The recipe list loads from `F_RECIPE_MST`. `M_DEFINE_MST` / `M_URL_MST` go together, on any earlier failure or a clear loaded flag (0x541D0F-0x541D9D) |
| deleteCorruptFile | acts only while byte 0x184AA40 is set (0x5420D8). FeatureCheck `corrupted_mst_check` sets it (0x1F0419, 0x1F0435); image default 1 |
| **initial units** | DefineMst 0x5D7840 split on "," "-" ":" "@". True only when each element 1..4 has a unit with a non-empty id (0x8DF6FF-0x8DF7EA) |
| text dictionaries | `createDownloadCheckList` -> 0x3918E0 ("sgtext_" ... ".csv" over `F_RESOURCE_MST`); `LoginScene` extracts the ZIPs through 0x392DA0 (0x8E5069, 0x8E5194) |
| content versions | VersionInfoResponse slot 6 0x531A00 acts on 16 content-kind rows (`F_UNIT_IMG` with "unit_" + ".png", ..., `M_EFFECT_GROUP_MST`) |
| arm64 corroboration | every two-string entry of `tools/mst_download_map.json` (132) matches x86's keys. Its `F_MULTI_GUILD_NPC_MESSAGE_MST` entry is 28 merged strings, opening with x86's `F_MULTI_MR_MST` file key; x86 says `Y70NXZUz` / `zafH9rL1`, and no file under either key exists to decrypt. It lacks 21 `F_MULTI_*` tables. arm64 symbols (`deleteCorruptFile`, `resetFileVersion`, `loadFile` with `getRecordNum`, `saveVersion`, `onReadFinished`, `setInitialUnits`) agree in shape |

### 4.2 The server's delivery path (code, not changed in Session 19)

- **Announcement:** `Initialize.cpp` merges `deploy/mst/version_info_mst.json` (169 rows: 140 F_,
  29 M_) with `gme::mstVersions()`. That reads `<document root>/content/<TABLE>/manifest.json` and
  announces a table only when readable, complete and in its own folder, with every part present. A
  staged row replaces the first baseline row of its table.
- **Parts:** `BfWebController::HandleWebPage` for `/mst/<name>` serves `<document root>/mst/<name>`,
  then `findMstPart` (`content/`, then one table folder deep), then `findContentByName` (any depth,
  first by name), otherwise 404.
- **Response tables:** the 29 checked tags are all `InitializeResp` fields (`gimuserver/packets/all.hpp`).
  Initialize answers them from its cache whatever the account: a fresh install gets them after
  `GET /accounts/guest/login/` and Initialize with no user id.

### 4.3 The audit and its test

- `python scripts/check_mst_bootstrap.py [--localstate DIR] [--server PORT] [--json OUT]`. It
  classifies each registered table for a client state (default an empty LocalState) as available,
  empty, missing, wrong-version, incomplete or malformed. The requirement comes from the login check:
  required, required-conditional, tolerated or unchecked. It also checks the response tags (with
  `--server`), the initial units, the feature, the dictionaries and the content-version rows. Exit 0
  means nothing required is blocked.
- `python scripts/test_mst_bootstrap.py [SERVER_EXE]`:
  - synthetic deliveries in every state, including a partial one (2 of 3 parts) and a staged table
    with a missing part (never announced);
  - an interrupted download (one part of two held, the server holding the other), and a held part
    that does not decode;
  - a version change: staged v6 over a held v5, and a broken v7 never announced;
  - wrong version, record count, malformed, required-empty, the gates, and a missing dictionary;
  - the real inventory against its recorded baseline;
  - with the executable: the real roots on a fresh save live, equal to offline table by table; then
    the synthetic tree served live and changed while running (a part completed, a version raised, a
    part removed), each Initialize re-reading the manifests.
- Both are registered: `test_mst_bootstrap.py` in `SERVER_SUITES`, `x86_mst_bootstrap_check.py` in
  `OTHER` / `TOOLS_BIN`.

### 4.4 Inventory: what this server delivers to an empty LocalState

Offline and live agree (0 differences; live = guest login, Initialize, every part GET):

| Requirement | Available | Empty | Missing | Not registered |
|---|---:|---:|---:|---:|
| required (login check) | 48 | 0 | 0 | |
| required-conditional (`F_RESOURCE_BASE_MST`) | 1 | 0 | 0 | |
| tolerated (soft) | 1 (`F_RELOAD_FILE_MST`) | 0 | 1 (`F_NPC_MESSAGE_MST`, not announced) | |
| unchecked | 73 | 4 (`F_CHRONOLOGY_*`, announced with 0 records) | 26 (never announced) | |
| announced rows readParam ignores | | | | 42 (29 M_ response-table versions, 13 content-kind / F_CDN rows) |

- **29 of 29** checked response tables carry rows in a fresh Initialize; **104 of 104** dictionary
  ZIPs are served; the initial units pass; `corrupted_mst_check` = 1. **Nothing required is blocked.**
- Per-table key, endpoint, version, parts, records, source and part names: Appendix A, and in full
  `out/qa/current/session19/bootstrap/deploy_wire.json`.
- **Reconciliation of "147 kept vs 21 staged"** (names only; `out/qa/current/session19/bootstrap/reconcile.py`):
  - "147" is the file count of `deploy/game_content/mst`, which holds 127 tables.
  - The 21 staged tables all also sit there at older versions, and the staged versions are
    announced instead.
  - The live client and C16, C1, C1 Yes and ContentTest each hold 142 `Ver*` files of exactly
    those 127 tables, at exactly the announced versions. No table a warm client holds is missing
    for a fresh install.
  - 127 tables are announced and served (116 parts from `game_content/mst`, 26 from staged
    folders). The 27 registered but never announced (22 `F_MULTI_*` and five others) are absent on
    every warm client too.

### 4.5 Remaining delivery blockers, and the smallest next step

None among the deliveries the login check requires. Open, and outside an audit:
1. **A true first launch, observed on an isolated fresh package.** The audit predicts it. Only a run
   shows that, for example, the client really requests 142 parts and FILE_CHECK ends with no notice.
   Checklist: [CLIENT_SESSION_19_FIRST_LAUNCH_STEPS.md](CLIENT_SESSION_19_FIRST_LAUNCH_STEPS.md).
2. **Consumers of the 27 never-announced tables** were not traced. No login check reads them and no
   warm client has them; any feature built on them, such as multi-guild or F_FIRST_DESC_MST's
   descriptions, sees an empty table on any install.
3. **`F_RESOURCE_BASE_MST`'s gate** (0x199734F and 0xE13190's flags) is not resolved. The table is
   delivered, so it passes either way.

**Smallest next step:** item 1, as its own authorized task, comparing the server log of a fresh
package's first launch with `check_mst_bootstrap.py --server`'s prediction.

## 5. Negative controls: 11 of 11 caught

`python out/qa/current/session19/build/run_mutants.py [ID ...]`. Each mutant is one exact
replacement, applied, then checked by the durable suites; it counts as caught only when every listed
check fails. The source is then restored byte for byte with a new mtime: `write_bytes`, never
`copy2`, so the next build recompiles the restored file. Pre-mutation hashes:
`out/qa/current/session19/mutants_pre_hashes.txt`. Results: `out/qa/current/session19/mutants/*.json`
and logs. C++ mutants were linked into `session19/bin_mutant` only. The candidate in `session19/bin`
was rebuilt afterwards (§7).

| Id | Control the directive asks for | Mutation | Caught by |
|---|---|---|---|
| P1 | wrong naming rule | item frame for type 5 -> `item_frame_5` | manifest unit 115/1 |
| P2 | wrong filter rule | area plate under /dungeon/ for type 1 only | manifest unit 115/1 |
| P3 | wrong filter rule (the server's curation, ported) | a Vortex area without dungeons kept | manifest unit 115/1 |
| P4 | ignored new source digest (generator) | `server_binding` ignores the `served` digest | manifest unit 115/1 |
| P5 | dropped missing dependency | absent names removed from the sets | manifest unit 94/22 |
| P6 | dropped requirement of an unknown list | the weapon stand-in loses `item/` | manifest unit 115/1; rules check 183/1 |
| P7 | bootstrap: incomplete delivery accepted | the record-count rule ignored | bootstrap test 25/1 |
| P8 | bootstrap: binary facts drifted | one encode key changed in `mst_bootstrap_x86.json` | bootstrap facts 36/1; bootstrap test 23/3 |
| C1 | ignored new source digest (broker, runtime) | `bindServerTables` skips the digest of `served` entries | server tables 37/3 (the three stale served-area states read complete) |
| C2 | omitted binding accepted (broker) | the `inputs.tables` -> `client_files` refusal disabled | server tables 39/1 (the omitted-binding case) |
| C3 | the server's snapshot drifts from the generator | `StartupProjection` header columns swapped | server tables 20/20 (every bound state refused) |

Every applied mutant was restored, and `sha256sum -c mutants_pre_hashes.txt` passes for the C++
sources (`ContentBroker.cpp` `f4548fca…`, `ServerCache.cpp` `553832b2…`). Build chain:
- the C3 mutant build recompiled `ContentBroker.cpp` from its restored source;
- the final build recompiled the restored `ServerCache.cpp`;
- no source is newer than the final EXE.

The final candidate passes every suite each C++ mutant failed (§6).

## 6. Commands and results (October 22)

Runner: `bash out/qa/current/session19/build/run_s19_suites.sh`. It prints the hashes first. Logs are
`out/qa/current/session19/suite_s19_*.log`; the console log is `build/run_s19_suites_console.log`.
All against the final candidate `02b95f90…` (§7).

| Check | Result |
|---|---|
| `python scripts/test_startup_coverage_manifest.py` | **116 passed, 0 failed** (Session 18: 71) |
| `python tools/bin/x86_startup_rules_check.py` | **184 passed, 0 failed** (Session 18: 129) |
| `python tools/bin/x86_bundle_exclusions.py --check` | 203 slots, identical to the binary (unchanged) |
| `python tools/bin/x86_mst_bootstrap_check.py` | **37 passed, 0 failed** (new) |
| `python scripts/check_startup_evidence.py --manifest <s19 final> --report <its report>` | consistent with the client evidence (the S18 manifest is contradicted on the 12 login-end names) |
| proxy unit `offline-proxy/build/tests/content_provider_tests.exe` (unchanged `671ead24…`, re-run) | **677 passed, 0 failed, 5 skipped**. The skips are the live integrations; the live suites below run them |
| `check_attach_overlap.py` [`--negative`], `check_startup_sites.py` [`--negative`] | PASS, negative controls PASS |
| retained proofs `check_gate_flash_site.py`, `check_audio_reader.py`, `check_post_quest_sites.py` | PASS, 0 failures each |
| mocked harness `scripts/test_content_harness_startup.py` | **86 passed, 0 failed** (unchanged) |
| server tables `scripts/test_content_server_tables.py <exe>` | **40 passed, 0 failed** (Session 18 correction: 28) |
| coverage wire `scripts/test_content_coverage_wire.py <exe> --real <C8> <s19 final>` | **96 passed, 0 failed**. The broker agrees with the offline evaluation class by class and name by name on the real tree: login 2,257 ok; background 72,201 ok, 8 absent, 9 unowned; 17 problem names; 5 unknown; 13 I rows; bound |
| live startup `scripts/test_content_startup_live.py <exe> <tests> --real <C8> <s19 final>` | **28 passed, 0 failed**. Real tree: the C++ side read "incomplete", so the census was built and the prompt asked |
| bootstrap `scripts/test_mst_bootstrap.py <exe>` | **38 passed, 0 failed**. Synthetic 22; real inventory equal to its baseline 4; live real roots (equal to offline table by table, 104 dictionaries, 29 response tables) 4; live synthetic tree as built and after three changes while running 8 |
| bootstrap audit `scripts/check_mst_bootstrap.py --json bootstrap/deploy_offline_final.json` | `every required delivery is available` (169 announced rows); the live probe `bootstrap/wire_probe.py` on the final candidate: 0 differences from offline, no blocker |
| `scripts/check_content_policy.py <exe> --policy <C8>` | **8 passed, 0 failed**: 75,303 owned, 0 collisions, 0 unavailable (unchanged) |
| C8 through the DLL, `scripts/test_content_family_live.py <exe> <tests> <C8> <trace> --effect-report <C3> --policy-report <C8 report>` | **11 passed, 0 failed**; proxy 6,360 passed, 0 failed, 4 skipped; 36,321 game reads byte-exact; 5,629 traced names byte-exact (unchanged) |
| negative controls `out/qa/current/session19/build/run_mutants.py` | **11 of 11 caught**, every source restored byte for byte (§5) |
| `python scripts/check_client_inputs.py --manifest <s19 final> --localstate <C16 LocalState> --package <C16 package> --no-banner-dir` | exit 0, read only: C16 holds exactly the 13 client files (the resourceMap and 12 LocalState files of 11 tables; Session 19 added `F_ITEM_MST` and `F_DUNGEON_MST`); the 7 server inputs (6 `mst` + `served:3SG2wX0R`) bind for an isolated server (`check_client_inputs_c16_final.log`) |

### 6.1 Server regressions

`python scripts/run_bugfix_regressions.py out/qa/current/session19/bin/gimuserverw.exe`, October 22,
21:30–21:52. The runner hashed the executable: `02b95f90…`.
- **61 entries, 2,640 checks passed, 0 failed**, no failed and no expected-failure entry.
- The mission validator equals its recorded baseline: 3,117 findings, digest `7bae35bbe120`, the
  same as Sessions 16 and 18.
- New entries: `test_mst_bootstrap.py` (38/0, with the executable), `x86_mst_bootstrap_check.py`
  (37/0), `check_startup_evidence.py` (marker) and `x86_startup_rules_check.py` (184/0).
- The run includes the server tables (40/0), the coverage wire suite on the small tree (87/0), the
  mocked harness and the manifest unit suite (116/0).
- The Session 18 correction run had 57 entries and 2,339 checks.

The summary is copied to `out/qa/current/session19/regressions/SUMMARY.md` / `.json`, because the
runner replaces `out/qa/current/SUMMARY.*` each time. The previous summary (the Session 18 correction,
October 22, 13:21) is kept in `out/qa/current/session19/pre/qa_summary_before/`.

**Reused from Session 18 without change, and why that holds:**
- the DLLs and the proxy test binary (no DLL source changed; re-run: 677/0/5, family live 6,360/0/4);
- the C8 policy (re-checked 8/0);
- the hook-signature, attach-order, gate-flash, audio-reader and post-quest proofs (re-run, PASS).

The only skips are the proxy suite's 5 (Session 18: 5), and the family run's 4 live integrations;
those integrations run in their own suites above. There are no other skips and no failures.

## 7. Hashes (QC: verify on disk)

| Artifact | SHA-256 |
|---|---|
| **server candidate** `out/qa/current/session19/bin/gimuserverw.exe` / `.pdb` | `02b95f90c98bcf921a247cc305c0244917c3a65274546ffc7eb23a074ade9272` / `4eb03c20257d344dafa2d37f310abdd12f6ad0fefdab2904333f9177b5fc4154` |
| superseded: the intermediate Session 19 build s19b `66a342bd…` | relinked over by the final build in the same folder (not retained) |
| mutant builds in `session19/bin_mutant` (C1, C2, C3; the folder holds C3) | `b13fe058070a97015444f4a7e33ef231d108e2872ecabbc4faeaa5eb75cc5bb9`, `863e72144f46df1b0e28a400af13370ee88c69d31fd7dce5696bc1f047c0a750`, `f9752efa1a699791cc3aa9c55d0a0e080c353bed0453f43f7b24e5741f03e337` |
| proxy test binary `offline-proxy/build/tests/content_provider_tests.exe` (unchanged) | `671ead24947ee1296a8834fe5864aacef9ed3c45148105519355b6ef2c898955` |
| DLLs (unchanged): `session18/proxy_29960_s18/libcurl.dll`, `proxy_9960_s18/libcurl.dll` | `cfb825da79de8ca7a68db12009e9a9c66f5181bdb0af38d6a7660d3b44aba545`, `37d30448d215de69e40d250e34618677c06f068c962cd9eeabadfce383d8716a` |
| the supported client `BraveFrontier.Windows.exe` | `fb4f3c25f02944ca9e0f80fe5555998f0d6fae5e9901e4cf09795be90c8e3c49` |
| C8 policy / report (unchanged) | `388185d921265ca5197bc8e27671e36037e2d4703484116ec8cd2a7b118583ea` / `f5cd79a571bcf5aac4635cafd9dc6d1715506e7ad15d6ba6d6ca215b7f610802` |
| **manifest** `out/qa/current/session19/coverage/content_startup_s19_final.json` / report | `9b3068e69b88e3408662bb51c6d0d9a4f0aa61a3c8452526a72d6a49ea51cd04` / `adbe3a320500517afe12d314fad9baddf891842a4495753f775ef6a55411e48e` |
| intermediate manifest `content_startup_s19.json` (kept) | `f7f959b6505adf16d7e8f7299a861a331cd270e9431b01d9f7007c253482e73d` |
| gap record `coverage/item_area_gaps.json`; live inventory `bootstrap/deploy_wire.json` | `a4d4b8885d3c905a8b65ff90a8e4668b0da6b55850a39e874773435ebc5aeed4`; `049e24c1f2c25d0e7f8b0abb977727ec7323ba8d8e7b88fa000e1ba9196ba9bb` |
| unchanged identities: live in-tree server `out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe`; C16 layout DLL; C1 DLL; Session 18 servers `session18/bin`, `session18/bin_correction` | `6ee7d454d8b75f47fc0d6e8be44c5284871ab885b3f6299ac74f25ac6df08a1b`; `45a2c1b18e3ad1da07db735282636bf62f52003c5552e43347ba05ea1a41c1c8`; `09056ee55d52936ad584a1ec79e044369c817da0b904901a9e3deb7947ba9c24` (`content_client_c1/package/libcurl.dll`); `995a088d3c4b2a675464b4595cf0ef028d8392df8c592779931a65b66bdf53bc`, `4eded167851c4b30eb0ae5bf23833a2b08bb6adb873c0d8c35ba2a9ba8e19856` |

**Changed sources, durable fixtures and docs (full SHA-256; this handoff excluded, it cannot hold its own):**

| File | SHA-256 |
|---|---|
| `scripts/startup_coverage.py` | `ad20a58c5228fe704ae5369d90d18a9d3a1d10f2dcc617b1e2f0850e5a61e07c` |
| `scripts/test_startup_coverage_manifest.py` | `07cf60ec5b77a379b8be5da1ad51a4a7d9a5e339a76724e9031823192863f828` |
| `scripts/check_startup_evidence.py` | `c8cdad3c3004486181835335c4021e74025536f1d90364343b9b2b0b55fd43b6` |
| `scripts/check_client_inputs.py` | `e2f3701dcbad3f39868e32ebb4a7a4cfbe9aa09928dcd78d3e86fada68beeb7e` |
| `scripts/test_content_server_tables.py` | `283f8f16408e93deadd8af8fa96d77f32ad671f96891fa1481a66bbeb6fbbf3e` |
| `scripts/test_content_coverage_wire.py` | `35b4911da5fd08c1256db20fb13212e7f9f9ec48ba7d039ce02c7060788c4f47` |
| `scripts/test_content_startup_live.py` | `1c12f8887d18f6ea2d7b09a9d6bb283d3543b477107d88c7b6d0ce10ba5a4711` |
| `scripts/check_mst_bootstrap.py` | `665d5bd4bca5edb461be049d5564f1e5583527f09af7c7a98854c1ebb322961b` |
| `scripts/test_mst_bootstrap.py` | `f3ea1d29aa38e7e1e4af5f9ec8b91740f032becf54dd1cdaa56ce5a1b9c7f010` |
| `scripts/run_bugfix_regressions.py` | `5242380dca63f5face9fe269780d9de4d1425c12f71879cf844bc10cd725ab5e` |
| `tools/bin/x86_startup_rules_check.py` | `d218d1d117e30f95eb45b5e3d36aa92356033c5e15ac1320a472f319aec04a30` |
| `tools/bin/x86_mst_bootstrap_check.py` | `c9011e4435ccf14426b0e227f838a821b8e60a484f1179b81c8fd643816bbef5` |
| `gimuserver/content/ContentBroker.cpp` | `f4548fca94c776e1f3192802a7adc2e2eb6f95ab050fd8228a050010b2fa909a` |
| `gimuserver/content/ContentBroker.hpp` | `e56a827dea83d3112cadf8cf8e4255c4f275b131aa7be8e818c36ec3a8211641` |
| `gimuserver/drogon/ServerCache.cpp` | `553832b2d370e483ae153f1d9e07faa2934c47e2c5a07b5e459fcfeb39ecd160` |
| `gimuserver/drogon/ServerCache.hpp` | `e46258fe7a6d2670eca685af8f7a54aed61864cc3651e3881dd909cbdef50670` |
| `gimuserver/drogon/GimuServer.cpp` | `2f0293169dc3d974b61e2769d571025f65950e565e1169ed7a6653d5ed735de2` |
| `scripts/qa_baselines/c16_provider_off_queue_20261021.json` | `ef64d968e0a2240d1b0c1a01dc828780022479550225b3aefff82281718a1a5a` |
| `scripts/qa_baselines/mst_bootstrap_x86.json` | `2b92c32fa4a43f4bbabc1138bfbba6fbf93414a9628327ae4b5b2d828facf73d` |
| `scripts/qa_baselines/mst_bootstrap_deploy_20261022.json` | `d262c54d15ac85897f39b8245f19f1e718fd099986d15941d95939c8f01c5e25` |
| `docs/OFFLINE_STARTUP_TRANSITION_PLAN.md` | `540f7f329fbb570fb09e2159b53d6a251f0b3068131a984942b2e3d28ddd319d` |
| `docs/BF_OFFLINE_SERVER_HANDBOOK.md` | `e5e6c5227f90a47388c606115a87ea577e779df6759fc332bf543457e098a594` |
| `docs/CLIENT_SESSION_19_FIRST_LAUNCH_STEPS.md` | `36c0cd83eb42f5854025ad16ab6b729a839b3602c1a35600467433e84468b35f` |

## 8. Remaining blockers and the next bounded action

Coverage stays **incomplete**; the bypass stays off:
1. **5 unknown lists:** unit / initial units, weapon / summoner_img, gacha (with gacha_effect),
   effect / effect_group, and the login-end preloads (x86 not traced).
2. **8 absent names and 9 families**: the 5 of Session 16-18 plus the 3 area banners, and the 9
   unowned roots. These are content decisions.
3. **Two area collisions to review** (§3): the C8 aliases disagree with the area trail and the warm
   caches.
4. **A true first launch, observed** (§4.5). The master-table contract is satisfied today on paper
   and over the wire, which is not a client run.

**Next bounded action, proposed:** derive one or two more of the unknown lists the same way. The
login-end preloads are the smallest: their requests are a short x86 tail after
`createDownloadCheckList`, and C16 evidence exists. Separately authorized, the first-launch
observation of §4.5.

## 9. Proposed release file list (not staged, not committed)

Nothing from Sessions 7-19 of the startup and content work is committed. Releasing Session 19
therefore means releasing the stack it builds on. Files Session 19 changed or added are marked
**S19**. Everything listed is on disk now.

**A. Startup coverage, server-table binding and fresh-install bootstrap**

| Kind | Files |
|---|---|
| server | `gimuserver/content/ContentBroker.cpp` / `.hpp` (**S19**), `gimuserver/drogon/ServerCache.cpp` / `.hpp` (**S19**), `gimuserver/drogon/GimuServer.cpp` (**S19**), `gimuserver/controller/OfflineModController.cpp` / `.hpp` |
| generators and audits | `scripts/startup_coverage.py` (**S19**), `scripts/check_client_inputs.py` (**S19**), `scripts/check_startup_evidence.py` (**S19**, new), `scripts/check_mst_bootstrap.py` (**S19**, new), `scripts/check_content_policy.py`, `scripts/content_inventory.py`, `scripts/content_provider_client_test.py`, `scripts/gen_unit_family_policy.py`, `scripts/gen_effect_family_policy.py`, `scripts/gen_ui_family_policy.py`, `scripts/gen_audio_policy.py`, `scripts/gen_gate_script_policy.py`, `scripts/gen_gate_scripts.py`, `scripts/gate_overlay_model.py`, `scripts/make_gate_test_mst.py` |
| tests | `scripts/test_startup_coverage_manifest.py` (**S19**), `scripts/test_content_server_tables.py` (**S19**), `scripts/test_content_coverage_wire.py` (**S19**), `scripts/test_content_startup_live.py` (**S19**), `scripts/test_mst_bootstrap.py` (**S19**, new), `scripts/test_content_harness_startup.py`, `scripts/test_content_broker_wire.py`, `scripts/test_content_generation_qc.py`, `scripts/test_content_family_live.py`, `scripts/test_content_provider_live.py`, `scripts/test_content_audio_live.py`, `scripts/test_content_visible_swap.py`, `scripts/test_content_disposable_commands.py`, `scripts/test_gen_effect_family_policy.py`, `scripts/test_gate_overlay_model.py` |
| shared test plumbing | `scripts/bf_testkit.py`, `scripts/run_bugfix_regressions.py` (**S19**), `scripts/check_client_fixture.py`, `scripts/make_debug_fixture.py`. The runner also carries the gameplay suites of group B. |
| recorded data the scripts read | `scripts/qa_baselines/c16_provider_off_queue_20261021.json` (**S19**), `scripts/qa_baselines/mst_bootstrap_x86.json` (**S19**), `scripts/qa_baselines/mst_bootstrap_deploy_20261022.json` (**S19**) |
| technical docs | `docs/OFFLINE_STARTUP_TRANSITION_PLAN.md` (**S19**: §10), `docs/BF_OFFLINE_SERVER_HANDBOOK.md` (**S19**: current direction), `docs/OFFLINE_CONTENT_PHASE_C1_PLAN.md`, `docs/OFFLINE_CONTENT_PHASE_C2.md` ... `_C7.md`, `docs/ASYNC_REVERSE_PROGRESS.md`, `docs/POST_QUEST_PROMPTS.md` |
| other repository | `offline-proxy`: the Session 16-18 DLL sources and tests, unchanged by Session 19 (listed in the Session 18 handoff §1) |

**Decide before committing: `tools/` is git-ignored by design.** `.gitignore:505`, "Local dev
tooling + docs — not for upstream", ignores `tools/`; nothing in it is tracked. The regression
runner still runs `tools/bin/x86_startup_rules_check.py` and `tools/bin/x86_mst_bootstrap_check.py`.
They need `tools/bin/pe_x86.py`, `tools/bin/x86_bundle_exclusions.py` and the client EXE. Either
force-add those four files or keep them local and drop their two runner entries in a fresh
checkout. Session 19 moved the bootstrap facts to `scripts/qa_baselines/` so the audit itself needs
nothing from `tools/`. `.gitignore` was not changed.

**B. Other pending work in this tree, not reviewed in Session 19.** These are gameplay and database
changes from earlier sessions: the handlers (`gimuserver/gme/handlers/*`, 38 files) and
`gimuserver/gme/common/{Common.hpp, LoginCampaign.cpp, SelectorRotation.cpp, Transactions.hpp,
Warehouse.cpp}`; `standalone_frontend/{DebugCli.cpp, main.cpp}`; the `packet-generator` submodule
pointer (commit inside the submodule first); `scripts/audit_transactions.py`,
`scripts/make_small_roster_fixture.py`, `scripts/repair_leader_skills.py`,
`scripts/verify_town_harvest.py`; and the gameplay wire tests, new and modified
(`test_clearunits_wire.py`, `test_craft_row_identity_wire.py`, `test_db_commit_contention_wire.py`,
`test_db_exception_atomicity_wire.py`, `test_leader_skill_grants_wire.py`, `test_town_harvest_wire.py`,
`test_verify_town_harvest.py`, `test_reward_lock_snapshots_wire.py`, `test_synthesis_wire.py`,
`test_warehouse_*_wire.py`). Propose them separately, with their own review.

**Excluded, kept on disk:**
- `out/` (all QA outputs, candidates, evidence);
- `deploy/system/content_provider.json` (a deployment artifact: committing it changes what a server
  reads at startup);
- session, QC, checklist and coordination documents: `docs/CLAUDE_SESSION_*.md` (directives and
  handoffs, this one included), `docs/CLIENT_SESSION_*_STEPS.md`, `docs/CLIENT_C1_YES_STEPS.md`,
  `docs/CLIENT_SESSION_8_CHECKLIST.md`, `docs/CLIENT_SESSION_19_FIRST_LAUNCH_STEPS.md`,
  `docs/QC_SESSION_*_REVIEW.md`, `docs/SESSION_*_CLIENT_RESULTS.md`, `docs/BUG_BACKLOG_STATUS.md`.

## 10. Resume steps for Codex QC

1. Verify §7 on disk: live `6ee7d454…`, C1 `09056ee5…`, C16 `45a2c1b1…`.
2. Read the diffs against `out/qa/current/session19/pre/` (`SHA256SUMS`).
3. Rerun `bash out/qa/current/session19/build/run_s19_suites.sh` (about 25 minutes). It hashes the
   binaries first.
4. Rerun the binary checks: `python tools/bin/x86_startup_rules_check.py` and
   `python tools/bin/x86_mst_bootstrap_check.py`.
5. Optionally: `python out/qa/current/session19/build/run_mutants.py P1-item-frame ...`. It rewrites
   one source per mutant temporarily and restores it byte for byte. The C++ ones rebuild
   `session19/bin_mutant`, about 5 minutes each. Rebuild the candidate afterwards:
   `bash out/qa/current/session19/build/build_server_s19.sh <tag> bin`.
6. The audit against any server: `python scripts/check_mst_bootstrap.py --server <PORT>`, on an
   isolated server with its config as argv[1].

## Appendix A. Bootstrap inventory: the required deliveries

From the audit of the real roots over a running isolated server on a fresh save (final candidate;
`out/qa/current/session19/bootstrap/deploy_wire.json`; generated by `bootstrap/inventory_md.py`).
Every part is `/mst/<name>`, base64(AES-128-ECB(encode key zero-padded to 16, PKCS#7)) of a JSON array
of row objects, saved under the same name in LocalState. "Records 0" means the client does not
compare the row count (`loadFile` 0x543030).

| Table | Requirement (consumer) | File key / encode key | Announced v / parts / records | Rows | Served from | LocalState parts |
|---|---|---|---|---:|---|---|
| F_AREA_MST | required: M_AREA_MST at 0x5405F7 | `vj6fEsg7` / `73bUW8wn` | 670 / 1 / 411 | 411 | content/F_AREA_MST/ | Ver670_vj6fEsg7.dat |
| F_CHLNG_MISSION_GRADE_MST | required: deleteCorruptFile 0x5414B7 | `6q9ID0wP` / `1nte4gCY` | 1 / 1 / 0 | 7 | game_content/mst | Ver1_6q9ID0wP.dat |
| F_CHLNG_MISSION_ITEM_SET_MST | required: deleteCorruptFile 0x541577 | `gm4oR9D5` / `zBxwtg89` | 2 / 1 / 0 | 4 | game_content/mst | Ver2_gm4oR9D5.dat |
| F_CHLNG_MISSION_MST | required: deleteCorruptFile 0x541457 | `8GhcfX4Q` / `rvIa85RA` | 1 / 1 / 0 | 1 | game_content/mst | Ver1_8GhcfX4Q.dat |
| F_CHLNG_MISSION_REWARD_MST | required: deleteCorruptFile 0x541517 | `31BhsATV` / `6Zwi24yp` | 1 / 1 / 0 | 10 | game_content/mst | Ver1_31BhsATV.dat |
| F_DBB_BOND_RECIPE_MST | required: deleteCorruptFile 0x541E4F | `ru80uthU` / `1ozLjap7` | 10 / 1 / 105 | 105 | game_content/mst | Ver10_ru80uthU.dat |
| F_DBB_MST | required: deleteCorruptFile 0x541DF6 | `L5t2Wfrq` / `SbZW6TMJ` | 136 / 1 / 141 | 141 | game_content/mst | Ver136_L5t2Wfrq.dat |
| F_DUNGEON_MST | required: M_DUNGEON_MST at 0x540642 | `B57MJYoF` / `w5W8RPSG` | 1003 / 1 / 1532 | 1532 | content/F_DUNGEON_MST/ | Ver1003_B57MJYoF.dat |
| F_EFFECT_GROUP_MST | required: deleteCorruptFile 0x5412DE | `BQJq29Zh` / `3FWEGtX9` | 608 / 2 / 14290 | 14290 | game_content/mst | Ver608_BQJq29Zh_1.dat .. _2 |
| F_EFFECT_MST | required: deleteCorruptFile 0x541285 | `0VIAZHT9` / `9ojr0tPC` | 583 / 1 / 8557 | 8557 | game_content/mst | Ver583_0VIAZHT9.dat |
| F_ET_MISSION_MST | required: deleteCorruptFile 0x541F0F | `IwODP3o4` / `s4tsRd7H` | 176 / 1 / 460 | 460 | game_content/mst | Ver176_IwODP3o4.dat |
| F_GACHA_CATEGORY_MST | required: deleteCorruptFile 0x541EAF | `Jba1QEOD` / `khrEOkEv` | 503 / 1 / 380 | 380 | game_content/mst | Ver503_Jba1QEOD.dat |
| F_HELP_DETAIL_MST | required: deleteCorruptFile 0x541397 | `EwQ0S5cX` / `8HG9C1nw` | 42 / 1 / 180 | 180 | game_content/mst | Ver42_EwQ0S5cX.dat |
| F_ITEM_MST | required: deleteCorruptFile 0x54122C | `83JWTCGy` / `houmK14R` | 1028 / 1 / 1668 | 1668 | content/F_ITEM_MST/ | Ver1028_83JWTCGy.dat |
| F_LAND_MST | required: M_LAND_MST at 0x540B5E | `1M65tIPz` / `drCPNM10` | 147 / 1 / 26 | 26 | game_content/mst | Ver147_1M65tIPz.dat |
| F_LEADER_SKILL_MST | required: deleteCorruptFile 0x541337 | `4dE8UKcw` / `HGYZ1Ag5` | 750 / 1 / 1812 | 1812 | content/F_LEADER_SKILL_MST/ | Ver750_4dE8UKcw.dat |
| F_MEDAL_MST | required: deleteCorruptFile 0x5415D0 | `DX7KZ0Tf` / `UH1ruJ36` | 4 / 1 / 0 | 1 | game_content/mst | Ver4_DX7KZ0Tf.dat |
| F_MISSION_MST | required: deleteCorruptFile 0x5410BF | `4gA3WCQX` / `y1hZzE74` | 1119 / 1 / 3433 | 3433 | content/F_MISSION_MST/ | Ver1119_4gA3WCQX.dat |
| F_MISSION_SCRIPT_MST | required: deleteCorruptFile 0x5413F7 | `F2Dz3QHU` / `U91CxXGi` | 288 / 1 / 228 | 228 | game_content/mst | Ver288_F2Dz3QHU.dat |
| F_RAID_BATTLE_GROUP_MST | required: deleteCorruptFile 0x5418DD | `fYJjG4B6` / `c0Ejwh7S` | 52 / 1 / 11121 | 11121 | game_content/mst | Ver52_fYJjG4B6.dat |
| F_RAID_BOSS_MST | required: deleteCorruptFile 0x54181D | `4KAXRjg0` / `a4jIgR2T` | 76 / 1 / 287 | 287 | game_content/mst | Ver76_4KAXRjg0.dat |
| F_RAID_BOSS_PARTS_MST | required: deleteCorruptFile 0x541B6F | `w8Phatd4` / `6de7rGsP` | 78 / 1 / 865 | 865 | game_content/mst | Ver78_w8Phatd4.dat |
| F_RAID_BOSS_ROUTE_MST | required: deleteCorruptFile 0x5417BD | `z7UiGPQ1` / `L4pBaT0q` | 45 / 1 / 347 | 347 | game_content/mst | Ver45_z7UiGPQ1.dat |
| F_RAID_MAP_MST | required: deleteCorruptFile 0x54169D | `39mGIANe` / `HMib7V50` | 54 / 1 / 44 | 44 | game_content/mst | Ver54_39mGIANe.dat |
| F_RAID_MISSION_BOSS_MST | required: deleteCorruptFile 0x541BCF | `W3Q62S1F` / `yu28TMIo` | 68 / 1 / 264 | 264 | game_content/mst | Ver68_W3Q62S1F.dat |
| F_RAID_MISSION_CLEAR_CND_MST | required: deleteCorruptFile 0x541A5D | `Pgnu7d8k` / `E81Hkv3g` | 71 / 1 / 235 | 235 | game_content/mst | Ver71_Pgnu7d8k.dat |
| F_RAID_MISSION_MST | required: deleteCorruptFile 0x54187D | `f9eHCSc8` / `4WbIjfB3` | 396 / 1 / 154 | 154 | game_content/mst | Ver396_f9eHCSc8.dat |
| F_RAID_MISSION_POINT_MST | required: deleteCorruptFile 0x5419FD | `gVmh3Lw2` / `qga56iME` | 77 / 1 / 1378 | 1378 | game_content/mst | Ver77_gVmh3Lw2.dat |
| F_RAID_PLAYSTYLE_MST | required: deleteCorruptFile 0x54193D | `xMFHp36r` / `7z5RKwhU` | 6 / 1 / 6 | 6 | game_content/mst | Ver6_xMFHp36r.dat |
| F_RAID_POINT_MST | required: deleteCorruptFile 0x5416FD | `79MTkNus` / `76rVpcGL` | 57 / 1 / 344 | 344 | game_content/mst | Ver57_79MTkNus.dat |
| F_RAID_RC_MST | required: deleteCorruptFile 0x54199D | `i7xd5oMb` / `gui7x0cr` | 11 / 1 / 6 | 6 | game_content/mst | Ver11_i7xd5oMb.dat |
| F_RAID_USER_ROUTE_MST | required: deleteCorruptFile 0x54175D | `2sjB0RvV` / `Ny6rB7HV` | 56 / 1 / 4135 | 4135 | game_content/mst | Ver56_2sjB0RvV.dat |
| F_RAID_WORLD_MST | required: deleteCorruptFile 0x54163D | `35fQnyPs` / `8ozA4wZv` | 12 / 1 / 1 | 1 | game_content/mst | Ver12_35fQnyPs.dat |
| F_RECIPE_MST | required: M_RECIPE_MST at 0x5407E8 | `3TM68IaF` / `na7Wb8u1` | 345 / 1 / 617 | 617 | game_content/mst | Ver345_3TM68IaF.dat |
| F_RESOURCE_MST | required: deleteCorruptFile 0x5411D3 | `hdt2N6mr` / `z5dp9GX8` | 3275 / 2 / 49342 | 49342 | game_content/mst | Ver3275_hdt2N6mr_1.dat .. _2 |
| F_SG_DAILYLOGIN_REWARDS_MST | required: deleteCorruptFile 0x541F68 | `ni16xAsw` / `n3th3poV` | 14 / 1 / 192 | 192 | game_content/mst | Ver14_ni16xAsw.dat |
| F_SG_UNIT_TYPE_BONUS_SKILL_MST | required: deleteCorruptFile 0x542021 | `pRo1Azab` / `J2b3drej` | 19 / 1 / 24 | 24 | game_content/mst | Ver19_pRo1Azab.dat |
| F_SKILL_LEVEL_MST | required: deleteCorruptFile 0x541173 | `zLIvD5o2` / `rF97pVdT` | 1487 / 4 / 33381 | 33381 | game_content/mst | Ver1487_zLIvD5o2_1.dat .. _4 |
| F_SKILL_MST | required: deleteCorruptFile 0x541119 | `wkCyV73D` / `JA03wvHG` | 1536 / 6 / 33381 | 33381 | content/F_SKILL_MST/ | Ver1536_wkCyV73D_1.dat .. _6 |
| F_STAMP_MST | required: deleteCorruptFile 0x541AB6 | `W2j4E3te` / `NFJ74Wmg` | 8 / 1 / 100 | 100 | game_content/mst | Ver8_W2j4E3te.dat |
| F_STAMP_SET_MST | required: deleteCorruptFile 0x541B0F | `KuCe7t3p` / `em68GxrB` | 10 / 1 / 6 | 6 | game_content/mst | Ver10_KuCe7t3p.dat |
| F_TRIAL_MISSION_MST | required: deleteCorruptFile 0x541C31 | `F9fGSe42` / `Hxidv1y0` | 72 / 1 / 34 | 34 | game_content/mst | Ver72_F9fGSe42.dat |
| F_UNIT_CGS_MST | required: deleteCorruptFile 0x541006 | `6rjmY7tV` / `Y7ezNyn2` | 811 / 1 / 2292 | 2292 | game_content/mst | Ver811_6rjmY7tV.dat |
| F_UNIT_COMMENT_MST | required: deleteCorruptFile 0x540F0A | `2npmC9Nw` / `Apc5eIw6` | 706 / 1 / 2282 | 2282 | content/F_UNIT_COMMENT_MST/ | Ver706_2npmC9Nw.dat |
| F_UNIT_EVO_MST | required: deleteCorruptFile 0x540F6A | `Ef6oG0mz` / `R9IryC5v` | 597 / 1 / 1342 | 1342 | game_content/mst | Ver597_Ef6oG0mz.dat |
| F_UNIT_MST | required: deleteCorruptFile 0x540EAA | `2r9cNSdt` / `7nL1WTUb` | 1086 / 1 / 2291 | 2291 | content/F_UNIT_MST/ | Ver1086_2r9cNSdt.dat |
| F_UNIT_SELECTOR_GACHA_TICKET_MST | required: deleteCorruptFile 0x541FC1 | `LaD2pF1e` / `jHsf3JDz` | 106 / 1 / 16 | 16 | game_content/mst | Ver106_LaD2pF1e.dat |
| F_UNIT_TYPE_MST | required: deleteCorruptFile 0x54105F | `J4heGZ2U` / `4a6GF3Pt` | 52 / 1 / 6 | 6 | game_content/mst | Ver52_J4heGZ2U.dat |
| F_RESOURCE_BASE_MST | required-conditional: deleteCorruptFile 0x541D0A | `a4M4dh1i` / `kIj2n6s5` | 24 / 1 / 2744 | 2744 | game_content/mst | Ver24_a4M4dh1i.dat |
| F_NPC_MESSAGE_MST | tolerated: resetFileVersion 0x540E51 | `q25fApus` / `gN1u7ZeG` | not announced | -- | -- | -- |
| F_RELOAD_FILE_MST | tolerated: resetFileVersion 0x541C98 | `kpN3M6tK` / `1U9ouXzP` | 779 / 1 / 562 | 562 | game_content/mst | Ver779_kpN3M6tK.dat |

| Response table | Initialize tag | LocalState file | Rows in a fresh Initialize |
|---|---|---|---:|
| M_LEVEL_MST | `YDv9bJ3s` | `EEPBM76L2G.dat` | 1000 |
| M_GACHA_MST | `5Y4GJeo3` | `SA8H9C1H7V.dat` | 1309 |
| M_UNIT_EXP_PATTERN_MST | `JYFGe9y6` | `S1IRSBZSOW.dat` | 804 |
| M_TROPHY_GRADE_MST | `Ked15IpH` | `LXIBV996C0.dat` | 690 |
| M_TROPHY_GROUP_MST | `1NTG2oVZ` | `CPBS5LY8X4.dat` | 10 |
| M_TROPHY_MST | `6CTU8m2v` | `8DEI9K72W4.dat` | 230 |
| M_RECIPE_MST | `8f0bCciN` | `7DKCJFK1J7.dat` | 617 |
| M_INFORMATION_MST | `5nBa3CAe` | `0CO33AEKBA.dat` | 254 |
| M_TOWN_LOCATION_MST | `1y2JDv79` | `NI8KQGWAOW.dat` | 4 |
| M_TOWN_LOCATION_LV_MST | `9ekQ4tZq` | `SU3JE2GPHE.dat` | 52 |
| M_TOWN_FACILITY_MST | `Lh1I3dGo` | `FJKS1E2BLI.dat` | 7 |
| M_TOWN_FACILITY_LV_MST | `d0EkJ4TB` | `RYCOS1SH7L.dat` | 114 |
| M_HELP_MST | `9x4zZCeN` | `KT11KBEIQG.dat` | 31 |
| M_HELP_SUB_MST | `5C9LuNrk` | `POQ6R28YT7.dat` | 180 |
| M_ARENA_RANK_MST | `6kWq78zx` | `DHJO1NWC4F.dat` | 101 |
| M_SOUND_MST | `36Sd0Aub` | `ZAMBUBS6A2.dat` | 108 |
| M_NPC_MST | `hV5vWu6C` | `TJL6WBLLRP.dat` | 2 |
| M_GACHA_EFFECT_MST | `Pf97SzVw` | `29YBNDDB77.dat` | 15 |
| M_FROHUN_HR_MST | `h09mEvDR` | `HK2E45BZL9.dat` | 239 |
| M_FROHUN_MISSION_GRADE_MST | `zW1i02aG` | `XGQE8LJGNR.dat` | 2569 |
| M_FROHUN_MISSION_ITEM_SET_MST | `dn0NfRy1` | `8U24YUTZ8N.dat` | 1529 |
| M_FROHUN_MISSION_MST | `5M8jI4cP` | `XJYRERKG11.dat` | 383 |
| M_FROHUN_MISSION_REWARD_MST | `4C1Wt8sS` | `SP9SVZ9GWX.dat` | 3740 |
| M_FROHUN_MST | `mQC4s5ka` | `UISZI2MBAO.dat` | 97 |
| M_FROHUN_MVP_RANKING_MST | `nUmaEC41` | `IE92NDJVFL.dat` | 1061 |
| M_FROHUN_RANKING_REWARD_MST | `P8V71kbw` | `B0SXA8Y6FD.dat` | 3707 |
| M_DUNGEON_KEY_MST | `4NG79sX1` | `ZX8TREMHH4.dat` | 3 |
| M_DEFINE_MST | `VkoZ5t3K` | `ZSPFQ9CFY4.dat` | 1 |
| M_URL_MST | `At7Gny2V` | `ELAUWID095.dat` | 1 |
