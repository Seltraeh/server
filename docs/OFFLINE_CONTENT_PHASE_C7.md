# Offline client, Phase C7: startup families and what is left

October 11-20, 2026 (Session 16, task 1). A generated **policy** plus the startup
coverage contract it is judged by ([OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md)
§8). Evidence: `out/qa/current/session16/`. Status: implemented and
automated-tested; not client-tested, not deployed. Corrected after the two October 20
QC reviews ([QC_SESSION_16_REVIEW.md](QC_SESSION_16_REVIEW.md),
[QC_SESSION_16_CORRECTION_REVIEW.md](QC_SESSION_16_CORRECTION_REVIEW.md)) and returned for
QC: the policy is unchanged, the manifest it is judged by was re-derived, and the verdict
is **incomplete**.

| | |
|---|---|
| Policy | `out/qa/current/session16/policies/family_policy_c7.json` `049951614c8b16a2cf7caf9320b9bdcdf768e6394377ff2e8584040d5e2d18f4` |
| Report | `out/qa/current/session16/policies/family_report_c7.json` `79d62c8354b52265187733300bf26c67f869c770d1ffef551a4031c50a748b26` (regenerated October 20 by the corrected generator: the same 57 legacy names, 35 of their reasons worded differently; the earlier report is `policies/superseded/family_report_c7_04995161.json` `3343127a…`) |
| Base | C6 `out/qa/current/session15/policies/family_policy_c6.json` `e3d29e25…` (every C6 bundle unchanged) |
| Manifest it is judged by | `out/qa/current/session16/coverage/content_startup.json` `3be2dae4220e4c82736beaa050c15b5e0e16d25258b96e5be770691305795a0c` (superseded: `coverage/superseded/content_startup_7e6a7364.json`) |
| Verdict | incomplete: 16 startup lists the manifest cannot derive exactly (`unknown`), 67 required names game_content does not hold (`absent`: 56 in no source, 11 only in the app package), 26 required families unowned (27 problem rows: `layout_skill_cutin.csv` is in both sets; §3); [OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) §8.4 |
| Generator | `python scripts/startup_coverage.py policy --base <C6> --manifest <manifest> --out <C7> --report <report> --legacy-cache "%LOCALAPPDATA%\Packages\gumi.BraveFrontier.ContentTestC1Yes_99p3jr0gh0z6w\LocalState"` (the cache is only read: it settles collisions) |
| Superseded (kept, never use) | `policies/superseded/family_policy_c7_3e842020.json` (2026-10-11: owned 35 roots whose closure is incomplete as single files), `_a8e8caca` (owned the 22 unresolved screen layouts as single files), `_a296453a` (one animation family not bundled whole) |

## 1. Which legacy download windows it removes

The Session 11 cold trace (fresh package, no-prompt branch:
`out/qa/current/session11/first_visit_downloads.json`) has 36 transitional
windows and 5,612 downloads. C6 left 1,685 of them to the game's downloader and
fully owned 5 windows; **C7 leaves 8 and fully owns 28.** Each of the 8 remaining
windows keeps exactly one name, and each is a family this policy may not claim (§3):

| Window | Downloads | Legacy under C6 | Legacy under C7 |
|---|---:|---:|---|
| #19 LoginScene -> ConfirmScene | 2,272 | 1,373 | `layout_skill_cutin.csv` |
| #27 ConfirmScene -> HomeScene2 | 761 | 27 | `layout_home2.csv` |
| #77 UnitDictionaryScene -> UnitDescriptionScene | 70 | 29 | `layout_menu_unit_desc.csv` |
| #94 RewardsTopScene -> PresentTopScene | 5 | 5 | `layout_present_top.csv` |
| #207 HomeScene2 -> DungeonSelectScene2 | 66 | 7 | `layout_dungeon_select.csv` |
| #223 StepScene -> MissionStartScene | 1,487 | 11 | `effect_prt_1011_white.plist` |
| #257 BattleSkillCutinScene -> MissionResultScene | 25 | 6 | `layout_mission_result_top.csv` |
| #266 MissionResultMaterialScene -> MissionResultUnitScene | 1 | 1 | `layout_mission_result_unit.csv` |
| #227 MissionStartScene -> MissionScene, #268 and 26 more | | | none |

The trace lists what was downloaded; a file that exists nowhere is queued too
(and fails), so these screens keep a short transitional screen on a cold client
whoever serves their layout.

## 2. The families

