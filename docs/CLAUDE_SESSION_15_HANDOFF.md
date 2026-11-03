# Claude Session 15 handoff — October 9, 2026

QC follow-up: [QC_SESSION_15_REVIEW.md](QC_SESSION_15_REVIEW.md). Use the
corrected client checklist for the C6 timeout and separate Part C baseline.

For Codex QC. Nothing was deployed, committed or pushed; no live save, installed
client or client cache was touched. The registered [ContentTest C1] package
still holds its Session 10 DLL (`523c77ed…`); installing the candidate is the
first client step ([CLIENT_SESSION_15_STEPS.md](CLIENT_SESSION_15_STEPS.md)).

Order of work, per the October 9 override in [CLAUDE_SESSION_15.md](CLAUDE_SESSION_15.md):
#24 first (§1), then priority 1, sounds from RAM (§2), then the UI-content and
verification-screen priorities (§3). **#24 is not fixed until the player's run
after QC.**

**The candidate is one DLL** with both changes:
`out/qa/current/session15/proxy_29960_s15/libcurl.dll` `af96f41b2df3359cc2326bb57e58bb9c34610251e07d3a54bd7de2ab8f6e5200`
(isolated port; the 9960 twin `proxy_9960_s15` `78a95fb0…` is not for this run).
With the C3 policy (Parts A/B) it owns no sound, so the audio hooks only pass
through. The gate-flash-only build of §1 (`proxy_29960_gateflash` `f1334e14…`)
stays known to the harness as the fallback if QC accepts §1 but not §2.
Part C pairs the DLL with policy C6 (`family_policy_c6.json` `e3d29e25…`: C3 +
426 sounds + 3,439 Home/Summon images, §2-§3); the server binary is unchanged
(`session13/bin/gimuserverw.exe` `90e3eff9…`).

## 1. #24 — the reveal flash now follows the final gate's opening

### What the player's Session 14 run showed

[SESSION_14_CLIENT_RESULTS.md](SESSION_14_CLIENT_RESULTS.md): with the
energy-overflow fixture the stock scripts whiten an unopened gate (Omni) and
give the single black gate no final white (7-star); the Session 14 guard removed
the early white but the final white stayed missing (Omni, 7-star); 5-star and
the flat-energy Omni were correct. Both failures are the same defect: the
client's trigger counts every animation on screen.

### The trigger, and why the script layer cannot fix it

`SummonsAnimeScene2::updateEvent` (x86 RVA 0x875F20; arm64 0x169D080) runs, every
frame after `ScriptEngine::process`, the block 0x8760EF..0x8761C1: if the GLOBAL
`SuperAnimList` holds exactly 3 animations, no overlay exists yet, the third is
not Touch/Hold.sam and its frame (`cvttss2si [anim+1B0h]`) is ≥ 45, it creates
the white `fillRect` (0x8761C3; `fill.png`, layer 5) and stores it at
`[scene+238h]`; 0x876238 fades it in over 15 frames; nothing in this scene
removes it, so it stays until the reveal replaces the summon scene. The reveal
does not depend on the overlay: each frame updateEvent first asks the
`FileLoader` (singleton 0x1997B90, RTTI `FileLoader`) whether its downloads are
done -- if not it pushes a `DownloadScene` (ctor 0xC7DAB0, `[vtbl+1FCh]`) -- and
when the script engine's state stops being "running" (0x876020) it exits the
script (0x64B3C0) and replaces the scene with the unit reveal,
`UnitDetailCommentScene` (ctor 0xCA0EB0, `[vtbl+210h]`). (An earlier draft of
this paragraph called the 0x875FAB check the script engine; RTTI shows it is
the FileLoader.)

The final Open script draws exactly one gate pair: `GachaGateOpen{2..6}.sam` +
`GachaGateOpenAdd.sam` / `GachaGateOpen6Add.sam` (all 15 GachaEffectMst rows,
all 8 Open scripts). So the stock rule only works when exactly ONE other
animation is on screen ([other, gate, blend] = 3, the blend reaching frame 45):
with none it never whitens; with two, a waiting or changing gate is the third
entry (early white) and the opening makes four (no white). A script cannot
express "after the final gate opens" through a count it does not control; any
script-drawn white would be removed by `ScriptEngine::exit` (it disables every
script node, Session 15 RE `out/qa/current/session15/re/scriptengine_exit.txt`),
flickering away before the reveal. Hence the narrow client hook the brief allows.

