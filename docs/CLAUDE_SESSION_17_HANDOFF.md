# Claude Session 17 handoff — the startup hook installation (attach order)

October 21, 2026. Directive: [CLAUDE_SESSION_17.md](CLAUDE_SESSION_17.md), after
[SESSION_16_CLIENT_RESULTS.md](SESSION_16_CLIENT_RESULTS.md) and
[QC_SESSION_16_FINAL_REVIEW.md](QC_SESSION_16_FINAL_REVIEW.md).

**Status: fixed and automated-tested; returned for Codex QC. Not client-tested, not deployed.**
The new isolated DLL is staged in `out/qa/current/session17/proxy_29960_s17/` and **not** swapped
into the C16 layout, which still holds the superseded `8634857a…`. Nothing was committed or
pushed. Not touched: the live save (`deploy/gme.sqlite`, unchanged since October 4), the
installed client, C1 (DLL `09056ee5…`, verified), the C16 registration, consent and LocalState
(last written 05:57 by the Codex attempt), the `s16_cold` save (still the seed, `554a80b7…`),
`deploy/`, the server (`b2552da2…`, not rebuilt), the signature tables and the coverage contract.
No client was launched or operated. Visuals are not claimed.

## 1. Root cause

The C16 client log (`out/qa/current/session16/content_client_c16/collected/20261021-055758/offlinemod_content.log`):

```
0.0 content provider hooks installed ... waiting for the server's broker at 127.0.0.1:29960
1.7 startup coverage path DECLINED: censusLookup: censusLookup: byte +0 is E9, expected 55. ...
```

- `main.cpp` `DetourAttach()` ran `ContentProvider_Attach()` and then `Startup_Attach()`.
- `ContentProvider_Attach` detours 12 functions in one transaction. One of them is the
  downloaded-name lookup at 0xE417E0 (`isFileAlreadyDownloaded`). Detours replaces its first
  whole instructions with `E9 rel32` to `Hook_IsFileAlreadyDownloaded`. Here that is
  `55 8B EC 51 56` (push ebp; mov ebp,esp; push ecx; push esi), exactly 5 bytes, no padding.
  The trampoline keeps those 5 bytes, followed by a jump back to 0xE417E5.
- `Startup_Attach` then compared `kStartupSignatures` with live memory. Its `censusLookup` row is
  the same function, because the startup proof relies on that lookup's null-map test. So byte +0
  read `E9`, not `55`, and the startup path declined.
- The loop stopped at that first mismatch. The client therefore never checked the four sites after
  it. The static proof below shows that none of them is touched.
- The log names the site twice because the old `Declined()` prefixed the name to a reason that
  already began with it. The new code does not.
- The earlier tests missed this. `TestStartupSignatures` and `check_startup_sites.py` read the
  untouched file, and the decisions were tested on their own. Nothing ran the attach sequence on
  an image the provider had patched.

Overlap and lifecycle, proved by `offline-proxy/tests/fixtures/check_attach_overlap.py` (new).
It reads the sources and the EXE and decodes each Detours replacement with capstone:

| Component | Patch | Bytes changed |
|---|---|---|
| content provider | `existsLocalFile` detour | 0x237EB0..0x237EB5 (6: `55 8BEC 83E4F8`) |
| content provider | `isFileAlreadyDownloaded` detour | **0xE417E0..0xE417E4 (5)**, inside startup's `censusLookup` [0xE417E0, 0xE417F2) |
| content provider | `getTexture` / `addLoadFile` / `addLoadFilePriority` detours | 5 each at 0x2353D0 / 0x634FA0 / 0x1049A0 |
| content provider | 7 cocos2d detours | another module |
| startup | census detour; prompt call rel32 | 0xE415D0..0xE415D4; 0x8E51A9..0x8E51AC |
| gate flash; post quest | trigger jmp; two branch opcodes | 0x8760EF..0x8760F3; 0xA25CD6; 0xA1F92E |