C7 = C6 plus 17,655 names (75,286 in all), in bundles that are all or nothing
(withdrawing one member withdraws every bundle naming it; bundles may share
members):

| Step | Family | Bundles | Rule |
|---|---|---:|---|
| 1 | SuperAnim definitions + pages | 11 | `FileLoader::processSamAndCutSceneFile` 0x106B10 reads a downloaded `.sam`'s image table (short header layout) and queues each page; a definition is owned only with every page |
| 1 | scripts + referenced art and audio | 1,556 | its `*,type=PARAM` lines (`param=` split on `:`, tokens ending `.png/.jpg/.mp3/.plist/.sam`), with what those pull in |
| 1 | fonts + atlas pages | 18 | `.fnt` `page` lines |
| 1 | plists + textures | 7 | a particle plist's `textureFileName` -- always without embedded data; with embedded data only when some copy exists (cocos2d-x `CCParticleSystem::initWithDictionary` draws the named texture when it can read it and the embedded `textureImageData` otherwise); a sprite-frame plist's `<stem>.png`, whether or not it exists |
| 1b | **screen layouts + their resourceMap entries** | 382 | a layout CSV is geometry only (rectangles and element names); what its screen draws is that screen's resourceMap entry. A layout is owned with every file of every entry naming it and their closure. A stem with no extension (`item_frame_`, `trial_label_deck_name_party`, `guild_event_deck_name_squad`; 18 layouts) is not a file -- the client completes it at runtime. A file only the Windows package ships is drawn from the package as on a stock client (`layout_title.csv`: `FB_btn_label1/2.png`, `support_btn_label1/2.png`, `wiki_btn_label1/2.png`, `title_logo2.jpg`). Scene `999999` is the login bundle -- a download list, not a screen: the 129 layouts only it lists are owned alone in step 3 |
| 2 | character animations | 435 | frame layouts (`_cgg`/`_cgs` CSVs) only with their textures, as C1 per unit; every group is bundled whole |
| 3 | every other servable file | 9,975 | one name per bundle -- never a name a family rule refused |

Ownable means a flat name the client namespace holds, not client state
(`IsProtectedStateName`), an extension with a proven reader (`.png .jpg .sam
.plist .csv .mp3 .txt .fnt .gif .css .cgg .cgs`), one copy (or identical copies,
or an alias settled in §4), and -- for a SuperAnim definition the app package
ships with other bytes -- a startup list requesting it (a stock client downloads
those at its first login and reads the server's copy; 156 such names are owned,
all required by a startup set).

## 3. What stays legacy, and why

57 roots, every one because something it needs exists in **no source**
(game_content, the Windows package, the APK assets, the installed client's
LocalState). The policy never names them; the not-owned controls of the family
test check each one.

Effect families (4; `F_RESOURCE_MST` or the effect category requires them, so
the background-download set is incomplete):

| Root | Missing |
|---|---|
| `30156BattleEffectLawdarcUBB003Obj_R.sam` | page `30156BattleEffectLawdarcUBB003Obj_R_280x280.png` (its other nine pages exist) |
| `83027BattleEffectPunchBackR.sam` | pages `83027BattleEffectPunchBack R_154x459.png`, `83027BattleEffectPunchBack R_723x368.png` -- the definition spells them with a space; only `83027BattleEffectPunchBackR_154x459.png` / `_723x368.png` exist |
| `effect_prt_021.plist` | particle texture `021.png` (nothing embedded) |
| `effect_prt_1011_white.plist` | particle texture `(null).tiff` (nothing embedded) |

Screen layouts (22; all required by the background set, `layout_skill_cutin.csv`
by the login set too):

