# Offline client, Phase C5: sounds from RAM (the audio reader hook)

October 9, 2026 (Session 15, priority 1). Directive: [CLAUDE_SESSION_15.md](CLAUDE_SESSION_15.md).
A DLL change (offline-proxy) **and** a candidate policy; no server change.
Evidence: `out/qa/current/session15/`. Labels as in the handbook: **x86** =
`BraveFrontier.Windows.exe` / `libcocos2d_v2.2.5_Windows_8.1.dll` RVAs.

## 1. Why sounds were the gap

Phase C4 ([OFFLINE_CONTENT_PHASE_C4.md](OFFLINE_CONTENT_PHASE_C4.md)) showed that
owning a script removes the legacy downloader's dependency discovery for it, so
the sounds only that script names would be silent on an uncached client. Sounds
could not be owned at all: their reader bypasses `getFileData`, and owning a
name makes `existsLocalFile` answer "present", which stops its download.

## 2. The read contract (x86, proven on the binary)

- All sound loads, music and effects alike, go `SimpleAudioEngine::playEffect /
  playBackgroundMusic / preloadEffect` -> cocos `Audio::PreloadSoundEffect`
  (0x6320; it renames `.mp3` to `.wav`) -> `MediaStreamer::Initialize` (0x2EB50;
  full wide path) -> the game's `SGAudioHandler` (installed once at startup by
  0xEB0BD0, vtable 0x1961B04) slot 0 = **the reader 0xEC0030**. With a handler set,
  cocos' own WAV reader (CreateFile2/ReadFile) is never used.
- The reader renames `.wav` back to `.mp3` and decodes the whole file to 16-bit
  PCM: `fopen_s(&f, path, "rb")`, `fread(buf, 1, 210, f)` until it returns 0,
  `fclose(f)`. It is the EXE's only `fopen_s` user (one reference to the import
  slot in the whole image; no `GetProcAddress` name), and `f` reaches nothing but
  `fread` / `fclose` -- no seek, tell, eof or other stream call
  (`offline-proxy/tests/fixtures/check_audio_reader.py`, byte-level scans).
- No streaming: a sound is decoded at first play / preload. The game's sound
  manager (0x64EFC0) keeps a capped list of loaded effects and unloads the oldest
  (`unloadEffect`, 0x64FAB0), so a sound is opened and decoded again later --
  the "reopen" case. Effects volume 0 skips the play entirely (no open).
- Paths: the play wrappers pass the absolute LocalState path when
  `existsLocalFile` says the sound is downloaded, else the packaged resource
  path (Session 14, arm64 id=34; x86 0x64EFC0 receives the built path).
- Cached sounds are plain MP3, byte-identical to game_content's files: all 204
  in the [ContentTest C1] LocalState and all 352 in the installed client's
  (read-only comparison; no FileCrypt).
- The script dependency processor is x86 0x106B10 (arm64
  `FileLoader::processSamAndCutSceneFile` 0x1CD9BC0): for a downloaded `.txt`
  script it scans `*,type=PARAM` lines, splits `param=` on `:` and queues every
  `.png/.jpg/.mp3/.plist/.sam` token `existsLocalFile` reports missing, through
  `addLoadFilePriority` -- the C1 enqueue filter's hook.

## 3. The hook (offline-proxy `OfflineMod.win32/content/cp_hooks*.cpp`)

The EXE's import slots `msvcr120_app!fopen_s`, `fread`, `fclose` (module-scoped,
like the existing `fopen` / `remove` tripwires), all three or none; fread and
fclose patched first (they pass every real FILE* through), fopen_s last.

