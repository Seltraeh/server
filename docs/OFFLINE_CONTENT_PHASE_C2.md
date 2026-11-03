# Offline client, Phase C2: what the first visits still download, and the unit art that now stops

October 5, 2026 (Session 11). Inputs: the two client-confirmed C1 runs
([QC_SESSION_10_REVIEW.md](QC_SESSION_10_REVIEW.md)), the packages they left
behind (read only), [OFFLINE_CONTENT_PHASE_C1_PLAN.md](OFFLINE_CONTENT_PHASE_C1_PLAN.md).
No proxy (DLL) or server binary change: C2 is a generated policy. Evidence is in
`out/qa/current/session11/`.

## 1. How the downloads were traced

The proxy logs only the queue entries it suppresses, never the names it passes
to the game's downloader, so the provider log alone cannot name a download.
The packages can. Every downloaded asset lands flat in the package's
`LocalState` with a wall-clock write time; the provider log names every scene
change in process-relative milliseconds. One timestamp the game writes itself
ties the two together per process:

- `Settings\settings.dat` value `keyDLC_WARNING_MENU_FLG` carries the FILETIME
  of the Yes/No answer, and the answer pops the prompt's `ConfirmScene`, which
  the log records. Start = answer time - pop time (No branch: 16:12:30.397; Yes
  branch: 17:01:37.369).
- The last process in a log ends at the log file's mtime.

Five other settings values land at the same offset from their scene change in
both branches under these anchors (language 8-9 ms after the loading scene,
BUN_VER 1.2 s before the login census, ZGKLT 0.37 s after the login
connections, the daily-task cooling period 0.08-0.14 s and the login campaign
0.10 s after theirs), which is how a first, 6-second-off fit was caught. With them, the No branch's 5,752 writes attribute completely and,
apart from the login census, fall inside `TransitionalDLCScene` intervals --
the demand path the plan describes. The Yes branch's background writes run
through every scene, but the background list pauses while a transitional scene
runs, so its demand windows are clean too.

Reproduce: `python out/qa/current/session11/trace_first_visits.py` (read only)
-> `first_visit_downloads.json`.

## 2. What the named first visits downloaded

| Screen | Branch, window | Files | Family and folder |
|---|---|---|---|
| **Vargas detail** (unit 10017) | No #52, Yes #48 | `unit_ills_{battle,full,thum}_10017_2.png` | alternate art, `unit/img` |
| **Archive entry** (`UnitDictionaryScene`) | No #75 (13), Yes #56 (12) | `layout_menu_unit_dict.csv`; `unit_ills_{battle,thum}_61097_2`, `_860798_2`; `unit_ills_thum_<id>_100` for 810048, 820278, 820578, 830708, 850988, 860658, 860888; `unit_ills_thum_860798_2_100` | scene layout (`_dlcbundle`); alt art and DBB thumbnails (`unit/img`) |
| Archive, first unit description (No: 810048) | No #77 (70) / Yes #58 (23) | `layout_menu_unit_desc.csv` + 20 description buttons; placeholder unit `unit_anime_00000.png`, `unit_cgg_00000.csv`, `unit_idle_cgs_00000.csv`; 7 particle plists; `BattleEffect` SAMs of 10163/10164/20013/20014/810507/810508; 3 SE mp3s | `_dlcbundle`, `menu_unit_desc`, `effect/plist`, `sam/`, `sound` |
| Archive, second description (820278) | No #81 (69) | 12 particle plists, `820278BattleEffect*` SAM pages, 1 SE | `effect/plist`, `sam/`, `sound` |
| **Archive, Touka** (840767) | No #132 (**7**) | `effect_prt_1011`, `_101_2`, `_1020R_black`, `_1087`, `_1133_red` `.plist`; `20015BattleEffectBgBlackBgF60.sam` + its `_64x64.png` | particle plists, a SuperAnim effect -- **none of Touka's own art**, which the provider served (its anime, cgg, cgs are logged as served in that scene) |
| Training: Randall town | Yes #124 | `VillageIcon.sam` + 7 pages | `sam/` |
| **Training: friend select** | Yes #129 | `unit_ills_thum_850266.png` | a SuperAnim-animated unit's thumbnail (`unit/img`); C1 left the unit incomplete |
| Training: check | Yes #131 | `practice_start_battle_btn1/2.png` | `training_ground` |
| Training: start | Yes #134 | `BattleDungeon80066SandFront.sam`, 86 SE / voice mp3s | `sam/`, `sound` |
| **Training: battle** (`SandbagScene`) | Yes #138 | `Win.sam` + 11 pages; `unit_anime_3810838.png`, `unit_cgg_3810838.csv`, `unit_atk_cgs_3810838.csv` and the same for 3840768 | `sam/`; DbbMst combined-unit art (`xbb`) -- the same six also downloaded for the No branch's mission (#227) |
| Training: result | Yes #148 | `LevelUp*.sam` + pages | `sam/` |

