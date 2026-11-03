# Offline client, Phase C3: the local-content boundary, and SuperAnim and particle effects

October 8, 2026 (Session 13). Directive: [CLAUDE_SESSION_13.md](CLAUDE_SESSION_13.md).
Inputs: [OFFLINE_CONTENT_PHASE_C2.md](OFFLINE_CONTENT_PHASE_C2.md), the Windows
loader contract (`offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md`),
the four installed packages' caches (read only). Evidence and bulky output are
in `out/qa/current/session13/`. No proxy (DLL) or server binary change: C3 is
a generated policy, like C2. Counts below are inventory metrics, not a
percentage of the project.

Evidence labels as in the handbook: **x86** = `BraveFrontier.Windows.exe`
`fb4f3c25…` / `libcocos2d_v2.2.5_Windows_8.1.dll` `5488d108…` RVAs read with
`tools/bin/pe_x86.py` (new: functions, callers, IAT users, string users);
**arm64** = `libgame.so` semantics; **server-tested** = the real broker and the
real hook code, without a client.

## 1. The boundary, inventoried

Reproduce (read only, ~10 s):

```
python scripts/content_inventory.py --policy out/qa/current/session13/family_policy_c3.json --base-policy out/qa/current/session10/family_policy.json --base-policy out/qa/current/session11/family_policy_c2.json --trace out/qa/current/session11/first_visit_downloads.json --sample-stored 12 --out out/qa/current/session13/inventory
```

`inventory.json` lists every name with its class; `inventory.md` holds the
per-folder table. Summary:

