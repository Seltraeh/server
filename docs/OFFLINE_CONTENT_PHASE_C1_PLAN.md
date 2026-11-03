# Offline client, next bounded patch (Phase C1): account for every download first

October 4, 2026. **Research and plan only.** No proxy or client change was
made, no download or cache flow was removed or bypassed, no client data was
reset. Inputs: [CLIENT_SESSION_9_STEPS.md](CLIENT_SESSION_9_STEPS.md)
("Feedback for the next offline-patch session"),
[OFFLINE_CONTENT_LOADING_PLAN.md](OFFLINE_CONTENT_LOADING_PLAN.md) Phase C, and
`offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` (the x86 hooks that
exist today).

> **Address corrections (Session 11, October 5).** Every x86 address below was
> re-classified against the binary (`out/qa/current/session11/verify_x86_addresses.py`,
> log beside it). Three were virtual addresses (image base 0x400000), not
> RVAs, and are now given as RVAs: `SaveUtils::saveUserDefaults` /
> `loadUserDefaults` RVA **0x23FFD0 / 0x240300** (written VA 0x63FFD0 /
> 0x640300); the UserInfo singleton RVA **0x159782C** (written VA 0x199782C);
> and, in the proxy contract, the downloaded-name map global RVA 0x1597E20
> (written VA 0x1997E20). The rest are RVAs as written. The C1 sites mapped
> since: `FileLoader::addLoadFile` RVA 0x634FA0 (`ret 1Ch`, one caller, inside
> `BaseScene::requestFileWithEncode` 0x63E270) and `addLoadFilePriority` RVA
> 0x1049A0 (`ret 20h`, 33 call sites in 13 functions), both hooked; see
> `offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` §3.4.

Evidence labels: **arm64** = `libgame.so` symbols and disassembly (semantics,
not patch sites); **x86** = `BraveFrontier.Windows.exe` `fb4f3c25…` RVAs, found
by string-literal or MSVC-RTTI cross-mapping (the same literals and class names
exist in both binaries) — "x86-candidate" means the function boundary is not
yet confirmed; **observed** = read from the machine, read-only.

## 1. What the client downloads, and what decides it

### 1.1 Every demand download is decided by `existsLocalFile`

Scene changes go through `BaseScene::transitionalDLCScenePushHandler` /
`…ReplaceHandler` (arm64 0xFFF4FC / 0xFFF6E0). Unless the next scene is one of
six excluded types (dynamic_casts, not yet named), they call its
`downloadFiles()` (BaseScene vtable +0x268); if `FileLoader::priorityDownloadExist()`
is then true they pause background downloading and push a
`TransitionalDLCScene` (origin = the next scene) in its place, which downloads
the priority list and then changes to the origin. x86: `TransitionalDLCScene`
constructor **0x15C190** (RTTI vtables 0x1445670 / 0x14453F8); the handlers are
0x63EAB0 (push) / 0x63ED10 (replace). *Session 15 correction:* this paragraph
used to name 0x15C120 as the constructor and 0x15BE00 / 0x15C080 / 0xE700F0 as
handler candidates -- 0x15C120 is a std heap helper and those are its callers;
the six excluded types are ConnectScene, LoadingScene, NoticeScene, TitleScene,
GumiLiveScene and StepScene ([OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) §7).

