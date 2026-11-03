# Offline client, Phase C4: the summon gate scripts

> **Superseded for #24 (Session 15, October 9).** The player's run showed the
> guard removes the early white but not the missing final white (the stock
> trigger counts every animation on screen). The fix is now the proxy DLL's
> gate flash patch ([CLAUDE_SESSION_15_HANDOFF.md](CLAUDE_SESSION_15_HANDOFF.md#1-24--the-reveal-flash-now-follows-the-final-gates-opening)),
> paired with the C3 policy and the client's cached stock scripts. C4 remains
> only as the no-DLL fallback; this document is kept as its record.

October 8, 2026 (Session 14). Directive: [CLAUDE_SESSION_14.md](CLAUDE_SESSION_14.md).
Why it exists: [#24's root cause and fix](CLAUDE_SESSION_14_HANDOFF.md#1-24-double-gate-whiteout).
A candidate **policy only** (like C2 and C3): no proxy (DLL) and no server
binary change. Evidence and bulky output: `out/qa/current/session14/`.
Labels as in the handbook: **x86** = `BraveFrontier.Windows.exe` RVAs
(`tools/bin/pe_x86.py`), **arm64** = `libgame.so`, **server-tested** = the
real broker and the real hook code with fake game functions, **modeled** =
`scripts/gate_overlay_model.py`.

## 1. Why the gate scripts need the provider

The fix for #24 changes 9 of the summon gate scripts (`gWait*.txt`,
`gChange(*).txt`). Corrected scripts cannot reach an installed client through
the download path: the client caches them in LocalState and never re-checks
(`existsLocalFile` has no version; [handbook client-cache notes](BF_OFFLINE_SERVER_HANDBOOK.md)).
Deleting caches is ruled out. Owned by the provider, the scripts are read
through the hooks instead, from a candidate root; with the provider dormant
or the bundle withdrawn the client reads its cached copies exactly as before
(rollback = the C3 policy, or `"enabled": false`).

This is also the D1 family the startup plan calls for ("scripts with the
assets their `param=` lines name", [OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) §5),
bounded to the scripts the gate fix needs.

## 2. Read and dependency contract

| Step | Evidence | Consequence for owning |
|---|---|---|
| Effect choice | server `Gacha.cpp` picks a `gacha_effect_mst` row by unit rarity; the client maps the id to its three scripts (`GachaEffectMstList::getObjectWithEffectID`) | the 14 scripts the table names are the family; `gOpenGold(SR)`, `gOpenGold(SSR)`, `gOpenRed(SSR)` are named by no row and stay legacy |
| Enqueue | `SummonsAnimeScene2::downloadFiles` (arm64 0x169BFB0) `addLoadFilePriority` for the effect scripts | the C1 enqueue filter drops owned names: nothing is downloaded |
| Read | x86 `SummonsAnimeScene2::initialize` (0x874D60): `getLocalPath` 0x236950 then `getCsvList` 0x235C60, three times (arm64 0x169C9A8.. `getCsvListForLocal`), appended into one script for `ScriptEngine::initEngine` | `getFileData("<LocalState>\X.txt", "r")`: the provider answers `existsLocalFile` and serves the text-mode read -- the same path it already serves for unit CSVs (C1, client-confirmed) |
| Parse | `AdventureSystem::advSet` 0xFD4198: commands between `*` and `#` | a stray BOM between commands is harmless (`gOpenGold(Rare7)` ships one; Session 13's run requested `GachaGateOpen6.sam`, which only that line names) |
| Assets named | id=42 SuperAnim (gate, blend, Touch.sam), id=46 particle plist, id=34 sound | every SuperAnim and plist is already owned by C3; the generator refuses a policy otherwise |
| Sounds | id=34 plays LocalState if `existsLocalFile`, else the packaged resource path; it never downloads. `FileLoader::processSamAndCutSceneFile` (arm64 0x1CD9BC0) parses a **downloaded** script's `*,type=PARAM` lines and queues missing dependencies | a script that is never downloaded no longer pulls its sounds. `bf1126_zin_act_union.mp3`, `bf210_se_water2.mp3`, `bf236_se_debility.mp3` are not packaged: on a package that has not cached them, the colour-change sounds would be silent until the audio reader is hooked (D2). Both test packages and the player's package already hold all five gate sounds |

Never own `.mp3` (the audio reader bypasses `getFileData`, x86 0xEC0030).

## 3. The policy

```
python scripts/gen_gate_scripts.py --out out/qa/current/session14/gate_scripts
python scripts/gen_gate_script_policy.py --base out/qa/current/session13/family_policy_c3.json --scripts out/qa/current/session14/gate_scripts --root out/qa/current/session14/c4_content --out out/qa/current/session14/family_policy_c4.json --report out/qa/current/session14/family_report_c4.json
```

C4 = C3 plus `gacha/<14 scripts>` in scope, `.txt` in `extensions`, one
all-or-nothing bundle `gate_scripts`, and `"root"` = the candidate content
root `out/qa/current/session14/c4_content/content`: hard links to
`deploy/game_content/content` for C3's 53,766 files and the 5 unchanged Open
scripts, new files for the 9 guarded Wait/Change scripts (53,771 links, 9
files). `deploy/` is only read; the shipped policy is untouched.

| Artifact | SHA-256 |
|---|---|
| `family_policy_c4.json` (53,780 owned names, 7,950 bundles) | `8bdcd08ec614cf53ced6b1383238ac8c70c9d6476b2d35d299260e5b8aa1562b` |
| base `session13/family_policy_c3.json` | `5b9c5e52d34adfbd41f1b9792747245d316c674289c5056587f6043119b8c8b3` |
| report `family_report_c4.json` | per script: served and original SHA-256, dependencies, sound availability |

## 4. Verification (server-tested; client pending)

| Check | Result |
|---|---|
| `check_content_policy.py <90e3eff9> out/qa/current/session14/c4_content --policy C4` | 8/0: 53,780 owned, 0 collisions, 0 unavailable, every name published with the SHA-256 of the copy served (`check_policy_c4.log`) |
| Live family (`test_content_family_live.py <90e3eff9> <ad5cbc67> C4 <trace> --effect-report <C3 report> --script-bundle gate_scripts`) | driver 11/0, proxy 5,973/0 (2 unrelated live modes skipped): **36,335 game reads byte-exact** -- 4,233 SuperAnim definitions, 30,932 pages, 1,156 plists and the **14 scripts in text mode `"r"` by the LocalState path**, 9 of them different from `game_content` (the provider serves the candidate, not the cache); 2,542 traced names byte-exact; no read reached the client cache's reader or a download queue; no write. Provider private bytes 10.6 -> 34.5 MB with the 53,780-name index (`family_live_c4.log`) |
| Harness smoke of the player's commands (`session14/harness_s14_smoke.py`) | 23/0: C3 and C4 runs READY with `--mst-root`, seeded from the Session 14 fixture; C4 publishes the guarded scripts, C3 owns none; planted Town lists and energy overflow reach UserInfo; Test Gate: Omni -> effect 900, 7 Star -> 229, every pull; live save, fixture and policies unchanged; 29960 released (`harness_s14_smoke.log`) |

## 5. Limits

- Client acceptance is pending ([CLIENT_SESSION_14_STEPS.md](CLIENT_SESSION_14_STEPS.md)).
  The live test uses fake game functions around the real provider and hooks.
- Sounds on an uncached package (§2). Not a regression on any existing package.
- The candidate root lives under `out/`. Shipping C4 means a deliberate
  decision: either update `deploy/game_content/content/gacha` with the
  generator's output (then the policy needs no separate root, and newly
  downloaded copies are correct too) or ship a root; both are deployment
  steps, not taken here.
- Scale is C3's plus 14 names: broker ready ~35 s in the harness smoke.