Every check meets the untouched bytes except one: after the provider, the startup check meets the
provider's lookup detour at `censusLookup` +0..+4. That is the client's failure, and the proof
reproduces it for the pre-Session-17 order. No two patches touch the same byte. Detach is
`Startup_Detach`, then `ContentProvider_Detach`; the decisions use the provider, so that order is
required and is kept.

## 2. The fix, and why it is safe

The sites are checked twice: before anything of this DLL patches the image, and again after the
provider. The second check allows exactly one change, the provider's own recorded detour.

1. **`Startup_Prepare()`**, new. `main.cpp` now calls it before `ContentProvider_Attach()`. It
   checks the EXE identity and all 9 startup signatures on the untouched image and keeps their
   bytes (`cp_core` `SnapshotSites`). The signatures are the same as before; nothing is masked or
   dropped.
2. **`ContentProvider_Attach`** now records its 12 detours after its commit, in
   `hooks::Env::detours`. Each record holds the site, the patched address, where the jump must
   lead (`DetourCodeFromPointer` of the hook, the address Detours itself jumps to) and the
   trampoline Detours returned. The records are cleared on detach.
3. **`Startup_Attach()`** runs after the provider, using `cp_core` `VerifySitesSince` with
   `kSharedSites = { censusLookup <- isFileAlreadyDownloaded }`:
   - Every site must hold exactly its kept bytes, masked bytes included.
   - At `censusLookup`, the provider's `isFileAlreadyDownloaded` detour must be recorded, with its
     target inside the site.
   - The site must hold exactly that patch: `E9` to the recorded destination, then `CC` to the
     end of the displaced instructions.
   - The trampoline must hold exactly the displaced original bytes, then `E9` to the first
     instruction left in place.
   - Every other byte must be unchanged.
   - Only then are the census detour and the prompt redirect installed, with the decisions bound
     to the provider last (unchanged).

Anything else logs one `startup coverage path DECLINED: <reason>`: a jump anywhere else, a missing,
misnamed, misplaced or absent detour, a trampoline that doesn't resume the original, an unrelated
byte, another build, or no provider.

Why this is safe:
- **Original bytes are still validated.** That happens in step 1, before any patch of this DLL,
  so it is exactly the check Session 16 intended.
- **Only the expected owner may change a site.** Exactly one site may differ, only by the
  provider's own recorded detour, and the check is byte for byte, including its trampoline. No
  other `E9` passes.
- **The composition keeps the proof valid.** `Hook_IsFileAlreadyDownloaded`
  (`cp_hooks_logic.cpp:401-414`) answers an owned name itself and passes every other name to the
  trampoline. The trampoline runs the original's displaced first instructions, then jumps back
  into the unchanged rest of the function, which still contains the null-map test at +0x0E..+0x11
  that the left-out census relies on. So `check_startup_sites.py`'s proof still holds for every
  name the provider doesn't own.
- **It stays fail-closed.** An unsupported binary declines in step 1. If the provider declined, or
  its records are absent, step 3 declines.
- **Rollback is unchanged.** A decline changes nothing. A failed prompt redirect rolls back the
  census detour. `DetachImage` unbinds the decisions first. The detach order is kept, and the
  regression shows the image returns to the file's bytes.
- **Nothing outside the attach changes.** The signature tables (`cp_signatures.h`), the decisions,
  the coverage parser and the server are untouched. The installed log line now also says the
  sites were checked before and after the provider.

## 3. Changed files

Sizes against the pre-Session-17 copies in `out/qa/current/session17/pre/` (`SHA256SUMS.txt`,
`README.txt`). Two of those copies were reconstructed by reversing the exact edits, because their
backup was missed.