The Touka "seven-asset download" was therefore seven effect files, not the
seven configured image/CGG/CGS names the provider offers for the unit.
Enhancements and unfinished background work explain none of these windows.

Across both runs the demand windows hold 5,630 distinct names; the largest
remaining families are SuperAnim effects (`sam/`, about 1,280 files in the No
branch), particle plists (`effect/`, about 990), item and sphere art (`item`,
about 700), shared UI (`common`, about 400) and scene layouts (`_dlcbundle`,
about 380), then dungeon art, DBB UI, guild UI and audio (about 200 each).

## 3. Producers of the unit-art names (arm64, libgame.so)

| Name | Producer | Condition |
|---|---|---|
| `unit_ills_{full,battle,thum}_<id>_<n>.png` | `GameUtils::getUnitIllsImage` 0x117D084, `getUnitCutinImage` 0x117D694, `getUnitListThum` 0x117CBC4 | `_<n>` only when the image type n >= 2; the type is 2 only while the dictionary row's alt-art bit `2pAyFjmZ` is set |
| `unit_ills_thum_<id>[_2]_100.png` | `getUnitListThum` 0x117CBC4 | its DBB flag set **and** `DbbMstList::getDbbMstListByUnitId(id)` non-empty: the id is the first unit of a DbbMst pair (map lookup) or the second (`getDbbMstBySecondUnitId`) |
| `unit_anime_<x>.png`, `unit_cgg_<x>.csv`, `unit_atk_cgs_<x>.csv` (`xbb/`) | DbbMst row columns `anime_img` (2EF0d6ue), `cgg_file` (QqfI9mM4), `atk_cgs_file` (0QxL2is7) | set on 2 of 141 rows |

## 4. The C2 policy

`python scripts/gen_unit_family_policy.py --out POLICY --report REPORT` (now
`--phase c2` by default; `--phase c1` reproduces the Session 10 policy
byte for byte, report included). C2 appends after the C1 scope; nothing in C1
changes.

| Family | Bundles | Files | Rule |
|---|---|---|---|
| alt | 60 (`unit_<id>_alt`) | 180 | UnitMst ids whose three `_2` images exist, each a unique basename |
| dbb | 215 single-name (`unit_<id>_dbb_thum`, `unit_<id>_alt_dbb_thum`) | 215 | every unit on either side of a DbbMst pair (182, all in UnitMst, all with `_100`) and the 33 of them with alternate art (all with `_2_100`) |
| dbbart | 2 (`dbb_<row>`) | 6 | DbbMst rows naming combined-unit art; texture and frame layouts only together |
| ills | 135 (`unit_<id>_ills`) | 405 | illustrations of the units C1 left incomplete because they animate through SuperAnim (`unit_sam/unit_<id>/`); the animation itself stays legacy |
| total | 412 | 806 | 17,445 owned names (C1: 16,639) |

Left legacy and reported (4,080 files remain in the three unit folders):
Summoner avatar parts -- 8-digit ids with their `_U` / `_L` / `_U2` / `_L2`
pieces and their cgg/cgs -- 4,050; 18 guild guardian thumbnails; the four
files of 860126, which is not in UnitMst; and eight stray copies for UnitMst
ids: unit `1`'s thumbnail (a placeholder without full/battle art), six
`unit_anime_<id>.sam` copies in `unit/cgs` (the SuperAnim units' animations
live in `unit_sam/`) and `unit_move_cgs_10312.csv`, which UnitCgsMst does not
list for that unit. The SuperAnim unit animations (`unit_sam/`) stay legacy as
a family.