| Unresolved layout | Missing (in no source) |
|---|---|
| `layout_banner_video_tutorial.csv` | video_list_frame1.png; video_list_frame2.png |
| `layout_battle_option.csv` | actionsset1.png; actionsset2.png |
| `layout_battle_option2.csv` | actionsset1.png; actionsset2.png |
| `layout_challenge_arena_select.csv` | colosseum.png |
| `layout_coopraid_result_top.csv` | progress_active.png; progress_base.png |
| `layout_coopraid_result_unit.csv` | mission_result_get_unit.png |
| `layout_dungeon_select.csv` | map_dlc_btn_label1.png; map_dlc_btn_label2.png |
| `layout_friend_top.csv` | main_l_r_btn1_friend_introduction.png; main_l_r_btn2_friend_introduction.png |
| `layout_home2.csv` | home_leader_mark.png; home_win_raid_trial.png |
| `layout_item_mix.csv` | header_back_img.png; header_base_1_img.png |
| `layout_menu_option_lang.csv` | lang_thai_btn_label1.png; lang_thai_btn_label2.png |
| `layout_menu_option_transfer_code_issue_done.csv` | sub_m_btn_mail_label1.png; sub_m_btn_mail_label2.png; sub_m_btn_taking_screenshots_label1.png; sub_m_btn_taking_screenshots_label2.png |
| `layout_menu_unit_desc.csv` | Cutin.sam; CutinAdd.sam; speardude_3.sam |
| `layout_mission_result_top.csv` | progress_active.png; progress_base.png |
| `layout_mission_result_unit.csv` | mission_result_get_unit.png |
| `layout_present_top.csv` | orb4_multi.png |
| `layout_raidfieldpickscene.csv` | BattleUnitGetText.png |
| `layout_randall_library_chronology_roughly.csv` | time_frame.png |
| `layout_randall_library_chronology_top.csv` | navi_chara_50_1_full.png; time_list_frame_light.png |
| `layout_skill_cutin.csv` | Cutin.sam; CutinAdd.sam; CutinRight.sam; CutinRightAdd.sam |
| `layout_summons_unit_select.csv` | dia_icon.png; sale_icon.png |
| `layout_titleinfoscene.csv` | title_info_05_btn_label1.png; title_info_05_btn_label2.png |

Scripts (31; no startup set requires them): `grand_15_01..16.txt` and
`grand_15_op.txt` (`navi_chara74_0..3.png` / `navi_chara75_0..3.png`),
`grand_18_01..05, 07..11, op.txt` (`navi_chara_tt01_0.png` and siblings,
`tt_bg.jpg`), `map103-dungeon_ex2_clear.txt` / `_open.txt`
(`attledungeon81029objfogsky.sam`), `map106-staffroll_lib.txt`
(`bf1_ending_5add.sam`).

Not owned at all (135 flat names on disk): the 104 text-dictionary ZIPs, 27
`.dat`, one each of `.json` `.mp4` `.ini` `.tmp` -- client state or no proven
reader.

These are content decisions for the maintainer (supply the files, or correct
the definitions or the resourceMap entries), not provider work. None is hidden:
no placeholder, no wildcard.

## 4. Collisions: 69 names, each settled by a consumer

