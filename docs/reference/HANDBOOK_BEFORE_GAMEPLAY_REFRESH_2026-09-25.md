> Historical reference only. Superseded by ../BF_OFFLINE_SERVER_HANDBOOK.md. Local paths and dated gaps below are not current requirements.

# Brave Frontier offline server — working handbook

Updated 2026-09-14. This is the concise entry point for a new chat. It replaces the previous chronological handbook as the operating guide; the original is preserved verbatim in `BF_OFFLINE_SERVER_HANDBOOK_HISTORY_2026-09-09.md` beside this file. Use `BF_HANDBOOK_HISTORY_INDEX.md` to locate detailed investigations only when relevant.

Read this guide, then the current `NEW_CHAT_HANDOFF.md`, then inspect the actual checkout. Older documents are evidence and history, not a queue of commands to execute. The user's current request controls scope. Historical section numbers in source/KDL refer to the preserved history file.

## 1. Current project and evidence levels

| Item | Current baseline |

|---|---|

| Checkout | `C:\Users\Evan\BF\BF-WorkingDirRust` |

| Branch / last inspected HEAD | `audit-campaign` / `70e82f7`; re-check before committing |

| Packet generator | Git submodule `packet-generator`, branch `audit-campaign`; last inspected HEAD `5a9cb38e1eb6c2f207983a507b9eab0e001dde25` |

| Development server | Standalone Debug x64, HTTP `127.0.0.1:9960`; APPX client uses the local/no-SSL setup |

| Live configuration / save | `deploy/config.json` / `deploy/gme.sqlite` |

| Mission / unit archive | 942 / 2,053 records as inspected September 8–9; counts are a snapshot, not completeness claims |

| Working tree | Large pre-existing local changes in both repositories. Preserve them. |

The server emulates Gumi Live authentication, the encrypted GME gameplay API, and the asset CDN. C++/Drogon/SQLite/Glaze implement the server; the Rust generator supplies packet/archive C++ types. The same server library can also be built into the APPX proxy frontend; do not confuse that target with the standalone development server.

Keep four independent labels in findings:

- **Binary-confirmed:** cite the specific code that consumes a field, its address, and what it does. A setter establishes the field map, not the semantics.

- **Client-confirmed:** record exactly what Evan saw and when. Visible success does not prove all values, reward paths, or clients.

- **Authored policy:** balancing, probabilities, synthetic identities, or content chosen for the offline server. Do not present these as recovered official behavior.

- **Unverified / historical:** unresolved inference or an older report not rechecked against today's checkout. Absence of a direct getter call is not proof a field is unread.

### Established state

Reported client-confirmed in the September 8 handoff: sphere second-slot handling, imp fusion stats, BB gating, level rescaling, Great/Super success visuals, parade tier unlocking, and removal of the summon white screen. The September 9 session built the current server and tested Mimic responses. Evan confirmed that the guaranteed chest opened into a Mimic with the correct warning; he did not explicitly confirm the entire defeat/return path.

Ordinary Mimic enemies existed before the chest feature: 287 placements across 172 missions, including units 60142/60143/60144/60224. These fixed enemies are distinct from a chest-triggered encounter. A `unit_drop_chance` such as 80 is capture probability, not spawn probability.

The archive remains authored/incomplete. Many encounters have repeated placeholder-like stats; valid references do not establish live-game fidelity. Missing mission IDs still borrow mission 10's waves, relabelled to the requested ID. The random Mimic policy is removed from this unauthored fallback.

The September 9 wire sweep covered all 942 authored missions: 930 produced structurally valid MissionStart responses; 12 returned Archive error with Mimics both enabled and disabled. Missions 8311001–8320001 (steps of 1000) have special-monster visuals with an empty `anm_cgg`; 8360505/8360506 have placeholder monster 0 with empty image/CGG fields. Their data is unchanged from before this work. These are existing content gaps, not evidence that the new Mimic path failed. The September 10 local asset audit also checked the upstream mission-builder catalog and APK/APPX package lists: no verified missing CGG/visual replacement was found. Estia atlas 87600100 is only partially present; Xie Jing atlas 87605002 is a catalog lead, not an established mapping. Keep the twelve records rejected until their assets/identity are established.

## 2. Where information belongs

| Location | Responsibility |

|---|---|

| `packet-generator/assets/net/*.kdl` | Request/response structures, hashed keys, wrappers, types, consumer citations |

| `packet-generator/assets/mst/*.kdl` | Decoded master-data schemas and evidence |

| `packet-generator/assets/archive/*.kdl` | Server-owned authored archive vocabulary |

| `gimuserver/packets/all.hpp` | Generated from `assets/all.kdl`; do not hand-edit |

| `gimuserver/archive/archive.hpp` | Generated from `assets/archive.kdl`; do not hand-edit |

| `gimuserver/gme/handlers/` | Gameplay handlers using generated types |

| `gimuserver/gme/common/Common.hpp` | Shared identity, packet, persistence, and state helpers; inspect existing helpers first |

| `gimuserver/archive/*Archiver.*` | Read authored data and emit client-specific structures |

| `deploy/archive/` | Curated missions, units, AI, gacha, and shared Mimic policy |

| `deploy/mst/` | Decoded game reference data; some tables are consumed directly by client download |

| `deploy/system/` | Service configuration and response fixtures, distinct from MSTs |

| `deploy/game_content/content/` | Game assets; many local edits here are gitignored |

| `tools/SERVER_COMPONENT_AUDIT.md` | Detailed local field audit; its progress table can lag later corrections |

Put new field evidence in KDL, durable engineering rules here, detailed investigations in the audit/reference material, and current tasks in the short handoff. Do not append another debugging transcript to this guide.

## 3. Reversing workflow

1. Find the exact request producer (`createBody`) and screen flow. Check normal navigation, tutorial-forced navigation, and bulk actions separately.

2. Recover the response class through `GameResponseParser::getResponseObject` @0x1392568. Its key registry is global: a handler may include another feature's response block when needed to refresh state.

3. Read the complete `readParam`, including its tail, manual `getValue()` calls, and inlined stores. Preserve wrappers, array sizes, quoted numbers, defaults, and list operations.

4. Trace getters and member offsets into the actual consumer. Check derived getters, constructors, virtual calls, and scene code when direct xrefs are empty.

5. Cross-check relevant decoded MST values and existing assets. Distinguish a recovered multiplier from an invented probability.

6. Extend KDL, regenerate, inspect generated types, implement the emitter, build, parse the complete response, and test against an isolated save. Then request the relevant client observation.

### Review gate for new emitters (2026-09-14)

Use [BF_EMITTER_REVIEW_CHECKLIST.md](BF_EMITTER_REVIEW_CHECKLIST.md) beside this handbook as a one-page review card. Record: trigger/producer, schema/reader/consumer, every affected cache, authoritative state versus authored policy, and failure/retry tests. Keep disassembly/capture paths and binary hashes with addresses. Existing prose and regex audit output are leads to verify, not proof that a handler is correct.

The September 14 source review found four traps in working-looking code: normal mission losses still unlocked missions because only Frontier Gate checked loss at the clear write; town trophies counted claimed rather than accepted taps; Music House grants and payment were separate autocommit writes with exceptions swallowed; box purchases checked capacity before awaiting but guarded only currency at the write. These paths are now corrected and have isolated regression cases. Mission item batches also reject malformed/unknown values and stack overflow, and captured unit levels are checked against UnitMst. TownFacilityUpdate now rolls back upgrades, payment and response failures together, and counts the actual clamped debit. Its requested levels/payment still come from the client; this is not a complete server-authoritative upgrade validator.

For every mutation, trace all nested helpers: sharing a transaction is insufficient if a helper catches mandatory DB errors and returns success. Test failure after the first write, duplicate/overlapping requests, loss/cancel, and last-entry removal. Do not hide response serialization failures with `{}` after charging. A transaction does not deduplicate a replayed request, and a row-0 clear does not run for an empty array. Derive counters from accepted state changes. Declare unresolved behavior explicitly rather than treating empty fields, absent xrefs, or no logged requests as evidence it is unnecessary.

### Local binaries and tools

Symbol-bearing APK: `C:\Users\Evan\BF\BraveFrontier-APK\lib\arm64-v8a\libgame.so`.

APPX executable: `C:\Users\Evan\BF\BraveFrontierAppxClient\BraveFrontier.Windows.exe`.

Use `tools/bin/so_disasm.py`, `so_xref.py`, `so_response_map.py`, and `so_strref.py`; inspect each script's CLI rather than inventing flags. Python dependencies include Capstone and pyelftools. The September 9 task used the bundled Python runtime at `C:\Users\Evan\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe`; packages installed for that task are not guaranteed to exist in another environment.

APK addresses do not transfer directly to APPX. Treat cross-build behavior as a hypothesis until the Windows client or its own code confirms it. IDA is useful when local tools cannot resolve a path, but ensure the function is the real implementation, not its PLT thunk.

Tool limitations that have caused false conclusions:

- `so_response_map.py` counts direct setter calls, so counts are lower bounds. Its replace/merge verdict may miss specialized clears or replacement inside `addObject`.

- `so_strref.py` can miss code references when register tracking crosses branches. Raw literal presence/absence and code-reference coverage are different claims.

- A getter with no xrefs can be inlined or consumed through a sum getter. Match setter offsets to all reads before marking a field unread.

- If claiming a literal key is absent, check raw bytes in both binaries, and distinguish that from claiming the behavior is absent.

- Search alternative representations before declaring a mechanic missing: Hunter Orbs use `Aube`; second sphere capacity is a sentinel, not a count.

- Never infer absence from a truncated dump or response. Save full output and inspect the relevant tail.

## 4. Wire and KDL contracts

GME requests use `POST /bf/gme/action.php`. The outer header is `F4q6i9xe` with GroupId `Hhgi79M1`; encrypted content is `a3vSYuq2.Kn51uR4Y`. The inner JSON uses AES-ECB, a handler-specific key zero-padded/truncated to 16 bytes, PKCS#7 padding, then base64. Responses may be gzipped. Use `tools/gme_wire_test.py` rather than reimplementing this casually.

Find current GroupId/AES pairs in `gimuserver/gme/handlers/GmeControllerHandlers.cpp`. IDs and AES keys are not universally eight characters. A GroupId shown in a crash overlay can belong to a background poll instead of the feature just tapped.

For authenticated requests, `IKqx1Cn9` carries caller login information. `iN7buP2h` is the Gumi Live user ID; `h7eY3sAK` there is the local user ID. In `MissionStartInfo`, however, `h7eY3sAK` identifies the helper, not the caller.

KDL rules:

- All ordinary request/response and archive structures come from KDL; do not add local wire `glz::meta` structs to handlers.