| File | Change |
|---|---|
| `offline-proxy/OfflineMod.win32/main.cpp` (+8 -2) | `Startup_Prepare()` before `ContentProvider_Attach()` |
| `.../content/cp_core.h` (+53), `cp_core.cpp` (+159) | `CodeDetour`, `SiteSnapshot`, `SharedSite`, `SnapshotSites`, `VerifySitesSince` |
| `.../content/cp_hooks_internal.h` (+8), `cp_hooks.cpp` (+29) | `Env::detours`; the 12 detours recorded after the commit, cleared on detach |
| `.../content/cp_startup.h` (+7 -3), `cp_startup_internal.h` (+17), `cp_startup.cpp` (+105 -44) | `Startup_Prepare`; `PrepareImage` / `AttachImage` / `DetachImage` (image-parameterised, so the test drives the real code); `kSharedSites` |
| `offline-proxy/tests/CMakeLists.txt` (+4 -1) | `cp_startup.cpp` compiled into the tests; `detours` linked |
| `offline-proxy/tests/test_main.cpp` (+479) | `TestSiteSnapshots`, `TestStartupAttachOrder` (and `MappedImage`, `ProviderExeDetours`, `PreSession17StartupCheck`) |
| `offline-proxy/tests/fixtures/check_attach_overlap.py` (new, 260 lines) | the overlap and lifecycle proof |
| `offline-proxy/docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` (+50 -14) | §9: the two-step attach, the rule, the composition, the tests |
| `BF-WorkingDirRust/scripts/content_provider_client_test.py` (+14 -2) | `KNOWN_DLLS`: the Session 16 corrected pair SUPERSEDED (startup path declined); the `S17_*` pair |
| `scripts/test_content_harness_startup.py` (+11 -4) | registry checks for the Session 17 pair and the superseded one |
| `out/qa/current/session16/harness_c16_smoke.py` (+6 -2) | precondition: any known startup-path DLL (not the label text), so it still runs after the swap |
| `docs/CLIENT_SESSION_16_STEPS.md` (+56 -6) | the Session 17 notice, "Resume after Session 17" (R1-R3), the consent step A0, the exact installed line |
| `docs/BF_OFFLINE_SERVER_HANDBOOK.md` (+14), `docs/OFFLINE_STARTUP_TRANSITION_PLAN.md` §8.3 (+7 -3), this file | status |

## 4. Tests (October 21, against the §5 hashes)

Runner: `bash out/qa/current/session17/build/run_s17_suites.sh`. It prints the hashes first; its
console log is `build/run_s17_suites_console.log` and the suite logs are `suite_s17_*.log`.

| Check | Result |
|---|---|
| proxy unit suite `content_provider_tests.exe` (`b766d8c6…`) | **625 passed, 0 failed, 5 skipped**. The 48 new checks are 17 in `TestSiteSnapshots` and 31 in `TestStartupAttachOrder`. The 5 skips are the live integrations (broker, startup coverage, audio, family, visible swap); each runs in a live suite below. |
| `TestStartupAttachOrder` on the real EXE with the real Detours | Runs `main.cpp`'s order on the real EXE: `Startup_Prepare` passes on the untouched image. After the provider's five game-EXE detours, the lookup reads `E9 …` and its trampoline `55 8B EC 51 56 E9`. The pre-Session-17 rule then declines with **exactly** `censusLookup: byte +0 is E9, expected 55`, and so does checking the untouched image only after the provider. `Startup_Attach` installs: the census jumps to `Hook_Census`, the prompt call targets `Hook_PromptFlagRead`, the decisions are bound last, and a second attach is refused. Detaching in `main.cpp`'s order returns all 24 MB of the image to the file's bytes. The **11 negative controls** each decline and leave no edit in place: no records; the lookup's record missing; a jump to another hook; a changed relocated byte (+7); a changed null test (+15); an unrelated patch at `promptFlagRead`; a detour on the census where none is expected; the trampoline altered; no snapshot; no provider; another build. Restored, it installs again, and the image is the file again. |
| `TestSiteSnapshots` (synthetic) | The provider's detour is accepted with 5 and with 6 displaced bytes, and the old rule declines it. Ten declines: a detour where none is expected; an unrecorded or misnamed detour; a jump elsewhere; a detour outside the site; trampoline bytes changed; the trampoline jumping back elsewhere; non-CC padding; changed byte +7; changed byte +15; a recorded detour not in place. |
| mutation controls (`build/s17_mutants.py`, log `build/mutants_s17.log`) | **4 of 4 caught.** Pre-Session-17 site check: 14 fail, the first being "Startup_Attach after the provider: installed (censusLookup: byte +0 is E9, expected 55)". Any jump accepted: 2 fail. No trampoline proof: 3 fail. Whole shared site exempt: 4 fail. The sources were restored with a new mtime, rebuilt: 625/0/5, and both files hash as before (`build/mutant_sources_before.txt`). The first mutant-3 run crashed: the mutant wrongly attached, Detours made the trampoline read-only again, and the test's own restore write faulted. The test now re-protects before restoring, and the rerun failed cleanly. |
| attach overlap proof `check_attach_overlap.py [--negative]` | **PASS, 22 checks.** **6 of 6** negative models fail: the pre-Session-17 order; `Startup_Prepare` after the provider; an empty `kSharedSites`; an extra provider detour on the census; detach reversed; the test replay missing the lookup detour. |
| census and prompt static proof `check_startup_sites.py [--negative]` | PASS, 55 checks; 6 of 6 mutations caught |
| retained proofs: gate flash, audio reader, post-quest | PASS, 0 failures each |
| mocked harness `test_content_harness_startup.py` | **79/0** (one new registry check) |
| startup decisions against a real broker `test_content_startup_live.py <s16d> <tests> --real <C7> <manifest>` | **11/0**. Small complete tree: census and prompt left out. Parked file and another build: built and asked. Real tree: `live startup coverage: incomplete, generation 1, 67 absent-everywhere name(s); census built, prompt asked`. |
| C7 families through the DLL `test_content_family_live.py` | **11/0**. Proxy 6,318 passed, 0 failed, 4 skipped. 36,321 game reads byte-exact; 57 not-owned controls. |