Of 698 `downloadFiles` overrides, 446 request nothing (435 only clear the
scene's 0x2000 flag, 11 are empty). The other 252 call:

| Request helper | Overrides | What it asks for |
|---|---|---|
| `BaseScene::requestFile` (name, optional folder) | 137 | one file; never one the app bundle ships (`CommonUtils::existsBundleFile`) |
| `GameScene::requestUnitFiles(unit)` | 99 | `/unit/img/`, `/unit/cgg/`, `/unit/cgs/`, `/unit/voice/` names from UnitMst, plus the unit's skill effects and SAM files |
| `GameScene::requestItemFiles(item)` | 68 | the ItemMst image |
| `GameScene::requestSummonerUnitFiles` | 35 | summoner avatar files under `/unit/img/`, `/unit/cgg/`, `/unit/cgs/` |
| `GameScene::requestMstFiles` | 20 | MST parts through `VersionInfoList` (the MST version channel) |
| `FileLoader::addLoadFilePriority` directly | 9 | Land, Gate, MyTownTop, MyTownItemList/ItemMix, SummonsAnimeScene2, MysteryChest, ArenaBattle, HomeScene2 |
| DLCManager requests | 4 | see 1.3 |

Every helper ends in `BaseScene::requestFileWithEncode` (arm64 0x1000680):
`CommonUtils::existsLocalFile(name)` — **x86 0x237EB0, already hooked by the
provider** — and only if that says absent: mark the scene (0x100) and queue
the name with `addLoadFilePriority` (when the scene's byte +0x1EC is set) or
`addLoadFile` (the main list). So a name the provider reports present is never
queued, and a scene whose every demand is present gets no TransitionalDLCScene.
The Session 9 provider log already shows TransitionalDLCScene entered through
both `replaceScene` and `pushScene`.

### 1.2 The background-DLC prompt and its flag

| Step | arm64 | x86 |
|---|---|---|
| After the login file check finishes (`FileLoader::isFinished`), load `DLC_WARNING_MENU_FLG`; any value → LoginScene state 10; unset → confirm dialog `BG_DLC_PROMPT_TITLE` / `BG_DLC_PROMPT_MSG` (id 6) | `LoginScene::updateEvent` +0x3D14 / +0x3DB0 | in `LoginScene::updateEvent` ~0x8E2960 (refs 0x8E519F, 0x8E5277, 0x8E5294) |
| Yes → store `"true"`, No → store `"false"`; both → state 10 | `LoginScene::confirmAnswerYes/No` 0x174CF1C / 0x174CFE8 | **0x8E6A80 / 0x8E6B00** |
| Storage | `SaveUtils::save/loadUserDefaults` (Android: SharedPreferences, key `"key" + name`) | RVA **0x23FFD0 / 0x240300** (first written as their VAs 0x63FFD0 / 0x640300) → WinRT LocalSettings |

**Where Windows keeps it (observed):** not `LocalState\UserDefault.xml` but
`%LOCALAPPDATA%\Packages\<package>\Settings\settings.dat`, value
`keyDLC_WARNING_MENU_FLG` (string + timestamp). The test package
`gumi.BraveFrontier.ContentTest_99p3jr0gh0z6w` holds **`"true"`**, written
2026-10-04 08:39 local (during the Session 9 client run); the installed client
also holds `"true"` (2026-10-02 21:39 local). So **the prompt will not appear
again on either package**, background downloading is enabled on both, and
resetting LocalState would not change that. The provider never touches
`Settings\`.

Consumers of the flag:

| Reader | arm64 | x86 | Effect |
|---|---|---|---|
| `FileLoader::getConnectStartFlag` | 0x1CDD0E4 | **0x1062D0** | true only if UserInfo's foreground-download byte (arm64 +0x2D8, x86 +0x25C; singleton RVA 0x159782C, first written as its VA 0x199782C) is clear, FileLoader's ConnectStartFlg bit (arm64 +0xF4, x86 +0x68) is set, and the value is `"true"` |
| `FileLoader::update` | 0xFBF5A8 | not mapped | processes the main list in foreground mode, or when `getConnectStartFlag()` and `CommonUtils::isCanSimultaneousDownload()` |
| `onSceneVisible` of GameScene, PartyEditTopScene2, SummonerPartyEditTopScene, SummonerWeaponMixMainScene, UnitMixMainScene, CoopRaidPartyEdit | — | not mapped | call `getConnectStartFlag` (effect not traced) |
| `OptionTopScene::setOptionInfo` / `touchEnded` | — | **0x90D550 / 0x9142B0** | the settings toggle |
| `DownloadScene::initialize` | 0x1B5DFBC | **0xC7DC20** | if `"true"`: clears ConnectStartFlg and stores `"false"` |

ConnectStartFlg is set by `ScriptEngine::advParam` (a script op),
the transitional handlers, `InitialDownloadScene::initialize` and
`TransitionalDLCScene::updateEvent` / `changeNextScene`; cleared by
`InitializeScene::downloadFiles`, `LoginScene` (after building the check list),
`DownloadScene::initialize` and the handlers before they insert a
TransitionalDLCScene.

Three more persisted modes sit beside it (arm64 `DLCManager`, same storage;
none is set in either package's `settings.dat`, so both run on defaults):

| Key | Set by | Read by |
|---|---|---|
| `SELECTIVE_DLC_FLG` (+0x16) | `TitleScene::confirmAnswerYes` id 3: `CommonUtils::deleteCache`, every VersionInfo local version reset | DataMstManager (MST load list, corruption check), `DownloadMstFileList::loadMsts`, MissionStartScene / DungeonSelectScene2 `downloadFiles`, OptionTopScene, deep links |
| `INITIAL_DLC_FLG` (+0x17); `getHasFinishedInitialDLC` = not selective, or this flag | `confirmAnswerYes` of Home, Menu, Shop, MyTown, OptionLang, Summons, RandallTown, DungeonSelect, Land | touch handlers of Home, Menu, Shop, MyTown, Summons, AreaSPSelect; RandallTownScene, DataMstManager |
| `FULL_ILLS_DL_DLC_FLG` | OptionTopScene toggle | `LoginScene::createDownloadCheckList` / `updateEvent`, `InitializeScene::initialize` |
| map DLC (+0x18, not persisted here) | DungeonSelectScene2 | `getResourceForArea/Dungeon/Mission`, `filterOutAreasWithoutDLC` |

### 1.3 Save- and server-dependent demands

| Source | Trigger | Depends on |
|---|---|---|
| `GameScene::demandDLCUnitResources` (arm64 0x1644DEC) | `HomeScene2` / `AnotherHomeScene` `downloadFiles` — **every visit to Home** | every owned unit (`UserUnitInfoList::getAllKeys`): its list thumbnail in the art variant it shows (`getUnitImgType`), its DBB partners' thumbnails, the dictionary's alternate image; a missing thumbnail requests the unit's whole file set (`requestUnitFiles`) |
| `UnitDictionaryScene`, `ItemDictionaryScene`, `SphereDictionaryScene` `downloadFiles` | opening the Archive | the player's dictionaries (`UserUnitDictionaryList`, `UserItemDictionaryList`) |
| `SummonsResultScene`, `SummonsAnimeScene2` | a summon | the roster after the pull, the pulled unit |
| `UnitEvoDetailScene`, `UnitOmniEvoScene`, … | unit screens | the unit, its evolution materials |
| `refreshAfterDemandDLC` (25 scenes: friends, arena/colosseum/vortex arena, rankings, presents, guild hall/raid/ranking/requests/search, inbox, mystery chest/reward, Randall shop, event tokens, Summoner Journal, InitializeScene) | the list is shown; it refreshes when art arrives | server data: friend leaders, present contents, rankings |
| `DLCManager::requestUnitFilesForMission`, `requestResourcesForMission`, `requestMonsterAttackEffectFiles` | mission start | party, helper, enemies, mission |
| `DLCManager::requestResourcesForActiveAreas`, `prepareDLCForActiveAreas`, `requestResourcesForVortexDungeons` | quest / Vortex selection | unlocked areas, the Vortex rotation |
| `DLCManagerComplementor::downloadBundleResourcesViaSceneId` / `…ViaObjType` | scene entry | the Appx `resourceMap` (437 scenes) → `/content/_dlcbundle/<file>` |
| `LoginScene::createDownloadCheckList` (arm64 0x1745184) | **every login** (`FILE_CHECK`, foreground, before the prompt) | MSTs: effects, effect groups, initial units, DBB, SP challenges, campaign maps/quests, summoner images and arms, raid maps/points/bosses, gacha effects/info, dungeon keys, banners, stamps, resources, medals, items, areas, general events; minus the LocalState census (`getDLCompleteList`, `Utils::getCacheFiles`, `filterOutExistingFiles`) |

The served tree holds, for scale: `unit/img` 13,270 files (921 MB),
`unit/cgg` 2,215, `unit/cgs` 6,117, `item` 803, `sam` 23,155 (766 MB),
`_dlcbundle` 941; there is no `unit/voice` folder.

## 2. What that means for removing downloads

- A download/cache flow cannot simply be removed: every row above is a path to
  a missing asset, and a missing layout CSV is a null-deref, not a blank
  (handbook, client asset preflight). The flows go away **by making their
  decision point answer correctly**, not by skipping scenes.
- The decision point is `existsLocalFile` (and, for the login census and
  background list builders, the downloaded-name map, x86 0xE417E0) — both
  already hooked. What changes is the provider's authority: from 21 hand-picked
  names to **whole families** the server can serve completely.
- Save dependence is real and large: Home demands the whole roster, the Archive
  the whole dictionary. A family must therefore be owned for **every** id the
  MSTs can name, not for the ids one save happens to hold — otherwise another
  save, a new summon or an Archive page falls back to the legacy download.
- The prompt is not a correctness gate. With families owned, "No" leaves only
  demand downloads, which become no-ops for owned names; "Yes" lets
  `FileLoader::update` work through a main list that should then hold no owned
  names. C1 leaves the prompt, its flag and every scene untouched. Whether to
  pre-answer it later is a Phase D decision, made through its documented value
  (`"false"`, the "No" semantics), never by skipping LoginScene states or
  hiding the dialog.

## 3. The bounded patch (C1)

1. **Server — family policy.** Extend `content_provider.json` from names to
   families: `unit/img`, `unit/cgg`, `unit/cgs` first (they dominate the
   save-dependent demands), then `item`. A family is owned only if the broker
   indexes every name UnitMst / ItemMst can produce for it; names the MSTs
   produce but the tree lacks are reported as such (a policy check, like
   `check_content_policy.py`), never silently owned. Measure index build time,
   RAM and delta size at ~21,600 names before widening further.
2. **Proxy — enqueue filter.** Map x86 `FileLoader::addLoadFile` /
   `addLoadFilePriority` (and `priorityDownloadExist`, `update`) and drop owned
   names at enqueue, logged, so a stale "absent" can never cause a LocalState
   write. The existing tripwire stays. A scene whose demands are all owned then
   gets no TransitionalDLCScene on its own.
3. **Confirm the transitional handlers in x86** (candidates in 1.1) and add
   them as logged observation points: per scene change, how many names were
   queued and how many were owned.
4. **Out of C1:** the prompt and its flag, LoginScene states, DownloadScene,
   the MST channel (`requestMstFiles`), audio/voice, scripts, zip bundles, the
   `_dlcbundle` resourceMap downloads (unless their names fall in an owned
   family), and any LocalState cleanup.

Mapped and hooked since (Session 10, re-verified Session 11): `FileLoader::addLoadFile`
RVA 0x634FA0 and `addLoadFilePriority` RVA 0x1049A0; `BaseScene::requestFileWithEncode`
is RVA 0x63E270 (located, not hooked).

Still to map in x86 for C1: `FileLoader::update` / `priorityDownloadExist`, the two
transitional handlers, `GameScene::demandDLCUnitResources`,
`DLCManagerComplementor::downloadBundleResourcesViaSceneId`. Each with the
contract's PE-identity and masked-signature guard.

## 4. Acceptance for C1 (client)

- **Different saves:** the live-save copy (109 units), a new-account fixture,
  and a large-roster fixture (`grant_test_bundle.py` on a copy).
- **Without a pre-populated cache:** both existing packages already hold full
  legacy caches and `"true"`, and client data must not be reset. Lay out a
  **second test package identity** (new name, empty LocalState and Settings);
  never reset the existing ones. Its first launch shows the prompt: record
  both answers' behavior on separate runs.
- **First visits:** Home, unit list, party edit, fusion, evolution, Omni,
  item list, Archive (units, items, spheres), Summon and its result, a quest
  and Vortex selection, a mission with a helper, Training Grounds.
- **Newly acquired units:** a summon, a present, an exchange; then Home and
  the Archive again.
- **Pass:** zero LocalState writes and zero TransitionalDLCScene downloads for
  owned families (tripwire, provider log, `progress` census during the
  interval); no missing-asset popups or blank art; downloads of non-owned
  families continue as before; broker-on versus `--broker off` timing on the
  same save.

## 5. Open questions

- What fills the main (background) list besides non-priority scene requests
  (`InitialDownloadScene`, the login remainder) — not traced.
- The six scene types the transitional handlers exclude.
- What the `onSceneVisible` readers of `getConnectStartFlag` do.
- Whether the provider's 1 s delta poll and the index stay cheap at family scale.
