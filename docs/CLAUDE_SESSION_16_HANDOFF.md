# Claude Session 16 handoff — startup coverage contract, C7 families, C16 cold package

**October 21 Codex QC: corrected candidates cleared for the isolated C16 fallback visual
run, not deployment or startup-screen removal.** See
[QC_SESSION_16_FINAL_REVIEW.md](QC_SESSION_16_FINAL_REVIEW.md) for independent results.
The earlier QC reviews record superseded defects and partial-work status.

**QC correction pass -- finished, rebuilt and retested (Claude, October 20, 21:10-22:10).
Returned for Codex QC; not cleared for the client run until QC says so.** Every finding of both
reviews is addressed in code, with QC's reproductions kept as regression cases (§1). The rebuilt
candidates in §6 replace every earlier Session 16 hash; the pre-correction DLLs are known to the
harness only as SUPERSEDED. **The work is not complete:** with the shipped content the coverage
report is incomplete and cannot yet even be proven complete (§4), so the optimization stays
fail-closed -- on every real client the LocalState census and the background-download prompt stay
the game's own. Nothing was committed, pushed or deployed. The live save, the installed client, the
C1 package (its DLL `09056ee5…` verified in place), every existing cache and `deploy/` are
untouched; the C16 package carries the corrected DLL and is not registered or launched.

October 11-20, 2026 (resumed after usage stops on October 11 and October 20). Directive:
[CLAUDE_SESSION_16.md](CLAUDE_SESSION_16.md).

## 1. QC findings and their resolution

Original review, [QC_SESSION_16_REVIEW.md](QC_SESSION_16_REVIEW.md):