A flat name with several different copies is ambiguous to the broker; C7 adds an
alias only where a consumer shows which copy the client uses. Evidence kinds: 29 by
the server's own answer to the request a resourceMap scene makes
(`/content/_dlcbundle/<name>`, BfWebController's first match), 27 by a
background-downloaded cache holding that copy byte for byte (raw or FileCrypt),
8 by `F_RESOURCE_MST` naming one path, 5 by seasonal overlays (`_Halloween2015/`,
`_Holiday2015/`, `_Holiday2016/`, `_SecondAnniversary2015/`) no client list names
while the rest are identical. Undecided: 0. Full record with every copy:
`family_report_c7.json` `collisions`.

| Name | Copies | Chosen | Evidence |
|---|---:|---|---|
| `81035battleeffectblackback.sam` | 2 | `sam/81035BattleEffectBlackBack/81035BattleEffectBlackBack.sam` | background-downloaded cache |
| `arena_rank_crown_1.png` | 2 | `arena_top/arena_rank_crown_1.png` | resourceMap 601, 410, 1104, 1105 -> server answer |
| `arena_rank_crown_2.png` | 2 | `arena_top/arena_rank_crown_2.png` | resourceMap 601, 410, 1104, 1225 -> server answer |
| `arena_rank_crown_3.png` | 2 | `arena_top/arena_rank_crown_3.png` | resourceMap 601, 410, 1104, 1225 -> server answer |
| `base.jpg` | 5 | `common/base.jpg` | seasonal overlays in no list; rest identical |
| `caption_notice_reward2.png` | 2 | `notice/caption_notice_reward2.png` | background-downloaded cache |
| `challeng_rank_assistances_mvp.png` | 2 | `event/challeng_rank_assistances_mvp.png` | background-downloaded cache |
| `challeng_rank_bbkill_mvp.png` | 2 | `event/challeng_rank_bbkill_mvp.png` | background-downloaded cache |
| `challeng_rank_enter_mvp.png` | 2 | `event/challeng_rank_enter_mvp.png` | background-downloaded cache |
| `challeng_rank_multikill_mvp.png` | 2 | `event/challeng_rank_multikill_mvp.png` | background-downloaded cache |
| `challeng_rank_oneturnkill_mvp.png` | 2 | `event/challeng_rank_oneturnkill_mvp.png` | background-downloaded cache |
| `challeng_rank_overkill_mvp.png` | 2 | `event/challeng_rank_overkill_mvp.png` | background-downloaded cache |
| `challeng_rank_point_mvp.png` | 2 | `event/challeng_rank_point_mvp.png` | background-downloaded cache |
| `challeng_rank_quest0003_mvp.png` | 2 | `event/challeng_rank_quest0003_mvp.png` | background-downloaded cache |
| `challeng_rank_spark_mvp.png` | 2 | `event/challeng_rank_spark_mvp.png` | background-downloaded cache |
| `character_frame.png` | 5 | `home/character_frame.png` | seasonal overlays in no list; rest identical |
| `claim_stamp.png` | 2 | `_dlcbundle/claim_stamp.png` | resourceMap 999999 -> server answer |
| `convcircleset_190x100.png` | 2 | `sam/ConvEvolutionCircleSet/ConvCircleSet_190x100.png` | background-downloaded cache |
| `detail_btn_label1.png` | 2 | `common/detail_btn_label1.png` | resourceMap 863, 31, 999999 -> server answer |
| `detail_btn_label2.png` | 2 | `common/detail_btn_label2.png` | resourceMap 863, 31, 999999 -> server answer |
| `dungeon_name_frame_red_off.png` | 2 | `mission_select/dungeon_name_frame_red_off.png` | background-downloaded cache |
| `elemental_relation.png` | 2 | `event/elemental_relation.png` | resourceMap 70, 999999 -> server answer |
| `footer_btn.png` | 5 | `footer/footer_btn.png` | seasonal overlays in no list; rest identical |
| `friend_deletion_s_btn_label1.png` | 2 | `common/button/friend_deletion_s_btn_label1.png` | resourceMap 608, 102003, 102008, 102015 -> server answer |
| `friend_deletion_s_btn_label2.png` | 2 | `common/button/friend_deletion_s_btn_label2.png` | resourceMap 608, 102003, 102008, 102015 -> server answer |
| `frontier_rift_banner.png` | 2 | `challeng/frontier_rift_banner.png` | `F_RESOURCE_MST` names only this path |
| `grandmapboxopenicon_96x64.png` | 2 | `sam/GrandMapBoxRareOpenIcon/GrandMapBoxOpenIcon_96x64.png` | background-downloaded cache |
| `guild_01.txt` | 2 | `guild/guild_01.txt` | background-downloaded cache |
| `guild_02.txt` | 2 | `guild/guild_02.txt` | background-downloaded cache |
| `home_new_btn_box1.png` | 2 | `home/home_new_btn_box1.png` | seasonal overlay in no list; rest identical |
| `home_new_btn_box2.png` | 2 | `home/home_new_btn_box2.png` | seasonal overlay in no list; rest identical |
| `infobutton_152x65.png` | 2 | `title/InfoButton_152x65.png` | background-downloaded cache |
| `infobutton_152x65_2.png` | 2 | `title/InfoButton_152x65_2.png` | background-downloaded cache |
| `landimg001.jpg` | 2 | `event/landimg001.jpg` | resourceMap 2000 -> server answer |
| `landimg001_dl.jpg` | 2 | `event/landimg001_DL.jpg` | resourceMap 2000 -> server answer |
| `landimg001_tl.jpg` | 2 | `event/landimg001_TL.jpg` | resourceMap 2000 -> server answer |
| `landimg002.jpg` | 2 | `event/landimg002.jpg` | resourceMap 2000 -> server answer |
| `landimg002_dr.jpg` | 2 | `event/landimg002_DR.jpg` | resourceMap 2000 -> server answer |
| `landimg002_tr.jpg` | 2 | `event/landimg002_TR.jpg` | resourceMap 2000 -> server answer |
| `mapvillage.sam` | 3 | `sam/MapVillage/MapVillage.sam` | `F_RESOURCE_MST` names only this path (C6's `town` bundle, unchanged) |
| `mapvillage_111x55.png` | 2 | `sam/MapVillage/MapVillage_111x55.png` | `F_RESOURCE_MST` names only this path |
| `mapvillage_285x167.png` | 2 | `sam/MapVillage/MapVillage_285x167.png` | `F_RESOURCE_MST` names only this path |
| `mapvillage_421x157.png` | 2 | `sam/MapVillage/MapVillage_421x157.png` | `F_RESOURCE_MST` names only this path |
| `mapvillage_429x144.png` | 2 | `sam/MapVillage/MapVillage_429x144.png` | `F_RESOURCE_MST` names only this path |
| `mapvillage_640x208.png` | 2 | `sam/MapVillage/MapVillage_640x208.png` | `F_RESOURCE_MST` names only this path |
| `mytown_base.jpg` | 2 | `mytown_top/mytown_base.jpg` | resourceMap 999999 -> server answer |
| `navi_chara13.png` | 2 | `raid/navi_chara13.png` | background-downloaded cache |
| `ok_btn_label2.png` | 2 | `common/button/label/ok_btn_label2.png` | resourceMap 300, 132, 318, 951 -> server answer |
| `on_btn_label2.png` | 2 | `common/button/on_btn_label2.png` | background-downloaded cache |
| `raid_room_shadow_gather.png` | 2 | `raid/raid_room_shadow_gather.png` | background-downloaded cache |
| `raidback.jpg` | 2 | `raid/raidBack.jpg` | background-downloaded cache |
| `randall.jpg` | 2 | `event/Randall.jpg` | background-downloaded cache |
| `reset_btn_label1.png` | 2 | `common/button/reset_btn_label1.png` | resourceMap 268, 1283, 1221, 1504 -> server answer |
| `reset_btn_label2.png` | 2 | `common/button/reset_btn_label2.png` | resourceMap 268, 1283, 1221, 1504 -> server answer |
| `sp_quest_banner_light.png` | 2 | `mission_select/sp_quest_banner_light.png` | `F_RESOURCE_MST` names only this path |
| `state_others.png` | 2 | `battle/state_others.png` | resourceMap 70 -> server answer |
| `state_up.png` | 2 | `battle/state_up.png` | resourceMap 70 -> server answer |
| `sub_m_option_btn_payment13_label1.png` | 2 | `common/button/sub_m_option_btn_payment13_label1.png` | background-downloaded cache |
| `sub_m_option_btn_payment13_label2.png` | 2 | `common/button/sub_m_option_btn_payment13_label2.png` | background-downloaded cache |
| `sub_ss_btn1.png` | 2 | `common/button/sub_ss_btn1.png` | resourceMap 608, 239, 401, 863 -> server answer |
| `sub_ss_btn2.png` | 2 | `common/button/sub_ss_btn2.png` | resourceMap 608, 239, 401, 863 -> server answer |
| `sub_ss_btn_purchase_label1.png` | 2 | `common/button/label/sub_ss_btn_purchase_label1.png` | background-downloaded cache |
| `sub_ss_btn_purchase_label2.png` | 2 | `common/button/label/sub_ss_btn_purchase_label2.png` | background-downloaded cache |
| `sub_ss_btn_recover_label1.png` | 3 | `common/button/label/sub_ss_btn_recover_label1.png` | resourceMap 300 -> server answer |
| `sub_ss_btn_recover_label2.png` | 3 | `common/button/label/sub_ss_btn_recover_label2.png` | resourceMap 300 -> server answer |
| `sub_ss_btn_reward_label1.png` | 2 | `colosseum/sub_ss_btn_reward_label1.png` | resourceMap 1805 -> server answer |
| `sub_ss_btn_reward_label2.png` | 2 | `colosseum/sub_ss_btn_reward_label2.png` | resourceMap 1805 -> server answer |
| `sub_ss_r_btn1.png` | 2 | `common/button/sub_ss_r_btn1.png` | resourceMap 608, 102003, 102008, 102015 -> server answer |
| `sub_ss_r_btn2.png` | 2 | `common/button/sub_ss_r_btn2.png` | resourceMap 608, 102003, 102008, 102015 -> server answer |

"Background-downloaded cache" is the C1 Yes package's LocalState, which a stock
client filled from this server; "server answer" is the copy BfWebController's
recursive first-match returns for the requested flat name (replicated by
`server_first_found`). Either way the client already reads that copy today.

## 5. Verification (automated, rerun October 20 after the QC corrections; `out/qa/current/session16/suite_q_*.log`)

Against server `b2552da2…` (s16d), the proxy test binary `4958210e…` and, in the C16 layout,
DLL `8634857a…`; the runner printed every hash first (`build/run_qfix_suites_console.log`).

- `check_content_policy.py <s16d> --policy C7`: **8/0** -- the broker owns exactly the 75,286
  declared names, 0 collisions, 0 unavailable.
- `test_content_family_live.py ... --policy-report family_report_c7.json`: **11/0** (proxy
  6,270 passed, 0 failed; 4 other integrations skipped by design) -- every traced owned name
  served byte-exact through the DLL's own hooks, every C3 game read exact, and the 57 legacy
  roots unowned, asked of the game's own check and queued. The superseded `3e842020` (9/2) and
  `a8e8caca` (10/1) policies FAIL it (negative controls).
- `test_content_coverage_wire.py --real C7 manifest`: **56/0** -- the broker's report over the
  real tree equals the offline evaluation name by name (verdict, class counts, the 94 problem
  rows, the 16 unknown lists), and the DLL's rule does not prove it.
- `test_content_startup_live.py --real`: **11/0** -- the DLL's parser and decisions read that
  report as incomplete (census built, prompt left to the player).
- Harness smoke `out/qa/current/session16/harness_c16_smoke.py`: **19/0** -- the C16 commands
  (manifest install, coverage CLI, restart, broker off, no manifest, disposable park and
  restore) end to end without a client.
- Reproducible: rerun from the same inputs, the generator gives the policy `04995161…` byte
  for byte and a report differing only in the output path it records; the manifest
  derivation gives the same manifest but for its `generated` timestamp, and the offline
  evaluation is byte-identical.

## 6. Limits

- Not client-tested. The C16 cold run ([CLIENT_SESSION_16_STEPS.md](CLIENT_SESSION_16_STEPS.md))
  is the first visual check of C7's layouts, fonts and scripts.
- The 129 layouts only the login bundle lists are owned alone: no screen entry names
  what they draw. A screen whose code draws an image no resourceMap entry names is
  outside this model (as for every phase so far).
- Collisions settled by a cache copy rely on that cache having been filled from this
  server; a name never downloaded there and requested by no resourceMap scene stays
  ambiguous (none today).
- The per-MST category folders and general_event are enumerated from the files they
  hold today, which cannot show a file the client's ids need but nobody has. The
  manifest marks each of those 16 lists `unknown`, so the coverage report stays
  incomplete until each list's exact names are derived from its MST ids the way the
  client's request helpers build them.
- 67 names a startup list requires are not in game_content: 56 exist in no source,
  11 only in the app package; 62 of them are in the login set. They keep the report
  incomplete until client evidence shows the startup path does without them (or
  counts the packaged copy), or the files are supplied; the C16 run's `QUEUED`
  listing is the first such evidence.

## 7. C8 (Session 18, October 22): the screens whose missing files the client drops itself

Candidate, not client-run, not deployed: `out/qa/current/session18/policies/family_policy_c8.json`
`388185d921265ca5197bc8e27671e36037e2d4703484116ec8cd2a7b118583ea` (report `family_report_c8.json`),
generated by the same `startup_coverage.py policy` command from the same C6 base and legacy cache,
against the Session 18 manifest. The one change is how the generator reads the resourceMap: as the
client does, without the 203 names its exclusion table drops from every scene
([OFFLINE_STARTUP_TRANSITION_PLAN.md §9](OFFLINE_STARTUP_TRANSITION_PLAN.md)). A screen entry that named
only such files besides real ones now resolves, so 17 layouts §3 left legacy are bundled with their
entries: `layout_home2.csv`, `layout_dungeon_select.csv`, `layout_mission_result_top.csv` /
`_unit.csv`, `layout_coopraid_result_top.csv` / `_unit.csv`, `layout_skill_cutin.csv`,
`layout_menu_unit_desc.csv`, `layout_present_top.csv`, `layout_friend_top.csv`,
`layout_item_mix.csv`, `layout_challenge_arena_select.csv`, `layout_raidFieldPickScene.csv`,
`layout_randall_library_chronology_top.csv` / `_roughly.csv`, `layout_summons_unit_select.csv`,
`layout_menu_option_transfer_code_issue_done.csv`.

C8 = C7 + exactly those 17 CSVs: 75,303 names, none of C7's lost, the same 69 collision decisions
and aliases, 0 unavailable. Still legacy (40): the four effect families and five layouts whose entries
name files in no source (`layout_banner_video_tutorial.csv`, `layout_battle_option.csv`,
`layout_battle_option2.csv`, `layout_menu_option_lang.csv`, `layout_titleinfoscene.csv`), and the
other names of §3. Checks: `check_content_policy.py` 8/0, coverage wire on the real tree 70/0,
`test_content_family_live.py` 11/0 (36,321 game reads byte-exact).