### The fix (offline-proxy, compiled with the content provider, installed on its own)

`OfflineMod.win32/content/cp_gate_flash.cpp` overwrites the 5-byte
`call SuperAnimList::shared` at 0x8760EF with `jmp GateFlashThunk`. The thunk
(`cp_gate_flash_logic.cpp`) saves every register, asks `CP_GateFlashDecide(scene)`,
restores every register and jumps to one of the two places the stock block
itself ends at: 0x8761C3 (create the overlay) or 0x876238 (fade step). The
overlay, fade, input, script, cleanup and reveal stay the game's own code.

Decision (`gateflash::Decide`): white when no overlay exists and an **enabled**
animation whose path contains `GachaGateOpen` (case-insensitive) **that was seen
below frame 45 in this scene** reaches frame 45 (the stock frame, read with the
stock `cvttss2si`). The "seen below 45" arming makes a leftover opening animation
(first seen resting at its last frame) unable to fire, a disabled (being removed)
animation never counts, a frame with no opening animation forgets the armed set,
and a new scene starts empty. Nothing else on screen matters.

Safety:
- Installed only after the EXE identity (timestamp/size/entry) and three masked
  signatures match: `summonsUpdateEvent` (prologue), `gateFlashTrigger`
  (0x8760EF..0x87628D, 415 bytes, only the two string pushes masked) and
  `superAnimListShared`; plus the stock bytes `E8 9C B2 DB FF` at the site.
  Otherwise one `gate flash fix DECLINED` line and the stock trigger. Separate
  table `kGateFlashSignatures` (generator `tests/fixtures/make_signatures.py`),
  so the provider and the fix cannot decline each other.
- Not Detours (a mid-function branch, nothing of the block ever runs): raw
  5-byte write at DLL attach, before game code runs; stock bytes kept and written
  back on detach.
- The decision reads the list inside SEH; a fault continues at the fade step,
  writes the stock bytes back (stock trigger from the next frame) and logs once.
- Log lines (`%TEMP%\offlinemod_content.log`, collected by the harness): install
  or decline; per summon "the final gate is opening (…)", "white at frame 45 of
  … (N animation(s) on screen; the stock trigger fires on this frame too / would
  not have fired)", and once per scene "the stock trigger would turn the screen
  white now (…) -- not the final gate's opening, so no white".

Why the jump is equivalent (`tests/fixtures/check_gate_flash_site.py`, reads the
real EXE with capstone): the site is the 5-byte call; the block leaves only to
0x8761C3 / 0x876238 / 0x87627B; nothing can enter it -- a byte scan decoding every
offset of `.text` as each direct branch encoding (E8/E9 rel32, 0F 8x rel32,
EB/7x/E0-E3 rel8) finds none targeting its inside outside updateEvent, none
inside updateEvent but the block's own, and no 4-byte value anywhere in the image
(jump tables, function pointers, EH tables) points inside it; esp is balanced on
every exit (callee `ret N` read from the callees); the try level `[esp+58h]` is
-1 on entry and every exit; from either target the code reads no register value
the block computed (sub-register precise: only edi/esp/ebp plus ecx/edx, which
every path leaves call-clobbered) and no stack slot the block writes. Negative
controls (a target inside the block) fail 1 and 6 checks; the scanners find the
463 branch encodings that target `SuperAnimList::shared` (both updateEvent calls
among them) and the reader's address in its vtable.

*Correction made during the session:* the first version claimed "no jump anywhere
in the EXE lands inside the block" from a linear capstone disassembly of `.text`,
which stops at the first undecodable byte -- it covered 8.6% of the EXE. The
byte-level scans above replace it (`build/check_gate_flash_scan_controls.log`).

The C4 guard scripts are unnecessary with the fix (the count no longer matters);
the candidate pairs the DLL with the **C3** policy, i.e. the client's cached
stock scripts. C4 stays only as the no-DLL fallback: it removes the early white
but cannot give the final one.