Of the traced downloads, 27 distinct names are now owned: the whole Vargas
detail set, 12 of the 13 Archive-entry files (the layout CSV is a scene
bundle), five DBB thumbnails Home's first visit fetched, the Training
friend-select thumbnail, and the six DBB combined-art files of Training and
missions. The other 5,603 stay with the legacy path, download screens and the
background-download prompt included.

## 5. Verification (all against the unchanged server `270832d6` and C1 DLL code)

| Check | Result |
|---|---|
| C1 reproduction (`--phase c1` vs `session10/family_policy.json`) | policy and report byte-identical (`69f0ecd3…`) |
| C1 is an exact prefix of C2 (scope order, bundles, aliases, extensions, exclude, root) | yes; 806 new names, 0 overlap |
| `check_content_policy.py --policy` on the real tree | 8/0: 17,445 owned, 0 collisions, 0 unavailable, every identity = the file's SHA-256 |
| Scale, same machine and session, two repeats (`family_scale_results.json`) | ready 17.5-18.0 s (C1 16.0-18.8 s); index 11.9 s (C1 10.8-12.1 s); full index 2.41 MB (C1 2.29 MB); private memory 354-373 MB (C1 352 MB); empty delta poll median 2-4 ms |
| Proxy client + provider + hooks against the real broker serving C2 (`test_content_family_live.py`, new test binary `646f8786`) | 7/0 (5,948 checks inside, 0 failed): the index lists 17,445 names; all 27 traced owned names are answered present by `existsLocalFile` without the game's own check, served byte-exact and kept out of the queue; all 5,603 traced legacy names go to the game's own check and are still queued |
| Proxy tests standalone / against the fixture broker (`test_content_provider_live.py`) | 311 passed, 0 failed, 2 skipped (both live cases, by design) / 341 passed, 0 failed, 1 skipped (the family case), driver 5/0 |
| Harness: mocked suite / Session 10 save+policy smoke / disposable smoke | 49/0 / 9/0 / 13/0 |

## 6. Remaining limitations and the next slices

- C2 is server-tested only. The client check is that Vargas detail, the
  Archive entry and Training draw correctly while the provider log shows the
  new names served ([CLIENT_SESSION_11_STEPS.md](CLIENT_SESSION_11_STEPS.md)).
  The existing C1 packages already cache those 27 names, so they cannot show
  the download screen disappearing; that consequence is the C1 mechanism
  (`existsLocalFile` answers present, the queue filter drops the entry), which
  the integration test exercises with the real hook code.
- Next by evidence, each needing its own loader verification before
  ownership: **scene layouts and buttons** (`_dlcbundle` through
  `DLCManagerComplementor` and the Appx resourceMap; a missing layout is a
  null dereference, so a bundle must be complete per scene), **SuperAnim
  effects** (`sam/` pages read through `LoadSuperAnimMainDef` and page loads --
  proven for the Town bundle only), **particle plists** (`effect/plist`; the
  cocos particle loader's read path is not yet confirmed to go through
  `getFileData`), **item and sphere art** (the plan's next family). Audio and
  voice stay with the game: the EXE reads them with its own `fopen_s`.
- Scale: about 5% over C1 at 17,445 names. Owning `sam/` (23,155 files) would
  more than double the index; measure before widening.
- Found in the disposable smoke: a replace inside a folder (a swap, a restore,
  a content update) reaches the broker as a folder-level change, and its
  `rescan("directory change")` re-checks the digest of every file whose digest
  is known -- at this scale every owned file -- before publishing: 23-35 s at
  17,445 names, against 1-3 s for a plain removal. The provider keeps serving
  the pinned version meanwhile (nothing mixes), so this is latency, not
  correctness. Limiting that rescan to the changed folder, or to files whose
  size or time changed, is a server change for a later session; the harness's
  `swap` / `park` / `restore` now wait for publication and report the time.