Not rerun: the server suites and regressions (no server change); the coverage wire suite (server
side, unchanged); the C16 harness smoke (its runs use the C16 work folder's harness state; for
QC). No client run. The new candidate's real installation log is still to be produced, by the
registered client.

What the tests cannot show: the DLL's `DllMain` in the real AppContainer process. A desktop
process cannot host the game's DLLs. The regression runs the same startup attach code with the
same Detours patches on the real EXE bytes. The provider's own attach function runs only in the
client, so the first collected log of the resume run is the acceptance evidence for the order.

## 5. Hashes (QC: verify on disk)

| Artifact | SHA-256 |
|---|---|
| **isolated DLL, port 29960** `out/qa/current/session17/proxy_29960_s17/libcurl.dll` / `.pdb` | `45a2c1b18e3ad1da07db735282636bf62f52003c5552e43347ba05ea1a41c1c8` / `f84d949e9a4f171d169091c3484dba11e506a49fc1780b1c17717a2fa588739f` |
| port 9960 twin `proxy_9960_s17/libcurl.dll` / `.pdb` (switch-over only) | `5d2f3970fdde3a7a20f8a22126144ff4385bafa93fa2cd746534d2f532683d00` / `823b738ca0633b71655b986b532b63f5935a90608baecf86e6d5b8279784e114` |
| proxy test binary `offline-proxy/build/tests/content_provider_tests.exe` | `b766d8c654a1dff6b769ea5d2dfb5ea2ae952630ae2c3e9c0e979823ccdb440a` |
| server (unchanged) `out/qa/current/session16/bin/gimuserverw.exe` | `b2552da2b531d762f4a09225b41807c9ef34cb136067e71492ca8fc0fab82435` |
| C7 policy / startup manifest (unchanged) | `049951614c8b16a2cf7caf9320b9bdcdf768e6394377ff2e8584040d5e2d18f4` / `3be2dae4220e4c82736beaa050c15b5e0e16d25258b96e5be770691305795a0c` |
| C16 layout DLL now (not swapped) | `8634857aeb98d1ab2b269d4c92c23f5681ece9a76b633f084631bee8d4ca0095`, SUPERSEDED for startup-hook validation in `KNOWN_DLLS` |
| superseded | `8634857a…` / `86b062ef…` (Session 16 corrected pair: startup path declined after the provider's detour); `bf634d4b…` / `e5287b90…` (pre-correction); test binaries `4958210e…`, `fe30c3b0…`, `c34ca9d4…` |

Build hygiene: I built with `out/qa/current/session17/build/build_proxy_s17.sh`, which is the
Session 16 script with outputs under `session17` and its baseline and `bin_before` taken from
`session16`. It used the existing `offline-proxy/build` tree. It set a temporary
`-DOFFLINEMOD_PORT=29960`, built, ran `-U`, and the cache was identical to the saved copy. It
then built 9960. `build/bin` holds its pre-session `e24234c1…` / `31738788…` again.

The 29960 DLL pushes 29960 four times and 9960 once, like the accepted C1 and post-quest builds.
The twin pushes 9960 five times. Both contain the new reasons. Build logs are
`build/proxy_*_s17.log` and `build/build_proxy_s17_console.log`. No Session 16 or QC log was
overwritten: every Session 17 log is under `out/qa/current/session17/`. The live suites recreate
their scratch work folders on every run, and QC's own run did so too:
`out/qa/current/session16/startup_live/` (a fixture tree and two throwaway servers) and
`out/qa/current/content_family_live/`.

## 6. Resume steps

For Codex QC:

1. Verify §5 on disk, and check that C1 still holds `09056ee5…`.
2. Read the diff (§3). Rerun `bash out/qa/current/session17/build/run_s17_suites.sh` (about 6
   minutes). Optionally run `python out/qa/current/session17/build/s17_mutants.py`: it rewrites
   two sources temporarily and restores them byte for byte (about 8 minutes).
3. Optionally run the C16 harness smoke (no client): `python out/qa/current/session16/harness_c16_smoke.py out/qa/current/session16/bin/gimuserverw.exe out/qa/current/session16/policies/family_policy_c7.json out/qa/current/session16/coverage/content_startup.json out/qa/current/session16/fixtures/s16_cold_kit.sqlite`.

Then the isolated client run. Follow [CLIENT_SESSION_16_STEPS.md](CLIENT_SESSION_16_STEPS.md)
from "Resume after Session 17", with the game and the harness closed:

- **R1, swap the DLL:**
  `python scripts\content_provider_client_test.py --identity c16 dll --file out\qa\current\session17\proxy_29960_s17\libcurl.dll`.
  Expect `now Session 17 candidate … (sha256 45a2c1b1…, port 29960)`, and the old DLL kept in
  `dll_history`.
- **R2, snapshot:** `snapshot c16_after_attempt1 --policy … --all`. This records what the October
  21 launch left; expect 0 owned names.
- **R3, pre-flight:** `check …`. Expect the Session 17 label and `0 item(s) outstanding`.
- **Part A:** start the server command, run `coverage --expect incomplete`, then launch through
  the registered **[ContentTest C16]** shortcut.
  - **A0:** **the user** ticks the legal-age/parental-consent box. Automation must not.
  - After the title appears, run `collect`. The log must say `startup coverage path installed for
    build 6253F827-01756000-00ED273D: … Its 9 sites matched the untouched image before the content
    provider attached and again after it, the downloaded-name lookup (0xE417E0) carrying exactly
    the provider's own detour`.
  - A `DECLINED` line means stop and report: that is a declined hook, not the fallback.
- **Parts A-E** as written. With this content, the fallback shows as `LocalState census #…
  built … (startup coverage incomplete: 16 startup list(s) the manifest could not derive)` and
  `background-download prompt left to the player: …`. A `LEFT OUT` line would be a defect.
  Neither kind of stock behaviour proves a cache-free startup.

## 7. Remaining (out of scope, unchanged)

- **Still open from Session 16:** exact names for the 16 unknown startup lists; the decisions on
  missing content (67 absent names, 26 unresolved families); binding the master tables to the
  manifest's inputs; and a self-contained first launch.
- **The coverage report stays incomplete,** so the census and prompt stay stock on real content.
- **Cosmetic, left as it was:** the post-quest component's DECLINED line also prints the site name
  twice, the same way the startup line used to.