### Tests and results

| Check | Command | Result |
|---|---|---|
| Static site proof (real EXE) | `python tests/fixtures/check_gate_flash_site.py` (offline-proxy) | **PASS, 0 failures** with whole-image byte scans (`session15/build/check_gate_flash_site.log`); negative controls with a target inside the block fail 1 and 6 checks (`check_gate_flash_site_negative.log`); scanner controls (`check_gate_flash_scan_controls.log`) |
| Proxy unit tests | `build/tests/content_provider_tests.exe` | **373 passed, 0 failed**, 3 opt-in live skips (`session15/build/tests_run.log`) |
| — new: decision | `TestGateFlashDecision` | early white never (waiting / changing gates with overflow + icon: the stock rule fires, the fix never in 120 frames); final white at frame 45 for black, rainbow, gold, stone, red openings with 0, 1, 2, 3 other animations and ripples; same frame as the stock rule where it worked (1 other); rapid taps; leftover openings never fire; disabled / overlay / new scene; NaN and huge frames read as INT_MIN like `cvttss2si`; SSO and heap paths |
| — new: thunk ABI | `TestGateFlashThunk` | entered by `call` from naked asm with sentinel registers: both continuations receive eax/ebx/ecx/edx/esi/edi and esp unchanged; log lines; a fault (unreadable list) continues at the fade step, writes the stock bytes back to a test page (protection restored) and stays off |
| — new: real binary | `TestGateFlashSignatures` | the three signatures match the on-disk EXE; site `E8 rel32` -> 0x631390; targets' first bytes; one changed byte (`cmp eax, 2Eh`) declines |
| Mutation controls | `session15/build/mutation_gate_flash.py` | stock rule restored: 12 checks fail; no arming: 4; thunk branch flipped: 5; source restored with a new mtime and rebuilt: 373/0 (`mutation_gate_flash.log`) |
| Gate model | `python scripts/test_gate_overlay_model.py --scripts out/qa/current/session14/gate_scripts` | **68 passed, 0 failed** (`session15/gate_model.log`) |
| — new section G | rule `state` = the DLL's decision | served (cached stock) scripts: **2,715 summons** (15 effects x 10 baselines -- 0-4 other animations, unrelated loops, leftover openings -- x normal/rapid/late x 30/60/144 fps x ripple on/off, plus a leaving icon): every summon whitens during its Open, at frame 45 of the final opening (45/24 s after it is drawn), before the reveal; never earlier. Stock, guarded and candidate script sets: quick matrix, same. Where the stock rule gives the designed white (56 summons, one other animation) the fix whitens on the same frame (the accepted 5-star). Negative controls: the stock rule reproduces both player reports (overflow + icon: every double gate early, every single gate never) and fails even with one other animation at 30 fps (early white over the single black gate) and with rapid taps (no white). Mutations caught: no arming (early), any gate counts (early), blend-only marker (missing) |
| C3 live family (real broker + hooks, rebuilt test binary) | `python scripts/test_content_family_live.py <exe> ../offline-proxy/build/tests/content_provider_tests.exe out/qa/current/session13/family_policy_c3.json out/qa/current/session11/first_visit_downloads.json --effect-report out/qa/current/session13/family_report_c3.json` | **driver 9/0, proxy 6,035/0**, 2 unrelated live modes skipped (`session15/family_live_c3.log`) |
| Harness startup (mocked) | `python scripts/test_content_harness_startup.py` | **49/0** (`session15/harness_startup.log`) |
| `dll` swap + rollback on a throwaway layout | `python out/qa/current/session15/build/check_dll_swap.py` | **8/0** (`build/check_dll_swap.log`); refusals checked by hand: live-port gate DLL, reviewed Session 7 DLL, an unknown file; the registered package was not touched (`523c77ed…` before and after) |
| Smoke of the exact client commands (no client) | `python out/qa/current/session15/harness_s15_smoke.py <exe> <C3> <overflow seed> <flat seed> out/qa/current/session14/mst_gate_test` | **24/0** (`session15/harness_s15_smoke.log`): READY on 29960, C3 53,766 names, gate scripts not owned, energy 100/27 and 27/27, Omni -> 900, 7-star -> 229, 5-star -> 13/15, 6-star -> 19/21 every pull; live save, seeds and policy unchanged; 29960 free after |

