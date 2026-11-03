# Claude Session 18 handoff — exact startup lists, the client's own exclusions, inputs bound to the client

October 22, 2026, for Codex QC. Directive: [CLAUDE_SESSION_18.md](CLAUDE_SESSION_18.md).

> **Correction (QC, October 22): see [§10](#10-correction-qc-october-22-the-manifest-bound-to-the-servers-response-tables).**
> The broker now binds the manifest's `inputs.server_files` to the response tables the server serves.
> The server candidate is now `out/qa/current/session18/bin_correction/gimuserverw.exe` (`4eded167…`);
> the `995a088d…` named in §7 is the pre-correction server, kept as it was. The DLLs, policy and
> manifest below are unchanged.

**Startup coverage is still incomplete**, and the optimization stays fail-closed:
- 6 of the 16 startup lists remain unknown;
- 5 required files exist in no source;
- 9 families need such a file.

No census or prompt is left out on real content, and nothing removes a startup screen.

**What was not done:**
- No client was launched, reset or installed into.
- Nothing was deployed, committed or pushed.
- The live save was opened read-only only, by the regression runner's backup copies.
- C1 (`09056ee5…`) and C16 (`45a2c1b1…`, consent accepted, warm) are exactly as Codex left them.
- **Objective 5, the consent screen, was not worked on.** My first response to it was stopped by a
  safety classifier, so I left it out entirely and the user was told. No age or parental-attestation
  state was read, set or simulated.

## 0. Status

| Objective | Implemented | Automated-tested | Client-observed | Deployed |
|---|---|---|---|---|
| 1. Result-screen continuation cue | analysis | yes: `check_result_cue.py` 15/0 | C16 logs re-read; no new run | n/a |
| 2. Exact startup lists | 10 of 16 derived, 6 still `unknown` | yes | consistent with C16: the provider-off client queued all 408 derived names | no |
| 2/3. The client's resourceMap exclusions | yes | yes | yes: C16 priority queue (complete listing) matches exactly | no |
| 3. Content gaps: C8 policy candidate | candidate | yes | no (C8 has not run on a client) | no |
| 4. Inputs bound to the client's master tables | yes: broker, DLL and offline tool | yes, incl. mutants | read-only file check of C16, C1 and the installed client | no |
| 5. Consent screen | **not worked on** | — | — | — |

## 1. Changed files

Pre-edit copies: `out/qa/current/session18/pre/` (`SHA256SUMS`). Line counts are against those
copies (CRLF ignored).

| File | Change | sha256 (first 16) |
|---|---|---|
| `scripts/startup_coverage.py` | +522 −73: `BUNDLE_EXCLUSIONS` + `bundle_map`; the `DERIVED` lists with their evidence; `Field` / `DerivedList` (split tokens, tiles, sound superset, row filter, extra sources); `inputs.client_files` / `server_files`; build guard; `validate_manifest` / `evaluate` for client files; the Session 16 general_event token rule removed | `a3a34e1c4f5141b8` |
| `scripts/test_startup_coverage_manifest.py` | +117 −7: exclusions (exact case, case variant kept, the `\r` token), every derived-list case, 10 malformed-`client_files` schema cases | `edc916973c9c7f10` |
| `scripts/check_client_inputs.py` | new: the DLL's input rule, offline, read only | `50be390ccf08035f` |
| `tools/bin/x86_bundle_exclusions.py` | new: reads the 203-slot table from the EXE; `--check` compares it with the embedded one | `0da6bd5c4c6e7894` |
| `tools/bin/x86_startup_rules_check.py` | new: re-reads every fact the `DERIVED` rules cite (129) | `87c00721a70e085f` |
| `gimuserver/content/ContentBroker.cpp` / `.hpp` | +72 −2 / +11: strict `inputs.client_files`; `inputs N` + `I` rows; "names none of the client's own inputs" → `complete 0` | `b5e96f39046ee109` / `28edc45aa6f9f6b5` |
| `scripts/content_provider_client_test.py` | +49 −9: `parse_coverage` mirrors the DLL's input rules; coverage command lists the inputs; `KNOWN_DLLS` Session 18 pair | `256ea36552664757` |
| `scripts/test_content_coverage_wire.py` | +42 −7: fixture client files; inputs and I rows checked; a manifest naming none; real-tree I rows | `c74c1327c99e0fdc` |
| `scripts/test_content_startup_live.py` | +30 −7: fixture folders handed to the proxy suite; the `other-client` cases | `2ba3377a959cc9c8` |
| `scripts/test_content_harness_startup.py` | +21 −2: inputs in the reports, 3 more refusal cases, Session 18 registry and staged-hash checks | `d699adc5e7080118` |
| `offline-proxy/OfflineMod.win32/content/cp_core.h` / `.cpp` | +26 / +205 −1: `ClientInput`; `ParseCoverage` I rows + required `inputs`; `VerifyClientInputs` | `1a2fe9da35c22f76` / `a9cada95a727d5aa` |
| `offline-proxy/OfflineMod.win32/content/cp_startup_logic.cpp`, `cp_startup_internal.h`, `cp_startup.cpp` | +33 −4, +3, +14: `ClientHoldsInputs` before both decisions; `Env::packageDir`, set in `Startup_Attach` before `AttachImage` (the Session 17 attach and validation code is unchanged) | `d04c3ad77ec4ea2d`, `601c0b9be1232972`, `63e9435431450f4b` |
| `offline-proxy/tests/test_main.cpp` | +227 −6: input fixture; `TestClientInputs`; decision cases; the live `other-client` mode | `0b4e774b4000c77e` |
| `offline-proxy/tests/fixtures/coverage_reports.txt` | +276: inputs in 26 reports; 9 new cases (38 in all) | `cadd8978edf39a6f` |
| docs | this file; [CLIENT_SESSION_18_STEPS.md](CLIENT_SESSION_18_STEPS.md) (new); a dated section appended to [SESSION_16_CLIENT_RESULTS.md](SESSION_16_CLIENT_RESULTS.md); [OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) §9 (+ pointer in §8); [OFFLINE_CONTENT_PHASE_C7.md](OFFLINE_CONTENT_PHASE_C7.md) §7; handbook current direction | |

Session helpers and evidence, all under `out/qa/current/session18/` (never committed):
- `queued_oracle.py` (now records every list a name went to) and `queued_broker_off.json`;
- `x86_readparam_offsets.py`, `x86_key_split.py`, `x86_names.py`, `dis_window.py`, `scan_singleton_flag.py`,
  `check_result_cue.py`, `corpus_add_inputs.py`;
- `build/` (build and suite scripts, mutants, logs);
- `coverage/`, `policies/`, `bin/`, `proxy_*_s18/`.

## 2. Objective 2: exact names, and the client's own exclusions

### 2.1 The resourceMap exclusion table

`DLCManagerComplementor::loadBundleResourceMap` (x86 0x16F7F0) works like this:
- It splits the file on `\n` (0x168BC44).
- It takes the part of each line after its first `:` (0x168A218); a line needs two parts (0x16FA1E).
- It splits that part on `,` with the find-based split 0x65F510, which keeps empty tokens.
- It keeps a token only if it is not `"\r"` (0x168F604) and equals none of the 203 std::string slots
  at VA 0x184DF00. The loop 0x16FA90–0x16FB1C compares size, then bytes: exact case.

The static initializers from 0x6B10 fill the table, and `tools/bin/x86_bundle_exclusions.py` reads
them back: 203 slots, 201 distinct names. A dropped name never enters the client's map, so no scene
download asks for it. The path is `downloadBundleResourcesViaSceneId` 0x16FE20 →
`downloadBundlePriority` 0x660870 → `addLoadFilePriority`, with no other filter.

`startup_coverage.bundle_map` implements the same parse. On the real file, with no exclusions, it
equals the old parse on all 437 scenes. Effect of the table:
- 228 tokens are dropped across 49 scenes; scene 999999 keeps 1,572 of its 1,722 tokens.
- All 62 Session 16 "absent" login names are among the dropped ones.
- 147 dropped names become `excluded`, with the table as the reason. The other 3 stay required by
  F_RESOURCE_MST: an exclusion never hides a required name.

**Client evidence**, from the C16 provider-off control (collection 20261021-210903, pid 13488):
- The priority listing is complete: 3,150 distinct names, under the 4,000 cap.
- All 2,257 names of the new login set were priority-queued.
- None of the 192 distinct names dropped anywhere was.

### 2.2 The derived lists

For every list below:
- The check list stores `<key><id>` (or the name itself) for **every** row of the list. The only filters
  are the ones the table names: null rows (campaignQuest), general_event's type, and
  sp_challenge_title's non-empty title, which is the same as the empty-field rule below.
- `LoginScene::updateEvent` 0x8E2960 tests keys by substring, in an order the rules check reads from
  the binary.
- Each request goes through 0x63E120: an empty field asks for nothing, and the name is the part after
  the last `/` (0x235C30).
- A split column uses strtok on `,` (0x236740): an empty value or `" "` has no token.

Each list's addresses are in `DERIVED`. `tools/bin/x86_startup_rules_check.py` re-reads all of them:
literals, folder pushes, getter fields, each parser's store, the gates, the dispatch order, and the
exclusion table.

**Where the rows come from.** This matters as much as the columns:
- A response parser **replaces** its list (`removeAllObjects`; `tools/bin/so_response_map.py`, and the
  x86 parsers' first-value clear).
- So a list holds the latest server response if a login response carries its table. Otherwise it holds
  what DataMstManager loads at boot from the client's own LocalState copy (0x540540, 0x53E2A0).
- The server serializes the response tables in `InitializeResp` / `UserInfoResp`
  (`gimuserver/packets/all.hpp`).

| List | Rows | Fields → folder | Rows / names | Evidence that decided the source |
|---|---|---|---|---|
| dungeonkey | `M_DUNGEON_KEY_MST`, Initialize `4NG79sX1` | `M2cv6dum` tokens 0–1 → /dungeon_key/; `VX0j1fni`, `9unNZ6b0` → /dungeon/ | 3 / 12 | no F_ table; token 0 is read **unchecked** (0x4E2C8C), so a row without it is undefined behaviour and keeps the list `unknown` |
| stampset, stamp | `F_STAMP_SET_MST`, `F_STAMP_MST` | `M2cv6dum` / `nGVc2zS1` → /stamp/ | 6 / 6, 100 / 100 | no response class exists for these tables |
| medal | `F_MEDAL_MST` | `M2cv6dum` (virtual setter +0x28 → +0x44) → /medal/ | 1 / 1 | `GHUsyi76` never sent |
| banner | `M_BANNER_INFO_MST`, Initialize `Pk5F1vhx` | `1gDkL6XR` → /banner/ | 4 / 4 | the parser 0x4CDEA0 clears the list on its first value; C16 queued exactly these 4. None of the 11 names of the client's own F_ copy that exist nowhere was ever queued, even in the uncapped provider-on runs |
| campaignQuest | `F_GRAND_MISSION_MST`, **plus** `grand_mission_mst.json` (`as8G75gK`) | `yTPw0BD7` (frame), `M2cv6dum` (thumbnail) → /grand/ | 52 / 52 | no login response; boot load 0x53F8AB. CampaignMissionGet / CampaignStart replace the list within a session, so both sources are required. Their image columns are identical today |
| campaignMap | `F_GRAND_MISSION_MAP_MST` (staged Ver68), plus `grand_mission_map_mst.json` (`iyCP4N1h`) | `b5VveUd8` tiles `name:x:y` → /raid/raid_map/ | 56 / 78 | boot load 0x53F8E6 |
| raidMap | `F_RAID_MAP_MST` | `b5VveUd8` tiles → /raid/raid_map/ | 44 / 58 | `K3H0uzhE` never sent; boot load 0x54168A |
| raid_point_img, raid_point_bgm | `F_RAID_POINT_MST` | `L8iA9M6c` → /dungeon/; `3sRN9BPS` → /sound/ (a sound: 0x238870 may skip a packaged one, so requiring all is a superset) | 344 / 109, 2 | `4XA9m7of` never sent |
| raid_boss_bgm | — | — | **empty** | the check list keys it `raidBossBgm` (0x8E2334); updateEvent tests `raid_boss_bgm` (0x8E4375, the literal's only use); no branch matches and the entry reaches the common exit 0x8E484C. The dictionary's only writers are the builder and its helper 0x8E28A0 |
| sp_challenge_title | `F_CHLNG_MISSION_MST` (boot load 0x541444; `d8DM0Fvx` never sent) | `jsRoN50z` → /dungeon/ | 1 / 0 | its one row has no title image |
| general_event | `M_GENERAL_EVENT_MST`, UserInfo `Md9gG3c0` | `t5R47iwj` token 0 → /sound/, only rows whose `b4D63uav` reads 1 (`getResouceFiles` 0x5E2270; stream `>> int`) | 6 / 0 | **the Session 16 token rule was wrong**: the client asks only for the sounds of type-1 rows, and there are none today. An empty parameter requests a bare folder, which keeps the list `unknown` |
| sgXbbSkill | `M_DBB_MST`, UserInfo `fTV3A0By` (sent every time) | cgg `QqfI9mM4`, cgs `0QxL2is7`, anime sheet `2EF0d6ue` → /xbb/ | 141 / 6 | parser 0xDECF0; arm64 setter names |

The login responses are processed before the check list is built. The builder is called only at the
end of login state 4 (0x8E325F), which first validates the USER_INFO data, then moves to state 5
(FILE_CHECK).

**Totals:**
- 408 distinct derived names, all servable.
- The C16 provider-off client queued every one of them (the main listing is capped, so this is
  positive evidence only).
- The 4 banner and 6 DBB names appear in no F_RESOURCE_MST row. Without these lists, nothing would
  require them.

**Tests** (`test_startup_coverage_manifest.py`, 71/0). There is a case for each of these:
- an empty table;
- a populated table;
- a required name in no source (still required, absent, incomplete);
- an empty or missing column;
- split columns, and an unguarded token missing (unknown);
- no copy of the table (unknown);
- the list no key reaches (exactly empty);
- a sound column, and a sound where none is expected (unknown);
- the general_event filter and its empty parameter;
- map tiles;
- an extra source, and a missing extra source.

### 2.3 What stays unknown, and two corrections

Still `unknown` (their existing files enumerated and required):
- unit / initial units, weapon / summoner_img, item, area, gacha (with gacha_effect), effect / effect_group;
- each goes through a `GameScene` request helper (0x7F0220, 0x7F6FA0, 0x7F7140, 0x7F7430, 0x7F76B0…)
  that builds several names per id with conditions of its own. None was reversed this session.

Two corrections came out of checking removed names against the C16 queue:
- Removing the raid-BGM entry's `sound/` enumeration would have dropped 66 `se_voice_b*.mp3` the
  client really queues. They are named by `F_EFFECT_GROUP_MST` `xW6TKu9G`. The still-unknown effect
  entry now enumerates `sound/` too, with that evidence.
- `F_RESOURCE_BASE_MST` fills `ResourceBaseMstList` (0xE1AB70), which the check-list builder never
  reads. It is not a startup list.

### 2.4 Manifest

`out/qa/current/session18/coverage/content_startup_s18.json`
`f9749ef2d0f16900765ea382ffa4a0e1afd9f535c69e9760f57c47ddc699b975`:

| | Session 16 manifest | Session 18 manifest |
|---|---:|---:|
| login | 2,418 | 2,257 (161 removed: 150 dropped tokens + 11 files only they reached; 0 added) |
| background | 72,282 | 72,203 (79 removed: 26 banner-folder files outside the client's banner list, 29 `grand/` images no mission row names, 24 `raid/raid_map/` tiles no map row names; 0 added) |
| excluded | 107 | 254 |
| absent | 67 | 5 |
| unknown | 16 | 6 |

None of the 240 removed names was queued by the C16 client on either list. The main listing is capped,
so for main-list names that is no proof. The removal rests on the binary rules.

## 3. Objective 3: content gaps

**The route improvement**, measured from the C16 collections:
- In every provider-on launch, the game's own downloader got `layout_home2.csv`. Most launches also
  sent `layout_dungeon_select.csv`, `layout_mission_result_top.csv`, `layout_mission_result_unit.csv`,
  `layout_skill_cutin.csv` and 17 more layouts there.
- C7 left them legacy because their resourceMap entries name files in no source.
- Those are exactly the names the client drops itself (Home: `home_leader_mark.png`,
  `home_win_raid_trial.png`; quest map: `map_dlc_btn_label1/2.png`; results: `progress_base/active.png`,
  `mission_result_get_unit.png`; cut-in: the four `Cutin*.sam`).

The C8 candidate (`388185d9…`; [C7 §7](OFFLINE_CONTENT_PHASE_C7.md)):
- C8 = C7 + exactly the 17 layout CSVs (75,303 names). Nothing is lost.
- The same 69 collision decisions and aliases, each with its consumer evidence in `family_report_c8.json`.
- Against the Session 18 manifest, the whole login set is owned (2,257 of 2,257).
- So is everything the C16 route priority-queued (3,150 names; C7 left 2 of them unowned).

| C16 provider-on runs left to the game's downloader | C7 | C8 |
|---|---|---|
| priority list, not client state | 6 (5 layouts, `effect_prt_1011_white.plist`) | 1 (`effect_prt_1011_white.plist`) |
| main list, not client state | 26 (17 layouts, 4 effect roots, 5 layouts with absent files) | 9 (4 effect roots, 5 layouts) |

**What remains, all files in no source.** No placeholders, wildcards or aliases were added:
- `30156BattleEffectLawdarcUBB003Obj_R.sam`: its page `…_R_280x280.png` exists nowhere.
- `83027BattleEffectPunchBackR.sam`: its image table names `83027BattleEffectPunchBack R_154x459.png`
  and `…_723x368.png`, with a space. The unspaced `83027BattleEffectPunchBackR_*` pages of the same
  sizes exist. Correcting the definition is a content decision for review; nothing was changed.
- `effect_prt_021.plist` (`021.png`) and `effect_prt_1011_white.plist` (`(null).tiff`): no embedded
  texture.
- Five layouts whose entries name files no source holds:
  - `layout_banner_video_tutorial.csv` (`video_list_frame1/2.png`);
  - `layout_battle_option.csv` and `layout_battle_option2.csv` (`actionsset1/2.png`);
  - `layout_menu_option_lang.csv` (`lang_thai_btn_label1/2.png`);
  - `layout_titleinfoscene.csv` (`title_info_05_btn_label1/2.png`; 01–04 and 06 exist).
- No C16 log mentions any of these missing names. The provider-on runs declined the background
  download, so the downloaded roots were never processed.

## 4. Objective 4: the manifest bound to the client's own files

**Generator:**
- `inputs.client_files` lists the installed client's files the sets were derived from, each with its
  SHA-256: `package Assets/Resources/resourceMap`, plus 10 LocalState tables (the two
  `Ver3275_hdt2N6mr` parts, medal, stamp, stampset, Grand Quest, its map Ver68, challenge missions,
  raid map, raid points). Each file is listed once, even when two lists read the same table.
- `inputs.server_files` lists the 6 deploy/mst response tables.
- `derive_manifest` refuses any client build but `6253F827-01756000-00ED273D`.

**Broker:**
- Validates `client_files` with the DLL's path rule: a LocalState input is one name; a package input
  is `/`-separated names, each 1–255 characters, not `.` or `..`, with no control character or
  `\ : * ? " < > |` and no trailing `.` or space. The digest is 64 lower-case hex, with no duplicate
  in any case.
- Reports `inputs N` and one `I <root> <path> sha256:<hex>` row per input.
- A manifest naming none gives `complete 0`, "the manifest names none of the client's own inputs".

**DLL:**
- `ParseCoverage` requires `inputs`, validates each `I` row and its count, and never proves complete
  with zero inputs.
- Before either decision would be left out, `ClientHoldsInputs` hashes the client's own copies:
  LocalState under the game's writable path (`hooks::EnsurePrefix`), package files under the EXE's
  folder.
- `VerifyClientInputs` requires every digest to match and LocalState to hold no other version or part
  of a named table (`Ver<v>_<key>[_<n>].dat`, any case). Which copy the game reads could not otherwise
  be told.
- Anything else builds the census and asks the prompt, with the reason
  "the report's manifest was not derived from this client's own files: …".
- Every decision re-hashes; nothing is cached.
- **Compatibility:** a Session 17 DLL reads a Session 18 report as not a report (unknown field
  `inputs`) and keeps the stock census and prompt. The Session 18 DLL reads a Session 17 broker's
  report the same way.

**The startup hooks themselves are unchanged.** The Session 17 two-stage validation and overlap guard
are left as they were:
- `main.cpp` is byte-identical to its pre-session copy.
- `PrepareImage`, `AttachImage`, `VerifySitesSince` and `SnapshotSites` are unchanged.
- The one addition to `Startup_Attach` is the package-folder assignment before `AttachImage`.
- Their negative cases pass as before: `TestStartupAttachOrder`'s 11 controls, and
  `check_attach_overlap.py --negative`'s 6 models.
- The new input rule has its own negative cases: 5 proxy mutants, the decision tests, and the live
  `other-client` cases.

**Offline:** `scripts/check_client_inputs.py` applies the same rule. All match the Session 18 manifest
(read only):
- C16: its LocalState and its own package folder;
- C1;
- the installed client.

**Generation changes stay incomplete as before.** A report must name the provider's adopted epoch and
generation, with nothing pending (`SameGeneration`), checked again after the fetch.

## 5. Objective 1: the result screen's continuation cue

Not a regression. The result top (fn 0xA25610) creates its only button (`sub_sss_r_btn1/2.png`, the
`going_end` labels) inside `if (MissionRepeatConfigInfo flag +0x14)` (0xA2586F–0xA25879, skipped to
0xA259D9):
- the singleton is MissionRepeatConfigInfo (RTTI);
- of the stores into +0x14, only 0x90B274 can write non-zero, in the quest-repeat option screen fn
  0x90B060;
- so outside repeat mode the stock screen has no Next control and the player taps, exactly what C16
  showed.

Two further checks:
- `battle_next_btn.png`, the one "next" image a result-area entry names, is referenced only by the
  exclusion-table initializer and exists in no source.
- The result layouts name no next/touch element (`next_btn` belongs to `layout_mission_result_share.csv`).

Proof: `python out/qa/current/session18/check_result_cue.py` → 15 passed, 0 failed.

## 6. Commands and results (October 22)

Runner: `bash out/qa/current/session18/build/run_s18_suites.sh`. It prints the hashes first; logs are
`out/qa/current/session18/suite_s18_*.log` and the console log is `build/run_s18_suites_console.log`.
The final consolidated run is the one in the console log; §7 gives the binaries it hashed.

| Check | Result |
|---|---|
| `python scripts/test_startup_coverage_manifest.py` | **71 passed, 0 failed** |
| `python tools/bin/x86_startup_rules_check.py` | **129 passed, 0 failed**; mutants `build/rules_check_mutants.py`: **7 of 7 caught** (`build/rules_check_mutants.log`) |
| `python tools/bin/x86_bundle_exclusions.py --check` | 203 slots, identical to the binary |
| proxy unit `offline-proxy/build/tests/content_provider_tests.exe` | **677 passed, 0 failed, 5 skipped** (Session 17: 625/0/5; the skips are the live integrations, each run below). Includes `TestStartupAttachOrder` on the real EXE with the real Detours: unchanged |
| proxy mutants `python out/qa/current/session18/build/s18_mutants.py` | **5 of 5 caught**: the census trusting the report, no version scan, digest unchecked, inputs not needed, `inputs` optional. Restored byte for byte (`build/mutant_sources_before.txt`), rebuilt 677/0/5 |
| `check_attach_overlap.py` [`--negative`], `check_startup_sites.py` [`--negative`] | PASS, negative controls PASS |
| retained proofs: `check_gate_flash_site.py`, `check_audio_reader.py`, `check_post_quest_sites.py` | PASS, 0 failures each |
| mocked harness `scripts/test_content_harness_startup.py` | **86 passed, 0 failed** (Session 17: 79). Corpus parity: the harness parser judges all 38 shared reports as the DLL does |
| coverage wire `scripts/test_content_coverage_wire.py <s18 exe> --real <C8> <s18 manifest>` | **70 passed, 0 failed**. The broker, the DLL's rule and the offline evaluation agree at every fixture state and name by name on the real tree: login 2,257 ok; background 72,189 ok, 5 absent, 9 unowned; 6 unknown; 11 I rows |
| live startup `scripts/test_content_startup_live.py <s18 exe> <tests> --real <C8> <s18 manifest>` | **17 passed, 0 failed** (Session 17: 11). New: a complete report on a client holding another copy of its master table, then another version beside it, builds the census and asks the prompt; the table restored → left out again |
| `scripts/check_content_policy.py <s18 exe> --policy <C8>` | **8 passed, 0 failed** (75,303 owned, 0 collisions, 0 unavailable) |
| C8 through the DLL, `scripts/test_content_family_live.py <s18 exe> <tests> <C8> <trace> --effect-report <C3> --policy-report <C8 report>` | **11 passed, 0 failed**; proxy 6,360 passed, 0 failed, 4 skipped; 36,321 game reads byte-exact; 5,629 traced names byte-exact |
| server regressions `python scripts/run_bugfix_regressions.py out/qa/current/session18/bin/gimuserverw.exe` | **56 entries, 2,269 checks passed, 0 failed, no failed entry** (§6.1) |
| `python out/qa/current/session18/check_result_cue.py` | 15 passed, 0 failed |
| `python scripts/check_client_inputs.py --manifest <s18> --localstate <C16 / C1 / installed LocalState> --package …` | all OK (read only) |

### 6.1 Server regressions

October 22, 07:06–07:22, against `995a088d…` (the runner hashed the executable):
- 56 entries, 2,269 checks passed, 0 failed, no failed and no expected-failure entry.
- The mission validator equals its recorded baseline: 3,117 findings, digest `7bae35bbe120`, the same
  as Session 16's run.
- The run includes the coverage wire suite on the small tree, the mocked harness and
  `test_startup_coverage_manifest.py` (71/0).

The summary is copied to `out/qa/current/session18/regressions/SUMMARY.md` / `.json`, because the runner
replaces `out/qa/current/SUMMARY.*` each time. The previous summary (Session 16, October 20) is kept in
`out/qa/current/session18/pre/qa_summary_before/`.

## 7. Hashes (QC: verify on disk)

| Artifact | SHA-256 |
|---|---|
| **isolated DLL, port 29960** `out/qa/current/session18/proxy_29960_s18/libcurl.dll` / `.pdb` | `cfb825da79de8ca7a68db12009e9a9c66f5181bdb0af38d6a7660d3b44aba545` / `0e302f482f5240e95f8153b05260e2e6d62210b6a57c87de84d5c45e569a5011` |
| port 9960 twin `proxy_9960_s18/libcurl.dll` / `.pdb` | `37d30448d215de69e40d250e34618677c06f068c962cd9eeabadfce383d8716a` / `76ca5c8d3f16b71b3aa76a359308b002d11b9064162a9d334c740158a2060086` |
| proxy test binary `offline-proxy/build/tests/content_provider_tests.exe` | `671ead24947ee1296a8834fe5864aacef9ed3c45148105519355b6ef2c898955` |
| **server** `out/qa/current/session18/bin/gimuserverw.exe` | `995a088d3c4b2a675464b4595cf0ef028d8392df8c592779931a65b66bdf53bc` |
| C8 policy / report | `388185d921265ca5197bc8e27671e36037e2d4703484116ec8cd2a7b118583ea` / `f5cd79a571bcf5aac4635cafd9dc6d1715506e7ad15d6ba6d6ca215b7f610802` |
| Session 18 manifest (its `generated` field holds the generation time, so regenerating it changes the bytes; the sets, unknown lists, exclusions, absent names and inputs come out the same) | `f9749ef2d0f16900765ea382ffa4a0e1afd9f535c69e9760f57c47ddc699b975` |
| unchanged: in-tree live server `out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe`; `offline-proxy/build/bin/libcurl.dll` / `.pdb`; C16 layout DLL; C1 DLL | `6ee7d454…`; `e24234c1…` / `31738788…`; `45a2c1b1…`; `09056ee5…` |

**Build hygiene:**
- Server: `build/build_server_s18.sh` (tag s18a) is the Session 16 recipe. It uses a temporary
  runtime-output override into `session18/bin`, builds `gimuserverw`, then removes the override.
  Afterwards the cache was clean and the in-tree executable unchanged.
- Proxy: `build/build_proxy_s18.sh` is the Session 17 recipe. It builds the test binary, then a
  temporary `-DOFFLINEMOD_PORT=29960`, then `-U` (the cache was identical to the saved copy), then 9960.
  `build/bin` was put back to its pre-session contents.
- Port constants: the 29960 DLL pushes 29960 four times and 9960 once; the twin pushes 9960 five times.
  Both contain the new reasons.
- Every staged binary is newer than every source it was built from.
- No timestamped build tree was created.

## 8. Remaining blockers (coverage stays incomplete)

1. **6 unknown lists:** the unit, weapon / summoner, item, area, gacha and effect request helpers,
   derived the way §2.2 derived the others.
2. **9 families and 5 absent names** (§3): files supplied, or definitions corrected (the PunchBackR
   space). Both are content decisions.
3. **A self-contained first launch:** the version channel must deliver every table the client keeps.
   The C16 fixture copied the installed client's tables, so it is asset-cold, not a fresh install
   (plan §8.4 item 4).
4. **Only then a client run with a complete report.** It would also exercise the input check in the
   real client: so far only unit tests and the live suite have run it, each against fixture folders.

## 9. Resume steps for Codex QC

1. Verify §7 on disk, and that C16 still holds `45a2c1b1…` and C1 `09056ee5…`.
2. Read the diffs against `out/qa/current/session18/pre/`.
3. Rerun `bash out/qa/current/session18/build/run_s18_suites.sh` (about 12 minutes).
4. Optionally run `python out/qa/current/session18/build/s18_mutants.py` (rewrites two proxy sources
   temporarily and restores them byte for byte; about 6 minutes) and
   `python out/qa/current/session18/build/rules_check_mutants.py`.
5. Optionally check the derivation against the binary: `python tools/bin/x86_startup_rules_check.py`.
6. The client observation is optional and does not repeat the October 21 controls:
   [CLIENT_SESSION_18_STEPS.md](CLIENT_SESSION_18_STEPS.md). It starts with the read-only
   `check_client_inputs.py` on C16; swapping the DLL is Codex's step.

## 10. Correction (QC, October 22): the manifest bound to the server's response tables

Directive: [CLAUDE_SESSION_18_CORRECTION.md](CLAUDE_SESSION_18_CORRECTION.md).

**Status: complete, and ready for Codex to QC independently and run the isolated visual route.**
Implemented and automated-tested; not client-run; not deployed; nothing committed or pushed. The
remaining lists, the consent screen and gameplay were not touched. No client was launched, swapped or
reset, and the live save was only read (the regression runner's backup copies).

| | |
|---|---|
| New server candidate | `out/qa/current/session18/bin_correction/gimuserverw.exe` `4eded167…` |
| Pre-correction server | `bin/gimuserverw.exe` `995a088d…`, kept as it was |
| Unchanged | the DLLs (`cfb825da…` / `37d30448…`), the proxy test binary, the C8 policy and the manifest |
| Report protocol | unchanged: the Session 18 DLL reads the corrected server's reports as before |

### 10.1 The finding, reproduced

`scripts/test_content_server_tables.py` (new, outside `out/`) builds an otherwise-complete fixture:
- a **copy** of `deploy/mst`: new files, never links. `deploy/mst` is only read, and the suite checks
  its six tables unchanged at the end;
- a manifest generated by `startup_coverage`'s own reader and rules from the copy's six response
  tables: 152 names;
- a content tree holding every one of those names, and a policy owning all of it.

It then changes one banner row's resource field (`1gDkL6XR`) to `banner_fixture_changed.png`, an asset
no source has, and restarts the server with the **old** manifest.

**On `995a088d` (`out/qa/current/session18/correction/repro_995a088d.log`): 15 passed, 13 failed.**
- Initialize now serves the new name, and the manifest regenerated from the served table requires it.
- Yet the broker still says `complete 1`, and the DLL's rule would accept it:
  `{'broker': '1', 'dll': True, 'offline': False}`.
- The other 12 failures are the same gap. These cases all still report `complete 1`:
  - an emptied table;
  - an unbound, partial, extra-table or wrong-digest manifest;
  - three malformed entries.

  The remaining four are the missing reason, status rows and log lines.

**A side effect of the first draft.** The first draft of the suite
(`correction/repro_995a088d_first.log`, 14/11) also restarted the server **without** the banner table.
- The cache's `LoadJson` throws: `Fatal exception during execution: Cannot read JSON file … file_open_failure`.
- Nothing listens, and the process stays behind as an unkillable zombie. It is PID 11928, running
  `995a088d` with the config `correction/server_tables/server/config.json`.
- `taskkill` reports "no running instance", and `Stop-Process` leaves `HasExited=True`. The memory
  notes record the same zombie for a missing system JSON.
- It holds that executable and the fixture save open until a reboot. It never bound a port, it is not
  the live server, and it touches nothing live.

The suite no longer restarts without a table: it tests deleted rows with an emptied table, and its
fixture moved to `correction/tables`.

### 10.2 The fix

**Server cache** (`gimuserver/drogon/ServerCache.cpp/.hpp`). Six tables load through `LoadStartupTable`.
It is the same `glz::read_file_json` call as `LoadJson`, plus the SHA-256 of exactly the buffer it
parsed, recorded in `startupTables()`:

| Table | Carried by |
|---|---|
| `dungeon_key_mst.json` | Initialize `4NG79sX1` |
| `banner_info_mst.json` | Initialize `Pk5F1vhx` |
| `general_event_mst.json` | UserInfo `Md9gG3c0` |
| `dbb_mst.json` | UserInfo `fTV3A0By` |
| `grand_mission_mst.json` | CampaignMissionGet / CampaignStart `as8G75gK` |
| `grand_mission_map_mst.json` | CampaignMissionGet / CampaignStart `iyCP4N1h` |

`GimuServer.cpp` passes them to `ContentBroker::start`.

**Broker** (`gimuserver/content/ContentBroker.cpp/.hpp`):
- `loadManifest` validates `inputs.server_files`. Each entry must be exactly `{root, path, sha256}`,
  with root `mst`, a single file name (the DLL's path rule), a lower-case SHA-256, and no repeat
  (case-insensitive). Anything else refuses the manifest with a `manifest_error`.
- `bindServerTables` runs once, in `start`. Every named table must be one a response serves, with the
  same digest, and every served table must be named. A manifest naming none is unbound.
- The reasons, problems listed in name order:
  - `the manifest names none of the server's response tables (inputs.server_files): it cannot be bound to what this server's responses serve`
  - `the manifest does not match the response tables this server serves: banner_info_mst.json: derived from sha256:1bef0893006ece45..., served sha256:89b14f6e4ce71178...`
  - also `X: not named (served sha256:…)` and `X is not a table any response serves to a startup list`
- `coverageText` ranks this reason right after the build check, ahead of unknown lists: a manifest
  derived from other rows describes other requirements.
- `/status` adds `server_tables N`, `server_binding 0|1` and one `T	mst	<file>	sha256:<hex>` row per
  served table. The server log names each served table and states the binding verdict.

**Generator and offline tools:**
- `scripts/startup_coverage.py` gains:
  - `response_tables()` and `served_tables(mst_root)`;
  - `server_binding(manifest, served)`, which gives the broker's text word for word;
  - the `server_files` schema in `validate_manifest`, and `evaluate(…, served)`;
  - the response root as a parameter (`manifest` / `evaluate --server-mst`).
- `scripts/check_client_inputs.py` applies the same rule. It is a preview only: a running broker binds
  to what its own cache parsed.
- `scripts/run_bugfix_regressions.py` runs the new suite.
- [CLIENT_SESSION_18_STEPS.md](CLIENT_SESSION_18_STEPS.md) names the corrected server, and S2 now
  expects 19 `OK` lines.

### 10.3 Are the tables immutable after startup?

Yes, for the life of the process:
- `ServerCache::Setup` is the only writer. It runs once, from `GimuServer::initAndStart`
  (`GimuServer.cpp:60`). Nothing reloads it: there is no other caller and no file watch on `mst_root`.
- The responses send the cached rows as they are:
  - Initialize copies its cached response and changes no dungeon-key or banner row (`Initialize.cpp:26`);
  - UserInfo copies its cached response (general_event untouched) and sets `dbb_mst = cache.dbbMst()`
    (`UserInfo.cpp:58`, `:298`);
  - the campaign responses copy both grand tables (`Campaign.hpp:212-213`).

  The suite checks this live: the six tables' rows in Initialize, UserInfo and CampaignMissionGet
  derive exactly the names the files do.
- So the recorded digests describe every response this process sends. The manifest is also read once,
  in `start`, so the binding is decided there, once per process (epoch).
- The content generation counts asset-tree changes only; it is independent of the binding. The report
  still names the epoch and generation it describes.
- A table edited or deleted on disk while the server runs changes nothing served, so the report must
  not change either. The suite checks this live: rewritten, then deleted, the served rows, the status
  and the report stay identical. A restart (a new epoch) binds the new snapshot, and the suite's restart
  after the edit reports the mismatch.

### 10.4 Compatibility

- **Old or unbound manifests** (no `server_files`, as in Sessions 16 and 17) are never complete.
- **The report protocol is unchanged.** The Session 17 and 18 DLLs refuse unknown report fields, so
  the binding reaches the DLL as `complete 0` plus the reason.
- **The new status lines are ignored by the old readers.** They are unknown keys, or T rows without a
  space, which the DLL's `ParseStatus` and the harness pass over. Shown live: proxy 6,360/0/4 in the
  family case, 17/0 in the live startup suite.
- **Real coverage stays incomplete.** The Session 18 manifest is bound to `deploy/mst`
  (`server_binding 1`, and the offline check agrees). The report still says
  `6 startup list(s) the manifest could not derive`, with:
  - login 2,257 ok;
  - background 72,189 ok, 5 absent, 9 unowned;
  - the same 14 problem names and 6 unknown lists.

### 10.5 Tests and results

Runner: `bash out/qa/current/session18/build/run_s18c_suites.sh`, run 12:48–13:02 against
`4eded167…`. It prints the hashes first. Logs: `out/qa/current/session18/correction/suite_s18c_*.log`;
console: `build/run_s18c_suites_console.log`.

| Check | Result |
|---|---|
| `test_startup_coverage_manifest.py` | **93 passed, 0 failed** (Session 18: 71). New: 11 malformed server-file schema cases, 7 binding cases, a restored binding named in another case, `served_tables` / `response_tables`, and the folder the generator reads |
| `test_content_server_tables.py <exe>` (new) | **28 passed, 0 failed**; on `995a088d`: 15 passed, 13 failed |
| `test_content_coverage_wire.py <exe> --real <C8> <s18 manifest>` | **87 passed, 0 failed** (Session 18: 70). The fixture names the served tables (status T rows, `server_binding 1`). A stale digest and no server tables are `complete 0` with `server_binding`'s reason word for word. The server refuses the 11 new schema cases. The real tree is bound and incomplete as before |
| `test_content_startup_live.py <exe> <tests> --real <C8> <s18 manifest>` | **17 passed, 0 failed**; the DLL's decisions are unchanged |
| `test_content_harness_startup.py` | 86 passed, 0 failed |
| `test_content_broker_wire.py <exe>` | 82 passed, 0 failed |
| `check_content_policy.py <exe> --policy <C8>` | 8 passed, 0 failed |
| `test_content_family_live.py …` (as in §6) | 11 passed, 0 failed; proxy 6,360 passed, 0 failed, 4 skipped |
| mutants `python out/qa/current/session18/build/s18c_server_mutants.py` | **6 of 6 caught** (below) |
| `check_client_inputs.py` on C16, read only | all `OK`, including `server tables: bound to the 6 response tables` (`correction/check_client_inputs_c16.log`). On a copy with the changed banner row: `DIFF` with the broker's reason, exit 1 |
| server regressions `python scripts/run_bugfix_regressions.py out/qa/current/session18/bin_correction/gimuserverw.exe` | **57 entries, 2,339 checks passed, 0 failed, no failed entry**. The new suite is the 57th entry |

The six mutants:
- the digest ignored;
- unnamed tables ignored;
- an unbound manifest accepted;
- the binding not consulted;
- any root accepted;
- the digest not taken from the parsed bytes.

The runner restored both sources byte-identical (new mtimes), and the candidate was rebuilt afterwards.

Regressions, 13:03–13:21: the mission validator equals its recorded baseline (3,117 findings, digest
`7bae35bbe120`). The summary is copied to `out/qa/current/session18/correction/regressions/`. The
summary it replaced (Session 18, 995a088d) is kept in `correction/pre/qa_summary_before/`.

Not rerun, because their binaries and inputs did not change:
- the proxy unit suite on its own (it runs inside both live suites);
- the DLL site checks;
- the x86 rules and exclusion checks.

### 10.6 Hashes (QC: verify on disk)

| Artifact | SHA-256 |
|---|---|
| **server** `out/qa/current/session18/bin_correction/gimuserverw.exe` / `.pdb` | `4eded167851c4b30eb0ae5bf23833a2b08bb6adb873c0d8c35ba2a9ba8e19856` / `eb6df7bd3ab66203dd027dda4a8e5f1f153c6ef1375fac75516de471136dde64` |
| unchanged: pre-correction server `bin/gimuserverw.exe`; DLLs 29960 / 9960; proxy test binary; C8 policy / report; manifest | `995a088d…`; `cfb825da…` / `37d30448…`; `671ead24…`; `388185d9…` / `f5cd79a5…`; `f9749ef2…` |
| unchanged: in-tree live server; `offline-proxy/build/bin/libcurl.dll` / `.pdb`; C16 layout DLL; C1 DLL | `6ee7d454…`; `e24234c1…` / `31738788…`; `45a2c1b1…`; `09056ee5…` |

Source and session files:

| File | Change | sha256 |
|---|---|---|
| `gimuserver/drogon/ServerCache.hpp` | +25 −0: `StartupTable`, `startupTables()` | `85d3373080ace8f7a19960ae2516e4d4813119ac5fdc6b0c28b9a98a3ff01064` |
| `gimuserver/drogon/ServerCache.cpp` | +54 −6: `Sha256Tag`, `LoadStartupTable`, the six loads | `2da63ff2a14aac5fbed5add269f6901b42f6aaca5b7d72ef14983bff3a5ed0c8` |
| `gimuserver/drogon/GimuServer.cpp` | +6 −2: the tables passed to the broker | `4d7a90d63a7cc64afbe7d817981efd8f1842ade3a645072a32752d64fc023e85` |
| `gimuserver/content/ContentBroker.hpp` | +38 −4: `ServedTable`, `StartupManifest::serverInputs`, `start(…, served)` | `a797f5a25e0d9ad6a5ccc0ea073ae91286e9676aa228ac832cd4668583161277` |
| `gimuserver/content/ContentBroker.cpp` | +134 −32: the `server_files` schema, `bindServerTables`, the reason, status lines, log | `87e45389a0280464d3140c1d2bd6fc0febe5883de84f3c240c6bbb1de05f5b66` |
| `scripts/startup_coverage.py` | +108 −20 | `b6f362aaf28f26cd361152993fbfa6db2c8045dfda129d17714df3edb8b1412e` |
| `scripts/test_startup_coverage_manifest.py` | +120 −29 | `a9f25ba44fa8cc82d35f676eb4a62db14ae28bf1a80ce654ca29e45f8f33c4a7` |
| `scripts/test_content_coverage_wire.py` | +43 −2 | `2b8dd7073efe757dc1c99142906175cca24553e9e8d62e23e2c70e36cf1ae5c2` |
| `scripts/test_content_server_tables.py` | new, 361 lines | `8740147af3a0f8a3ee1c84387657e6c890883311abcc3165a394aa5b52563cce` |
| `scripts/check_client_inputs.py` | +14 −10 | `71e132b4e784ae74570f9b4f73f2b58ebd1dddfce6cd6c8ecdc684a720a77b30` |
| `scripts/run_bugfix_regressions.py` | +1 −1: the new suite registered | `063e17cadb5f33a82e3cb7af85c333af155904a662bfaf838450cbe84ecab939` |
| `docs/CLIENT_SESSION_18_STEPS.md` | +14 −7: the corrected server | `98674c0c377dac25d117f78a128144a26c64aad1084b5b2be350f1579a1664c3` |
| `out/qa/current/session18/build/build_server_s18c.sh` / `run_s18c_suites.sh` / `s18c_server_mutants.py` | session scripts | `b946cae9…` / `94919066…` / `48c7e02b…` |

Pre-edit copies: `out/qa/current/session18/correction/pre/` (`SHA256SUMS`, 13 files). Line counts are
against those copies.

**Build hygiene:**
- `build/build_server_s18c.sh` is the Session 18 recipe with its own output folder. It writes only
  `bin_correction`, or `bin_mutant` for the mutation controls, and refuses any other folder.
- The temporary runtime-output override was removed after every build, and the cache was clean each
  time. The in-tree live executable (`6ee7d454…`) and `bin/` (`995a088d…`) were unchanged after every
  build.
- The first build (12:23–12:27) was a full rebuild, because `ServerCache.hpp` is in the precompiled
  header.
- The candidate was rebuilt (12:47–12:48) after the mutation runner restored the sources, so it is
  newer than every source it was built from.
- `bin_mutant/` holds the last mutation control (m6, `046edef5…`) and `NOT_A_CANDIDATE.txt`.
- No timestamped build tree was created.

### 10.7 Resume steps for Codex QC

1. Verify §10.6 on disk, and that C16 still holds `45a2c1b1…` and C1 `09056ee5…`.
2. Read the diffs against `out/qa/current/session18/correction/pre/`.
3. Reproduce the finding:
   `python scripts/test_content_server_tables.py out/qa/current/session18/bin/gimuserverw.exe` exits 1
   with the 13 failures above. With `bin_correction/gimuserverw.exe` it passes (about 80 seconds).
4. Rerun `bash out/qa/current/session18/build/run_s18c_suites.sh` (about 15 minutes).
5. Optionally rerun the regressions (about 18 minutes) and `build/s18c_server_mutants.py`. The mutant
   runner rewrites two server sources temporarily and restores them byte for byte (about 15 minutes).
6. The isolated visual route: [CLIENT_SESSION_18_STEPS.md](CLIENT_SESSION_18_STEPS.md). It now names
   the corrected server; the DLL, policy, manifest and every expected line are unchanged.
7. Zombie PID 11928 goes away at the next reboot. `correction/server_tables/` can be deleted after that.