- `[T]::size(1)` is a one-element array wrapper; `T` is an embedded object. They are not interchangeable.

- `u32::str` and similar types serialize quoted values; `::int` emits JSON numbers. Determine the representation from producer/reader/captures. An `addParam` overload alone does not prove encoding.

- Use specific types for actual numbers and flags. Keep genuinely packed/heterogeneous data as strings with documented grammar; do not use `str` as an uncertainty escape hatch.

- Bare vectors can emit `[]`; optional fields can be absent. Choose based on the client's operation and initialization semantics, not appearance.

- Shared shapes should share a KDL type. Shared hash keys across different classes or directions do not imply shared semantics.

- Use clean snake_case names; mixed hash-like names can be transformed unexpectedly by code generation.

- For scripted schema edits, use exact anchors or a parser, account for escaped quotes, regenerate, and review the output. Preserve pre-existing changes when repairing an edit; do not blindly reset a dirty schema file.

### Client list operations and initialization

| Block / case | Required understanding |

|---|---|

| `qC2tJs4E` unit list | Adds units absent from the cache; re-emitting an existing unit does not refresh it |

| `4ceMWH6k` full roster | Replaces the roster; fusion/evolution/stat mutations need the full current snapshot. Keep absent when no replacement is intended |

| `UT1SVg59` cleared missions | Merges clears; `readParam` @0x13FD758 adds without clearing |

| `yXNM8kL3` PermitPlace | Replacement snapshot; reconcile every represented unlock and active timed tier before emitting |

| `GV81ctzR` unit dictionary | Two lists: rows MERGE into `UserUnitDictionaryList` (Unit Guide), but row 0 REPLACES `UserUnitDictionaryReferenceList`, whose only reader is the summon lineup (`SummonsListScene` ctor, `existUnitID` @0x16D4200). Always send the complete dictionary (`gme::loadUnitDictionary`) |

| `bd5Rj6pN` item dictionary | Merges; `addObject` @0x12979D0 skips ids `getObjectWithItemID` already holds, so a full resend is safe |

| `9wjrh74P` warehouse, `eFU7Qtb0` keys, `dhMmbm5p` arms | Replacement lists (`removeAllObjects` at row 0); send complete state. `n5mdIUqj` summoner is a singleton `init()`ed before reading — send its one full row |

| Present box | Operation codes matter; a correctly shaped row with the wrong operation can do nothing |

| `r3D28bqW` milestones | Announcement queue drained by the client; each entry can trigger a popup |

| `PQ56vbkI`, `8jBJ7uKR` arena records | Singleton responses need initialized rows; empty arrays previously displayed uninitialized garbage |

These are class-specific findings, not universal rules for all lists. Read constructors and mutation operations. Do not equate omission, zero, and empty arrays.

PermitPlace is a discriminated shape: the entity-ID key also selects the entity type. Emitting all five ID keys in one flat struct lets the last one win. Existing code assembles this exceptional block separately; do not replace it with a guessed flat KDL object. Also do not duplicate its outer key when attaching the rebuilt fragment.

## 5. Persistence and handler discipline

Copy the current coroutine handler pattern, not the historical callback pyramid with a hardcoded user. Resolve the caller using `gme::getUserIdentity(...).nonEmpty()` and scope reads/writes to that identity. `getSoleUserId` is retired. Return appropriate handler errors; do not apply the old blanket instruction to turn database errors into empty success.

The SQLite pool is configured with **one connection**. Once a transaction owns it, all nested database helpers must use that transaction. The concrete MissionEnd hang was caused by `getClearedMissions(theDb(), identity)` inside an open transaction; passing `transaction` fixed it. Multi-await loops are not categorically invalid, but repeated queries and per-item round trips deserve scrutiny. Prefer batched operations and avoid cross-connection waits.

Keep persistent state minimal. KDL may model a large client surface without adding a database column for every field. Derive views from existing authoritative state. Migrations change durable schema; they are not a vehicle for one-off save edits or unexplained balance grants.

For a mutation:

1. Validate identity, ownership, requested IDs, and server-side costs before changes.

2. Use an atomic transaction when partial rewards or consumption would be inconsistent.

3. Update every affected client cache after committing the corresponding state. A successful SQL update is not a UI refresh.

4. Reconcile complete replacement snapshots. Updating only one newly bought parade can re-lock another tier when stale rows are re-emitted.

5. Test repeated calls and failure paths when those can duplicate rewards or leave partial state.

Client-supplied price fields are claims, not authoritative prices. Unit sale/fusion/evolution handlers must calculate cost from the appropriate game data. Mission result accounting still contains client-reported amounts; do not describe all rewards as fully server-authoritative merely because they are persisted by the server.

## 6. Build, test, and save protection

The played save is valuable. For an open client, take a consistent online SQLite backup using a read-only source connection and SQLite’s backup API. Stop the server before taking a byte-copy backup; check for WAL sidecars. Prefer wire tests against an isolated database/configuration on a different loopback port. Back up the current save immediately before testing, including progress since the previous session, and record the backup hash. An actively played live save can legitimately change during isolated tests; do not assert its hash stays fixed or restore it to make such a check pass. If the live save was actually used for a test, restore only while the server is stopped and only when doing so cannot overwrite newer user play. An untouched live save needs no redundant restore.

Control script: `tools/bin/bf_ctl.ps1 -Target server -Action status|stop|start|restart`. Parent/child processes can be legitimate; inspect which process owns port 9960. Stop processes holding the target executable before relinking. To keep a live client connected during builds, temporarily redirect CMake’s Debug runtime output to a separate test directory, then restore the cache setting after deployment. Stop only the identified test/live server PIDs; the control script’s stop action stops every server process. Hidden background launches should not open unsolicited console windows.

Build from an x64 Visual Studio tools environment. The inspected installation is:

```bat

call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64

set "VCPKG_ROOT=C:\Users\Evan\BF\vcpkg"

cd /d "C:\Users\Evan\BF\BF-WorkingDirRust"

cmake --build --preset debug-win64-debug -j 3

```

Use `vswhere` if the installation moved. Existing CMake configuration regenerates both `all.hpp` and `archive.hpp` with `cargo run --release -- generate --cxx --glaze`. Inspect the generated member names after a schema change. Keep parallelism at 2–3 to avoid paging/PCH failures.

A stale precompiled header has previously survived regeneration. If the generated member exists but compilation reports it missing, compare the `all.hpp(line)` the error cites with the struct's line in the current file. On September 10 they differed: configure regenerated `all.hpp` before ninja ran, but ninja never rebuilt the PCH, so a second build would have failed the same way. Deleting `out/build/debug-win64/gimuserver/CMakeFiles/gimuserver.dir/Debug/cmake_pch.cxx.pch` and `cmake_pch.cxx.obj`, then building without reconfiguring, fixed it. This is a known recovery, not a rule that every change always needs two builds. A running executable can also prevent the intended relink: compare the final EXE timestamp with the changed object files before trusting a test.

Verification must parse the entire response, reject duplicate JSON keys, check reference integrity and packed lengths, and distinguish emission success from client success. For randomized behavior test zero, full probability, no-eligible-drop cases, and a distribution sample; label the sample as a test, not proof of an exact population frequency.

## 7. Debugging client failures

- `deploy/log/http_log_<GroupId>_<timestamp>.log` holds decrypted request/response evidence when enabled in `config.json`. Inspect current-session files; logging may be reset at boot and historical notes warn about concurrent log handling.

- `deploy/log/dlc_404.log` identifies missing assets. Compare the requested URL with the real server file and client cache path.

- WER crash dumps may already exist under `%LOCALAPPDATA%\CrashDumps\BraveFrontier.Windows.exe.*.dmp`. Use `tools/bin/dmp_stack.py` and the APPX executable to identify the fault, then cross-map logic to APK symbols.

- No matching request log can mean a pre-request client crash. Example: a summon gate costing zero gems and zero friend points caused an integer division by zero while sizing the pull selector. Use the established nonzero test cost.