| # | Finding | Resolution (code) | Regression and negative tests |
|---|---|---|---|
| 1 | A manifest with a missing, empty or malformed required set was accepted (QC: a login-only and an empty-set manifest returned `complete 1`); the proxy trusted a bare `complete 1` | **Server** `gimuserver/content/ContentBroker.cpp:321-493` `loadManifest`: exactly `login` and `background`, each a non-empty array of distinct flat names; unknown keys and sets, an exclusion without a reason, a name both required and excluded, and an absent note for a name no set requires are refused (`manifest none`, `manifest_error`, `complete 0`). **Proxy** `offline-proxy/OfflineMod.win32/content/cp_core.cpp:403-565` `ParseCoverage`: a report is read only when well formed, and complete only when everything in it proves completeness (contract §9 of `CONTENT_PROVIDER_WINDOWS_CONTRACT.md`); `cp_provider.cpp` `FetchCoverage` passes the build it asked about. **Harness** `scripts/content_provider_client_test.py:780-888` `parse_coverage`: the same rule | QC's own probe against the rebuilt server: `missing_background complete=0`, `empty_sets complete=0` (`qc_manifest_probe_after.log`; QC had `complete=1` on s16c, `qc_manifest_probe.log`). `test_content_coverage_wire.py`: QC's two manifests and 15 more malformed ones refused by the broker AND the offline schema. `test_startup_coverage_manifest.py`: the 17 schema cases. `test_main.cpp` `TestStartupCoverage` (QC's bare claim and `A` rows unreadable; login-only, absent and other-build claims not complete) and `TestCoverageCorpus`; `tests/fixtures/coverage_reports.txt` (29 reports, QC's first) judged alike by the DLL and the harness (`test_content_harness_startup.py`) |
| 2 | Required names came from existing files: a missing dependency vanished (QC's `missing_atlas.plist` gave no PNG and no problem); category completeness unproven | `scripts/startup_coverage.py:223-263` `deps` returns every name a file names whether it exists or not -- a sprite plist's `<stem>.png` always, a particle's texture always without embedded data and, with embedded data, when some copy exists (cocos2d-x `CCParticleSystem::initWithDictionary`: named texture first, embedded data as the fallback); `:265-287` `closure` keeps names that do not resolve and records each as a problem; `:307-419` `build_manifest` / `derive_manifest` keep absent names required and list the 15 categories and general_event as `unknown` (enumerated from existing files, not derived from MST ids) | `test_startup_coverage_manifest.py` (45/0): QC's `missing_atlas` probe, three particle cases, 16 regenerations each without one required file -- the name stays required, is noted absent and the result is incomplete --, unknown categories and general_event. Negative control: the same test against the pre-correction generator fails (`manifest_test_negative.log`, CAUGHT) |
| 3 | Absent names (in no source) were accepted as satisfied | They are problem rows: `ContentBroker.cpp:1681-1804` `coverageText` (class `absent`, `P <set> absent <name>`; the old `A` rows are gone and now unreadable to the DLL and the harness); `startup_coverage.py:526-578` `evaluate` classifies them the same way | wire suite: an absent name is incomplete, one that appears is unowned (still incomplete), removed again it is absent; corpus "claims complete while names are absent from every source"; `TestStartupCoverage` "an absent claim is not complete"; the manifest test's absent cases |
| 4 | The census was left out on a complete report while the provider was still starting, before it had adopted the report's generation | `offline-proxy/OfflineMod.win32/content/cp_startup_logic.cpp:85-147` `DecideCensus`: stock unless the provider is Active, Active re-checked after the report arrives, and the report's epoch and generation equal the adopted ones with nothing pending (`SameGeneration`, :47); `:149-190` `DecidePrompt`: the same post-fetch checks | `TestStartupDecisions` (a starting provider with a complete report: built); `TestStartupProviderStates` (a delayed start that recovers, a start that never succeeds, a disabled broker, a report older than the adopted generation that later catches up, every non-complete corpus report against an Active provider); `TestLiveStartupCoverage` (a real broker before activation). **Mutation control:** restoring the bare-claim rule and the starting-provider skip fails 14 checks (563 passed, 14 failed; `build/tests_run_mutant.log`); restored with a new mtime: 577/0 |

Correction review, [QC_SESSION_16_CORRECTION_REVIEW.md](QC_SESSION_16_CORRECTION_REVIEW.md):

| # | Finding | Resolution |
|---|---|---|
| 1 (P1) | missing dependencies still omitted; categories enumerated from existing files | as finding 2 above |
| 2 (P1) | the offline evaluation disagreed with the server: absent names excluded from problems, unknown lists ignored (QC's probe -- absent files in both sets plus an unknown category -- returned `complete=True`) | `startup_coverage.py:433-483` `validate_manifest` mirrors `loadManifest` (an invalid manifest is never complete); `:526-578` `evaluate` classifies in the broker's order (owned, ambiguous, unavailable, unowned, absent, missing) and is complete only with no unknown list and no problem. QC's probe is the manifest test's "QC correction probe" case. Agreement: the wire suite compares the broker, the DLL's rule and the offline evaluation at every fixture state and, name by name, on the real tree |
| 3 (P2) | the harness reported `bfcp 2 / coverage 1 / complete 1 / generation 1 / end` as complete | strict `parse_coverage` (finding 1); QC's report is corpus case 1 and a mocked-harness case (unreadable); the CLI prints "the report itself says complete N" beside the proved verdict |
| 4 | candidates not rebuilt; parser, startup-race and wire negatives pending | server, both DLLs and the test binary rebuilt from the corrected sources (§2); manifest, offline evaluation and policy report regenerated; the C16 layout carries the corrected DLL; every focused suite rerun against the §6 hashes (§3) |

Assertions that encoded a defect were inverted, not removed: the unit case of a starting provider
with a complete report now expects the census BUILT; the check that the DLL asks for its own build
moved to after activation (a starting provider no longer fetches a report); the rewritten wire
suite keeps every case of its earlier version (removed file, withdrawn shared member, new
collision, absent appearing, unsupported and incomplete reports, generation change, recovery) with
absent names now expected incomplete. Nothing was relaxed or skipped to pass, and no placeholder
asset was added: the 67 absent names and 26 families stay gaps.

## 2. Builds (October 20, the handbook's isolated workflow)

| Artifact | How | Result |
|---|---|---|
| server `out/qa/current/session16/bin/gimuserverw.exe` | `build/build_server_s16.sh` (tag s16d): a temporary `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG` into `session16/bin`, `cmake --build` of `gimuserverw`, override removed | exit 0 (21:15); `b2552da2…`; cache clean (no runtime-output override); the in-tree `out/build/debug-win64/.../gimuserverw.exe` unchanged `6ee7d454…` (`build/build_server_s16d_console.log`) |
| proxy DLLs `proxy_29960_s16q/`, `proxy_9960_s16q/` | `build/build_proxy_s16.sh` (tag s16q): `-DOFFLINEMOD_PORT=29960`, build, `-U`, build 9960, `build/bin` restored | exit 0 (21:20); 29960 `8634857a…`, 9960 `86b062ef…`; CMake cache identical to the saved copy; `offline-proxy/build/bin` back to its pre-session `e24234c1…` / `31738788…`; the 29960 DLL pushes 29960 four times and 9960 once, as the accepted post-quest build (`build/build_proxy_s16q_console.log`) |
| proxy test binary `offline-proxy/build/tests/content_provider_tests.exe` | `cmake --build build --config RelWithDebInfo --target content_provider_tests` (`VCPKG_ROOT=C:/Users/Evan/BF/vcpkg`, MSVC 14.51 environment) | `4958210e…` (`build/tests_build_restored.log`) |
| C16 layout | `content_provider_client_test.py --identity c16 dll --file proxy_29960_s16q/libcurl.dll` | `content_client_c16/package/libcurl.dll` = `8634857a…`; the pre-correction `bf634d4b…` kept in the layout's DLL history; package not registered |

## 3. Tests (October 20, 21:12-22:10, against the §6 hashes)

All under `out/qa/current/session16/`; every server suite starts its own isolated server on a
fresh save; the runner printed the hashes first (`build/run_qfix_suites_console.log`).

| Check | Command (from `BF-WorkingDirRust`; `QA=out/qa/current/session16`) | Result |
|---|---|---|
| proxy unit suite | `offline-proxy/build/tests/content_provider_tests.exe` (`4958210e…`) | **577 passed, 0 failed, 5 skipped** -- the skips are the live integrations (broker, startup coverage, audio, family, visible swap), each run by a live suite below (`build/tests_run_restored.log`) |
| proxy mutation control | the bare-claim rule and the starting-provider skip patched back into `cp_core.cpp` / `cp_startup_logic.cpp`, test binary rebuilt | **563 passed, 14 failed** -- caught (`build/tests_run_mutant.log`); sources restored with a new mtime, rebuilt: 577/0/5 |
| manifest derivation and evaluator (offline) | `python scripts/test_startup_coverage_manifest.py` | **45/0** (`suite_q_manifest_offline.log`); against the pre-correction generator and evaluator the same test fails (`manifest_test_negative.log`: NEGATIVE CONTROL CAUGHT) |
| coverage wire, small tree and real tree | `python scripts/test_content_coverage_wire.py $QA/bin/gimuserverw.exe --real $QA/policies/family_policy_c7.json $QA/coverage/content_startup.json` | **56/0** (`suite_q_coverage_wire.log`): the broker, the DLL's rule and the offline evaluation agree at every state (baseline, missing file, shared member, collision, absent / appears / gone, unknown list, detail) and name by name on the real tree; QC's two manifests and 15 more malformed ones refused |
| QC's manifest probe | `python $QA/qc_manifest_probe.py` (QC's script, against s16d) | `missing_background complete=0`, `empty_sets complete=0` (`qc_manifest_probe_after.log`; QC on s16c: `complete=1` for both) |
| mocked harness | `python scripts/test_content_harness_startup.py` | **78/0** (`suite_harness_startup.log`): strict `parse_coverage`, QC's bare claim and old `A` rows unreadable, three "claims complete" reports incomplete, corpus parity 29/29, the superseded DLLs labelled |
| C7 policy | `python scripts/check_content_policy.py $QA/bin/gimuserverw.exe --policy $QA/policies/family_policy_c7.json` | **8/0** (75,286 owned, 0 collisions, 0 unavailable) |
| C7 families through the DLL | `python scripts/test_content_family_live.py <exe> <tests.exe> <C7> out/qa/current/session11/first_visit_downloads.json --effect-report out/qa/current/session13/family_report_c3.json --policy-report $QA/policies/family_report_c7.json` | **11/0** (proxy 6,270/0, 4 skipped); negative controls `3e842020` **9/2** and `a8e8caca` **10/1** (both must fail, both do) |
| startup decisions against a real broker | `python scripts/test_content_startup_live.py <exe> <tests.exe> --real <C7> <manifest>` | **11/0** (`suite_q_startup_live.log`): small tree complete -> census and prompt left out; a required file parked or another client build -> built / asked; restored -> left out again; real tree -> `live startup coverage: incomplete, generation 1, 67 absent-everywhere name(s); census built, prompt asked` |
| C16 harness smoke (no client) | `python $QA/harness_c16_smoke.py <exe> <C7> <manifest> $QA/fixtures/s16_cold_kit.sqlite` | **19/0** (`suite_q_c16_smoke.log`): the checklist's own commands -- READY with C7 and the manifest (75,286 owned), `coverage --expect incomplete` lists the 94 problem names the offline evaluation finds, `--expect complete` exits 1, restart keeps the save, broker off and no manifest read incomplete, park -> `login unavailable action_attack1.png` only, restore -> back; live save and seed byte-identical; 29960 free |
| expected CLI text | `python $QA/capture_coverage_text.py <exe> <C7> <manifest> <seed>` | written to `$QA/coverage_cli_expected.txt` (parts a, park, d, restore, e), quoted in the checklist |
| census / prompt static proof | `python offline-proxy/tests/fixtures/check_startup_sites.py [--negative]` | PASS, 55 checks; 6 of 6 mutations caught |
| retained proofs | `check_gate_flash_site.py`, `check_audio_reader.py`, `check_post_quest_sites.py` | PASS, 0 failures each |
| content suites | `bash $QA/build/run_content_suites.sh` | all pass (21:41-21:54; logs `suite_*.log`): broker wire **82/0**, generation QC **17/0**, shipped policy **8/0**, provider live **5/0** (proxy 607/0, 4 skipped), C7 family live **11/0** (proxy 6,270/0), audio live **12/0** (proxy 591/0 and 583/0), visible swap C2 **10/0** and page **10/0** (proxy 606/0 each), mocked harness **78/0**, disposable commands **5 tests OK** |
| server regressions | `python scripts/run_bugfix_regressions.py $QA/bin/gimuserverw.exe` | **56 entries, 2,226 checks passed, 0 failed, no failed entry** (21:54-22:08; the runner hashed the executable, `b2552da2…`); the mission validator equals its recorded baseline (3,117 findings, digest `7bae35bbe120`); includes `test_content_coverage_wire.py` 49/0 (small tree only), the mocked harness 78/0 and `test_startup_coverage_manifest.py` 45/0. Summary: `regressions_q/SUMMARY.md` (`eee7583f…`); the 17:32 summary for s16c stays in `regressions/` |
| save validation | `python scripts/check_client_fixture.py $QA/bin/gimuserverw.exe $QA/fixtures/s16_cold_kit.sqlite` (check only, no `--out`) | **16/0** (`suite_q_client_fixture.log`): squads drawable, no dangling unit, units 1/110, item box 23/100, 18 helpers in the picker and 8 on the Social list, mission 10 started with a helper and cleared, the post-quest state after a restart; the seed byte-identical (`554a80b7…`) |
| reproducibility | the manifest, policy and evaluation regenerated into the scratchpad from the same inputs | the policy `04995161…` and the offline evaluation `2a180327…` byte for byte; the policy report equal but for the output path it records; the manifest equal but for its `generated` timestamp; the manifest report `f8f7038b…` byte for byte |

Not run: any client (the checklist waits for QC); any check of C1 beyond its DLL hash.

## 4. Coverage verdict, and what is still unproven

Final C7 `04995161…` against manifest `3be2dae4…` (build `6253F827-01756000-00ED273D`); the
broker, the DLL's rule and the offline evaluation (`coverage/evaluate_c7.json` `2a180327…`) agree
name by name:

| Set | Required | Owned | Absent | Unowned |
|---|---:|---:|---:|---|
| login | 2,418 | 2,355 | 62 | 1 (`layout_skill_cutin.csv`) |
| background | 72,282 | 72,251 | 5 | 26 (4 effect families, 22 screen layouts) |

plus **16 unknown lists** (the 15 `createDownloadCheckList` categories and general_event). The
report: `complete 0`, reason `16 startup list(s) the manifest could not derive`; 94 problem rows
over 93 names -- 26 distinct unresolved families, `layout_skill_cutin.csv` being required by both
sets (earlier documents counted it twice as "27 families"). In the harness's disposable root (hard
links to the policy's files only) the 26 read `missing` instead of `unowned`; the names are the
same.
Absent means game_content does not hold the name: 56 exist in no source, 11 only in the app
package. Names: [OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) §8.4,
[OFFLINE_CONTENT_PHASE_C7.md](OFFLINE_CONTENT_PHASE_C7.md) §3. These counts are what the manifest
can see; the unknown lists may hide more, so they are not an estimate of the remaining work.

Unproven, and the evidence each needs before the bypass may engage on real content:

1. **The 16 unknown lists.** Their exact names, derived from the MST ids the way the client's
   request helpers build them (`GameScene::requestUnitFiles`, `requestItemFiles`, ... and
   `GeneralEventMstList::getResouceFiles`), each recovered from the binary and tested -- not
   enumerated from existing files.
2. **The 67 absent names.** For each, client evidence that the startup path does without it (never
   queued, queued and ignored, or -- for the 11 packaged ones -- the package copy counted as
   present), or the file supplied. The C16 run's `QUEUED` listing is the first source.
3. **The 26 unresolved families.** Their dependencies supplied, or the definitions / resourceMap
   entries corrected (content decisions).
4. **The client's master tables equal the manifest's inputs.** `F_RESOURCE_MST` `Ver3275` is
   byte-identical in the installed client, the C1 package and the server today; nothing checks it
   at runtime.
5. **A self-contained first launch.** The C16 fixture copies the installed client's master tables
   and account state: it is asset-cold, not a fresh installation. A true first launch needs the
   version channel to deliver every table the client keeps (147; 21 are staged).
6. **A client run with a complete report** (then a relaunch, a missing file, the provider off) --
   only after 1-5.

## 5. Which tests exercise the bypass, and which the fallback

| Exercises | Tests |
|---|---|
| **The bypass** (census or prompt LEFT OUT) -- decision level only, never on real content or a client | `content_provider_tests` unit cases with a fake broker (`TestStartupDecisions`, `TestStartupProviderStates`' recoveries, `TestStartupHooks`); `test_content_startup_live.py`'s small fixture tree (a complete manifest the test builds) against the real broker: complete -> census and prompt left out, restored -> left out again; `test_content_coverage_wire.py` baseline (a report the DLL's rule proves complete; no DLL decision) |
| **The fallback** (census built, prompt asked) on incomplete or unproven coverage | every other case: the parked file, another client build, broker off, no manifest, every malformed manifest and report, a starting / failed / stale provider; `test_content_startup_live.py --real` and `test_content_coverage_wire.py --real` on the shipped content; the C16 harness smoke; **the C16 client run** |

The C16 client run therefore tests the incomplete-coverage fallback, the C7 families visually and
the controls. It does not validate the removal of any startup screen.

## 6. Candidates and data (QC: verify on disk)

| Artifact | SHA-256 |
|---|---|
| server `out/qa/current/session16/bin/gimuserverw.exe` (s16d) | `b2552da2b531d762f4a09225b41807c9ef34cb136067e71492ca8fc0fab82435` |
| proxy, port 29960 `out/qa/current/session16/proxy_29960_s16q/libcurl.dll` / `.pdb` | `8634857aeb98d1ab2b269d4c92c23f5681ece9a76b633f084631bee8d4ca0095` / `6967a4e46c52fc5e413ad65d03a6a91c503f2deb5cdb2ed5461369c6b99d9d2d` |
| proxy, port 9960 twin `proxy_9960_s16q/libcurl.dll` / `.pdb` (switch-over only) | `86b062ef51234a0225118cff896e511516d19c31eb387d1fbc3d0ad5a094d0b1` / `cf7ac14ed2cc265ef0931c57183230e85ca1751d02bf99a5bc4caa040ef11d4c` |
| proxy test binary `offline-proxy/build/tests/content_provider_tests.exe` | `4958210e2aef06565d099192b3a381c1b88216cd442573cfc7492b4287f15d72` |
| C7 policy `policies/family_policy_c7.json` / report | `049951614c8b16a2cf7caf9320b9bdcdf768e6394377ff2e8584040d5e2d18f4` (unchanged) / `79d62c8354b52265187733300bf26c67f869c770d1ffef551a4031c50a748b26` (the same 57 legacy names; 35 reasons reworded by the corrected closure) |
| startup manifest `coverage/content_startup.json` / report | `3be2dae4220e4c82736beaa050c15b5e0e16d25258b96e5be770691305795a0c` / `f8f7038b1184bf67980d5a994f9e94dfb3c8c9c07504712c8c5a3dc7238e54b3` |
| offline evaluation `coverage/evaluate_c7.json` | `2a18032738fcb21b48a92361a2a90040295c5714b181b85df4361afcfbcc4edf` |
| C16 save `fixtures/s16_cold_kit.sqlite` / manifest | `554a80b7b531d2760393387791d44ff6a593d7f85db9c412832ba6f2758dd8d3` / `b2cf5dfa9b5f910e1c98b451d17e8da36af5e4d9a7a834e26908fc5ec05b19de` (unchanged) |
| C16 layout `content_client_c16/package` | 1,361 hard links to `BraveFrontierAppxClient`, identity `gumi.BraveFrontier.ContentTestC16` / `CN=BraveFrontier`, `libcurl.dll` = `8634857a…`; not registered |
| superseded, never use | server `9c1f6dce…` (s16c); DLLs `bf634d4b…` / `e5287b90…` (`proxy_*_s16/`, labelled SUPERSEDED in `KNOWN_DLLS`); test binaries `c34ca9d4…`, `fe30c3b0…` (QC's); manifest `7e6a7364…`, its report `1cd51358…` and evaluation `265a42b6…` (`coverage/superseded/`); C7 report `3343127a…` and policies `3e842020…`, `a8e8caca…`, `a296453a…` (`policies/superseded/`) |

## 7. What changed (whole session; sizes against the pre-session copies in `out/qa/current/session16/pre/`)

| Repo | Files | Change |
|---|---|---|
| server | `gimuserver/content/ContentBroker.{hpp,cpp}` (+54, +346) | startup manifest (`<system_root>/content_startup.json`, strict `bfcp-startup 1`: exactly `login` + `background`, non-empty, distinct, reasons required), per-generation coverage cache, `coverageText` (`absent` is a problem class; no `A` rows); `recomputeOwned` publishes the generation under the index lock |
| server | `gimuserver/controller/OfflineModController.{hpp,cpp}` (+10, +8) | `GET /offline_mod/content/v1/coverage?build=&detail=` |
| proxy | `OfflineMod.win32/content/cp_startup.{h,cpp}`, `cp_startup_internal.h`, `cp_startup_logic.cpp` (new; 20 + 156 + 89 + 261 lines) | the census hook and the prompt-read redirect, signature-gated, fail-closed; an Active provider required, re-checked after the fetch |
| proxy | `cp_core.{h,cpp}` (+45, +171), `cp_provider.{h,cpp}` (+8, +20) | strict `ParseCoverage` (`CoverageSet`, the proof rule, contradiction checks, the expected build), `BuildIdentity`, `Provider::FetchCoverage` |
| proxy | `cp_hooks_internal.h` (+43 -4), `cp_hooks_logic.cpp` (+97 -23) | decision counters: one totals line per scene, 20 samples per kind, full `QUEUED` listing, main-list asset vs client-state counts |
| proxy | `main.cpp` (+6), `OfflineMod.win32/CMakeLists.txt` (+4), `tests/CMakeLists.txt` (+1) | attach/detach and build wiring (CRLF kept) |
| proxy | `cp_signatures.h` (regenerated), `tests/fixtures/make_signatures.py` (+19 -1), `bf_2.19.6.0_signatures.json` | `kStartupSignatures` (9 sites; earlier tables byte-identical) |
| proxy | `tests/test_main.cpp` (+744), `tests/fake_broker.h` (+9), `tests/fixtures/check_startup_sites.py` (new, 301 lines), `tests/fixtures/coverage_reports.txt` (new, 29 reports) | unit, corpus, provider-state, ABI, signature and live cases; the static proof |
| proxy | `docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` | §2 rows, §3.4 observation, §3.5, §9 (rewritten for the corrections) |
| scripts | `startup_coverage.py` (new, 958 lines) | manifest derivation, C7 generator, `validate_manifest`, offline evaluator |
| scripts | `test_startup_coverage_manifest.py` (new, 219), `test_content_coverage_wire.py` (new, 403), `test_content_startup_live.py` (new, 109), `check_client_fixture.py` (new, 193) | §3 |
| scripts | `content_provider_client_test.py` (+302 -16) | `c16` identity, Session 16 `KNOWN_DLLS` (the pre-correction pair SUPERSEDED), `server --manifest`, strict `coverage` (`parse_coverage`), `snapshot --all`, faster `owned_targets` |
| scripts | `test_content_family_live.py` (+31 -3), `test_content_harness_startup.py` (+195 -3), `run_bugfix_regressions.py` (+2 -1: the coverage wire suite and the manifest test) | §3 |
| docs | this file, `CLIENT_SESSION_16_STEPS.md`, `OFFLINE_CONTENT_PHASE_C7.md` (new); `OFFLINE_STARTUP_TRANSITION_PLAN.md` §8, `BF_OFFLINE_SERVER_HANDBOOK.md`, `BUG_BACKLOG_STATUS.md` | QC's notices kept as written |
| packet-generator | none | its pre-existing ` M assets/net/items.kdl` (as on October 8) |

## 8. The failing test of October 11 (unchanged)

`test_content_family_live.py` failed `the trace names owned and legacy downloads -- (5630, 0)`:
C7 owned every traced download, so the test had no not-owned control left. Looking for one in the
generator's own report showed that its 35 "legacy" names **were owned anyway**: step 1 refused a
root whose closure is incomplete, but step 3 ("every other servable file") then owned the same root
as a single file -- a family declared ready because its root exists. Fixed in `generate_policy`: a
`refused` set (step 3 never owns a name a family rule refused; a self-check aborts generation
otherwise); screen layouts as step 1b (a layout is owned with its screens' resourceMap entries: 382
owned, 22 unresolved stay legacy, 129 login-bundle-only layouts owned alone); character animations
bundled whole. The test was strengthened, not relaxed: `--policy-report` makes every name the
generator left legacy a not-owned control, and the superseded policies are its negative controls
(`3e842020` fails 9/2, `a8e8caca` 10/1 -- rerun October 20 against s16d).

## 9. The C16 package and save

- `--identity c16` (`gumi.BraveFrontier.ContentTestC16_99p3jr0gh0z6w`), work folder
  `out/qa/current/session16/content_client_c16`. The player registers it and adds the loopback
  exemption (checklist step 0); `state` then copies the installed client's account state (`*.dat`,
  `*.xml`, `*.ini`, no assets) exactly as for C1 -- needed because the client keeps its own 147
  master tables and the server stages only 21, so an empty LocalState would not boot. "Cold"
  therefore means **no cached asset** and fresh Settings (the prompt unanswered), not a fresh
  installation.
- Save `s16_cold` is seeded on the first harness start from `s16_cold_kit` (level 22, Selena only,
  27/27 energy, 50 gems, 2,294,652 Zel, 685,202 karma, 2,290 friend points, units 1/110, item box
  23/100, 18 helpers in the picker, 8 on the Social list), validated on scratch copies (§3).
- The disposable root for C7 (75,286 hard links) is already built in the C16 folder by the smoke.

## 10. Remaining blockers and limitations

- **Not complete.** The unproven items of §4 stand; the bypass cannot engage on real content and
  has no client evidence. If content is later repaired so the report turns complete, the census
  and prompt would be left out without a new build -- QC may prefer the attach compiled out
  (`Startup_Attach` in `main.cpp`) until a complete report has been reviewed.
- **No client evidence of the corrected DLL.** The C16 run is the first; it checks the fallback,
  the C7 families visually, the first quest, a relaunch and both controls.
- Layout dependencies come from the resourceMap only; a screen whose code draws an image no entry
  names is outside the model, and the 129 login-bundle-only layouts are owned alone.
- Whether the 62 absent login names show short transitional screens (queued, failed, remembered?)
  is unknown until the C16 `QUEUED` listing.
- The REDOWNLOAD notice of the C6 run: the game's master-table corruption recovery after an
  interrupted launch is the most likely cause, not proven (the server log of that minute was
  overwritten); reproduction: close the test client during its login connection.
- Not done: deployment; a rebuild of the in-tree server; any change to C1; a client run.

## 11. Next

**For Codex QC, in order:**

1. Verify the §6 hashes and that C1 still holds `09056ee5…`.
2. Read the corrected sources (§1 references) and rerun §3: `bash out/qa/current/session16/build/run_qfix_suites.sh`
   (about 45 minutes) plus the coverage wire suite (§3); the negative controls must fail.
3. Confirm the verdict (§4) and the fallback / bypass split (§5); decide whether the attach should
   stay compiled in while coverage is unprovable (§10).
4. Then clear [CLIENT_SESSION_16_STEPS.md](CLIENT_SESSION_16_STEPS.md) for the player.

**The isolated client visual run** (the checklist, after QC; about 60 minutes; port 29960 only):

1. Step 0: close every Brave Frontier window and stop any harness; register the C16 layout
   (`Add-AppxPackage -Register …\content_client_c16\package\AppxManifest.xml`), add its loopback
   exemption (Administrator), `--identity c16 state`, `snapshot c16_prelaunch … --all` (expect 0
   owned, 0 other), `check …` (expect the corrected DLL's label and `0 item(s) outstanding`).
2. Part A: Terminal A starts the harness server with the C7 policy, the manifest, `--disposable`,
   save `s16_cold` seeded from `s16_cold_kit`; Terminal B `coverage --expect incomplete` (the exact
   lines are in the checklist); launch C16, count every download screen, answer **No** at the
   prompt, reach Home; `collect`, `snapshot c16_home`.
3. Part B: Mistral -> Adventurer's Prairie -> Start of Adventure with a helper, win, results;
   `collect`.
4. Part C: relaunch -- no prompt (No stored); open the quest list; `collect`.
5. Part D: `park action_attack1.png`, `coverage` (one more `unavailable` row), launch to Home,
   `restore`, `collect`.
6. Part E (last): restart the server with `--broker off`, `coverage` (`broker disabled`), launch,
   Home, Mistral; `collect`, `snapshot c16_after`.
7. Return the collected folders; the expected log lines per part are in the checklist ("What I
   check afterwards"). Never `LEFT OUT` with this content.