Not run: the full 54-entry `run_bugfix_regressions.py` (no server C++ changed;
its gate-model entry was run above); an actual client (the player's, after QC).

### Candidate

| Artifact | Path | SHA-256 |
|---|---|---|
| **DLL for the run (gate flash + audio), port 29960** | `out/qa/current/session15/proxy_29960_s15/libcurl.dll` (+ `.pdb` `61f6dc10…`) | `af96f41b2df3359cc2326bb57e58bb9c34610251e07d3a54bd7de2ab8f6e5200` |
| same, live port 9960 (not for this run; not QC-reviewed) | `out/qa/current/session15/proxy_9960_s15/libcurl.dll` (+ `.pdb` `1be51804…`) | `78a95fb022490e091b697a6dd744f061cc4294d25ad04f5fbd019110bb5b4279` |
| gate-flash-only fallback, port 29960 | `out/qa/current/session15/proxy_29960_gateflash/libcurl.dll` (+ `.pdb` `e11bf243…`) | `f1334e145f96aa72ad87337948905dc03413604b62db4bb248ef82e94ee5b679` |
| gate-flash-only, live port 9960 (not QC-reviewed) | `out/qa/current/session15/proxy_9960_gateflash/libcurl.dll` (+ `.pdb` `339e8ebb…`) | `0e41bffdd0c6539eec769e1e092a38d0df0986bd14109ef7956c7c4f5f325d1a` |
| Server (unchanged) | `out/qa/current/session13/bin/gimuserverw.exe` | `90e3eff9…` |
| Policy (unchanged, C3) | `out/qa/current/session13/family_policy_c3.json` | `5b9c5e52…` |
| Gate MST root (unchanged) | `out/qa/current/session14/mst_gate_test` | `gacha_effect_mst.json` `87c2113b…` |
| Seeds (unchanged) | `out/qa/current/session14/fixtures/s14_gate_town_kit.sqlite` (energy 100/27), `s14_gate_flat_kit.sqlite` (27/27) | `287e9b91…`, `554a80b7…` |

Built in the established tree `offline-proxy/build` (VS 18 2026, Win32,
RelWithDebInfo) by `out/qa/current/session15/build/build_proxy_gateflash.sh`
(gate-only) and `build_proxy_s15.sh` (final; identical steps, other folders):
temporary `-DOFFLINEMOD_PORT=29960`, then `-U` (cache **identical** to the saved
copy `build/proxy_CMakeCache_before.txt`), 9960 rebuild, and `build/bin`
restored to what it held before (`e24234c1…` DLL / `31738788…` PDB, the reviewed
Session 7 build). Both DLLs: 9 exports identical to C1; the 29960 build has the
same four `push 29960` immediates as C1; the thunk disassembles as written
(`pushad; push edi; call; test eax,eax; popad; jz; jmp [yes]; jmp [no]`).

### Open / for QC

- Client acceptance is the player's ([CLIENT_SESSION_15_STEPS.md](CLIENT_SESSION_15_STEPS.md)):
  overflow and flat energy; single black, rainbow -> black, gold/red -> rainbow;
  normal and fast taps. Each pull must show no white over a waiting gate and a
  white after the final gate opens, before the reveal.
- The white's start (frame 45 of the opening) is the stock design's moment, which
  the player accepted for 5-star in Session 14; no new timing was invented.
- An effect without an Open part would get no white under the fix (none exists:
  all 15 rows have one). A summon flow not using `SummonsAnimeScene2` is not
  affected.
- Deploying the fix to the player's normal client means installing a provider
  build (`OFFLINEMOD_CONTENT_PROVIDER=1`) such as the 9960 artifact above, after
  acceptance and its own review; the provider itself stays dormant unless the
  server's policy enables it. Not done.

## 2. Priority 1 — sounds from RAM: the audio reader hook (Phase C5)

Full record: [OFFLINE_CONTENT_PHASE_C5.md](OFFLINE_CONTENT_PHASE_C5.md). The
local-audio path was implemented (not the transitional fallback): the read
contract could be established on the binary.

**Readers traced (x86, with the arm64 equivalents from Session 14):** every
sound, music and effects alike, is decoded whole by the game's
`SGAudioHandler` override 0xEC0030 (installed at startup, vtable 0x1961B04 slot 0;
called from cocos `MediaStreamer::Initialize` 0x2EB50, which skips its own WAV
reader when a handler is set). The reader is the EXE's only `fopen_s` user and
does `fopen_s(&f, path, "rb")`, `fread(buf, 1, 210, f)` until 0, `fclose(f)` --
no seek, no other stream call, no streaming; the sound manager's capped
effect list makes a later re-open the "reopen/warm" case. The script dependency
processor is 0x106B10 (arm64 0x1CD9BC0): downloaded scripts' `.mp3` tokens are
checked with `existsLocalFile` and queued with `addLoadFilePriority`. All cached
sounds in both packages are byte-identical to game_content (204 and 352 files).

**Hook:** the provider patches the EXE's `fopen_s` / `fread` / `fclose` imports
(all or none). An owned, present LocalState sound opened for reading becomes a
RAM file of the provider's pinned, digest-verified bytes; fread / fclose serve
it and pass every real FILE* through. Removed or withdrawn -> `ENOENT`, never
the stale cached copy; provider idle/dormant or unowned -> the CRT's. Without
the patch in place, sound names are never owned. Two new provider signatures
(19 sites).

**Policies:** `scripts/gen_audio_policy.py`. Candidate C5 =
C3 + all 426 sounds (`out/qa/current/session15/policies/family_policy_c5.json`
`ea1e9e83…`, 54,192 names); `audio_only.json` (426, `e81cabf7…`) for the live test.
Owning all sounds also closes the C4 gap: every sound a gate script names is owned.
The harness refuses a sound-owning policy for a test DLL not known to hook audio
(`KNOWN_DLLS[...]['audio']`; only the two `_s15` builds).

| Check | Result |
|---|---|
| Reader contract on the EXE: `python tests/fixtures/check_audio_reader.py` (offline-proxy) | **PASS** (`session15/build/check_audio_reader.log`): one reference to the fopen_s import slot in the whole image (byte scan), handler vtable slot 0 = reader, "rb", FILE* only to fread (2 x 1 x 210) and fclose, no other stream call |
| Proxy unit tests | **411 passed, 0 failed** (4 opt-in skips; `build/tests_run_audio.log`): `TestAudioHooks` (38 checks) -- disposable EMPTY LocalState with the real CRT behind the hooks: cold byte-exact, CRT never opens the path, nothing written; warm repeat from RAM; existsLocalFile present + the dependency processor's queue entry dropped; unowned sound still queued; real FILE* interleaved with a RAM file; fread item semantics; write-mode tripwire; removed sound -> ENOENT, planted stale copy never opened; unhooked -> sounds stay legacy; dormant -> cached copy, uncached missing |
| Mutations: `build/mutation_audio.py` | stale fallback 2 failures, owning while unhooked 3, fread byte count 1; restored 411/0 (`mutation_audio.log`) |
| Real broker: `python scripts/test_content_audio_live.py <exe> ../offline-proxy/build/tests/content_provider_tests.exe out/qa/current/session15/policies/audio_only.json` | **12/0** (`session15/audio_live.log`): 424 sounds byte-exact (375 MB in ~7 s; RAM cache bounded at 46 of 48 MB), 5 warm repeats from RAM, 2 withdrawn -> ENOENT with stale copies untouched; dormant broker -> 3 planted cached copies, uncached missing; disposable LocalState and game_content unchanged |
| Harness smoke of Part C: `python out/qa/current/session15/harness_s15_audio_smoke.py <exe> <C5> <seed> <mst root>` | **15/0** (`harness_s15_audio_smoke.log`): the C1 DLL is refused C5 (no save seeded); with the capability simulated in-process only, C5 `--disposable` READY with 54,192 names, 0 collisions/unavailable; `swap bf253_se_gousei02.mp3 --like bf113_zin_title.mp3` and `restore` published; package DLL, game_content and live save untouched |
| Same smoke with C6 (the Part C policy, §3) | **18/0** (`harness_s15_c6_smoke.log`): 57,631 names; both Part C swaps (gate sound, summon background) and restore published |
| Real broker, rerun on its final port 19964 | **12/0** (`audio_live_final.log`) |
| C3 live family (final binary) | driver 9/0, proxy 6,073/0 (`family_live_c3_final.log`) |
| Proxy vs real broker, fixture root (final binary) | driver 5/0, proxy 441/0 (`provider_live_final.log`) |
| Visible swap end to end (final binary) | driver 10/0, proxy 440/0 (`visible_swap_final.log`) |

Client check: Part C of [CLIENT_SESSION_15_STEPS.md](CLIENT_SESSION_15_STEPS.md),
run with C6 (= C5 + §3's images): music and effects as usual from the server;
the swapped gate sound proves the provider's bytes play although the cache holds
the original; restore.

Remaining for cold-install claims: the client run; production pairing (a
deployed sound-owning policy must only meet this DLL generation -- the broker
cannot tell DLLs apart yet).

## 3. Priorities 2 and 3

### Priority 2 — the Home and Summon screens' images (Phase C6)

Full record: [OFFLINE_CONTENT_PHASE_C6.md](OFFLINE_CONTENT_PHASE_C6.md). Chosen
with `content_inventory.py` on the Session 11 trace because it removes a known
legacy download path: Home's entry queued 761 downloads, 704 still legacy under
C5, nearly all single images of `item`, `dungeon`, `gacha` (banners), `home`,
`summon/skill_icon`, `area`, `banner`. `scripts/gen_ui_family_policy.py` owns
those images one bundle per file, excluding flat-name collisions (6) and the 18
gate scripts, listing the 14 packaged copies with other bytes (a client draws
those only until Home downloads the server's copy). Families with dependencies
or unresolved collisions (`footer` frame plists, `common`, `common/button`,
`_dlcbundle` layouts and fonts) were not claimed.

| Check | Result |
|---|---|
| Inventory with C6 | first-visit demand downloads owned 3,938 of 5,630 (C5 2,745); Home entry 734 of 761, first mission 1,476 of 1,487; transitional windows: 5 gone (C3: 3) |
| Broker dry run `python scripts/check_content_policy.py <exe> --policy out/qa/current/session15/policies/family_policy_c6.json` | **8/0** (`check_policy_c6.log`) |
| Real broker `python scripts/test_content_family_live.py <exe> <tests> <C6> out/qa/current/session11/first_visit_downloads.json --effect-report out/qa/current/session13/family_report_c3.json` | **driver 9/0, proxy 6,073/0**: 3,938 traced owned names byte-exact (`family_live_c6.log`) |
| Part C smoke with C6 | 18/0 (above) |

C6 policy: `out/qa/current/session15/policies/family_policy_c6.json`
`e3d29e253a4330d1abbb6a49ba5e93bd3d09910a172e46ca05b112f86c2ce6fb` (57,631 names).

### Priority 3 — the verification screens: x86 routines resolved, exact patch points

Recorded in [OFFLINE_STARTUP_TRANSITION_PLAN.md](OFFLINE_STARTUP_TRANSITION_PLAN.md) §7.
Resolved this session: the x86 transitional handlers (`transitionalDLCScenePushHandler`
0x63EAB0, replace 0x63ED10), the six excluded next-scene types (ConnectScene,
LoadingScene, NoticeScene, TitleScene, GumiLiveScene, StepScene; plus a
TransitionalDLCScene as the current scene), `downloadFiles` at vtable +0x1B8,
the FileLoader singleton (0x1997B90, `shared` 0x634B80), the request-list test
that decides the screen, and the real `TransitionalDLCScene` constructor
**0x15C190** (the C1 plan's 0x15C120 was a std heap helper; corrected there).
Conclusion: transitional screens are purely the result of what a scene queued,
so ownership is the only safe lever; the login FILE_CHECK screen needs three
prerequisites before any guarded local-ready decision (coverage of the login
check list -- 1,373 legacy names left under C6; a broker coverage report; then
a census skip at 0xE415D0 with the FILE_CHECK wait at 0x8E2960 left with nothing
to wait for), plus a cold-package acceptance. Nothing was implemented: the
prerequisites are not met, so its initialization, missing-content and failure
behaviour cannot be demonstrated.

## 4. Changed files (all of Session 15)

Diff of everything against pre-session copies: `out/qa/current/session15/diffs/session15.diff`
(`make_diffs.py` rebuilds it; untracked files' pre-session text is reconstructed
by removing this session's blocks, each checked to occur once).

offline-proxy (dirty work preserved; CRLF kept):
- #24: new `OfflineMod.win32/content/cp_gate_flash.h`, `cp_gate_flash_internal.h`,
  `cp_gate_flash_logic.cpp`, `cp_gate_flash.cpp`, `tests/fixtures/check_gate_flash_site.py`;
  `OfflineMod.win32/main.cpp` (+6), `OfflineMod.win32/CMakeLists.txt` (+4 sources);
- P1: `OfflineMod.win32/content/cp_hooks_internal.h` (MemFile, audio typedefs and
  Env fields), `cp_hooks_logic.cpp` (HookClassify, Hook_FopenS / Fread / Fclose,
  OpenOwned), `cp_hooks.cpp` (the three IAT patches, detach, install line); new
  `tests/fixtures/check_audio_reader.py`;
- both: `tests/CMakeLists.txt` (+1), `tests/test_main.cpp` (gate flash tests,
  audio unit + live tests), `tests/fixtures/make_signatures.py` (`GATE_FLASH_SITES`
  and two audio-reader sites) and its regenerated `cp_signatures.h` /
  `bf_2.19.6.0_signatures.json` (the earlier 17 entries byte-identical);
  `docs/CONTENT_PROVIDER_WINDOWS_CONTRACT.md` (§2 rows, §3.3, §3.8 audio, §6, §8 gate flash).

BF-WorkingDirRust:
- `scripts/gate_overlay_model.py`, `scripts/test_gate_overlay_model.py` (#24 rule
  `state`, section G);
- `scripts/content_provider_client_test.py`: `dll --file`, four Session 15
  KNOWN_DLLS entries (`audio` flag on the two `_s15` builds), the audio-policy
  guard in `server`;
- new `scripts/gen_audio_policy.py`, `scripts/test_content_audio_live.py`,
  `scripts/gen_ui_family_policy.py`; `scripts/content_inventory.py` (the `.mp3`
  reader entry);
- docs: new `CLAUDE_SESSION_15_HANDOFF.md`, `CLIENT_SESSION_15_STEPS.md`,
  `OFFLINE_CONTENT_PHASE_C5.md`, `OFFLINE_CONTENT_PHASE_C6.md`; updated
  `BUG_BACKLOG_STATUS.md` (#24), `BF_OFFLINE_SERVER_HANDBOOK.md` (Session 15),
  `OFFLINE_CONTENT_PHASE_C4.md` (superseded note), `OFFLINE_STARTUP_TRANSITION_PLAN.md`
  (D2 status, §7), `OFFLINE_CONTENT_PHASE_C1_PLAN.md` (constructor correction).

QA-only (not durable source): `out/qa/current/session15/` -- build scripts and
logs, mutation scripts, smoke scripts, policies, inventories, RE notes.

## 5. Not run, open, for QC

- **Client:** everything here is pre-client. The player's run follows QC
  ([CLIENT_SESSION_15_STEPS.md](CLIENT_SESSION_15_STEPS.md): Parts A/B #24 on C3,
  Part C sounds and images on C6). #24 stays open until Parts A/B are observed.
- Not run: the full 54-entry `run_bugfix_regressions.py` (no server C++ changed;
  its gate-model entry, the harness startup suite and every content-provider
  live driver were rerun on the final binaries); a cold fresh package (plan D5).
- Production: the 9960 `_s15` DLL is built, not reviewed or deployed. A deployed
  policy that owns sounds must only meet DLLs with the audio hook; the broker
  cannot tell DLL generations apart.
- The C1 test package still holds the Session 10 DLL; step 0 of the client run
  installs the candidate (rollback printed by the same command). Its
  `disposable_root` is now built for C6 (harness scaffolding), so Part C starts
  without rebuilding it.
