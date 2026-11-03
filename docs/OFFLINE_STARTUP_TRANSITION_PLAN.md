# Offline client: startup and recurring verification, and a guarded path to local availability

October 8, 2026 (Session 13). Research and plan; **no startup behaviour was
changed**. Inputs: the Session 11 first-launch traces of two fresh test
packages (`out/qa/current/session11/first_visit_downloads.json`, both
background-download answers), the binaries (x86 `BraveFrontier.Windows.exe`
`fb4f3c25…` RVAs via `tools/bin/pe_x86.py`; arm64 `libgame.so` for
semantics), [OFFLINE_CONTENT_PHASE_C1_PLAN.md](OFFLINE_CONTENT_PHASE_C1_PLAN.md)
§1 and [OFFLINE_CONTENT_PHASE_C3.md](OFFLINE_CONTENT_PHASE_C3.md).

> **Session 16 (October 11-20, corrected after QC):** the coverage report and the
> two guarded decisions (census, background-download prompt) are implemented and
> automated-tested and fail closed; complete coverage is neither reached nor yet
> provable (16 unknown lists, 67 absent names, 26 unresolved families), so both stay
> stock. See §8.

## Conclusion

No guarded local-only startup path was implemented, because none is needed at
the screens themselves and the rest is not yet proven:

- Every download screen is a progress display over `FileLoader`'s queues. It
  appears only when something was queued, and owned names are never queued:
  the provider already answers `existsLocalFile`, the downloaded-name map and
  `isFileExist` for them and drops them at both enqueue functions. Making a
  screen disappear = owning every family that screen's demands produce.
- The foreground FILE_CHECK work at login is the text-dictionary and MST
  channel (client state the game extracts and versions), plus account
  initialisation and server requests. It must run; it is local already (the
  offline server serves it) and costs time only on first launch or a version
  change.
- The remaining families are listed in §4 with their blockers. Audio cannot
  be owned at all without a new DLL hook (its reader bypasses `getFileData`).

A cold fresh install is **not** supported by any of this until a fresh test
package completes first launch to Home on an isolated copy (§5, D5).

## 1. What runs, in order (fresh package, "No" branch, Session 11 Run 1)

| Scene change | Time | Downloads | What the screen hides |
|---|---|---|---|
| LoadingScene -> TitleScene | 3.8–4.4 s | 1 (wallpaper) | — |
| TitleScene -> LoginScene, **FILE_CHECK** | 19.0–42.5 s | 103 `sgtext_*.csv` (text dictionaries, extracted from `sgtext/*.zip`) | §2 |
| TransitionalDLCScene -> ConfirmScene (the background-download **prompt**) | 42.5–75.3 s | **2,272** (C3 owns 784: 689 of 704 SuperAnim, 68 of 80 `dbb`; legacy: `_dlcbundle` 291, `common/button` 196, `guild` 193, `sound` 105, `raid` 55, `event` 52, …) | the login check list's priority requests |
| (answer) -> TransitionalDLCScene -> HomeScene2 | 172.1–178.0 s | 761 (C3 owns 56; legacy `dungeon` 236, `item` 218, `gacha` 157, `home` 39, …) | Home's demands |
| first mission: -> MissionStartScene | 10.2 s | 1,487 (C3 owns 935; legacy `item` 455, `sound` 85) | party, enemies, effects |
| -> MissionScene | 3.3 s | 442 (C3 owns 408) | battle |

Across all 36 traced transitional windows of that run: 5,612 files, 2,532 now
owned (C2: 26); 3 windows would no longer appear at all. "Yes" branch: 19
windows, 3,287 files, 899 owned, 7 windows gone. The "Yes" branch's
background list then fetched 54,435 content names over the session; 37,125 of
those are owned under C3.

Every later launch runs the same LoginScene states; with the cache complete
nothing is queued and the screens pass quickly. Their time is the MST load and
corruption check, the LocalState census (the downloaded-name map built by
enumerating LocalState, x86 0xE415D0, ~71,000 files on the player's
install), the text-dictionary version check and the login requests.

## 2. Work hidden behind the login screens (must keep running)

`LoginScene::updateEvent` — x86 **0x8E2960** (FILE_CHECK text at 0x8E2A07 and
0x8E32A8), arm64 0x1747BD4 — in order:

| Work | Evidence | Kind |
|---|---|---|
| MST load list and corruption check; on corruption "REDOWNLOAD_MST_FILES" with a counter, then `DownloadMstFileList::saveVersion` | arm64 `DataMstManager::getLoadMstsList`, `checkCorruptionsOfMsts`; x86 strings at 0x8E2C46, counter helper 0x534140 | tables |
| `setInitialUnits` (DefineMst initial unit; "CORRUPT_MST_ERROR" 0x8E2DB3) | arm64 0x1744C38 | account initialisation |
| language change: purge the sprite-frame cache | `LocalisationManager::isLanguageChanged` | state |
| server-pushed reloads: `ReloadFileMstList` -> `getUpdateFileInfo` categories (unit, weapon, effect_group, gacha_effect, gacha, system, sp_challenge, effect, dungeonkey, stamp, banner, campaignQuest, general_event, medal, raidMap, raid_point_bgm/img, summoner_img, raid_boss_bgm, campaignMap, sgXbbSkill) -> `SaveData` reload list | arm64 0x174833C–0x17485C8; x86 0x8E37CD, 0x8E467F | forced refresh of named files |
| `createDownloadCheckList` — x86 **0x8DF880**, arm64 0x1745184: (category, id) entries from the MSTs (effects, effect groups, initial units, ResourceMst incl. text dictionaries via `TextDictionary_SG::DictionaryFileUpdateCheck` / `PurgeOldDictionary`, units, areas, items, summoner arms and images, gacha art, gacha effects, dungeon keys, …), each requested through the `existsLocalFile` gate | arm64 call list | download decisions |
| `FileLoader::getDLCompleteList` (census), `setConnectStartFlg`; FILE_CHECK progress until `FileLoader::isFinished` | arm64 0x17485F8–0x174B588 | download wait |
| `DLCManagerComplementor::loadBundleResourceMap` (x86 0x16F7F0, reads `resourceMap`) and `NoIdSceneSpecialHandle`; `InitialDownloadScene` when the foreground list is unfinished | arm64 0x174B4C0–0x174B554 | the per-scene bundle map |
| language change complete, footer reset, "OPTIMISE_DLC_COMPLETE" (0x8E501F), `TextManager::reload`, `TextureCacheList::removeAllObjects` | arm64 0x174B590–0x174B8E0 | text and texture state |
| background-download prompt: `DLC_WARNING_MENU_FLG` (0x8E519F) unset -> "BG_DLC_PROMPT_TITLE" (0x8E5294) -> `confirmAnswerYes/No` 0x8E6A80 / 0x8E6B00 store "true"/"false" in `Settings\settings.dat` | C1 plan §1.2 | a one-time player choice |
| `TextureLoadList` preloads (dungeon, Vortex, tier tourney, gacha category and banner art, bundle packs), `SoundPlayer::preloadLocalSe`, maintenance notice, daily login rewards, BadgeInfo, ControlCenterEnter, UpdateInfoLight, PermitPlaceInfo, then `changeNextScene` | arm64 0x174BAB8–0x174C368 | server requests and preloads |

