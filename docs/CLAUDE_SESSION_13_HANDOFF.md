# Claude Session 13 handoff — content boundary, C3 effects, startup map, backlog status

Independent QC completed October 8: [QC_SESSION_13_REVIEW.md](QC_SESSION_13_REVIEW.md).
Targeted checks passed; player instructions and FILE_CHECK goal qualifications
were corrected. Candidate binaries and policy are unchanged; visual acceptance
is still pending.

October 8, 2026. Directive: [CLAUDE_SESSION_13.md](CLAUDE_SESSION_13.md).
State: a reviewable candidate for Codex QC. **Nothing deployed, committed or
pushed; no client was launched; no client data reset.** The live save is
byte-identical (`239f9e69…`), the player's in-tree server EXE (`6ee7d454…`)
was not relinked, `deploy/game_content` is unchanged (75,931 content files,
newest file 2026-10-02), the shipped policy `deploy/system/content_provider.json`
is unchanged, and the Word bug list was only read (`76e8949a…`). QA output is
under `out/qa/current/session13/` (plus the suites' own established folders).

| Directive part | Result |
|---|---|
| 1. Boundary audit | Reproducible inventory `scripts/content_inventory.py` (every name classed: owned / existing unowned / collision / unavailable; readers with x86 evidence; client census; first-visit trace). New x86 reader `tools/bin/pe_x86.py`. Corrected the contract's "scripts read by direct fopen" (they go through `getFileData`). [OFFLINE_CONTENT_PHASE_C3.md](OFFLINE_CONTENT_PHASE_C3.md) §1 |
| 2. Coverage expansion | **C3 policy**: every SuperAnim definition with all its pages, plus particle plists — 36,321 more names, **53,766** total (policy only; no DLL change). Every C3 name read through the real hooks exactly as the game reads it. **Broker defect found and fixed**: bundles share members and all-or-nothing was order-dependent. §2–§4 |
| 3. Startup / verification | Mapped (x86 + arm64) with the work each screen hides and first-launch evidence; **no startup change** (not needed at the screens, rest not proven); guarded plan D1–D5 with addresses, prerequisites and acceptance. [OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) |
| 4. Backlog | [BUG_BACKLOG_STATUS.md](BUG_BACKLOG_STATUS.md): every item quoted verbatim and classed; defensible moves and remaining checks |
| Client | One run, [CLIENT_SESSION_13_STEPS.md](CLIENT_SESSION_13_STEPS.md); fixture prepared and the exact command smoke-tested |

## 1. Audit (part 1)

Key numbers ([OFFLINE_CONTENT_PHASE_C3.md](OFFLINE_CONTENT_PHASE_C3.md) §1):
75,931 files / 75,478 flat names; 347 identical duplicates and 71 collisions
(6 aliased: the Town); 87 resourceMap names held nowhere (unavailable);
21,647 existing unowned names; the installed client's cache holds 71,073
content names (53,764 owned under C3), 152 client-state files and 103
zip-extracted text tables. Readers: images, CSV, SAM, plist, scripts and
fonts all reach the hooked `getFileData`; **audio bypasses it** (0xEC0030
`fopen_s`), the ZIP extractor writes client state. A full scan of the EXE's
`.text` found exactly three FileCrypt loops (two decode sites known and
handled, one encode), so single-image families are safe with the existing
hooks.

## 2. C3 (part 2)

`python scripts/gen_effect_family_policy.py --base out/qa/current/session11/family_policy_c2.json --out out/qa/current/session13/family_policy_c3.json --report out/qa/current/session13/family_report_c3.json`
(reproduces `5b9c5e52…` / `570a88eb…` byte for byte).

- **Contract** (x86): `.sam` via `getLocalPath` -> `LoadSuperAnimMainDef`
  0x62B270 (`getFileData` "rb"; two header layouts, chosen by the caller);
  pages `<sam dir>/<page>` -> `SuperAnimSpriteMgr` 0x632AC0 -> `addImage`; the
  client's own dependency rule `processSamAndCutSceneFile` 0x106B10 ->
  `getSamSupportingFiles` 0x62CBA0 = the image table. Particles:
  `GameParticle::create` 0x629990 (56 sites, `getLocalPath`) ->
  `createCCDictionaryWithContentsOfFile` -> `getFileData` "rt". Both cached
  raw.
- **Policy**: 4,226 SuperAnim bundles (one per group of definitions sharing
  pages; 35,165 files) and 1,155 particle bundles (1,156 files); 17
  definitions/plists left legacy with reasons (collisions, missing pages,
  packaged copies that differ, the Town already in C2, textures that exist
  nowhere). Packaged copies that read identically are owned (459 names).
- **Broker fix** (`gimuserver/content/ContentBroker.cpp` `recomputeOwned`,
  header comment in `.hpp`): 58 names of the inherited C1 unit families are
  in several bundles (elemental variants drawn with one CGG). The single pass
  judged bundles in name order, so withdrawing a later bundle took the shared
  layout from an earlier bundle already judged complete — that bundle kept
  being served without its layout (the white-blob failure bundles exist to
  prevent). The pass now repeats until nothing more is withdrawn; every
  member of an incomplete bundle is still reported unavailable. Reproduced by
  4 new checks in `test_content_broker_wire.py`: **e30e2adf 78 passed, 4
  failed; candidate 82/0**. Diff: `session13/broker_shared_bundles.session13.diff`
  (against the exact pre-edit text, `ContentBroker.before_session13.*`).

## 3. Startup (part 3)

`LoginScene::updateEvent` x86 0x8E2960, `createDownloadCheckList` 0x8DF880,
the three download scenes (0x15D070/0x15C860, 0xC7E800/0xC7F1A0,
0xC7DC20/0xC7E3C0), the resourceMap loader 0x16F7F0, the prompt
(0x8E519F/0x8E5294, 0x8E6A80/0x8E6B00). On a fresh package the foreground
FILE_CHECK downloads only the 103 text dictionaries (client state; 23.5 s);
the next transitional download (before the prompt) fetched 2,272 files, Home
761, the first mission 1,487 + 442. Under C3, 2,532 of the 5,612 traced
first-visit downloads are owned and 3 of 36 transitional windows disappear
("Yes": 7 of 19). Plan D1 (policy families with per-scene layout coherence),
D2 (an audio hook — never own `.mp3` before it), D3 (prompt, optional), D4
(text/MST: keep), D5 (cold install test before any claim).

## 4. Validation

Server candidate **90e3eff9** (new) with the proxy test binary **ad5cbc67**
(new, test target only), unless named. e30e2adf results are the control.

| Check | Command (from `BF-WorkingDirRust`) | Result |
|---|---|---|
| Server QA build (override removed afterwards; ContentBroker/its two includers recompiled) | `bash out/qa/current/session13/build_qa.sh` | 90e3eff9 in `session13/bin`; in-tree EXE `6ee7d454…`; cache clean |
| Broker suite, candidate / control | `python scripts/test_content_broker_wire.py <exe>` | **82/0** / e30e2adf 78 passed, 4 failed (the defect) |
| Generator, synthetic tree (no server) | `python scripts/test_gen_effect_family_policy.py` | 21/0; three generator mutations each caught, generator restored byte for byte (`mutate_generator.log`) |
| Generation QC; shipped / C2 / C3 policy on the real tree | `test_content_generation_qc.py`; `check_content_policy.py [--policy …]` | 17/0; 8/0, 8/0, 8/0 (C3: 53,766 owned, 0 collisions, 0 unavailable, every identity = SHA-256) |
| **Live C3 family** | `python scripts/test_content_family_live.py out/qa/current/session13/bin/gimuserverw.exe ../offline-proxy/build/tests/content_provider_tests.exe out/qa/current/session13/family_policy_c3.json out/qa/current/session11/first_visit_downloads.json --effect-report out/qa/current/session13/family_report_c3.json` | driver 9/0, proxy 5,973/0: 36,321 game reads byte-exact (1,156 text-mode, 30,932 pages by `<dir>/<page>`), none to the cache reader or a queue, no write; 2,542 traced names served |
| **SuperAnim page swap / restore / park / restore** | `python scripts/test_content_visible_swap.py <exe> <proxy tests> <C3> --name Win_300x300.png --like LevelUp_68x80.png --page` | driver 10/0, proxy 340/0: pinned inside the scene, the safe point released exactly the `<dir>/<page>` key, parking one page withdrew all 6 names of `Win.sam`'s bundle, restore served it again; real files and live save unchanged; publication 50.4 / 44.9 / 6.5 / 46.0 s |
| Live C2 family / provider live / C2 Archive-tile swap | as in Session 12 | C2 family 7/0 (proxy 5,948/0); provider live 5/0 (proxy 341/0); C2 Archive-tile swap 10/0 (proxy 340/0; 18.6 / 13.2 / 5.1 / 13.3 s) |
| Client command smoke | `python out/qa/current/session13/harness_c3_smoke.py <exe> <C3> out/qa/current/session13/fixtures/c3_effects_kit.sqlite` | 10/0: READY with 53,766 owned (39 s start to stop), seeded from the fixture byte for byte, kept across a restart; live save and fixture unchanged; 29960 free after |
| Full runner (52 entries incl. the generator test) | `python scripts/run_bugfix_regressions.py out/qa/current/session13/bin/gimuserverw.exe` | **52 entries, 2,013 checks passed, 0 failed**, no failed entry; validator at its 3,117-finding baseline (`session13/SUMMARY_runner_90e3eff9.md`; Session 12: 51 entries, 1,992) |
| Proxy standalone | `build\tests\content_provider_tests.exe` (in `offline-proxy`, `BF_APPX_DIR` set) | 311/0, 3 skipped (live cases) |
| Scale C2 vs C3 (e30e2adf; broker code path unchanged by the fix except the bundle loop) | `python out/qa/current/session13/measure_family_scale.py <exe> c2=<C2> c3=<C3>` | ready 15 -> 28 s; private 352 -> 455–461 MB; index 2.4 -> 9.0 MB (0.5 s); delta poll 1.1 -> 4.3 ms; warm read 1.3 ms; replace published 45–54 s (C2 13–21 s); 32-bit provider index 9 -> 25 MB |

Skipped by design: the client run; a DLL rebuild (no provider source
changed: all 11 `OfflineMod.win32/content` files hash as before,
`session13/proxy_sources_before.sha256`); a deploy.

## 5. Candidates and artifacts

| Artifact | SHA-256 |
|---|---|
| **Server candidate** `out/qa/current/session13/bin/gimuserverw.exe` | `90e3eff97739b0d5d5713cae983db9fc4cadaa7e105b8bdf009a81b1444b5ebc` |
| Server, Session 12 (control) `session12/bin/gimuserverw.exe` | `e30e2adff2f016393de962e8229066d111f2503c2418f3a648f633ea01ea97a4` |
| In-tree player server (untouched) | `6ee7d454d8b75f47fc0d6e8be44c5284871ab885b3f6299ac74f25ac6df08a1b` |
| **Proxy tests** `offline-proxy/build/tests/content_provider_tests.exe` | `ad5cbc67ba473365ecf2c6890c692ecae8609ef63745170521a7828f8f9870d5` (intermediate builds 80798648, baf3fe9d are superseded; every result above is from ad5cbc67) |
| `offline-proxy/build/bin/libcurl.dll`, unchanged / C1 DLL in the test packages, unchanged | `e24234c1…` / `523c77ed…` |
| **C3 policy** / report | `5b9c5e52d34adfbd41f1b9792747245d316c674289c5056587f6043119b8c8b3` / `570a88ebacdd17756334976f5ae6f5910eee901300e0ad1ef360c7924517b018` |
| **Client fixture** `session13/fixtures/c3_effects_kit.sqlite` / manifest | `e2244ed25a92381f1b1d0e168efe1e478554448c766e0b0e6f7c1ec6afd4e6bd` / `da323d2274bccb6d63fe2c74718815795c18c8217a1c18850b96c1a6c57475de` (from `small_roster_repaired` `402b2fb0…`, unchanged, + `gems 50`) |
| Inventory | `session13/inventory/inventory.json`, `inventory.md` |
| Bug list text, verbatim | `session13/bug_doc_text.txt` `8595eae3…` |

## 6. Changed files (this session's hunks; earlier uncommitted work preserved)

| File | Change | SHA-256 now |
|---|---|---|
| `gimuserver/content/ContentBroker.cpp` / `.hpp` | shared-member bundle fixpoint; comment | `4ae9255e…` / `6bbf332a…` |
| `scripts/gen_effect_family_policy.py` (new) | C3 generator | `8ad22928…` |
| `scripts/content_inventory.py` (new) | boundary inventory | `ef41adaf…` |
| `scripts/test_gen_effect_family_policy.py` (new) | synthetic generator test | `d6d5f422…` |
| `scripts/make_debug_fixture.py` (new) | validated fixture from debug-console commands | `86ab1186…` |
| `scripts/test_content_family_live.py` | `--effect-report`: every C3 name read the game's way; argparse | `882b5ddf…` |
| `scripts/test_content_visible_swap.py` | `--page` (`<dir>/<page>` path) | `3d0691e0…` |
| `scripts/test_content_broker_wire.py` | 4 shared-bundle checks | `137cfb74…` |
| `scripts/run_bugfix_regressions.py` | the generator test in `OTHER` | `868c7294…` |
| `offline-proxy/tests/test_main.cpp` | `read`/`fileexist` expectations and CRT oracle in the family case; memory readout; `sep f` in the swap case | `7979e352…`; diff `session13/proxy_test_main.session13.diff` against a reconstruction made by reversing these edits — its hash `8f91fd24…` does not equal Session 12's recorded `47f064a1…`, so that baseline is unverified |
| `offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` | §3.3 correction, §3.7, C3 scope, limits | `fb993a0e…` |
| `docs/OFFLINE_CONTENT_PHASE_C3.md`, `docs/OFFLINE_STARTUP_TRANSITION_PLAN.md`, `docs/BUG_BACKLOG_STATUS.md`, `docs/CLIENT_SESSION_13_STEPS.md`, this handoff (new) | | |
| `docs/BF_OFFLINE_SERVER_HANDBOOK.md` | Session 13 pointer paragraph | |
| `tools/bin/pe_x86.py` (new, git-ignored local tool) | x86 functions/callers/IAT/string users | `67c65672…` |

No KDL, migration, MST, archive, `deploy/` file or provider DLL source changed.
QA-only helpers (not deliverables) in `session13/`: `build_qa.sh`,
`rerun_content_suites.sh`, `rerun_proxy_final.sh`, `rerun_candidate.sh`,
`harness_c3_smoke.py`, `mutate_generator.py`, and a copy of Session 11's
`measure_family_scale.py` (results beside it). No new client results arrived
this session, so `SESSION_11_CLIENT_RESULTS.md` is unchanged.

## 7. Risks and unfinished work

- C3 is server-tested only; drawing in the real client is the open check.
  The live tests use fake game functions around the real provider and hooks.
- Scale (§4): replace latency 45–54 s at C3 and the larger index/delta cost
  are open follow-ups (the Session 11 rescan item); not optimised here.
- The broker fix changes which names are reported unavailable when bundles
  overlap; the shipped 21-name policy has no overlap, C1–C3 have 58 names.
- Startup: nothing changed. The x86 transitional handlers and
  `FileLoader::priorityDownloadExist` / `update` remain unmapped.
- Backlog: no gameplay code was touched; the classification rests on the
  cited suites, data and recorded player observations.
- The Session 12 client checks (repaired small roster, visible Archive swap)
  are still owed and unaffected.