- `fopen_s` of an owned, present LocalState name, read mode: the provider's
  bytes (pinned version, digest-verified, through the RAM cache) become a
  `MemFile`; the FILE* the game holds is its address, registered. Absent
  (removed or withdrawn) -> `ENOENT`, null FILE*, the cached copy is **never**
  opened. Superseded -> fails until the next scene change, as getFileData does.
  A write mode on an owned name is a tripwire (counted, then the CRT's).
- `fread` / `fclose`: a registered MemFile is served from RAM (the CRT's item
  semantics); any other FILE* goes to the CRT untouched.
- Provider idle, starting or dormant, or a name it does not own: the CRT's own
  `fopen_s`, exactly as without the proxy.
- If the audio patch is not in place (`audioHooked` false), sound names
  (`.mp3`, `.wav`) are never owned, whatever the index says: existsLocalFile,
  isFileExist, getFileData and the queue filter all treat them as the game's.
  The install line says `audio reader on (sounds may be owned)` or `OFF`.
- Signatures: `audioReader` (0xEC0030 prologue) and `audioReaderOpen`
  (0xEC01B1..0xEC0203, the open and the first read) join the provider's table
  (now 19 sites).

## 4. The policy

`scripts/gen_audio_policy.py`: every `.mp3` under game_content (426: `sound/` 419,
`sound/bgm/` 4, `sound/se/` 3; 385 MB, largest 8.4 MB, under the provider's 16 MB
entry cap) as its own `snd_<name>` bundle, `.mp3` added to the extensions.

| Policy | Names | SHA-256 | Use |
|---|---|---|---|
| `out/qa/current/session15/policies/audio_only.json` | 426 | `e81cabf7…` | the live test |
| `out/qa/current/session15/policies/family_policy_c5.json` | 54,192 (C3 + 426) | `ea1e9e83…` | the client candidate |

The 42 packaged sounds all exist in game_content: 39 byte-identical; the title
music/voice `bf001_title.mp3`, `bf026_map_load.mp3`, `bf113_zin_title.mp3`
differ -- owning them serves game_content's copy, which is what a client that
downloaded them already plays. With C5 every sound a gate script names is
owned, so an owned script (C4) no longer loses its sounds.

**Only with a DLL that hooks the audio reader.** An older DLL would answer
"present" for owned sounds and the bypassing reader would find nothing for an
uncached one (silent). The harness refuses such a pairing for test packages
(`KNOWN_DLLS[...]['audio']`); the DLL itself keeps sounds legacy if its patch
failed. A broker-side capability check for unknown DLLs does not exist yet
(production pairing is the maintainer's; see §6).

## 5. Verification (server-tested; client pending)

| Check | Result |
|---|---|
| Audio reader contract on the EXE (`check_audio_reader.py`) | PASS: one fopen_s reference (the reader), handler vtable slot 0 = reader, mode "rb", FILE* only to fread x2 (1 x 210) and fclose, no other stream call |
| Unit (`TestAudioHooks`, fake broker, REAL CRT behind the hooks, DISPOSABLE empty LocalState) | cold read byte-exact, CRT never opens the path, nothing written; warm repeat from RAM; existsLocalFile present and the dependency processor's queue entry dropped; unowned sound still queued; real FILE* and RAM file interleaved; fread item semantics; write-mode tripwire; removed sound -> ENOENT with a planted stale copy never opened; audio unhooked -> sounds stay the game's; dormant provider -> the cached copy, uncached missing |
| Mutations (`mutation_audio.py`) | stale-cache fallback 2 failures, owning while unhooked 3, fread byte count 1; restored 411/0 |
| Real broker (`scripts/test_content_audio_live.py <exe> <tests> audio_only.json`) | **12/0**: 424 sounds byte-exact through the hooks and WinINet (375 MB, ~7 s, cache bounded at 46 of 48 MB), 5 warm repeats from RAM, 2 withdrawn -> ENOENT, stale copies untouched; dormant broker -> the 3 planted cached copies, uncached missing; disposable LocalState and game_content unchanged |
| Harness smoke (`harness_s15_audio_smoke.py`) | **15/0**: the C1 DLL is refused C5; with the capability simulated in-process, C5 `--disposable` READY with 54,192 names, swap / restore of the gate sound published; nothing else changed |

## 6. Limits

- Production pairing: a deployed server policy that owns sounds must be used
  only with this DLL generation; old DLLs cannot be told apart by the broker.
- Sounds are fetched synchronously on the thread that loads them (as the disk
  read was); a first play of the 8.4 MB track costs one loopback fetch.
- Packaged sounds remain the fallback when `existsLocalFile` is false (provider
  idle/dormant); the provider never serves the package path.
- Voice/video other than `.mp3` sounds are not covered (none found in the
  reader's path).