The download scenes add no work of their own:

| Scene | x86 | Behaviour |
|---|---|---|
| TransitionalDLCScene | `initUpdateUi` 0x15D070 ("DOWNLOAD_NOTICE"), `updateEvent` 0x15C860 ("DLC_PROBLEM_ENCOUNTERED", vtable slot 149) — constructor **0x15C190** (Session 15; the C1 plan's 0x15C120 is a std heap helper) | pushed by the transitional handlers (x86 0x63EAB0 / 0x63ED10, §7) only when the next scene's `downloadFiles` left requests; downloads the priority list, then changes to the origin scene |
| InitialDownloadScene | initialize 0xC7E800 ("INITIAL_DOWNLOAD_NOTICE1", `LoadingBlack.sam`), update 0xC7F1A0 | progress over `FileLoader::count`/`finishCount`; pops at `isFinished`; error list on failures |
| DownloadScene | initialize 0xC7DC20 ("DOWNLOAD_NOTICE"; turns a stored "true" back into "false"), update 0xC7E3C0 ("CONNECT_ERROR2") | progress; retry on connection errors |
| background list | `FileLoader::getConnectStartFlag` 0x1062D0 | `FileLoader::update` works through the main list only while the answer is "true" |

## 3. Already local (no new hook)

For every owned family the existing hooks make the decision local: the login
check list, the background list and every scene demand ask
`existsLocalFile` (or the downloaded-name map), get "present" from the
provider, and queue nothing; a direct `addLoadFile`/`addLoadFilePriority`
call is dropped. A scene whose demands are all owned therefore gets no
TransitionalDLCScene, and the background list (if "Yes") holds only legacy
names. The prompt still appears once per package: with full coverage either
answer downloads nothing.

## 4. What still reaches the game's downloader, by gate

| Gate | Families (first-launch evidence) | To make it local |
|---|---|---|
| prompt / login priority list (2,272) | `_dlcbundle` layouts and art, `common/button`, `guild`, `raid`, `event`, `battle`, `challenge_arena`, `system`, `header`, sounds | D1 + D2 |
| Home (761) | `dungeon`, `item`, `gacha`, `home`, `summon`, `area`, banners | D1 |
| mission start (1,487 + 442) | `item` (455), sounds, `challeng`, `battle` | D1 + D2 |
| FILE_CHECK (103) | text dictionaries | none: client state from the server, first launch or version change only |

## 5. The guarded plan

Each phase keeps the default recoverable: the provider passes everything
through while starting, dormant or absent (no broker, `"enabled": false`, an
unknown client build), so the game's own downloads return; `"enabled": false`
plus a restart is the rollback; nothing touches `Settings\` or the prompt's
flag unless a phase below says so.

| Phase | Change | Prerequisites | Acceptance |
|---|---|---|---|
| **D1** policy only | own the remaining hookable families: UI and item art; monster art (MonsterMst img + cgg + cgs bundles); `_dlcbundle` layouts **per scene** (resourceMap: a scene's set complete or not owned — a missing layout is a null dereference); cut-scene/tutorial/gacha scripts with the assets their `param=` lines name; `.fnt` with atlas pages; sprite-frame plists with textures | one generator per family like C3's (collisions, packaged copies that read differently, dependency completeness); the 87 resourceMap names held nowhere listed and handled per scene | a fresh package's first launch to Home and a first mission: no TransitionalDLCScene except windows whose only legacy names are sounds; provider log tripwire 0; screens drawn normally |
| **D2** DLL | serve audio: hook the EXE's audio reader 0xEC0030 (`fopen_s` + `fread`) for owned `.mp3` | **Session 15: built, server-tested and player-confirmed on October 11 (replacement and restoration; cold-package acceptance still pending)** -- the EXE's `fopen_s` / `fread` / `fclose` imports serve owned sounds as RAM files ([OFFLINE_CONTENT_PHASE_C5.md](OFFLINE_CONTENT_PHASE_C5.md)); policy C5 owns all 426 sounds. **Never own `.mp3` names with a DLL that lacks this hook**: `existsLocalFile` would answer present, the game would stop downloading them and the bypassing reader would find nothing (silent audio) | sounds play from game_content with no `.mp3` in LocalState; tripwire 0 |
| **D3** optional | the prompt | only if D1+D2 leave nothing for either answer to download: the broker computes, from the MSTs, the names `createDownloadCheckList` and the background list would produce, and reports coverage; leave the prompt to the player while coverage is incomplete | identical behaviour for Yes and No; no download after either |
| **D4** none | FILE_CHECK text dictionaries and MSTs | — | stays the game's own (client state) |
| **D5** test | a cold fresh install | D1 (and D2 for sound) on a NEW test package identity and a copied save, never the player's | first launch to Home and a mission with the provider active; record every download screen; then a relaunch |

Until D5 passes, describe the offline client as "provider-served for owned
families", never as an install that needs no downloads.

**D1 progress (Session 14, October 8).** The summon gate scripts are the first
script family with a proven contract: Phase C4 ([OFFLINE_CONTENT_PHASE_C4.md](OFFLINE_CONTENT_PHASE_C4.md))
owns the 14 scripts `gacha_effect_mst` names, read by `getLocalPath` +
`getCsvList` in text mode, with every SuperAnim and plist they name already
owned; server-tested (36,335 byte-exact hook reads incl. the 14 scripts),
client pending. It removes the scripts from the Summon entry's
TransitionalDLCScene; that screen's sounds stay legacy. New D1 constraint
found: owning a script stops `processSamAndCutSceneFile` from queuing the
sounds only that script names (it parses downloaded scripts), so a script
family must either name only owned or packaged assets, or wait for D2.
Remaining D1 families are as in §4; none was started.

## 6. Open questions

- Removing the login-to-Home FILE_CHECK screen remains a user goal. D4's
  decision to retain dictionary/MST handling is a current safety boundary,
  not completion of that goal. A later change must preserve necessary table
  initialization/update detection while replacing redundant verification;
  asset coverage alone does not establish that this screen can be skipped.

- ~~The x86 transitional handlers~~ (mapped in Session 15, §7);
  `FileLoader::update` is still unmapped; D1 does not need it, D3 might.
- ~~What the six scene types the transitional handlers exclude are~~ (§7).
- Whether a fresh package's census enumeration time matters once most names
  are never written to LocalState (they would not be: owned names are never
  downloaded).

## 7. Session 15 (priority 3): the transitional routines in x86, and the exact patch points

October 9, 2026. Read on `BraveFrontier.Windows.exe` `fb4f3c25…` with
`tools/bin/pe_x86.py`, RTTI from the vtables' complete-object locators
(`out/qa/current/session15/re/x86_transitional_push.txt`, `_replace.txt`).
**No startup behaviour was changed.**

| Routine | x86 | What it does |
|---|---|---|
| `BaseScene::transitionalDLCScenePushHandler(next)` | 0x63EAB0 | if `this` is a `TransitionalDLCScene`, or `next` is a `ConnectScene`, `LoadingScene`, `NoticeScene`, `TitleScene`, `GumiLiveScene` or `StepScene` (seven `dynamic_cast`s, 0x63EAEA..0x63EB94) -> returns `next` untouched and never calls its `downloadFiles`. Otherwise `FileLoader::shared()` (0x634B80, singleton 0x1997B90, RTTI `FileLoader`) gets flag +0x68 bit 0, `next->downloadFiles()` runs (vtable **+0x1B8**), and if the request list at `[this+0E4h]` holds anything (+0x1C or +0x34 non-zero) the flag is cleared and a `TransitionalDLCScene` (0x248 bytes, ctor **0x15C190**, vtable 0x14453F8) holding `next` (+0x214) and the origin (+0x114) is returned instead |
| `...ReplaceHandler(next)` | 0x63ED10 | the same checks and construction (second `TransitionalDLCScene` call site 0x63EE9F) |
| download scene from inside a scene | e.g. `SummonsAnimeScene2::updateEvent` 0x875FAB | asks the FileLoader (method 0x635CC0) whether its downloads are done; if not, pushes a `DownloadScene` (ctor 0xC7DAB0) |

What this settles:

- A transitional screen is purely the result of what the next scene's
  `downloadFiles` queued. Owned names never reach the queue (`existsLocalFile`
  answers "present"; both enqueue functions drop them), so **ownership is the
  only safe lever**. Forcing the handler to skip the screen would skip downloads
  the scene needs (a missing layout is a null dereference: see the asset
  preflight notes).
- Measured on the Session 11 trace (no-prompt branch, 36 transitional windows,
  5,612 files): C3 owns 2,532 and removes 3 windows; C5 2,734 / 4; **C6
  3,927 / 5**. Home's entry keeps 27 legacy names under C6 (collisions such as
  `character_frame.png`, `home_new_btn_box1/2.png`; `freeenergy*.png`,
  `freespin*.png`, `guild_home_icon*.png`, `item_frame_*.png`; `layout_home2.csv`;
  `tuto101.txt`), the first mission's entry 11, the battle 27.

The login FILE_CHECK screen (user goal) -- exact prerequisites, none met yet:

1. **Coverage of the login check list.** `createDownloadCheckList` (0x8DF880)
   produced 2,272 priority requests on the trace; C6 still leaves 1,373 legacy:
   1,123 `.png` (`guild` 193, `common/button` 189, `common` 135, `raid` 55,
   `battle` 48, ...), 183 `_dlcbundle` layouts, 21 `.txt`, 18 `.fnt`. Layouts
   must be owned per scene with every image they draw (resourceMap), fonts with
   their atlas pages; `common/button` and `common` have 21 flat-name collisions
   and 39 packaged copies with other bytes to settle first.
2. **A coverage report (D3).** The broker computes, from the MSTs, every name
   `createDownloadCheckList` and the background list would ask for, and says
   whether all are owned and present. Without it no client-side decision can
   know that nothing would be downloaded.
3. **Only then a guarded local-ready decision**, at two patch points:
   the census (the downloaded-name map builder, 0xE415D0, which enumerates
   LocalState -- about 71,000 files on the player's install) may be skipped
   when the report says complete, because the provider already answers
   `isFileAlreadyDownloaded` / `existsLocalFile` for every owned name; and the
   FILE_CHECK wait in `LoginScene::updateEvent` (0x8E2960; FILE_CHECK text
   0x8E2A07 / 0x8E32A8) then has nothing queued to wait for. The MST load and
   corruption check, `setInitialUnits`, the text-dictionary version check and
   the login requests must keep running on every launch.
4. **Acceptance before shipping it:** a NEW test package identity (D5) reaching
   Home with no FILE_CHECK download and no transitional screen, a relaunch, a
   forced missing file (park) showing the game's own error path, and the
   provider switched off returning the original flow.

Not implemented this session: prerequisite 1 is a multi-family D1 effort and 2
does not exist, so initialization, missing-content and failure behaviour of a
skipped census cannot be demonstrated yet.

## 8. Session 16: the startup coverage contract and the two guarded decisions

**Session 18 (October 22) supersedes this section's numbers and lists: see §9** (10 of the 16
lists derived, the client's own resourceMap exclusions, the manifest bound to the client's tables).

October 11-20, 2026, corrected after two QC reviews
([QC_SESSION_16_REVIEW.md](QC_SESSION_16_REVIEW.md),
[QC_SESSION_16_CORRECTION_REVIEW.md](QC_SESSION_16_CORRECTION_REVIEW.md)). Prerequisites 2
and 3 of §7 are implemented and automated-tested. **Complete coverage is not reachable, and
not yet even provable** (8.4): sixteen of the client's startup lists cannot be derived exactly,
67 required names are not in game_content and 26 families are unresolved, so the guarded decisions
stay stock on every real run. The optimization is fail-closed; nothing here removes a startup
screen. Client run (fallback only): [CLIENT_SESSION_16_STEPS.md](CLIENT_SESSION_16_STEPS.md).
Families: [OFFLINE_CONTENT_PHASE_C7.md](OFFLINE_CONTENT_PHASE_C7.md). Handoff:
[CLAUDE_SESSION_16_HANDOFF.md](CLAUDE_SESSION_16_HANDOFF.md).

### 8.1 What the client asks for at startup: the manifest

`scripts/startup_coverage.py manifest` (`build_manifest` for any tree, `derive_manifest` for the
supported client) derives the names the two startup paths request, from the client's own lists:

| Set | Source (x86 `fb4f3c25…`) | Names |
|---|---|---|
| `login` | resourceMap scene `999999` (`DLCManagerComplementor::loadBundleResourceMap` 0x16F7F0) plus every name those files name -- `FileLoader::processSamAndCutSceneFile` 0x106B10 reads a downloaded SuperAnim's image table and a script's `type=PARAM` lines; a font's atlas pages; a sprite plist's `<stem>.png`; a particle's texture | 2,418 |
| `background` | `LoginScene::createDownloadCheckList` 0x8DF880: every `F_RESOURCE_MST` path (`Ver3275_hdt2N6mr`, 48,500 flat names), the same closure, and the existing files of the per-MST category folders | 72,282 |

Corrected rules (QC finding 2):

- **A dependency stays required whether or not it exists.** `Content.closure` keeps every name a
  file names and records each one with no single servable copy as a problem; a sprite plist
  requires its `<stem>.png` even when it is missing (QC's `missing_atlas.plist` probe now yields
  its PNG). The one exception follows the consumer: cocos2d-x `CCParticleSystem::initWithDictionary`
  draws a particle's named texture when it can read it and its embedded `textureImageData`
  otherwise, so with embedded data the named file is required only when some copy exists (1,077
  such names exist nowhere, each with its plist's embedded texture); without embedded data it is
  always required (`021.png`, `(null).tiff` are).
- **An enumerated list is unknown.** The fifteen `createDownloadCheckList` categories and
  general_event are built by the client from MST ids; the generator only enumerates the files their
  folders hold today, which cannot show a file the ids need but nobody has. Each is an `unknown`
  entry, so the report stays incomplete until its exact names are derived. Their existing files are
  still required.
- **Absent is a gap.** A required name game_content does not hold -- in no source at all (the
  Windows package and the APK assets checked too), or only in the app package -- stays required
  and is noted in `absent`; it is never accepted (QC finding 3).

Excluded, each with its documented client-side reason and kept out of every set (107): the 104
text-dictionary ZIPs (client state, unpacked during FILE_CHECK), two client-state extensions with
no reader, and `.DS_Store` (the lookup 0xE417E0 answers "present" for `Thumbs.db` / `.DS_*` itself).

Manifest: `out/qa/current/session16/coverage/content_startup.json`
`3be2dae4220e4c82736beaa050c15b5e0e16d25258b96e5be770691305795a0c` (superseded: `7e6a7364…`),
client build `6253F827-01756000-00ED273D`. Offline tests:
`scripts/test_startup_coverage_manifest.py` -- each of 16 required files removed and the manifest
and an owning policy regenerated from the smaller tree: the name stays required and the result
incomplete; it fails against the pre-correction generator.

### 8.2 The coverage report (server)

`ContentBroker::loadManifest` requires the format `bfcp-startup 1`, a client build, and exactly
the `login` and `background` sets, each a non-empty array of flat names without duplicates; it
refuses unknown keys and sets, an exclusion without a reason or inside a set, and an absent note for
a name no set requires (QC finding 1: a login-only and an empty-set manifest had returned
`complete 1`). A refused manifest gives `manifest none`, a `manifest_error` line and `complete 0`.

`GET /offline_mod/content/v1/coverage?build=<id>[&detail=1]`, `bfcp 2` convention: `coverage 1`,
`enabled`, `ready`, `epoch`, `generation`, `policy` and `manifest` digests, `manifest_build`,
`client_build`, `build_match`, `unknown`, `excluded`, one `set` line per set (required / ok /
absent / unowned / missing / ambiguous / unavailable), `complete 0|1`, a `reason`, then `U` rows
(unknown lists) and `P` rows (problems, 25 per class, every one with `detail=1`), `end`.

Each required name is classified owned -> ambiguous (a collision) -> unavailable (a bundle member
the policy declares but cannot serve) -> unowned (in the tree, not owned) -> absent (noted absent,
still missing) -> missing; every class but owned is a problem. `complete 1` requires an enabled,
ready broker, a loaded manifest, the manifest's client build, no unknown list and no problem.
The offline evaluation (`startup_coverage.evaluate`, with `validate_manifest` mirroring the loader)
classifies the same way; `test_content_coverage_wire.py` compares the broker, the DLL's rule and the
evaluation at every fixture state and on the real tree.

### 8.3 The two decisions (proxy)

Installed in two steps (Session 17, after the C16 client logged `DECLINED: censusLookup: ...
byte +0 is E9, expected 55`): `Startup_Prepare` checks the module identity and `kStartupSignatures`
on the untouched image before the content provider attaches; `Startup_Attach` installs the edits
after it, only if every site still holds exactly those bytes except the provider's own recorded
detour of the downloaded-name lookup (anything else logs `DECLINED` and leaves the game stock;
`offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` §9,
`offline-proxy/tests/fixtures/check_attach_overlap.py`).

- **What counts as complete** (`cp_core.cpp ParseCoverage`, QC finding 1): a report is read only
  if every header field appears once, every row is a known kind (`P` with a known class, `U`), every
  set line has all seven counts, and it ends with `end`; anything else -- including the first
  Session 16 broker's `A` rows -- is not a report. A report proves completeness only when it says
  `complete 1` and nothing contradicts it: enabled, ready, `build_match 1`, client and manifest build
  equal to the build the DLL asked about, loaded manifest and policy digests, no unknown list,
  exactly the `login` and `background` sets, each non-empty and fully owned, counts that add up, no
  problem row, no reason. A bare `complete 1` is never enough. The acceptance harness's `coverage`
  command applies the same rule (`content_provider_client_test.parse_coverage`); both judge the
  shared corpus `offline-proxy/tests/fixtures/coverage_reports.txt` (29 reports) alike.
- **The LocalState census** (0xE415D0, `void __cdecl()`; call sites 0x8E32DF LoginScene
  FILE_CHECK, 0xA03CA1 MissionStartScene, 0xE427E1 the startup path 0xE427B0) is left out only when
  the provider is **Active** -- re-checked after the report arrives -- on the very epoch and
  generation a proving report describes, with nothing pending (QC finding 4: a complete report while
  the provider was still starting had sufficed). A provider still starting, one whose start fails,
  a dormant one, a pending generation, or a report older or newer than the adopted generation keeps
  the census. Leaving it out keeps the map global null, exactly a fresh package (proof:
  `offline-proxy/tests/fixtures/check_startup_sites.py`, 55 checks, 6 mutations caught).
- **The background-download prompt**: the `call SaveUtils::loadUserDefaults` at 0x8E51A8 is
  redirected (rel32). A stored answer always stands. An unset answer is read as `"false"` (No) for
  this session -- nothing stored -- only under the same Active-and-proven condition **and** with no
  asset queued to the main list (`addLoadFile`) since start; otherwise the game asks.

Tests: `content_provider_tests` `TestCoverageCorpus`, `TestStartupCoverage`,
`TestStartupDecisions`, `TestStartupProviderStates` (a delayed start that recovers, a start that
never succeeds, a disabled broker, a report older than the adopted generation, every non-complete
corpus report live) and `TestLiveStartupCoverage` (a real broker, before and after activation). A
mutation build restoring the bare-claim rule and the starting-provider skip fails 14 of them.

**Which tests exercise the bypass.** The decisions are LEFT OUT only in unit tests and in
`test_content_startup_live.py`'s small fixture tree (a complete manifest the test builds). No real
content, and no client run, has exercised a left-out census or prompt; every other test checks the
fallback.

### 8.4 Result with the shipped content: incomplete

C7 (`04995161…`, 75,286 names) against the corrected manifest, broker and offline evaluation
agreeing name by name:

| Set | Required | Owned | Absent | Unowned |
|---|---:|---:|---:|---:|
| login | 2,418 | 2,355 | 62 | 1 (`layout_skill_cutin.csv`) |
| background | 72,282 | 72,251 | 5 | 26 (4 effect families, 22 screen layouts) |

plus **16 unknown lists**. The report's reason is the unknown lists first ("16 startup list(s) the
manifest could not derive"). The 26 unowned roots (27 problem rows: `layout_skill_cutin.csv` is
required by both sets) are families whose dependency exists in no source (table below). The 67
absent names are required names game_content does not hold -- 62 in the login set, 5 in the
background set (the effect pages and textures of the table below):

- 56 exist in no source, among them five literal templates the client completes at runtime
  (`omni_paradigm_0%d.png`, `guild_guardian_select_skill%d_btn_1.png` / `_2.png`, `item_frame_`,
  `multi_guild_info_`), two layouts (`layout_battle_side_menu.csv`, `layout_frontiergatetop.csv`),
  the four `cutin*.sam` definitions and 19 `multi_*` names;
- 11 only the app package ships (`bonus-ad-spin_a/b.png`, `daily-free-spin_a/b.png`,
  `daily-login-slot-machine_base/top/wheel.png`, `downloading_icon.sam`, `iphx_footer/header.png`,
  `ripple.sam`). Whether the startup path counts a packaged copy as present is client evidence still
  to collect; until then they count as gaps.

| Root (required by) | Missing dependency |
|---|---|
| `30156BattleEffectLawdarcUBB003Obj_R.sam` (`F_RESOURCE_MST`) | page `30156BattleEffectLawdarcUBB003Obj_R_280x280.png` |
| `83027BattleEffectPunchBackR.sam` (`F_RESOURCE_MST`) | pages `83027BattleEffectPunchBack R_154x459.png`, `_723x368.png` (spelled with a space; only the unspaced files exist) |
| `effect_prt_021.plist` (`F_RESOURCE_MST`) | texture `021.png` (no embedded data) |
| `effect_prt_1011_white.plist` (effect category) | texture `(null).tiff` (no embedded data) |
| `layout_skill_cutin.csv` (login bundle; screens 236, 90009) | `Cutin.sam`, `CutinAdd.sam`, `CutinRight.sam`, `CutinRightAdd.sam` |
| `layout_home2.csv` (Home, screens 40, 42) | `home_leader_mark.png`, `home_win_raid_trial.png` |
| `layout_dungeon_select.csv` (61, 62) | `map_dlc_btn_label1.png`, `map_dlc_btn_label2.png` |
| `layout_mission_result_top.csv` (80, 1592, 1706) / `layout_coopraid_result_top.csv` (100107) | `progress_base.png`, `progress_active.png` |
| `layout_mission_result_unit.csv` (85) / `layout_coopraid_result_unit.csv` (100109) | `mission_result_get_unit.png` |
| `layout_present_top.csv` (230) | `orb4_multi.png` |
| `layout_menu_unit_desc.csv` (841, 4060) | `Cutin.sam`, `CutinAdd.sam`, `speardude_3.sam` |
| 13 more screen layouts | `family_report_c7.json` `layouts.unresolved`, [OFFLINE_CONTENT_PHASE_C7.md](OFFLINE_CONTENT_PHASE_C7.md) §3 |

The 26 unowned and 67 absent names are only what the manifest can see: the unknown lists may hide
more. This is not an estimate of the remaining work.

**Evidence still needed before the bypass can be enabled** (none of it exists yet):

1. **Exact names for each of the 16 unknown lists**, derived from the MST ids the way the client's
   request helpers build them (`GameScene::requestUnitFiles`, `requestItemFiles`, ... and
   `GeneralEventMstList::getResouceFiles`), each recovered from the binary and tested -- not
   enumerated from existing files.
2. **For each of the 67 absent names, client evidence** that the startup path does without it
   (never queued, queued and ignored, or drawn from the package), or the file supplied; and for each
   of the 26 unresolved families, its dependency supplied or the definition / resourceMap entry
   corrected. The C16 run's `QUEUED` listing is the first source of that evidence.
3. **The client's own master tables equal the manifest's inputs.** The installed client's
   `F_RESOURCE_MST` (`Ver3275`) is byte-identical to the server's today, and the C16 package copies
   it; nothing checks this at runtime yet.
4. **A self-contained first launch.** The C16 fixture copies the installed client's master tables
   and account state: it is asset-cold, not a fresh installation. A true first launch needs the
   version channel to deliver every table the client keeps (147; 21 are staged). (Session 19,
   §10.4: 147 is a file count of 127 tables, all announced and served; the first launch itself is
   still to be observed.)
5. **A client run with a complete report**, then a relaunch, a missing file and the provider off --
   only after 1-4.

### 8.5 The "Refreshing the latest data files" notice

(Session 19: corrected in §10.4 -- any of the 79 checked lists, not only an `M_` table.)

The text is `REDOWNLOAD_MST_FILES`, shown by `DataMstManager::checkCorruptionsOfMsts`
when an `M_` master table loads with zero rows: the game deletes the file, resets its
version, shows the notice and closes; the next launch downloads the table again. In
the C6 run it followed an interrupted launch, not the image swap: in
`out/qa/current/content_client_c1/collected/20261011-120149/offlinemod_content.log`
process 13456 installed its hooks (line 80783) and ended about 2 s into the login
connection with no crash dump; process 10896 (line 80856) reached `NoticeScene` at
15.9 s (line 80929). The provider never routes `.dat` files (client state), so it
cannot have emptied a table. Most likely the first process was closed while saving a
table. The server log for that minute was overwritten, so the trigger is not proven;
the notice and the corruption check are left exactly as they are. To reproduce: close
the test client during its login connection, then relaunch it.

### 8.6 What acceptance still needs

The evidence list at the end of 8.4, in that order. The C16 cold run
([CLIENT_SESSION_16_STEPS.md](CLIENT_SESSION_16_STEPS.md)) tests the incomplete-coverage
fallback on a real client and collects the `QUEUED` evidence for item 2; it does not
validate the removal of any startup screen.

## 9. Session 18: exact lists, the client's own exclusions, and inputs bound to the client

October 22, 2026 ([CLAUDE_SESSION_18_HANDOFF.md](CLAUDE_SESSION_18_HANDOFF.md)). Implemented and
automated-tested; not client-run, not deployed. §8 stays as the Session 16-17 record.

### 9.1 What changed in the manifest

- **The client's resourceMap exclusions.** `DLCManagerComplementor::loadBundleResourceMap` 0x16F7F0
  drops every resourceMap token that is `"\r"` or one of 203 hard-coded names (std::string table at
  VA 0x184DF00, exact case; `tools/bin/x86_bundle_exclusions.py --check`). No scene download -- the
  login bundle 999999 included -- asks for one. `startup_coverage.bundle_map` reads the file the same
  way. 150 of 999999's 1,722 tokens are dropped; every one of the 62 Session 16 "absent" login names
  is among them. The C16 provider-off control priority-queued all 2,257 login names and none of the
  192 distinct dropped names.
- **Exact lists (`DERIVED`).** 10 of the 16 unknown entries are now derived from the rows the client
  holds, the way `LoginScene::updateEvent` 0x8E2960 builds each request, each with its binary trail in
  the code and re-checked by `tools/bin/x86_startup_rules_check.py` (129 facts):

  | List | Rows (source) | Requested |
  |---|---|---|
  | dungeonkey | `M_DUNGEON_KEY_MST` (Initialize `4NG79sX1`) | `M2cv6dum` tokens 0-1 -> /dungeon_key/, `VX0j1fni` / `9unNZ6b0` -> /dungeon/ (12) |
  | stampset, stamp | `F_STAMP_SET_MST`, `F_STAMP_MST` | `M2cv6dum` / `nGVc2zS1` -> /stamp/ (6, 100) |
  | medal | `F_MEDAL_MST` | `M2cv6dum` -> /medal/ (1) |
  | banner | `M_BANNER_INFO_MST` (Initialize `Pk5F1vhx`, replaces the list) | `1gDkL6XR` -> /banner/ (4) |
  | campaignQuest | `F_GRAND_MISSION_MST` (+ the server's copy CampaignMissionGet sends) | `yTPw0BD7`, `M2cv6dum` -> /grand/ (52) |
  | campaignMap, raidMap | `F_GRAND_MISSION_MAP_MST` (+ server copy), `F_RAID_MAP_MST` | `b5VveUd8` tiles `name:x:y` -> /raid/raid_map/ (78, 58) |
  | raid_point_img, raid_point_bgm | `F_RAID_POINT_MST` | `L8iA9M6c` -> /dungeon/ (109), `3sRN9BPS` -> /sound/ (2) |
  | raid_boss_bgm | -- | **empty**: the check list keys it "raidBossBgm", updateEvent tests "raid_boss_bgm" |
  | sp_challenge_title | `F_CHLNG_MISSION_MST` | `jsRoN50z` -> /dungeon/ (0: its one row has none) |
  | general_event | `M_GENERAL_EVENT_MST` (UserInfo `Md9gG3c0`) | `t5R47iwj` token 0 -> /sound/ for rows with `b4D63uav` = 1 (0 today); the Session 16 token rule was wrong |
  | sgXbbSkill | `M_DBB_MST` (UserInfo `fTV3A0By`) | cgg / cgs / anime sheet -> /xbb/ (6) |

  All 408 derived names are servable, and the provider-off client queued every one. Still unknown
  (enumerated, `unknown` entries): unit / initial units, weapon / summoner_img, item, area, gacha
  (with gacha_effect), effect / effect_group -- the `GameScene::request*Files` helpers. The effect
  entry now also enumerates `sound/` (its `F_EFFECT_GROUP_MST` `xW6TKu9G` names the voices the client
  queued, which the removed raid-BGM enumeration used to cover).
- **Inputs bound to the client.** `inputs.client_files` names the installed client's files the sets
  were derived from, with their SHA-256: the package's `Assets/Resources/resourceMap` and 10 LocalState
  master tables (`F_RESOURCE_MST` parts, medal, stamp, stampset, Grand Quest, its map, the challenge
  missions, the raid map and points); `inputs.server_files` the 6 deploy/mst response tables.
  `derive_manifest` refuses any client build but `6253F827-01756000-00ED273D`.

Manifest `out/qa/current/session18/coverage/content_startup_s18.json`
`f9749ef2d0f16900765ea382ffa4a0e1afd9f535c69e9760f57c47ddc699b975`: login 2,257, background 72,203,
excluded 254 (147 more: login-bundle names the client drops and no list requires), absent 5,
unknown 6.

### 9.2 The binding, server and DLL

- **Broker** (`ContentBroker::loadManifest`): `inputs.client_files` must be an array of
  `{root: localstate|package, path, sha256}`; the path follows the DLL's rule (`/`-separated names,
  no `..`, no reserved characters; a LocalState input is one name), the digest is 64 lower-case hex,
  no duplicate. The report gains `inputs N` and one `I <root> <path> sha256:<hex>` row each, and a
  manifest naming no client file is `complete 0` ("the manifest names none of the client's own
  inputs").
- **DLL** (`cp_core.cpp ParseCoverage`, `VerifyClientInputs`; `cp_startup_logic.cpp`): `inputs` is a
  required field and each I row is validated; a report naming no input never proves completeness.
  Before either decision is left out, the DLL hashes its own copies -- LocalState under the game's
  writable path, package files under the EXE's folder -- and requires every digest to match and
  LocalState to hold no other version or part of a named table (`Ver<v>_<key>[_<n>].dat`, any case).
  Otherwise the census is built and the prompt asked, with the reason logged. A Session 17 DLL reads a
  Session 18 report as not a report (unknown field `inputs`) and keeps the stock behaviour.
- **Offline**: `scripts/check_client_inputs.py` applies the same rule to any client folder (read
  only). C16 (LocalState and its own package folder), C1 and the installed client all match.

### 9.3 Result with the C8 candidate: still incomplete

C8 (`388185d9…`, 75,303 names) against the Session 18 manifest; broker, DLL rule and offline
evaluation agree name by name:

| Set | Required | Owned | Absent | Unowned |
|---|---:|---:|---:|---:|
| login | 2,257 | **2,257** | 0 | 0 |
| background | 72,203 | 72,189 | 5 | 9 |

plus **6 unknown lists**. The 9 unowned roots are families whose dependency exists in no source: the
four effect roots of §8.4's table and five layouts (`layout_banner_video_tutorial.csv`,
`layout_battle_option.csv`, `layout_battle_option2.csv`, `layout_menu_option_lang.csv`,
`layout_titleinfoscene.csv`; their entries name `video_list_frame1/2.png`, `actionsset1/2.png`,
`lang_thai_btn_label1/2.png`, `title_info_05_btn_label1/2.png`). On the route the C16 client played,
everything it priority-queued (3,150 names) is owned under C8, where C7 left `layout_home2.csv`,
`layout_dungeon_select.csv`, both mission-result layouts and `layout_skill_cutin.csv` to the game's
downloader.

**Evidence still needed before the bypass can be enabled:** the 6 remaining lists derived; the 9
families' files supplied or their definitions corrected (a content decision: `83027…PunchBackR.sam`
names its pages with a space); a self-contained first launch (§8.4 item 4); then a client run with a
complete report, which would also exercise the input check on a real client.

## 10. Session 19: item and area derived exactly, and the fresh-install master-table contract

October 22, 2026 ([CLAUDE_SESSION_19_HANDOFF.md](CLAUDE_SESSION_19_HANDOFF.md)). Implemented and
automated-tested; not client-run, not deployed. §8 and §9 stay as the record of Sessions 16-18;
§10.4 corrects §8.4 item 4 and §8.5.

### 10.1 Item and area

Both are now `DERIVED` lists, each request rebuilt the way the supported x86 client builds it and
re-checked by `tools/bin/x86_startup_rules_check.py` (184 facts with Session 18's):

| List | Rows (source) | Requested |
|---|---|---|
| item | the client's own `F_ITEM_MST` (LocalState, `inputs.tables`) | `Mt3Y0bo5` -> /item/; the frame of `getItemFrameName` -> /common/ (713) |
| area | the area table the server curates at boot, sent as UserInfo `3SG2wX0R` (`served:3SG2wX0R`), joined to the client's own `F_DUNGEON_MST` on `VjCY7rX4` | `b5VveUd8` -> /area/; `H0UB8pqm` -> /dungeon/ for types 1 and 3, else /area/; each of the area's dungeons' `21VKZo0E` -> /dungeon/ (347) |

- **Rows the server synthesizes are bound by a snapshot, not by a file.** `ServerCache` curates the
  area table once at boot (Vortex filtering, Parade Garden order, translated banners) and never
  changes it while the process runs. It records the SHA-256 of a projection of those rows (the four
  columns the rule reads, one escaped line per row) as `served 3SG2wX0R`; the broker requires the
  manifest to name it with that digest, so an edit after startup keeps the old binding and a restart
  rebinds or refuses. `startup_coverage.served_area_rows` ports the curation for the generator.
- **Every LocalState table a manifest was derived from is bound.** `inputs.tables` records the
  parts read; each must appear in `inputs.client_files`, or the broker refuses the manifest
  (`validate_manifest` agrees). The Session 18 DLL then checks the client's copies as before.
- **What deriving them exposed.** Enumerating dungeon/ for area used to cover the end of
  `LoginScene::updateEvent` by accident (arm64 0x174BDF8-0x174BF44: PermitPlace places, DungeonMst
  icons, VortexArenaMst art). It is now its own unknown list, "login-end preloads": dungeon/ plus 12
  names the C16 provider-off client queued in its login window (`scripts/check_startup_evidence.py`
  checks every login-window name is required). And the weapon branch (x86 0x7F6CE0) asks for /item/,
  so the weapon / summoner_img stand-in now enumerates item/ as well.
- **Unknown lists 6 -> 5**: unit / initial units, weapon / summoner_img, gacha, effect / effect_group,
  login-end preloads.

### 10.2 Result with C8: still incomplete

| | Session 18 manifest | Session 19 manifest |
|---|---|---|
| login | 2,257 (all owned) | 2,257 (all owned) |
| background required | 72,203 | 72,218 (+12 login-end names, +3 area names) |
| owned / absent / unowned | 72,189 / 5 / 9 | 72,201 / 8 / 9 |
| unknown lists | 6 | 5 |

The 3 new absent names are `sp_quest_banner_800141_1/2/3.png`, the banners of area 800142's
dungeons (in no source; the C16 save never had the area permitted). Every item name and 344 of 347
area names are owned and available. Two area names have two different copies in the tree and are
served through C8 aliases (`dungeon_name_frame_red_off.png` -> mission_select/,
`frontier_rift_banner.png` -> challeng/); the area rule asks for the /dungeon/ copies, and both
warm clients' caches, decoded, hold the /dungeon/ copies. Left for review; no alias was changed.

### 10.3 The fresh-install master-table contract

Read from the x86 client and re-checked by `tools/bin/x86_mst_bootstrap_check.py` (37 rules; the
extracted facts are `scripts/qa_baselines/mst_bootstrap_x86.json`):

- **Version channel.** `VersionInfoResponse::readParam` 0x52C900 reads each Initialize `KeC10fuL`
  row (`moWQ30GH` table, `d2RFtP8T` version, `H6k1LIxC` parts, `5kbnkTp0` records) and, for 154
  tables, adds it to the download list with its file key and encode key (static-initializer
  globals). It compares `F_AI_MST` and drops the result. `GameScene::requestMstFiles` 0x7F48F0 asks
  for `/mst/Ver<v>_<key>.dat`, or `_<n>` from 1 when there are several parts. Each part is
  base64(AES-ECB(encode key, PKCS#7)) of a JSON array. `existsLocalFile` skips a part already held.
- **Response channel.** 52 `getResponseObject` tags save one fixed LocalState file each (e.g.
  `YDv9bJ3s` -> `EEPBM76L2G.dat`, M_LEVEL_MST); the 29 the login check needs all arrive in
  Initialize, before any account exists.
- **Login.** State 3 of `LoginScene::updateEvent` loads the load list (0x53E2A0, 134 entries), then
  runs `checkCorruptionsOfMsts` 0x540540. On failure it shows `REDOWNLOAD_MST_FILES` (counter "1",
  state 0xC). On success it clears the counter and calls `DownloadMstFileList::saveVersion`, so
  versions are saved and lower-version files deleted only after every list loaded. State 4 runs
  `setInitialUnits` 0x8DF020: DefineMst `21ovwqYT` split on `,` `-` `:` `@` must give each element
  1-4 a unit id, otherwise `NETWORK_SERVER_ERROR` -4000.
- **Record count.** `loadFile` 0x543030: when a table's rows differ from its announced record count,
  and that count is not 0, every part is deleted and the table re-queued as invalid at each login.
- **Corruption check: 79 lists.** 45 version tables are hard: an empty list means `deleteCorruptFile`
  (version "0", files removed, the result cleared). `F_RESOURCE_BASE_MST` is one of these, checked
  only under a runtime gate. 2 are soft: `F_NPC_MESSAGE_MST` and `F_RELOAD_FILE_MST` only get
  `resetFileVersion`. 32 are named for response files: 29 saved from Initialize tags, and three
  in-memory lists that loading `F_AREA_MST`, `F_DUNGEON_MST` and `F_LAND_MST` fills. Recipes load
  from `F_RECIPE_MST`. The check acts only while the FeatureCheck flag `corrupted_mst_check` is set
  (image default 1; the server sends 1).
- **Text dictionaries and content versions.** The `.zip` paths of `F_RESOURCE_MST` (104) are queued by
  `createDownloadCheckList` and extracted at login. 16 content-kind version rows (`F_UNIT_IMG` ...)
  make the client purge its cached files of that kind when the version rises (0x531A00).

### 10.4 What this server delivers to an empty LocalState

`scripts/check_mst_bootstrap.py` classifies every delivery, offline or against a running server.
`scripts/test_mst_bootstrap.py` covers partial, interrupted, version-change, wrong-version, malformed
and record-count cases, and pins the real inventory
(`scripts/qa_baselines/mst_bootstrap_deploy_20261022.json`). A fresh save on the real roots gives the
same answers live (guest login, Initialize, every part fetched) as offline:

- **Nothing required is blocked.** All 48 required tables are announced, served and decodable,
  with matching record counts; so are `F_RESOURCE_BASE_MST` and `F_RELOAD_FILE_MST`. All 29 checked
  response tables carry rows. The initial units pass. All 104 dictionaries are served.
- **Correction to §8.4 item 4.** "147" is the file count of `deploy/game_content/mst`, which holds
  127 tables. The installed clients hold the same 127 tables (142 files) at exactly the announced
  versions, and the 21 staged tables override older copies in that folder. A fresh install
  therefore receives every master table a warm client holds.
- **Not delivered:** 27 registered tables are never announced: 22 `F_MULTI_*`, plus
  `F_FIRST_DESC_MST`, `F_NPC_MESSAGE_MST`, `F_CHLNG_MISSION_PROGRESS_REWARD_MST`,
  `F_FRIEND_POINT_MST` and `F_GACHA_FIX_UNIT_GROUP_MST`. No login check reads them, and no warm
  client holds them either. Four `F_CHRONOLOGY_*` tables are announced with 0 records and load
  empty.
- **Correction to §8.5.** The notice follows an empty list among all 79, not only an `M_` table.

### 10.5 What acceptance still needs

The 5 remaining lists derived; the 8 absent names and 9 families resolved; the two collisions
reviewed. Then a true first launch on an isolated fresh package, observed: the contract above says
the master tables are deliverable, but an audit is not a first run. Only then a client run with a
complete report.