| Quantity | Count |
|---|---|
| game_content files / flat names / bytes | 75,931 / 75,478 / 3.15 GB |
| names with several copies: identical / differing (collisions) | 347 / 71 (6 aliased by the policy: the Town) |
| owned by C1 / C2 / C3 | 16,639 / 17,445 / **53,766** |
| existing, unowned (the game's own download path) | 21,647 names |
| resourceMap (437 scenes) names / owned / only in the app package / **unavailable** | 4,067 / 447 / 21 / **87** |
| installed client cache: content names / owned under C3 / legacy content / client state / zip-extracted | 71,073 / 53,764 / 17,309 / 152 / 103 |
| Session 11 first-visit demand downloads / owned under C2 / **owned under C3** | 5,630 / 27 / **2,542** |

**Unavailable** = asked for, held nowhere: 87 resourceMap names are in neither
game_content nor the package (e.g. `layout_battle_side_menu.csv`,
`layout_frontiergatetop.csv`, `cutin.sam`, the `multi_*` co-op art, the
templates `item_frame_` and `omni_paradigm_0%d.png` that the client fills in
itself), and 3 pages named by SuperAnim definitions (§3.4). No traced
first-visit download was unavailable. **Existing unowned** = 21,647 names the
tree holds that no phase owns yet (§5).

### 1.1 Who reads what (the provider can only own what reaches `getFileData`)

| Kind | Read path | Class |
|---|---|---|
| `.png` `.jpg` | `CommonUtils::getTexture` 0x2353D0 or `CCTextureCache::addImage` (cocos 0x16A590) -> `getFileData` "rb" | hooked |
| FileCrypt | The EXE's only decode loops: `getTexture` 0x2355A4 (only when forced or the PNG/JPEG magic is absent) and the two-image reader 0x65FD93 / 0x65FE44 (always; served encoded by return address). 0x222F22 encodes downloads. Found by scanning all of `.text` for `imul r,r` + byte `sub`/`add` (no SSE byte arithmetic exists) | — |
| `.csv` | `CommonUtils::getCsvList` 0x235C60 -> `getFileData` "r" (text mode) | hooked |
| `.sam` | `SuperAnimDefMgr::LoadSuperAnimMainDef` 0x62B270 (the only `.SAM` magic check, 0x62B44F) and `SuperAnim::getSamSupportingFiles` 0x62CBA0 -> `getFileData` "rb" | hooked |
| `.plist` | `CCSAXParser::parse` (cocos 0x1381B0) / `createCCDictionaryWithContentsOfFile` (0x135470) -> `getFileData` "rt" | hooked |
| `.txt` (scripts) | `getCsvList(ForLocal)` -> `getFileData` "r" (arm64 `GameUtils::getTutorialScript`; x86 0x106B10 calls 0x235C60). Scripts name further assets (`param=/dungeon/…jpg`) | hooked, with dependencies |
| `.fnt` | `CCBMFontConfiguration` (cocos 0x114E90) -> `CCString::createWithContentsOfFile` 0x108870 -> `getFileData` "rb"; pages by `addImage` | hooked, with dependencies |
| `.mp3` | the EXE's audio reader 0xEC0030: `fopen_s` + `fread` | **bypass** |
| `.zip` | extractor 0x391EE0: cocos `unzOpen` (its own CRT) and `fopen "wb"` of each entry into LocalState | **bypass**, writes client state |
| `.dat` `.json` `.xml` `.ini` `.tmp` | MST parts, key files, `XMLFileProcessor.xml`, `UserDefault.xml` | client state, refused by the provider |
| `.mp4` `.ttf` `.css` `.gif` | no `getFileData` user traced | not routed |

The EXE has ten direct `fopen` sites and one `fopen_s`: the download writer
0x223090, `saveFile` 0x238330, the ZIP extractor, `existsLocalFile`'s size
check 0x237E50 (above the hook), `winbfcert` 0xE1C410, the audio reader, and
`XMLFileProcessor.xml` state (0x175660, 0x175E80, three in 0x106B10).
**Correction:** the contract's §3.3 row "0x107229 / 0x106C3B script/param
parser, direct fopen" is wrong — the `fopen`s in 0x106B10 open
`XMLFileProcessor.xml`; scripts are read through `getFileData` (the contract is
updated).

### 1.2 Mutable client state (never provider scope)

LocalState holds, besides downloads: 150 `*.dat` (MST parts `Ver*`, account
keys), `UserDefault.xml`, `XMLFileProcessor.xml` (per-`.sam`/script "parsed"
flags, §3.2), 103 `sgtext_*_<n>.csv` extracted from the `sgtext/*.zip` text
dictionaries by the ZIP extractor, and `Telemetry`. The background-download
answer lives in `Settings\settings.dat`, outside LocalState. Sampled stored
forms: `sam/`, `unit_sam/`, plists, CSVs, `_dlcbundle`, sounds and scripts are
cached raw; most UI art (`item`, `event`, `gacha`, `dungeon`, `logincampaign`,
…) FileCrypt-encoded; `common`, `battle`, `raid` both (per download call
site). Several caches hold other-size copies (`Randall`, `event`, the
`_Holiday*` folders): stale versions the client never re-checks.

## 2. The family chosen: SuperAnim definitions with their pages, and particles

The largest coherent unowned family was SuperAnim: 4,248 definitions and their
pages across `sam/` (23,155 files), `unit_sam/` (12,261), `dbb/sam`,
`monster/img`, `_dlcbundle` and others. Its whole read and dependency contract
could be established, and every name is a real client request: the installed
client has cached 35,215 of the 35,220 family names (the other 5 are 3
missing pages and one unused effect). Particle plists (1,158) share the same
read path and were included with equivalent evidence.

### 2.1 SuperAnim read and dependency contract

- **Definition.** Callers resolve the `.sam` with `CommonUtils::getLocalPath`
  0x236950 (`existsLocalFile` -> the LocalState path, else the packaged copy)
  and load it through `Load_GetSuperAnimMainDef` 0x62C860 ->
  `LoadSuperAnimMainDef` 0x62B270: `getFileData(path, "rb")`, magic `MAS.`,
  version 1. **Two header layouts:** x/y/w/h as 16-bit fields (the default)
  or 32-bit fields when the caller passes mode 2 (arm64 `cmp w27, #2` ->
  `ReadLong` x4 at 0xFF0228; the unit and monster animations, 258 files).
- **Pages.** Each page path is `<directory of the .sam> + "/" + <image name>`
  (arm64 0xFF0BA4), loaded by `SuperAnimSpriteMgr` 0x632AC0:
  `CCSpriteFrameCache::spriteFrameByName`, else `CCTextureCache::addImage` ->
  `getFileData` "rb". A missing page shows the "is missing." box.
- **The client's own dependency rule.** `FileLoader::addLoadFilePriority`
  records `_dlcbundle` `.sam` downloads in `XMLFileProcessor.xml`
  (`specialSamHandler`); `FileLoader::processSamAndCutSceneFile` (x86
  0x106B10, from `updatePriority`) then reads each unparsed definition
  (`isFileExist` -> `getSamSupportingFiles` 0x62CBA0, `getFileData` "rb", its
  image table) and queues every page `existsLocalFile` reports absent
  (`downloadBundlePriority`). So the image table **is** the dependency list.
  Its cleanup only edits XML nodes, never files.
- **Stored raw**, read raw: every sampled cached `.sam`/page is byte-identical
  to game_content.

### 2.2 Particle read contract

`GameParticle::create` 0x629990 (`new CCParticleSystemQuad` +
`initWithFile`; the EXE's only particle construction) has 56 call sites: 54
pass `getLocalPath(name)`, the title screen uses an `isFileExist` variant
(0x65FA30), and 0x811430 forwards its callers' `getLocalPath` result. cocos
reads the plist through `createCCDictionaryWithContentsOfFile` ->
`getFileData(path, "rt")`. The texture: `textureFileName` next to the plist if
that file exists, else the embedded `textureImageData`. All 1,158 particle
plists embed their texture; one names another plist as its texture (merged
into one bundle); two name a texture that exists nowhere with nothing embedded
(left legacy).

## 3. The C3 policy

```
python scripts/gen_effect_family_policy.py --base out/qa/current/session11/family_policy_c2.json --out out/qa/current/session13/family_policy_c3.json --report out/qa/current/session13/family_report_c3.json
```

C3 appends after C2 (scope, bundles, aliases, exclusions an exact prefix;
`.plist` added to the extensions). The proxy needs no change: it refuses only
client-state names, and text-mode reads were already supported.

| Family | Bundles | Files | Rule |
|---|---:|---:|---|
| sam | 4,226 | 35,165 | 4,233 definitions with every page; one all-or-nothing bundle per connected group (41 pages are shared, groups of at most 3 definitions), so no name is in two bundles |
| particle | 1,155 | 1,156 | each particle plist, with its named texture when that file exists |
| **total C3** | **5,381** | **36,321** | policy: 53,766 owned names, 7,949 bundles |

### 3.1 Left to the game's own path (reported, never silently owned)

| Reason | Definitions |
|---|---|
| a page whose copies differ (collision) | ConvCircleSet, GrandMapBoxOpenIcon, InfoButton |
| the `.sam` itself collides | 81035BattleEffectBlackBack (two folders) |
| a page missing everywhere | 30156BattleEffectLawdarcUBB003Obj_R, 83027BattleEffectPunchBackR (page names with a space) |
| the app package ships different bytes | 104BattleEffectThunderAdd, ArrowLeft, ArrowRight, GachaEndCircleGekiReaGet, GachaEndCircleGet, MapVillage_iph5, ProgressBarLightAdd |
| already owned by C2 (kept as C2 has it) | MapVillage, MapVillageAdd (the Town) |
| particle texture exists nowhere, nothing embedded | effect_prt_021, effect_prt_1011_white |

A packaged copy that reads identically is owned: 459 C3 names are also in the
app package — 338 SuperAnim names (63 definitions, 275 pages) and 121
particle plists, 120 of which differ from the packaged copy only in CR LF line
ends, which the text-mode read removes. Owning a packaged name switches its
source from the package to game_content; the bytes the reader gets do not
change.

### 3.2 Broker fix: bundles that share members

Checking C3 for overlap found that the inherited C1 unit families already put
58 names in several bundles (elemental variants drawn with one frame layout,
e.g. `unit_cgg_60132.csv` in six unit bundles). The broker judged bundles in
one pass in name order: when a bundle sorting later was incomplete, its
withdrawal took the shared layout away from a bundle already judged complete,
which stayed served without it — the "texture without its frame layout"
failure bundles exist to prevent. `ContentBroker::recomputeOwned` now repeats
the pass until nothing more is withdrawn, and still reports every member of
an incomplete bundle unavailable. Four new checks in
`scripts/test_content_broker_wire.py` reproduce it: Session 12's server
e30e2adf fails them (78 passed, 4 failed), the new candidate passes (82/0).
The C3 generator additionally keeps its own bundles disjoint (one bundle per
group of definitions sharing pages) and asserts it.

## 4. Verification

Against the new server candidate **90e3eff9** (Session 12's e30e2adf plus the
broker fix; e30e2adf runs are the control) and the proxy test binary
**ad5cbc67** (test target only; the DLL `e24234c1…` and the C1 candidate
`523c77ed…` unchanged). The generator itself has a synthetic test
(`scripts/test_gen_effect_family_policy.py`, 21/0, in the regression runner;
three mutations of the generator each caught).

| Check | Result |
|---|---|
| `check_content_policy.py --policy C3` (real tree) | 8/0: 53,766 owned, 0 collisions, 0 unavailable, every identity = the file's SHA-256 (`cand_check_policy_c3.log`) |
| **Live C3 family** (`test_content_family_live.py … --effect-report`): real broker, real tree, real hook code | driver 9/0, proxy 5,973/0: **all 36,321 C3 names read exactly as the game reads them** — 4,233 definitions as `<LocalState>\X.sam` "rb", 30,932 pages as `<LocalState>/<page>` "rb", 1,156 plists "rt" — each equal to the CRT's own read of the file in that mode; none reached the client cache's reader or either download queue; no persistent write; every definition present to the dependency processor's `isFileExist`; the 38 generator-legacy names stay with the game; 2,542 traced first-visit names served byte-exact (`cand_family_live_c3.log`) |
| **SuperAnim page swap / restore / park / restore** (`test_content_visible_swap.py --name Win_300x300.png --like LevelUp_68x80.png --page`) | driver 10/0, proxy 340/0: generation pinning inside the scene, the safe point released exactly the `<dir>/<page>` key, parking one page withdrew all 6 names of `Win.sam`'s bundle (absent from the provider alone, no fallback to the cache, the queued download reaches the game), restore served it again; real files byte-identical with unchanged mtimes; live save unchanged; publication 50.4 / 44.9 / 6.5 / 46.0 s (`cand_visible_swap_sam_page.log`) |
| Existing content suites on the candidate | broker 82/0 (e30e2adf: 78 passed, 4 failed — the shared-bundle defect), generation 17/0, shipped / C2 policy 8/0 each, provider live 341/0, C2 family 5,948/0, C2 Archive-tile swap 340/0; full regression runner **52 entries, 2,013 checks, 0 failed** (`cand_*.log`, `SUMMARY_runner_90e3eff9.md`) |
| Client command smoke (`harness_c3_smoke.py`) | 10/0 on the candidate: `--identity c1 server --policy C3 --save … --seed c3_effects_kit` READY with 53,766 owned, seeded from the fixture byte for byte, kept across a restart, live save and fixture unchanged |
| Proxy standalone | 311/0, 3 skipped (the live cases) |

## 5. Scale (same machine and session, two repeats each; `family_scale_results.json`)

| | C2 | C3 |
|---|---|---|
| broker ready after start | 15.1–15.6 s | 27.2–28.0 s |
| index build | 10.2 s | 20.8–21.5 s |
| server private memory at ready | 352 MB | 455–461 MB |
| full index | 2.41 MB, 161–187 ms | 8.96 MB, 501–568 ms |
| empty delta poll (each client, once a second) | 1.1 ms median | 4.1–4.6 ms median |
| owned-file read, warm | 1.2 ms | 1.3 ms (14 ms cold) |
| a replace published (swap/restore) | 13–21 s | **45–54 s** |
| the 32-bit provider holding the index (live family case) | +9 MB private (2.2 -> 11.2 MB) | +25 MB private (10.5 -> 35.2 MB) |

The replace latency is the open follow-up recorded since Session 11: a
folder-level change re-hashes every owned file with a known digest. The
provider keeps serving the pinned version meanwhile, so it is latency, not
correctness; it matters for content edits and these tests, not for play.

## 6. What is still the game's own, and the next slices

Largest existing unowned groups: `event` 4,292 (2,758 images, 1,546 cut-scene
scripts), Summoner avatar parts in `unit/img` 3,850 (+ cgg/cgs), `gacha`
1,519, `logincampaign` 1,154, `common/button` 1,018, `_dlcbundle` 890 (754
layouts), `dungeon` 883, monster art 1,621 (`monster/cgs` 825, `img` 439,
`cgg` 357), `item` 803, `guild` 585, `sound` 426, `mytown_top` 394.

| Next | Status of its contract | Blocker / what to prove |
|---|---|---|
| **UI and item art** (single images) | read path established: `getTexture`/`addImage` -> `getFileData`; the decode-loop scan shows no unconditional decoder besides the handled two-image reader | per-name: 16 collisions in `common/button`, 11 in `challeng`, 12 in `event`; packaged copies that differ; then a generator like C3's. Owning `item` would also draw the slot-result tiles #27 reports blank (`docs/BUG_BACKLOG_STATUS.md`) |
| **monster art** | the unit-art contract (`img` + `cgg` + `cgs` per MonsterMst row) | a MonsterMst-driven bundle generator, like C1's |
| **scene layouts** (`_dlcbundle` CSV) | `getCsvList` "r"; `GameScene::loadLayout` takes the downloaded copy when `isFileAlreadyDownloaded` | a missing layout is a null dereference: own per scene (resourceMap) complete or not at all; 87 resourceMap names exist nowhere |
| **scripts** (`event/*.txt`, tutorials, gacha) | `getFileData` "r" | each script names further assets (`param=/dungeon/…`): bundles must include them; the cut-scene processor queues them like SAM pages |
| fonts, sprite-frame plists | `getFileData` | 18 `.fnt` + atlas pages; 7 sprite-frame plists, 3 with missing textures |
| audio | **bypass** (0xEC0030 `fopen_s`) | a new DLL hook on the audio reader, or stays legacy |
| text dictionaries, MST parts | client state written by the extractor / version channel | not provider scope |

## 7. Limits

- C3 is server-tested only; the client check is
  [CLIENT_SESSION_13_STEPS.md](CLIENT_SESSION_13_STEPS.md).
- The live tests stand in for the game with fake game functions; that the real
  SuperAnim loader draws provider pages and the real particle system parses
  provider plists is shown by the binary reading (§2) and Session 9's Town
  (a client-confirmed `.sam` with pages), not yet by a C3 client run.
- Family-scale replace latency (§5) and the larger index are open.