- Cache location: `%LOCALAPPDATA%\Packages\gumi.BraveFrontier_99p3jr0gh0z6w\LocalState\`. A fixed server asset and a stale cached asset can differ.

- An unregistered GroupId is answered "Unsupported request" with a Close command, and the client ends the session — it looks like a crash. Those replies are now logged (`LOG_ERROR` plus an `http_log_<GroupId>` file with an UNSUPPORTED line), because they cannot be decrypted without the key. Background polls are the usual culprit: Home and the Vortex home send **UpdateInfo** (`RUV94Dqz`) 30 minutes after launch (`UpdateInfo::isNeedUpdate`, 0x708 s) plus **NoticeUpdate** (`68pTQAJv`), and **UserLoginCampaignInfo** (`5fc8bf2c`) after 12 hours — all three were unregistered until 2026-09-11 and are now handled. Still unregistered and reachable from queued requests (`ConnectRequestList`): TrialDeckEdit `MbdL5D4K`, DbbBond `0EtanubR`, InboxMessageManage `rYSfaC4P` (only after an inbox action), ItemUseTimeLimit `4eCLR4rq`, VideoAdComplete `r234ydi1`, ArenaRetire `eyg8sA32`, and the raid/guild requests.

- `tools/asset_preflight.py` runs the resourceMap check below on demand, separating a missing LAYOUT (a null deref) from missing art (merely blank). As of 2026-09-14: **48 of 437 scenes cannot draw complete, 122 files**, almost all of them multi/guild/arena/colosseum. It also credits the 58 layouts bundled inside the Appx, which the earlier manual pass counted as gaps.

- **"Missing map assets" is not always a missing asset.** Grand Quest's black field map on 2026-09-14 was NOT a server gap: `CampaignFieldDownloadScene::downloadFiles` @0x14E9050 reads `CampaignMapMst::getMapImg` — a packed `<file>:<x>:<y>` list of four 640x960 tiles — and requests them from a HARDCODED `/raid/raid_map/` regardless of the `grand_map*` filenames. All 28 map rows' tiles are present there, all answer HTTP 200 with real sizes, and `dlc_404.log` is empty. What the CLIENT cache holds is the tell: `LocalState` had only map 80's four `grand_map801_*` tiles and 0/4 for every other map. So the files are served and the client has simply not fetched them — check the cache before concluding the server is short an asset.

- **A scene can be missing its LAYOUT, not just art.** `Assets/Resources/resourceMap` in the Appx client is `sceneId:file,file,…` — every file the DLC manager downloads before that scene draws (`DLCManagerComplementor::downloadBundleResourcesViaSceneId` @0x1EA8ACC, fetched as `/content/_dlcbundle/<file>`). Preflight it against `deploy/game_content/content` (any subfolder counts) and the gaps are exactly the popups and blank screens waiting to happen: on 2026-09-12 that was 114 files over 55 of 437 scenes, including `layout_unit_selector_gacha{,_details,_confirm}.csv` (the selector summon, scenes 90016/90015/90014) and `grand_quest_banner_light.png` (scene 1594).

- A layout CSV is `x,y,width,height,fontSize,customID` per row, LF, trailing comma (`GameScene::setLayoutParam` @0x1606904 strtoks them in that order); the screen is 640x960 with y growing downward, and `GameScene::loadLayout` @0x1606E0C uses the DOWNLOADED copy if `FileLoader::isFileAlreadyDownloaded`, else the client's bundled `Assets/Resources` (58 layouts ship inside the Appx). **A missing layout is a crash, not a cosmetic issue**: `LayoutCacheList::getObject` returns nullptr for an unknown name and `LayoutCache::getX` is `ldr s0,[x0,#0x30]` — a null deref. Author the missing file from the nearest sibling screen (the resummon screens are the selector's twins: same `scroll_layer`/`scroll_bar`/red-button widget set) and recover the element names from the scene's own `getObject` calls — but note **short names (<= 7 chars) are built from mov/movk immediates, not .rodata literals**, so a plain string scan misses them (`header` in the selector confirm scene; `hp_frame` and `lv_frame` in its details scene, the latter as `"hp_frame" | 0x604`).

- Before retrying a historical dead end, consult the relevant history section. Evan's recollection is a reason to look for another representation, not to conclude a mechanic never existed after one failed search.

## 8. Feature-specific facts worth retaining

### Units, fusion, and equipment

Second sphere slot: `eqip_item_frame_id2 = -1` means no slot, `0` means an empty unlocked slot, `1..14` means equipped sphere type. `UnitMixMainScene::mixUnitDoubleSphereCheck` @0x1C118F8 checks `"-1"`. Units begin with one slot; the Sphere Frog unlocks the second.

Imps update `ext_*`, not `add_*`; totals can combine base+add+ext while the orange imp display uses ext alone. Apply per-stat caps, gate BB behavior on `bb_id`, and rescale base stats after level changes. Refresh existing units through the replacement roster path.

`success_type` is 0 normal, 1 Great, 2 Super (`ConvSuccessSet` @0x1C200A4). Experience multipliers 1.5/2.0 come from DefineMst. The current 10%/2% probabilities, doubled for same-element materials, are authored policy, not recovered probabilities.

Unit/item capacity purchases (the shop's capacity screens and the Expand button on the Over Capacity warning) arrive as `ShopUse` type 2 (unit box) / 3 (item box) with the slot count in `5gXxT7LZ`. The 5-slot step is the `5.0f` the box scenes' ctors load; DefineMst `unit/item_box_ext_count` (1) is the gem price per step, and `max_unit_count`/`max_warehouse_count` (4000/3200) cap it. The client sums two capacity fields; the server keeps the starting 100 unit slots in `add_unit_count` (`kBaseUnitBoxSlots`) and bought slots in `user_info.max_unit_count`, while the item box keeps everything in `max_warehouse_count`. The server recomputes the price rather than trusting the claimed one.

Every gem button in the client is a `ShopUse` type, and the client changes no counter itself afterwards: `ShopUseConnectScene` only checks the reply, and the energy, orb and friend setters have no caller outside `UserTeamInfoResponse::readParam` and the mission/result scenes. So the `team_info` reply IS the purchase; a `{}` reply takes nothing and restores nothing. Implemented: 1 and 10 (fully restore Energy, `action_point_heal_count` gems; 10 is Quest Repeat's Auto Energy Recovery), 2/3 (boxes), 4 (Arena Orbs, `fight_point_heal_count` — `fight_point`/`max_fight_point` are the ARENA orbs, not the Hunter Orbs), 5 (Hunter Orbs, unchanged), 7 (+5 friend slots, `friend_ext_count`). Unhandled: 6 stamps, 8 Colosseum, 9 unit reset (`UnitDetailVirtuallyInfoScene`), 11 summoner item, 12 summoner remake, 103 Vortex Arena. Level-ups raise only energy, cost and FRIEND capacity (the only level-up strings are LV/VIT/COST/FRIEND_UP): UserLevelMst `friend_count` is the base (10 → 50 at lv 81) and its `add_friend_count` is the most that can be BOUGHT (0, then 50 from lv 81) — `ShopFriendExtScene` stops selling when team_info's sum reaches `friend_count + add_friend_count`. team_info `add_friend_count` therefore carries the bought slots (`user_info.max_friend_count`); sending the level's cap there, as before, gave them away and made the purchase unreachable.

### Missions, unlocks, and timers

**PermitPlace is THREE channels, not one, and that changes what "replacement snapshot" means.** `PermitPlaceInfoList` holds three disjoint partitions and each response clears only its own at row 0: `yXNM8kL3` calls `removeNormalObjects` @0x126C9B0, `Y73tHKS8` (PermitPlaceSp) calls `removeSpecialObjects` @0x126C744, `Y73mHKS8` (PermitPlaceML) calls `removeMLObjects`. Every remover resolves a row to its AreaMst — directly for an area row, through DungeonMst or MissionMst for the others — and skips rows that are not its own, testing `AreaMst::isSpecial` @0x1309278 (`(area_type | 2) == 3`, so type 1 or 3) or `isML` @0x1309364 (type 10). `area_type` is `3v1qg7Uj`, previously `unk2`. **A land or gate row resolves to no area and is skipped by all three, so nothing can ever withdraw one.** In this data 340 areas are type 1, 3 are type 3 and 35 are type 10, leaving 33 of 411 normal — so sending special-area permits on `yXNM8kL3` means they are appended at every login and every MissionEnd and can never be removed. A snapshot measured on 2026-09-13 was 659 rows, **443 of them special**, and an expired parade tier was among the rows that could not be withdrawn. Fixed the same day: `buildPermitPlace` now routes each row by its area type and emits all three keys, and `injectPermitPlace` splices all three (erasing the serialiser's empty SP/ML placeholders first so no key is emitted twice); the SP/ML fields were added to MissionEndResp, DungeonKeyUseResp and UpdatePermitPlaceInfoResp, which had only carried the normal key. Wire-tested: the permitted id set is byte-identical before and after, 216 rows now on the normal channel and 443 on special. **APPX confirmation still needed** — this is the progression gate, and only a client run proves unlocks still behave.

**An interrupted battle can now be resumed.** The subsystem is `5PR2VmH1` MissionBreakInfo plus two GroupIds that were unregistered (and so would have closed the session on use): **MissionContinue** `p8B2i9rJ` / `G3FwvQfy5hcxHMen`, fired by `MissionGameOverScene::initContinue` @0x17FB3C0 — the pay-to-revive prompt after a wipe, tracked as the analytics event "Revival in Quest" / "Gems" — and **MissionRestart** `IP96ys7T` / `0Zy3G9eD`, fired by `MissionRestartScene::initConnect`. The battle state is CLIENT-authoritative like Grand Quest's suspend data: `MissionRestartScene::initialize` @0x17FC7A8 reloads the blob from its own SaveData (`existMissionSuspendData` / `decodeMissionSuspendData`) and feeds THAT to `setMissionBreakInfo`. What the server owns is the TRIGGER — `LoginScene::changeNextScene` @0x174C544 only reaches the resume screen when `getState()` is non-zero (then `setLoginState(5)`, `existBattleMstFiles(serial)`, and scene 0x53; 0x3b6 mid-tutorial, Home otherwise). The old KDL had `j0Uszek2` backwards as "0=paused, 1=cleared": 0 means nothing pending, and the value echoed back is the MissionStatus the interruption carried (4 continue, 6 auto-continue, 5 restart). A revival costs DefineMst `continue_dia_count` (`QW3HiNv8`, 1 gem) — the same value `initContinueConfirm` shows the player — and the server charges it but never refuses, because `loopContinue` @0x17FB474 resumes on the reply regardless and nothing client-side touches the gem counter. The record is set by MissionContinue, left alone by MissionRestart (a resume does not finish a battle), and cleared by MissionEnd and by the next MissionStart. A stale record is safe: no local suspend data means `setMissionRestartFlg(false)` and the scene carries on.

MissionStart must consistently identify the requested mission in start info, serial state, and battle groups—even when borrowing fallback waves. A mismatched mission ID has crashed the client.

`PermitPlaceInfo` constructor sets enterable to 1 @0x126B3BC; explicitly sending zero locks it. Its remaining-time semantics differ from `PermitRecipeInfo`: zero can mean permanent on the former and expired on the latter. Read each consumer's comparison.

Parade purchases unlock exactly the purchased tier for its duration; current authored limits are 1800 seconds, with `active_tier`/`active_until` and `qY49LBjw` countdown emission. A CLOSED parade is sent enterable with no countdown, never `C1vG0iKh:"0"`: `AreaSPSelectScene::touchEnded` checks `isEnterable` (+0x5b4) before its key branch, so a locked parade opens the blank "Opening Conditions" page (`MissionFlgConfirmScene`, which lists only `need_mission_id` quests) instead of the key prompt. The key branch (+0xa1c) reads `getRemainingTime`: at least 1 enters, below 1 opens the key/tier prompt (+0xce8). The key path builds a fresh PermitPlace snapshot, including other active tiers and removal of expired or superseded missions; it does not reuse a stored login fragment. All 19 parade missions are authored locally.

MissionEnd unlock refresh was implemented and wire-tested on 2026-09-10: it emits clear history (`UT1SVg59`) plus a full current normal PermitPlace snapshot (`yXNM8kL3`). `gme/common/PermitPlace.cpp` is shared by UserInfo, MissionEnd, and DungeonKeyUse; the old login-snapshot cache is gone. Progression, current weekday rotation, and all active/expired parade tiers are rebuilt together. MissionEnd and key purchases build/serialize inside their transactions; forced failures verified complete rollback. Evan confirmed immediate next-mission unlocking in APPX on September 10. See `outputs/PERMITPLACE_REFRESH.md` and `work/permit-refresh/` in the Codex task for evidence.

MissionStart energy ordering was verified fixed on 2026-09-10: the handler now populates and serializes the complete response before consuming energy. The old code was reproduced charging 20 energy for a rejected Unholy Tower start. After the fix, all 12 malformed missions preserve energy and its refill timestamp; successful starts charge their authored cost once per request. This is not replay deduplication or a repair of the malformed visual records. Existing content checks and diagnostics remain in place; do not bypass them to make broken missions appear to pass.

MissionEnd now also emits the complete positive warehouse (`9wjrh74P`), full item dictionary (`bd5Rj6pN`), and current recipe permissions (`51yQrDBR`). Warehouse reader @0x14060E0 and recipe reader @0x13E9B70 clear their lists; partial snapshots lose unrelated entries. All reads use the reward transaction. UpdatePermitPlaceInfo is no longer an empty stub: its login-only request producer @0x13AFB58 now receives the shared current snapshot. Replaying tutorial mission 1 cannot lower a later tutorial checkpoint and re-arm mission 2’s five-gem grant. Malformed/unknown unit rewards now abort the complete reward transaction; positive decimal fields must parse fully and personality type must be 1–6. Item-reward strict validation and stack bounds were added September 14; optional extra-skill persistence remains a gap. See the task’s `outputs/EMITTER_BATCH_2026-09-10.md` for checks and play tests.

A second September 10 batch closed the remaining reward-path refresh gaps. No present-box, Rewards-menu or unit-operation scene calls a client warehouse or roster mutator, and HomeInfo rebuilds only the header and roster, so those payouts stayed stale until relaunch. PresentReceipt now sends new units plus the complete unit dictionary, the full warehouse and item dictionary, key inventory, summoner row and arm list — each only when a claim changed it. UnitSell/UnitMix/UnitEvo send the warehouse when `returnEquippedSpheres` hands spheres back; Mystery Chest, Brave Slots and Daily Spin send units and items through `gme::emitGrantedRewards`, and Daily Spin now also sends `fEi17cnx`. MissionEnd, Gacha and the selector ticket send the complete `GV81ctzR`. Present key grants no longer lower a count above `possession_limit`. Wire-tested on an isolated copy (each block equal to a following UserInfo and to the database; old/new UserInfo identical apart from the random first-read town roll); APPX observation is still needed. CampaignReceipt was left unchanged: its only request producer is `CampaignCodeRequestScene`, so its Grand Mission premise needs verifying first.

### Mimic chests — confirmed path and current policy

Treasure entries use `monsterOrder/chestType/params` (`MissionDropInfo::parseTreasureBoxDropInfo` @0x1266644). Type 1 is ordinary; `isMimic` @0x12699DC tests `!= 1`. Type 2 is the emitted and APPX-confirmed Mimic branch.

`BattleTreasure::create` @0x10E7AAC splits Mimic params on `:`. Token 0 is the battle monster group ID. One token is valid and creates no unit capture. If a second token exists, tokens 3–5 are read without an adequate length check; **never emit 2–5 tokens**. Tokens 3/4/5 are unit ID/level/type, token 6 optional extra skill; tokens 1/2 are unread in this function and their historical meaning remains unknown.

`MonsterParty::entryMimic` @0x10C218C enters the side group and flags its first member. It must contain one valid monster. It does not add an ordered battle wave. The false flag to `entryMonsters` bypasses normal unit/chest attachment @0x10C1FCC, preventing recursive host-chest drops. `MissionScene::initMimicAppear` @0x18064A8 handles the warning/effect/counter.

Asset support is present for all four known Mimic species. `GameScene::requestMonsterMstFiles` @0x1610A58 iterates every MonsterMst key and requests its assets; requestMissionFiles also iterates all monster keys. Keep group, monster, unit/visual, and AI references consistent.

Current rollout: optional shared `deploy/archive/mimic.json` supplies a **10% substitution chance after an ordinary chest successfully drops**, approved by Evan for all authored missions. Missions with no successful chest roll get no extra chest. Mission-local `random_mimics` can override the shared policy; chance 0 disables it. Existing mission Mimic species are preferred, otherwise the first catalog entry (basic Mimic) is used. Candidate selection is uniform. These rates, selection rules, and catalog balancing are authored policy.

Special-boss placement: `BattleMonsterGroupMst` positions are in a 640x960 space the client halves (`MonsterParty::entryMonsters` @0x10C1D10: ×0.5), and a boss atlas's `monster_cgg` offsets are drawn 1:1 from that anchor. Where the ART runs off a frame edge, that edge is meant to sit on the screen edge — Demon Abaddon's body is cut at its frame's left side, so its frame's left edge goes to x = 0 (`160:302`, not the builder's `180:302`), and a bottom-cut frame is lowered until the cut hides behind the boss HP panel (~y 186 of 480). Applied 2026-09-11 to the archive's special bosses by `tools/bin/fix_special_boss_positions.py` (`--dry` to preview; re-run after regenerating the archive). The builder still defaults every boss to `180:302` with placeholder stats; the AI archive has only the "Attack random" record, and enemy skill actions need the client AI grammar (`BattleUnit::setAiTargetList`, action "skill") decoded before bosses can cast. Dungeon stories: `DungeonMst.N4XVE1uA` = "start script,end script"; the client plays them from dungeon/area selection (`isFinishDungeonEvent` callers are the select scenes), not on the result screen, tracked in LoginInfo `N4XVE1uA` as `finishedEventId,type@areaEvent@landId@landFlag` groups.

The header Energy gauge has its own gem refill: `GameScene::touchBegan` shows ENERGY_RECOVER_GEM_MAX_THRESHOLD when energy ≥ DefineMst `eRQvzLeF` (absent from our data, so 0: it always said "max"), else ENERGY_RECOVER_GEM_RECOVER_ENERGY, then ShopUse type 1. None of the three ENERGY_RECOVER_GEM_* strings existed in any bundle. UserInfo, HomeInfo and MissionEnd now send a one-field DefineMst block `VkoZ5t3K: {eRQvzLeF: max energy}` (its readParam never resets the singleton), and the three strings were appended to `sgtext_en_338` in BOTH the server zip and the client's LocalState cache (same version, so it never re-downloads; originals backed up).

Fixed `mimic_chests` remain available for deterministic tests and take precedence on their host enemy. The guaranteed mission 10 fixture has been removed; the original stages and treasure rates are restored. The September 10 emitter now rolls an authored 80% capture chance independently of the 10% chest substitution chance. Successful captures emit `group:0:0:unitId:1:type`; failed rolls emit just the group. Type uses the existing MST appearance weights. `entryMimic` attaches BattleDropUnit @0x10C2314; `BattleUnit::initDead` forwards it to battle loot @0x10FD514; `BattleItemUnit::exitItemMove` forwards it to BattleRewardList @0x1049920. Wire tests cover all four species and MissionEnd persistence; APPX capture/result/roster behavior still awaits Evan’s confirmation. Optional extra-skill rewards remain unimplemented.

### Friends and future simulation

The existing synthetic helper is `DecompFriend`, with data based on the player's leader (fallback highest-level unit). Its reserved user-unit ID is 999999, avoiding a collision with real player units. The helper picker and Social list use different caches: FriendGet needs both `xZH6EIQ7` ReinforcementInfo and `tojMy68W` FriendInfo. `T_FIXED_REINFORCEMENT` is a third, distinct path. Do not populate only a friend-detail singleton and expect the picker to update.

Honor (the client's name for Friend Points, `FRIEND_POINT^Honor`) was built end to end on 2026-09-13; before that it could never be earned. The sink had been complete for some time — gacha_category_mst row 2 (`small_banner_03Honor.png`) points at gate 1000, gates 1000/1001 cost 200 `J3stQ7jd`, Gacha charges `user_info.friend_points`, and gate 1000 has a 33-unit pool — so the Honor Summon could only ever answer "Insufficient Honor Points". Two things gated the display: `ReinforcementInfo::getFriendPoint` @0x126DDA8 returns 0 outright while THAT ROW's own friend_point is below 1 (stored XOR-obfuscated: `setFriendPoint` @0x126DEE4 keeps a random byte at +0x30c and value^rand at +0x308), and past that gate it ignores the row and reads the `6e4b7sQt` FriendPointInfo singleton — `4vamo28W` when `FriendInfoList::existTypeOK` says the helper is a friend, `KgbM81wd` when not. FriendGet sent 0 for the row and nothing ever sent the singleton, whose ctor @0x1260DD8 defaults both to `"0"`, so every helper card read "Honor +0". Both amounts are comma-separated LISTS indexed by `getNormalPointNum(int)` @0x1260EF8, and every helper card uses index 0. `existTypeOK` @0x12607CC is also where friend_type is pinned down: it returns true only for a row whose type is exactly 1. UserInfo and FriendGet now send the singleton; FriendGet's row carries the gate. MissionStart records the borrowed helper in `user_info.reinforce_user_id` because MissionEnd's request never names one (`MissionEndRequest::createBody` @0x13A78F8 sends 75 keys, none of them `h7eY3sAK`), and MissionEnd credits it on a clear only (`j3g5P4cq` 2; 103 of 104 captured ends) and clears the field whatever the outcome, so a loss, a solo run, or a replayed end cannot pay. `MissionStartInfo::getFriendPoint` has zero callers — that echo is unread. The amounts (50 friend / 10 stranger) are AUTHORED: DefineMst has no Honor field and the help text says only that a friend is worth more. `friend_p_get`/`friend_p_use` are new `user_team_archive` columns, which lights up trophies 100090/100100 (grades at 5,000/30,000/100,000) and achievement band 2000, "N Honor Points Accumulated" — the band Achievements.hpp had been leaving at -1 for want of a lifetime total.

User-requested future direction: multiple level-threshold-scaled synthetic friends, followed by simulated Arena and Colosseum opponents/progression. This is a roadmap, not implemented functionality. Stabilize existing emitters first; then derive friend stats from real unit/level tables and establish each competitive mode's request/result/rank semantics before adding persistent progression.

### What is built but not yet seen working

`tools/CLIENT_TEST_LOG.md` is the standing checklist of features that are

wire-tested and **not client-confirmed**, ordered by blast radius rather than by

interest. A wire test proves the bytes; only the client proves the screen

redraws, which is the entire point of the emitter campaign. Add to it whenever

something ships without an observation, and record FAILs with what was done and

what was seen — the evidence rule in §1 applies to negative results too.

### Emitter inventory from the September 13 sweeps

The following sweeps are historical inventories, not completeness proofs. Missing data blocks some features; implementation and consistency bugs can remain in already-emitted paths. Re-run and inspect the relevant producer/consumer before classifying a gap:

- `tools/unsent_key_sweep.py` — channel 1, `getResponseObject`. **221 sent, 7 modelled-unsent, 264 unmodelled.** It credits `ServerCacheMst.hpp`'s `auto_cache(key, Mst)` as delivery: 55 tables reach the client that way and never appear in `all.hpp`, and counting them as gaps overstated the backlog by a third. It now excludes keys handled INLINE at the tail of that function rather than by a `*Response` ctor; `QxWZoA04` is one, a field of `AhSmZF07` that UpdateInfoLight has always sent, and the old heuristic filed it as never-sent forever.

- `tools/bodytag_sweep.py` — channel 2, `parseBodyTag`, new. **73 tags, 20 never sent, and none of the 20 has a data file behind it.** Watch one trap it prints: a tag and its channel-1 key can differ, so "no data" there is not the whole answer — UnitSkillMst's tag is `5hafnym7` but our dump sits under `8aiBoHg5`.

- `tools/handler_refresh_audit.py` — handler DB writes vs. the fields their reply carries. Now carries a `NOT_A_GAP` table of the client-authoritative handlers with the reason each one is correct, because without it the report is mostly noise and each session re-investigates the same files.

Of the 7 channel-1 gaps, two are **not gaps at all** and should not be "fixed": `2375D38i` FeatureGatingInfo is the level-gate catalogue, and `FeatureGatingHandler::shouldGateLocked` @0x1C87BDC returns 0 when its list is empty — so sending it would ADD padlocks to features that are currently open. `yNnvj59x` NpcMessageOverwriteInfo has zero readers anywhere in the binary; its list is written by `readParam` and never touched again. The rest are blocked: notices (`F_NOTICE_INFO_MST` was never dumped), gifts (needs a friend system), IAP, and release info (no `FunctionReleaseMst` data).

**UnitSkillMst is the one table with data and no delivery, and it is the argument for channel 3.** `skill_mst.json` holds 33,381 rows under `8aiBoHg5`, one of the two `UnitSkillMstResponse` keys, and its 20 columns match `readParam` exactly bar `6E2fGPWT` setRange. `UnitSkillMstList` has 251 references — `BattleUnit::setUnitUltimateSkill` @0x10F1E60, `DLCManager::requestMonsterAttackEffectFiles`, `ChallengeArenaRivalUnit::initialize`. When the lookup misses, `cbz x23` skips the `BattleUnitSkill` construction entirely, so an absent table costs the skill rather than crashing. But the file is **19.2 MB**: it cannot ride a login reply, which is exactly what the version/download channel exists for. A per-battle subset on MissionStart is the other option, and it touches battle data, which has crashed the client before — treat it as its own piece of work with a client observation attached.

### Rewards, tutorial, and local assets

The Rewards button leads to multiple native systems, not one handler. The historical “Rewards is unbuilt” summary is obsolete. Brave Slots are native, use Brave Medals, take picture IDs rather than reel indices, and use packed prize data `"<targetId>@<count>"`. The client prize presentation cannot simply be sent an empty/no-prize result. Read the topic history and KDL before changes.

Daily Spin fields can be reward IDs, not day numbers. A limit increase alone does not grant spins; a wrong state transition can block Home. Journal `setValue` means point reward, `setProgress` means target, and `setClaimStatus` means a reward is waiting in the audited class. Bulk claim shapes differ by screen.

V2 summon-ticket user info has **both ID and amount**. Older claims that `Rs7bCE3t` is fictional, or that each row means one ticket, were superseded by the later reader audit.

Badges come from the server, never from client-side counting. The Rewards button and the Presents / Mystery Chest tiles read `present_count` (`EfinBo65`) and `mysterybox_count` (`Qo9doUsp`) off team_info (`HomeScene2::setLayoutControl`, `RewardsTopScene::checkForBadgeAlert`); `gme::getTeamInfo` counts unclaimed presents and open chests, so every header refresh updates them. The key-ready badge is `BadgeInfo.dungeon_key_num` under `h23iRjGN`: `BadgeInfoResponse::readParam` has no reset and is its only writer, so every key reply re-sends just that count. `CampaignReceipt` (5Imq3wC0) is the Serial Code screen (`CampaignCodeRequestScene`; the request carries only the typed code, `pCIRMw04`), not Grand Quest rewards.

Unit Selector (k57TdKDj): the request's `C5QbG2DM` is the TYPE the player picked on the confirm screen (`getSelectedUnitType`, button index + 1 = UnitTypeMst id 1–6), and `H6k1LIxC` is the constant "1"; the handler now grants that type. The selector ticket inventory is `CGHaOZda` (UnitSelectorGachaUserInfo: `XIvaD6Jp` ticket id, `H6k1LIxC` count) — the same key as the request group, NOT the V2 ticket list (no V2 ticket targets a selector gacha). Its readParam clears the whole map at the first param of the first row, and an empty list never reaches readParam, so every send must be the complete inventory and a spent ticket must go out with count 0; no client code changes the count locally. The inventory is `user_selector_tickets` (ticket id = the selector MST's `XIvaD6Jp`, its `setTicketId`): tickets arrive as **present type 8005** (`PresentCommon::createPresentName` sends 8000–8005 through a second jump table at 0x232822C; 8005 names the present after the selector whose ticket id equals target_id), a pick spends one of that selector's tickets and is refused at 0, and UserInfo, the 8005 claim and the pick reply each send the whole catalog with counts (0 included) through `gme::loadSelectorTickets`. The MST's required-tickets field (`JRqU2bS6`) is absent from the data; the client defaults it to 1 and clamps it to ≥ 1, so the pull-count division is safe. Debug CLI: `selector` lists, `selector <id|all> [n]` queues 8005 presents.

Grand Quest (the client's "Campaign") runs end in **CampaignEnd**: status `j3g5P4cq` 2 = cleared, 3 = failed or abandoned (`CampaignResultInitScene::initConnect` @0x154C3BC; the field's give-up also sends 3). Only DeckEdit, Save and End name the mission (`2I9V0o6J`); BattleStart/BattleEnd do not, so the server keeps `user_campaign_state.active_mission_id`. A battle is not a clear: BattleEnd only records the run's archive (`wVTBA6b5`, cumulative zel/karma). A clearing End pays the archive's zel/karma (sent as a two-field `F5Vs19mb` — the full MissionRewardInfo would zero `before_lv` and fake a level-up) plus every F_GRAND_MISSION_REWARD_MST row earned: type 4 on the first clear, 1 when completion reaches the row's percent, 2 when the run set the row's flag; completion = the percents of the flags in `RseDpY04.JcKMjH64` (every mission's flag percents sum to exactly 100). init_flag 1 rows pay once per user and go back as the mission's `get_reward` (`JQ23rIvk`, a CSV `CampaignRewardScene::isGetRewardCheck` splits); init_flag 0 rows pay on every clear. Zel/karma/gems are credited directly (the result scenes count them up off team_info, and `bonusUpdateHeader` adds each back); units, items, keys and medals go to the present box. `p04iC2wr` is the EARNED list only the result screens read (the catalog is the client's own CampaignRewardBonusMst), so CampaignStart no longer sends the catalog under it; an empty list cannot clear an older one (readParam clears at row 0), so a clear that earns nothing can still show the previous run's bonuses. A run pays once (`run_open`, closed by End). The Grand Quest item loadout (`Ckf3Zx7E`/`NjZ6ds1S` equipped, `5EByfWJ4` reserve) lives outside the warehouse: CampaignItemEdit mirrors the client's local warehouse move by the per-item difference, Save/End store the list as it stands. Suspend data stays client-side (`campaignEncodeSuspendData`); Save and Restart just acknowledge. Guest party members' guest ids are not stored. Wire-tested 2026-09-11 (`test_grandquest.py`); no Grand Quest has been played in the client yet. Frontier Gate's retire result is `mu0kXAlV` (FrontierEndInfoResponse: `Cdv07KEU` grade, `Najhr8m6` zel, `HTVh8a65` karma, `TPR79fyI` score, `idfCDG70` achieve points, `5Z1LNoyH` bonus rate, `sc83dkh3` prestige) plus `QXFCkE67` (FrontierResRewardInfo).

The Music House works, and the note that said it was not worth persisting was wrong. `d98mjNDc` UserSoundInfo is a full-replace LIST (`UserSoundInfoResponse::readParam` @0x1401614 calls `removeAllObjects` at row 0), read by `MyTownSoundRoomScene` and `SandbagChangeSoundScene` — it had been modelled as a one-element wrapper and never filled, so every launch replaced the jukebox with nothing and tracks the player had paid Zel for were gone. The purchase is client-local: `confirmAnswerYes` @0x1902514 reads `SoundMst::getAmount` (`Rs7bCE3t`, 1,200–98,000 Zel across 108 rows), pays with `UserTeamInfo::getZel/setZel`, adds the row to its own list, and queues a TownUpdate carrying the new ids as a comma list under `LzKDI2i7`. The server charges the Zel itself when it records them — the client's decrement is local and TownUpdateResp's own contract is that the next UserInfo carries the authoritative Zel, so persisting the track without charging would make every track free after one relaunch. Replays are not double-billed (already-owned ids are skipped, which matters because the client resends its whole `getBuySoundList` until the flush lands) and an id absent from SoundMst is refused. `content/mytown_sound` is present, so the screen draws. This also answers achievement band 14000, "Bought a Song from the Music House", which had been left at -1 for want of a music house.

Tutorial status is a chapter ID; its script mapping is not identity. Forced tutorial navigation may skip setup normally done by a UI click. Gate 2000 is the tutorial summon. Check fresh-save provisioning separately from the played save.

Summon gate scripts (`content/gacha/gWait*` and `gChange(*)`): each `PARAM id=N` line is `ScriptEngine::advParam(N, param)` @0xFC7708. `id=20` = `clearScreen()` (hides the message window, REMOVES every script-drawn node, disables the touch prompts); `id=21 x:y` = `setTouchAnime`, which only ever ADDS a `Touch.sam` at z 200 to a list nothing but `clearScreen` empties; `id=42 name:x:y:a.sam:b.sam` = a named anime at z 30 and `id=43 name` disables and drops it. Stock ends each Wait/Change with `id=20` after the tap, which on this server leaves a white screen; removing it (2026-09-06) left every prompt up until the unit appeared, two on a changing gate. `tools/patch_gacha_scripts.py` (idempotent; patches the server content AND the client's LocalState copy) drops `id=20`, draws the prompt as the named anime `touch_prompt` (`Touch.sam`, same position) and removes it with `id=43` right after MSGWAIT — TOUCH shows, the tap clears it, a changing gate repeats that. Script `type=` opcodes and `PARAM id=` operations are distinct vocabularies. The content is gitignored, so re-run the patcher after a fresh content drop.

Frontier Gate runs are paid at their end, in the result scene (`FrontierGateResultScene`, scene 0x66A), reached two ways: **Retire** (FrontierGateEnd; the interval scene changes scene on the reply) and a **lost battle** (FG sends MissionEnd win or lose; `j3g5P4cq` 2 = won — `FrontierGateBattleEndScene::updateEvent` only leaves when `FrontierGateComInfo::getResult()` is set, which only `mu0kXAlV` does, so a lost battle's MissionEnd must carry it or the client sits there). The run's floors and score survive between battles only when the server hands them back: `FrontierBattleInfoResponse` (`eIQ79KO2`) is the sole setter of the run's progress / running score / support, so MissionStart sends it (zeros on a new run) and MissionEnd stores and echoes what the client reported. A floor is a wave: a gate's floor rewards top out at the sum of its missions' battleCount. Rewards are `F_FROGATE_REWARD_MST` (ported as `frontier_gate_reward_mst.json`; no wire key — the client loads it itself), ONE-TIME per gate: cond_type 1 by floors, 2 by score (8001 is FG+); the gate's reward list marks a reward obtained when its id is in `FrontierGateInfo.reward_info_list` (`JQ23rIvk`), kept as `user_frontier_gates.rewards_got`.  They pay THE MOMENT a battle reaches them, not at the end of the run — starting a gate again abandons the open run, and a first live test lost a 125,874-point run's rewards that way.  The run's own list (`run_rewards`) is what the result screen shows. Zel/karma/gems are credited, everything else goes to the present box. The result is `mu0kXAlV` (grade "1".."7" = D..SSS, drawn with Frontier Hunter's art and graded here on its F_CHLNG_MISSION_GRADE_MST ladder; merit points = run score / 100, a DECIDED rate with no data behind it; bonus rate 1 = none) plus `QXFCkE67` (reward_type 1 "Stage N Cleared!!", 2 "Esteem Reward"). See `gme/common/FrontierGate.hpp`.

Hunter Orbs were wired on 2026-09-13, and the field identification that had been left to a probe is now settled from the binary. They are NOT `team_info.fight_point` (that is the Arena Orbs); they are `ChallengeHeaderInfo::Aube`, and the only writer is the `kN2i7qds` ChallengeUserTeam response. `ChallengeUserTeamResponse::readParam` @0x13D3F94 names all five of its setters in the open, so the three fields ChallengeBase had been filling with probe placeholders are `38sHatGk` = setAube (the count), `6mh0jiKJ` = setAubeTimer (+0x44, whose getter has NO callers — write-only) and `RHKA30s5` = setAubeRestTimer (seconds to the next orb). The earlier 4/5/6 probe was inconclusive only because every value sat above the cap. **The client runs the regeneration clock itself**: `decFightRestTimer` @0x1258270 ticks the rest timer down a second at a time and, each time the remainder divides by DefineMst `recover_time_frohun` (`3xAsgHL8`, 10800s), adds an orb and reloads, stopping at `max_frohun_p` (`SziK4Xg1`, 3) — so the server sends a starting state, not a running clock, and `gme/common/HunterOrbs.hpp` derives it from `user_info.hunter_orbs` + `hunter_orb_rest_ts` the way UserEnergy derives from `energy_full_ts`. Spending is split by the binary, not by convention: `MissionStartScene::initConnect` @0x1823358 debits client-side but only while `FrontierGateUtils::nowFrontierGate` is FALSE, so the Frontier Gate charge is the server's — taken at FrontierGateStart (refused at 0, as a clean `{}` so the session survives) and at FrontierGateRetry (which does NOT refuse, because stranding a player mid-run is worse than a free retry). ShopUse type 5 now refills the real counter instead of fight_point. With the count permanently 0 beforehand, `FrontierGateConditionScene::startCheck` @0x15DE978 took its no-orbs branch on every single entry.

**Frontier Hunter is enterable as of 2026-09-14, and it needed no battle machinery of its own.** `ChallengeLobbyScene::missionScene` @0x155C45C hands off to `MissionSelectScene2` with `ChallengeBase::getDunMst()` — the ordinary quest-select screen — so the mode reuses the whole existing mission pipeline; `ChallengeStartRequest` exists but has zero callers. All it ever needed was its topology in the permit list. ServerCache now collects the ACTIVE challenge only (event 97 → land 99, area 1000010, dungeon 1009700, missions 1009700-1009703), because all 97 rows expired in 2022 and permitting the rest would put ~380 unreachable missions on the map. That area is AreaMst type 1, so the rows ride `Y73tHKS8` and can be withdrawn when the event changes. ⚠ None of the four missions has authored battle content, so they take MissionStart's mission-10 template fallback — enterable and winnable, but not the real waves. **The general lesson: before gating a feature off because "the button behind it would crash", xref the request class's CONSTRUCTOR, not just the registration table.**

`ChallengeUserInfo` (`jF3AS4cp` / `Nst6MK5m`) was an unregistered GroupId that actually killed a live session — deploy/log has it at 2026-09-13 07:28:53, right after a DungeonEventUpdate whose `9yVsu21R` list ends in "challenge," (the Frontier Hunter intro), with a relaunch thirty seconds later. It is registered now and answers `s7A3bGLe` (eight setters across ChallengeInfo and ChallengeHeaderInfo, both singletons, one full row) plus `kN2i7qds`. Its `j0Uszek2` frohun_stat is **1** since 2026-09-14. It was 0 for a day on the mistaken theory that a lit Enter button would fire `ChallengeStart` (`sQfU18kH`, unregistered) and move the session kill one tap further in — but `ChallengeStartRequest` has **zero callers** in the binary, so nothing can fire it. `btnSetSt` @0x155B904 pins one more value: 3 is the ended-with-rewards phase. None of this affects Frontier Gate, whose entry check reads the orbs and never looks at frohun_stat.

Merit Points are the Randall achieve-point currency, and `idfCDG70` is the BALANCE, not an id: `RandallAchievementDedicateScene::setAchievePoint` @0x1A39D58 prints `UserAchievementInfo` +0x18, which is that key (so UserInfoResp's `Bnc4LpM8.id` is the balance — `user_info.achieve_point`, capped by DefineMst max_achieve_point 1JFcDr05 = 999,999).  Nothing in the data says what a Frontier Gate run is worth: the gate MST's `Q1z2IFjZ` is fed to `ChallengeBattleSetting` as client-side SCORING rates (`GameUtils::setFrontierGateSetting` @0x119E5BC — spark/overkill/multikill/bb-kill/weak/total-damage rates, one- and three-turn bonuses, crystal rates and drop percents), and `RlFWcCqN` FixPt is empty on every row and belongs to the gate's conditions screen.

Grand Quest's entry point is a PERMIT, not progression.  `GateScene::campaignBtnSet` / `LandScene::campaignBtnSet` draw the icon only when the feature switch `bf_campaign_grand_quest` is 1 in the featureCheck reply AND `GameUtils::checkGrandPermit` @0x11A767C is true.  That walks F_GRAND_MISSION_MST, looks each mission's DUNGEON up in PermitPlaceInfoList and returns `isEnterable()` for the one whose place id is the literal `"5000000"`.  The dungeon id is not in the Grand Mission table at all — `CampaignUtils::missionMstReflect` @0x11E4F98 copies it (plus name, description, scripts, AP) from the like-numbered F_MISSION_MST row, so a Grand Mission needs its twin there.  Grand ids sit above kSpecialIdFloor, so the Grand Gaia gate skips them; `ServerCache::grandQuestPermits` collects the land/area/dungeon/mission set and PermitPlace emits it.

The summon rail is `gacha_category_mst`: a category's `3rCmq58M` is the gate id it opens and `In7lGGLn` is only its art, so a banner can wear selector artwork while opening something else entirely (test gates 99007/99008 did, which read as "the selector banner is broken").  A selector banner must point at the selector's own gate — `UnitSelectorGachaMst.gacha_id` (7Ffmi96v), e.g. 20154 for the Brave Burst ticket — because `SummonsDetailScene::isUnitSelectorGachaTicket` @0x1D8E880 is just "this gacha has selector MST rows".

The Grand Quest icon is only the first of two gates.  Inside the mode, the series banners come from AreaMst (area_type 1) plus the permits, but a series' QUEST TILES come from `CampaignMissionSelectScene::setDungeonMstList`, which drops every Grand Mission that has no `CampaignMissionInfo` row (`CampaignMissionInfoList::getObjectMissionID` @0x152E304, null -> skip), whose dungeon_type is not 4, or whose dungeon permit is missing.  So **CampaignMissionGet / CampaignStart must return the WHOLE F_GRAND_MISSION_MST catalogue**, unplayed rows at zero, or the player sees series with nothing inside and no way in — which is what an empty `2I9V0o6J` did until 2026-09-12.  In that list `j0Uszek2` (state) has no client reader at all; only 2 means anything server-side (UserInfo's cleared set), and `JcKMjH64` (mission_on_flg) is a comma-separated FLAG LIST fed to `CampaignMissionEventInfo` (`CampaignSceneBase::setMissionOnFlg` @0x14D9F54), not a boolean — it stays empty while nothing tracks field quests.  `PermitPlaceInfo::getRemainingTime` @0x126B548 returns **-1 when no time limit is set**, and both tile filters accept -1, so dateless permit entries are fine.

The selector summon is a separate three-scene flow, not a button on the summon gate.  `SummonsDetailScene::initSummontickets` @0x1D8E304 fills the scene's vector with every `UnitSelectorGachaMst` row whose `gacha_id` matches the open gate (and a parallel one for `SummonTicketV2Mst.target_gacha_id`); `setGachaInfo` then prints "<requiredTickets> <ticketName>" as the cost instead of gems, and skips the "use tickets" checkbox entirely.  Tapping the summon button with that vector non-empty hands the rows to `UnitSelectorGachaUnitObjManager` and calls `changeSceneWithSceneID(90016)` — the unit grid (5 per row, `iconWidth+2` apart, `iconHeight+15` per row), then 90015 for a unit's details and 90014 to confirm, where the radio buttons pick the unit TYPE that becomes the request's `C5QbG2DM`.  The three scenes' layouts were missing from the content dump (see section 7) and are now authored under `deploy/game_content/content/_dlcbundle/`.  `force_use_summon_tickets` in features.json is NOT this flow — it forces the V2-ticket checkbox off and is read by `setGachaInfo`/`touchBegan` only.

**The present_type vocabulary, decoded in full (2026-09-12).**  `PresentCommon::createPresentName` @0x11C37F4 dispatches through TWO jump tables plus three explicit compares, and that is the authoritative list of what a present can be: table 0x2328200 indexed `type - 2` covers 2-23, table 0x232822C indexed `type - 8000` covers 8000-8005, and 10001/10002/10003 are the Challenge Arena's gold/silver/rainbow coins.  **Types 10 and 8003 land on the unhandled branch (0x11C44C0) — the client has no name for them**, which matters because 2,462 Frontier Hunter reward rows use type 10; it is never shown as a present.  The 8000 band: **8000 = the plain summon ticket** (loc key SUMMON_TICKET, the one counter in `user_info.summon_tickets` that team_info carries), **8001 = a V2 summon ticket** (named from SummonTicketV2MstList by target_id), **8002 = a guild token** (GUILD_TOKEN_SINGULAR/PLURAL — a NAME only: no balance accessor exists anywhere in the binary, so 198 Frontier Gate rows of it stay unclaimable until the guild feature has somewhere to put it), **8004 = an event token** (`EVENT_TOKEN_%04d_NAME`), **8005 = a Unit Selector ticket**.

**Summon tickets are two economies, not one.**  The plain counter is spent by the "use summon tickets" checkbox and checked against `UserTeamInfo::getSummonTicket` (@0x16A7F1C).  The V2 tickets are a per-TYPE inventory (`SummonTicketV2UserInfo`, wrapper `a3d5d12i`, id + amount), each type redeemable on the one gate its `SummonTicketV2Mst.target_gacha` names.  **That inventory is load-bearing**: `SummonsDetailScene::touchBegan` @0x16A7FA8 walks `SummonTicketV2UserInfoList`, sums the amounts of the types targeting the open gate, and refuses the pull when the total is zero — so a ticket the box paid could not be spent while the list was absent (it was: the table `user_summon_tickets_v2` had existed since a May migration with nothing reading or writing it).  `readParam` @0x1C6EBB4 clears the list first, so the send is a full replace and a type spent to zero must keep reporting itself.  GachaAction's `324b023k` now actually pays: V2 types for the gate first, the plain counter as the fallback, and the currency cost is skipped when a ticket covers the pull.  ⚠ No ticket gate has an authored pool in `deploy/archive/gacha.json` (only 1000, 2000, 11960 and the four test gates do), so the Brave Summon Ticket (id 8) is retargeted from its own dead gate 17160 to 11960 "Rare Summon: All Star" — a substitution, like the missing banner art.

**Event tokens.**  `EventTokenInfo` (wrapper `l234vdKs`) is the per-event currency balance, field-mapped 2026-09-12 from `EventTokenInfoResponse::readParam` @0x1C49274: `Slkc395l` token id (read StrToInt then IntToString, so "0008" == "8"), `s35idar9` name, `qp37xTDh` description, `lDk4hv20` amount, `fE2d6ivS` timeleft (the one setter that survived inlining — `setTimeleft` @0x1C7E504 — which is what fixes the numeric pair), `0Dk4fc81` dungeon ids, `pn16CNah` unit ids.  Token **8 is the Rift Token, the Frontier Gate's own currency**, and 505 F_FROGATE_REWARD_MST rows pay it.  No F_EVENT_TOKEN_MST was ever dumped, but the client names all of them itself: `deploy/mst/event_token_mst.json` is generated by `tools/gen_event_token_names.py` from `sgtext_Exchange_Token_83.zip` (96 names, plus four ids the Frontier Gate uses that the archive skips, filled with their band's name).  The f49als4D handler answers with the player's balances instead of `{}`, and the box pays type 8004 into `user_event_tokens`.  Not built: the exchange itself — `EventTokenExchangeInfoRequest` exists and the shop's 61 item names are in the same sgtext, but no exchange MST was dumped, so there is nothing to spend them on yet.

**Frontier Gate currency payouts** now go straight in rather than to the box: event tokens (8004) and both ticket kinds (8000/8001) join zel/karma/gems as `present_flg 0` in the result list, and MissionEnd / FrontierGateEnd carry the refreshed `l234vdKs` / `a3d5d12i` so the balance is not stale until the next login.

**Three more emitter gaps closed (2026-09-12).**  The method that finds them: diff what a handler WRITES against what its reply CARRIES, and separately list the response keys `getResponseObject` knows (504, audited in tools/ida/audits/getResponseObject_FULL.txt) against the keys this server ever sends — 247 are never sent, and the ones that matter are the caches the client can only learn from us.

  * **The BONUS item bar.**  The quest-prep screen has two item bars: the battle items (`71U5wzhI`) and the bonus slots (`nAligJSQ`, UserEquipBoostItemInfoResponse @0x13FE0D8 filling UserEquipBonusItemInfoList — item_id / disp_order / item_num, full replace).  ItemEdit received both and persisted only the first, and UserInfo reported only the first, so a bonus pick never survived the screen.  It is not cosmetic: `MissionStartRequest::createBody` @0x13AB3C4 carries that list into the fight and `GameUtils::createUserWarehouseInfoList` subtracts it from the warehouse listing.  Now stored in `user_equip_bonus_items` and reported beside the battle bar.

  * **The town header.**  TownFacilityUpdate clamps the reported payment to the available karma. This prevents a negative balance but does not independently validate the requested upgrade or its cost — but the reply carried only the recipe list and the tile details.  It now carries `fEi17cnx` too.

  * **Trophy grades.**  `PlayerInfoBattleResultScene::getTrophyGrade` @0x1799F34 does NOT derive a trophy's star tier: it asks `UserTrophyGradeInfoList::exist(gradeId)` for each of that trophy's F_TROPHY_GRADE_MST rows and takes the highest present.  We never sent `H18CjPKI`, so every trophy on the Records screen read grade 0 no matter how high the counter beside it ran.  `gme::loadTrophyGrades` derives them from the archive counters (31 trophies of groups 100/200 map onto counters we already keep; the dictionary trophies 100110/100180 come from the dictionaries) and GetPlayerInfo sends them.  The mode trophies we do not track (arena/raid/guild/colosseum) are left unattained rather than faked.

**The summon screen is TWO tables, and a gate needs a row in both (2026-09-13).**  `IBs49NiH` GachaCategoryMst is the rail tile and `1IR86sAv` GachaInfoMst is the gate; `SummonsTopScene::getGachaInfoList` @0x16DD398 intersects them, keeping only the GachaInfoList rows the current category's `GachaCategoryMst::hasGachaID` @0x1C8BD04 accepts.  Three consequences, all reported from the client as separate bugs and all the same shape:

  * **`3rCmq58M` is a COMMA LIST, not one id.**  `setGachaIDList` @0x1C8BBC0 splits it with `CommonUtils::parseList` on "," into the vector `hasGachaID` searches.  That list is what the left/right arrows on `layout_summons_top.csv` page through - every category here held exactly one gate, so nothing was ever scrollable.  The four test gates are now one category of four.

  * **A category naming a gate with no GachaInfo row opens an empty pager.**  Both selector banners (20154, 19790) did exactly that: `deploy/archive/gacha.json` is what `GachaArchiver::populateAllPackets` emits, and neither gate was in it.  All 16 selector gates are now archived.

  * **`JukkSeNA` (the selector CATALOGUE) was loaded at boot and never sent.**  This is the one that made "selector summons are not working" a *silent* failure rather than a visible one.  `SummonsDetailScene::initSummontickets` @0x1D8E304 fills the scene's selected-selector vector by walking `UnitSelectorGachaMstList` for rows whose `gacha_gate_id` is the open gate, and `touchBegan` @0x16A73FC routes SUMMON to the unit picker (scene 0x15FA0) only when that vector is non-empty - otherwise it falls through to the ordinary PAID pull.  `SummonsScrlItem::addObject` @0x16D9F70 reads the same list for the ticket count on the tile.  UserInfo and GachaList now both send it.

Selector banners are added per player rather than authored into gacha_category_mst: `gme::selectorGachaCategories` emits a tile for each selector ticket the player holds, so a banner appears with the ticket and goes when it is spent.

**The party marker on the Grand Quest map, and the ten tables nobody was sending (2026-09-13).**  The Grand Quest master data reaches the client from exactly two places - `DataMstManager::loadCampaignMsts` @0x1213608, which reads a locally CACHED copy of each table through its response class, and a live reply carrying the key.  There is no cached copy in this fork (the client's LocalState holds no MST files at all), so `as8G75gK` mission, `iyCP4N1h` map, `aqb6LS9y` spot, `Ri1S0JQX` route, `HUo5G8gz` icon, `hpGBs2R1` treasure, `27tgwCUN` flag, `0sT4d2zV` end-condition, `x3kCNR4Y` event and `Q83eyrJM` reward-catalogue were all empty lists: a map with no canvas, no spots to tap and no routes.  CampaignStart carries all ten now (~900 rows, every block a REPLACE).  One shipped route is dropped - 322 on mission 5000002 runs 20 -> 24 and that map stops at 23 - because `CampaignFieldScene` resolves an edge's ends against the spot list; spot 20 keeps its other exit.

Where the party STANDS is `Yusr3Zg5` (CampaignMissionDeckInfo: deck, `now_point_num`, `arrival_order`).  The client owns it during a run and reports it back through `createBodySaveDataPos` on Save @0x13B4170, BattleEnd @0x13B036C and End @0x13B1E54; nothing read it, and CampaignStart replied with ten rows of zeros, so a resumed run always came back at point 0.  Stored in `user_campaign_deck_pos` and replayed on CampaignStart.

**What a Grand Quest BATTLE still needs, and why it was not guessed at.**  `MonsterParty::entryCampaign` @0x10C1404 asks `CampaignBattleGroupMstList::getObjectGroupID(groupId, battleOrder)` for the row, reads `getMonsterGroupID()` off it and hands that to `entryMonsters`, which resolves through `BattleMonsterGroupMstList` and `MonsterMstList` exactly as an ordinary mission does.  There is NO fallback: an empty `Kwj0nX9T` list means no enemies.  That table (7 keys: `ZSf8e1MG` group, `VETu07N6`, `ug9xV4Fz`, `Qzhp8B40`, `etM5TCb9`, `2Smu5Mtq`, `nDf28LtU`) was never in the decoded dump, unlike the ten map tables, so it has to be AUTHORED — and the monster data has to come with it, the way MissionStart carries `75t0sx9z` / `89ausgc4` / `Kz7qfSs5` / `U0v5IeJo` / `pj41dy9g` from `MissionArchiver::populatePacket`.  The precedent is the Frontier Gate, which serves mission 10's waves relabelled.  Not attempted blind: handbook §3.1 is the crash that follows an internally inconsistent battle payload, and it kills the client mid-load rather than failing politely.

**The unregistered-request sweep, and what it actually says (2026-09-13).**  271 client request classes carry a GroupId (every `*Request::getRequestID` returns one); 192 are unregistered here.  The overwhelming majority are arena, raid, guild, colosseum, purchase and social — deferred or unbackable.  What matters is the evidence from the LOGS rather than the list: in the whole of `deploy/log`, exactly ONE unsupported request has ever been recorded (`2b9D01b4` GuildRanking, 2026-09-11), that only describes the requests present in the inspected log set, not all reachable paths or future play.  Worth registering when their features arrive, in rough order of reachability: `4Dk4spf9` UnitOmniEvo, `p8B2i9rJ` MissionContinue and `IP96ys7T` MissionRestart (the lose-a-battle path, unreachable only because nothing can kill the party), `6AqUE0xj` Reinforcement, `a5k36D28` BannerClick, `983D5Dii` UserEnteredFeature, `5s4aVWfc` NoticeList, `49zxdfl3` EventTokenExchangeInfo.

**Merit Points live in their own singleton.**  `Bnc4LpM8` (UserAchievementInfo) is NOT part of `fEi17cnx`: `RandallAchievementDedicateScene::setAchievePoint` @0x1A39D58 prints UserAchievementInfo +0x18, which only `UserAchievementInfoResponse::readParam` @0x13FBB58 writes.  The Frontier Gate is the one thing here that moves the balance and its reply carried the currencies without it, so the result screen showed the credit while the running total stayed at whatever the last UserInfo said - reported as "I see the merit points being credited but the number at the top right doesn't increase".  `gme::loadAchievementInfo` and FrontierGateEnd fix it.

**Event tokens are drawn BY DUNGEON, so `0Dk4fc81` decides whether they are visible at all.**  `EventTokenInfoList::addObjectIntoMap` @0x1C7E81C splits that comma list and keys the token by each dungeon in it; `MissionSelectScene2::setLayoutControl`, `MissionCheckScene::setEventTokenInfo` @0x187B8E4 and `MissionResultScene::scoreDraw` @0x18D0180 each ask `getObjectWithDungeonId` for the dungeon on screen and skip the whole panel when nothing answers.  We were sending the list empty, so a token could be earned and banked and still appear nowhere.  It is now derived: F_FROGATE_REWARD_MST says which gate pays which token (present type 8004) and FrontierGateMst says which dungeon backs that gate.  Note **only gates 101-111 pay Rift Tokens** - the Halls (1-91) pay units and items, which is why a Panache run credited none.

**The additive glow that could never be removed (2026-09-13).**  Every `gOpen*.txt` adds two ParticleAnime systems and names them both `light`.  `id=47` deletes by name out of a `std::map` (find / `deleteAnime` / `removeObjectForKey` @0x1CB3E8-0x1CB40C), so the map can only ever hold one of them and one removal is all that is reachable; the other is orphaned on the game layer with nothing naming it, and `effect/plist/eff_page_725_*.plist` are pure additive (blendFuncSource and blendFuncDestination both GL_ONE).  Stock never hit this because its `id=20 clearScreen` @0xFCD590 emptied the layer wholesale - which is the same op removed on 2026-09-06 because the wipe took the gate background with it.  `tools/patch_gacha_scripts.py` now renames the duplicates (`light`, `light2`) and clears both.  Unrelated and by design: a 6-star/7-star pull runs Wait -> **Change** -> Open, which is why two gates and two Touch prompts appear in one summon (`gacha_effect_mst` gives each effect a wait/change/open triple).

**There is a THIRD MST channel, and it is entirely unimplemented — the biggest single lever left (2026-09-13).**  `KeC10fuL` is both a REQUEST key and a RESPONSE key.  The client sends its 168 (table, local version) pairs; `VersionInfoResponse::readParam` @0x141E980 reads `moWQ30GH` (table), `d2RFtP8T` (version), `H6k1LIxC` and `5kbnkTp0` back, compares against `VersionInfo::getLocalVersion`, and for every table the server declares newer calls `DownloadMstFileList::add(target, fileName, …)` — about 150 compiled-in call sites, one per known table.  The client then downloads that file and `DataMstManager::loadFile` reads it back (base64 -> picojson), which is the same format `saveFile` writes, so no encryption has to be matched from scratch.

This server never reads `mst_requests` and never answers with a version row, which is why the client's LocalState holds **no MST files at all** and why tables that travel ONLY this way can never arrive.  `F_ET_MISSION_MST` is the one that bites today: 460 rows of (token id, mission id, amount) that say which quest pays which event token and how much, read by `MissionCheckScroll::drawET` @0x1893344 — the "this quest pays N Rift Tokens" label.  Implementing this channel would also retire the workarounds: the ten Grand Quest tables would not need hand-carrying on CampaignStart, and the achievement tables would not need parseBodyTag.

**A version match is a lead for identifying an unknown MST table.**  The client's `KeC10fuL` request lists every table with a version, and the decoded dump names its files `..._Ver<N>.json`, so an `F_UNKNOWN_*_Ver176` may be correlated with manifest entries at version 176. Version numbers alone do not prove table identity or uniqueness: corroborate the wrapper, columns and binary reader before renaming or emitting it.  Most of the 19 unknown files in the dump already carry an `// IDENTIFIED AS:` header from the 2026-05-16 audit: `IwODP3o4` = F_ET_MISSION_MST (460 rows), `MZGk4Tx5` = F_FRONTIER_GATE_AREA_MST, `Jba1QEOD` = F_GACHA_CATEGORY_MST (380 rows, the real banner rail), `gyY4TCm8` = F_PVP_FIXED_SETTING_MST, and eight minigame tables.  ⚠ Those files open with `//` comment lines, so a plain `json.loads` fails — strip them first.

**There are TWO delivery channels for MST tables, and the second one was invisible to the key sweep.**  `GameResponseParser::getResponseObject` @0x1392568 fills an in-memory list.  `GameResponseParser::parseBodyTag` handles a SEPARATE set of 58 tags by writing the client's local MST file (`DataMstManager::save*`), which `DataMstManager::load*` reads back at boot.  A key absent from the first table is therefore not necessarily unreachable - check the second.  The three that matter next are `H9ATfJ38` AchievementSubjectMst, `82CcMZhp` AchievementTradeMst and `1tJiqKgZ` AchievementDeliverRateMst; others nobody sends include `s24PDXZU` TrialMissionMst, `o71XxSBw`/`P8vNa5ZH` Scenario and `n4y6zU1I` NpcMessageMst.

**The Randall Achievement system, built 2026-09-13.**  `GetAchievementInfo` (YPBU7MD8) answered with the signal key alone and the player opens that screen regularly.  Three tables existed in the decoded dump and had never been ported; `tools/gen_achievement_mst.py` ports and curates them into `deploy/mst/achievement_{subject,trade,deliver_rate}_mst.json`:

  * `F_ACHIEVEMENT_SUBJECT_MST` 15,173 rows -> **873** after dropping the header row (row 0 carries literal column names) and everything already expired.

  * `F_ACHIEVEMENT_TRADE_MST` 32,500 -> **7**.  The cuts: 24 offers pay reward type 15 (ALTERNATE ART, a unit skin with no system here), and 146 sit in the 991/992/993 id bands, which are ACCOUNT RECOVERY ("Recover: Swordswoman Seria" and its evolution materials, all at 5,000 points) rather than shop stock.  `INCLUDE_RECOVERY` in the porter turns the latter back on.

  * `F_ACHIEVEMENT_DELIVER_RATE_MST` 53, ported for the Deliver flow that is not built.

All three reach the client through **parseBodyTag**, not getResponseObject — `H9ATfJ38`, `82CcMZhp`, `1tJiqKgZ`.

⚠ **WHAT AN ACHIEVEMENT COUNTS IS ITS ID BAND, NOT `cond_type`.**  `3rhygS9K` is the screen's filter and groups unrelated conditions: value 1 alone covers logins (band 1000), Honor Points (2000), player level (3000), friends added (4000), favouriting a friend (5000) and editing a comment (6000).  The first cut of this read progress off cond_type, and "25 Friends Added" reported the login count and claimed itself finished.  The thousand-band of the subject id is the real key, across 80 bands; `cond_param` is the threshold except in the three bands that name ONE mission — 35000/36000 (ordinary quests) and 40000 (Grand Quest), where it is a mission id and the answer is membership of the cleared set.  Two rows sit in a band that is not theirs and are handled by id: 7500 ("600 Mission Records Cleared", in the merit band) and 18100 (whose cond_param is a LIST of six sphere ids).  Twelve bands map onto counters this server already keeps; the rest stay at 0, on the same discipline as the trophy grades.

The loop closes: `AchievementRewardReceive` (uq69mTtR / cbE74zBZ) pays a finished achievement's points, latching on `user_achievement_subjects.reward_received` so a replay pays once and checking completion INSIDE the same transaction; `AchievementTrade` (m9LiF6P2 / 0IWC9LVq) spends them, enforcing the purchase limit in the same statement that records the buy.  Both carry `Bnc4LpM8` because Merit Points are not part of the currency block.  Accept (dx5qvm7L) and Deliver (vsaXI4M0) are still unregistered.

Two things the audit flagged that are NOT gaps, recorded so they are not "fixed" later: ItemSell / ItemSphereEqp / ItemMix reply empty because the client applies those changes itself (legacy-verified, see items.kdl), and CampaignItemEdit deliberately withholds `9wjrh74P` because its reply lands in whatever scene happens to be up.

## 9. Corrections to the historical handbook

| Historical claim | Current treatment |

|---|---|

| Current branch is `packet-generator-quests` | Last inspected branch is `audit-campaign`; inspect Git afresh |

| Only three mission records / all battles hardcoded to mission 10 | 942 authored records; only missing IDs use the relabelled fallback |

| Unit archive has 44 entries | 2,053 at the inspected baseline |

| Copy the callback pyramid with a hardcoded user and empty-OK errors | Use modern coroutine handlers, validated identity, and appropriate errors |

| V2 ticket amount key is fictional / one field per ticket row | Superseded: ID and amount both exist |

| Unit mutations require relaunch to display | Full-roster replacement fixed that path; MissionEnd unlock refresh now has a separate shared snapshot emitter |

| Trophy counters arrive at MissionEnd and are discarded | Current code accumulates the eight battle counters; later audit sections also record the fix |

| MonsterMst still lacks the original 14 additional fields | Current emitter/schema contain the expanded fields; audit table is stale |

| Rewards menu remains wholly unimplemented | Later work implemented substantial parts; inspect individual handlers |

| Every KDL edit always requires two builds | Stale-PCH recovery is conditional; inspect generation and timestamps |

| Every JSON key is exactly eight hashed characters | Literal keys and longer identifiers exist; verify actual producers/readers |

| Old branch push commands are the current workflow | They are historical examples; inspect current remotes/branches and user's commit scope |

## 10. Local work and handoffs

Do not stage the large mission archive merely because it changed: Evan explicitly excluded it from commits while its content remains incomplete. Existing local exclusions also include debug CLI, selector work, `tools/`, and Markdown audit docs. Preserve gitignored content/cache edits. Do not broadly stage the dirty tree.

When a future authorized change is committed, schema work belongs in the submodule first, then the superproject points to an available submodule commit. Verify the intended branch/remotes and keep unrelated local changes out; old handbook branch names are not commands for today's work.

A new handoff should fit on roughly one page: current request and gate, files changed, build/test evidence, save backup status, client-confirmed observations, authored choices, and next proposed action. Keep this guide stable. Update a contradicted fact in place, rather than appending another competing account. Preserve substantial investigations in the indexed history or component audit.
