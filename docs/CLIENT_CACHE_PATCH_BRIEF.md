# Offline client patch brief: stop trusting the asset cache

For: an agent working in **Seltraeh/offline-proxy** (branch `fps-cap`, the
current offline patch build). Written 2026-10-03 from the server side
(BF-WorkingDirRust). This document explains **why** the Windows client needs a
patch and gives the facts and contract needed to build it. It does not change
either repository.

Evidence labels: **binary-confirmed** (read from the client's code; addresses
are for the arm64 Android build `libgame.so`, which carries symbols),
**observed** (server logs and the client's cache on the maintainer's machine),
**to verify** (not yet checked on the x86 Windows build).

---

## 1. The problem in one paragraph

The client downloads every asset (images, SuperAnim `.sam` files, scripts,
layout CSVs) from the offline server once, stores it in its LocalState folder,
and **never checks it again**: nothing in the code paths traced compares a
version, size or hash, and in practice a cached file is used forever. When the server's `deploy/game_content` changes (a data
release, an art fix, a patched script), an installed client keeps showing the
old file forever. Today the only fixes are deleting individual cached files by
hand or running Windows' **app reset**, which wipes everything and triggers a
background re-download of ~70,000 files (2.7 GB) lasting well over half an hour
while the game shows missing or broken art. We want `game_content` to be the
single source of truth.

## 2. Why this matters: incidents

All observed on the maintainer's machine.

| Date | What happened | Cause |
|---|---|---|
| 2026-09-22 | Town, Shop and Challenges crashed until the app was reset | 38 cached files no longer matched the server's copies (different sizes: `MapVillage*`, `challeng_rank_*`, …); the client never re-fetched them |
| 2026-10-02 | A Town map fix (`MapVillage.sam`) did nothing in the client | The client already had the old file cached. The fix has the same size, so even the size-comparing cleanup tool (`tools/reset_client_cache.py`) could not see it |
| 2026-10-02 21:36 → 22:30+ | After a Windows app reset the client re-downloaded **~71,000 files (2.7 GB)** at ~1,000 files/min while the player played | App reset = empty LocalState |
| 2026-10-02 21:44 | A helper unit (Mauve Tenebrosity Zeal, 60956) appeared in battle with no sprite or portrait | Its art was still queued in the background re-download; the battle did not wait for it |
| 2026-10-02 22:24 | The same helper drew as a **white blob** | Its frame layout (`unit_cgg_60956.csv`) had arrived at 22:18 but its texture (`unit_anime_60956.png`) only at 22:25:59 |
| 2026-10-02 21:56 | A colour-changing summon gate showed a white flash | Happened during the re-download; not seen again once it had finished (player, 2026-10-03) |

Testing suffers the most: every visual bug report first needs "is the client
running a stale or half-downloaded file?" ruled out, and the server-side fixes
cannot be verified in the client without manual cache surgery.

## 3. How the client's cache works today

### 3.1 Where files live (observed)

- LocalState: `%LOCALAPPDATA%\Packages\gumi.BraveFrontier_99p3jr0gh0z6w\LocalState`
  — **flat**, keyed by file **basename** (`MapVillage.sam`,
  `unit_anime_60956.png`, …). 71,327 files / 2.7 GB after a full download.
- The same folder also holds files that must never be touched by this patch:
  `UserDefault.xml`, `XMLFileProcessor.xml`, the `Ver*_*.dat` MST tables (they
  have their own version channel), the random-named `*.dat` files (purpose not
  fully mapped; the save key is among them), and anything else the server does
  not serve.
- Server side: `deploy/game_content/content/<folder>/<file>` (75,931 files,
  3.2 GB). The server answers `GET /content/<path>` by exact path first, then
  falls back to **basename** (first match in a depth-first walk) —
  `BfWebController::findContentByName` in BF-WorkingDirRust.
- The client requests the same basename through different paths depending on
  the code path: e.g. `MapVillage.sam` was fetched as
  `/content/sam/MapVillage/MapVillage.sam` at login, while the Town scene's
  preflight would use `/content/_dlcbundle/MapVillage.sam`. Both land in the
  same flat LocalState file; the last download wins.

### 3.2 Write path (binary-confirmed, arm64)

- `FileLoader::addLoadFile` (0xFBE8DC) / `addLoadFilePriority` (0x1CD8AA8)
  queue a `WrapAsyncFileLoad` per file and set `setEncodeFlg(bool)` on it.
- `WrapAsyncFileLoad::connectionDidFinishLoading` (0xFBE044): if the encode
  flag is set, `FileCrypt::encode(data, len)` (0xFBCF30); then
  `WrapAsyncFileLoad::writeFile` (0xFBE3C4) writes
  `CCFileUtils::getWriteablePath() + name` with `fopen("wb")`/`fwrite`.
- **FileCrypt** scrambles in place, same size: cached byte
  `c[i] = (b[i] + i*i) & 0xFF`; `FileCrypt::decode` (0xFBCFF0) reverses it.
  Observed for every `unit/img/*.png` (full, anime, thum, battle); SAM pages,
  gate scripts, plists and `unit_cgg`/`cgs` CSVs are stored raw. Verified again
  2026-10-02: Zeal's four PNGs decode to byte-identical copies of the server's.
- Downloads go through the game's in-process **libcurl** — the very DLL the
  proxy replaces (`proxy_curl_setopt` already sees every `CURLOPT_URL`).

### 3.3 Read path and the "do I have it?" test (binary-confirmed)

- `CommonUtils::existsLocalFile(name)` (0xFB8ADC) returns
  `FileLoader::isFileAlreadyDownloaded(name)` (0x1CDE25C): a lookup in an
  **in-memory** `std::map<std::string,char>` at `FileLoader+0xb0` (value 1 =
  downloaded). Neither function compares a size, hash, date or version.
- `BaseScene::isExistFile(name)` (0x1000A1C) takes the basename
  (`CommonUtils::getFileName`), memoizes per scene, and defers to
  `existsLocalFile`.
- `CommonUtils::getLocalPath(name)` (0x1CE7D00) = writable path + name.
- `CommonUtils::getTexture(…)` (two overloads; decode calls at 0xFB64A0 and
  0x1CE76FC) runs `FileCrypt::decode` on cached images.
- Fallback: `CommonUtils::existsBundleFile` (0x1CEB480) /
  `getResourcePath` (0xFB6038) read the copy shipped **inside the app package**
  (`Assets/Resources`) when no local copy is registered — e.g.
  `MyTownTopScene::initialize` (0x18F0B34) tries local, then bundle.
- `XMLFileProcessor.xml` (class `XMLFileProcessor_SG`) only tracks ~200 SAM /
  script "parsed" flags. Its removal calls (`markFileForRemoval`,
  `removeDownloadedFilesFromXML`) are internal bookkeeping — callers are
  `FileLoader::removeCompletedParsedFile`, `cleanXMLFile`,
  `purgeCompletedPrioriryList` — **not** a server-driven invalidation channel.
  No such channel was found.

### 3.4 Behaviour that follows (observed)

- A file deleted from LocalState **while the game runs** is not re-fetched in
  that session: the running download pass skipped it (its list was fixed when
  the pass started), and the in-memory "downloaded" entry is presumably still
  set.
- A file deleted **before a launch** is re-downloaded: `MapVillage.sam`,
  deleted at 22:11, was fetched 25 s after the next login (22:22:36) and the new
  copy was the server's current file.
- After an app reset the client runs sequential background passes over a fixed
  list (~55 ms per file). Scenes that need a file still queued in that pass do
  not wait for it — that is the missing sprite / white blob.

### 3.5 The Windows build (what the proxy can reach)

- x86 (32-bit) UWP/AppContainer process. Game code is in
  `BraveFrontier.Windows.exe` (**stripped**); cocos2d-x 2.2.5 is
  `libcocos2d_v2.2.5_Windows_8.1.dll` with **named exports**, e.g.:
  - `?sharedFileUtils@CCFileUtils@cocos2d@@SAPAV12@XZ`
  - `?getWriteablePath@CCFileUtilsWinRT@cocos2d@@UAE?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ`
    (the spelling the game calls; `getWritablePath` also exists)
  - `?isFileExist@CCFileUtilsWinRT@cocos2d@@UAE_NPBD@Z`
  - `?getFileData@CCFileUtils@cocos2d@@UAEPAEPBD0PAK@Z`
  - `?fullPathForFilename@CCFileUtils@cocos2d@@UAE?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PBD@Z`
- The game functions in §3.2–3.3 have **no symbols** on Windows. Locating them
  would need byte-signature scans anchored on string literals both builds share
  (e.g. `"wb"` next to `fwrite` in `writeFile`, `"simultaneous downloads %d"` in
  the `FileLoader` constructor, `"XMLFileProcessor.xml"`, `"/_dlcbundle/"` in
  `CommonUtils::downloadBundlePriority` 0xFB7AF4). Prefer designs that avoid
  them.

## 4. What the patch must achieve

1. **`game_content` is authoritative.** After any change to a file the server
   serves, the next game launch uses the new file. No app reset, no manual
   deletion.
2. **No collateral damage.** Never delete or alter files the server does not
   serve (saves, keys, `UserDefault.xml`, `XMLFileProcessor.xml`, `Ver*.dat`).
3. **Fail safe.** If the server is unreachable or answers garbage, do nothing
   and let the game run as today.
4. **Cheap when nothing changed.** Startup cost near zero when the server's
   content is unchanged since the last launch.
5. **Do not regress** the existing hooks (InternetConnect/HttpOpenRequest
   redirection, the FPS cap) or the `OFFLINE_DEPLOY` build (embedded server via
   `offlinemod.dll`).
6. **Log what it did** (which files were invalidated and why), like the FPS-cap
   diagnostics.

## 5. Recommended design — Phase 1: reconcile the cache at startup

No hooks into the stripped game code. The proxy deletes stale cached files
**before the game starts using them**, and the client's own download path
(which handles FileCrypt correctly) fetches the current copies when a scene
asks for them — the behaviour observed in §3.4.

### 5.1 Server contract (to be added in BF-WorkingDirRust, `OfflineModController`, next to `/offline_mod/fps_cap`)

`GET /offline_mod/content_manifest` → `application/json`:

```json
{
  "generation": "sha256 of the listing below",
  "files": {
    "MapVillage.sam": [[338618, 1790993460], [338618, 1790989323], [338658, 1777140112]],
    "unit_anime_60956.png": [[133437, 1608688028]]
  }
}
```

- Key = basename. Value = every served file with that basename:
  `[size_bytes, mtime_unix_seconds]` (the server may serve the same basename
  from different paths, see §3.1). The example is real: `MapVillage.sam` exists
  as `sam/MapVillage/`, `_dlcbundle/` and `_dlcbundle/MapVillage/` copies.
- Built once at server start (and rebuilt when `content/` changes, if cheap).
  ~76k entries, a few MB — fine over loopback; gzip optional.
- Optional later: `"purge_before": <unix seconds>` to force-invalidate
  everything downloaded before a big release.

### 5.2 Proxy algorithm

1. **When:** one-shot, at the first valid `CCEGLView::Render` **before** the
   first `CCDirector::mainLoop` — the pattern `fps_cap.cpp` already uses for its
   server probe (the server may still be starting when `DllMain` runs; no
   network or heavy work inside `DllMain`). Share the existing Render detour
   rather than adding a second one; the cache step must also run when the FPS
   cap is compiled out. **To verify:** no asset in LocalState is opened before
   that first Render (DIAG build: log `CreateFile2`/`fopen` on LocalState paths
   during startup).
2. **Fetch** the manifest (WinINet, bounded timeout, like
   `FetchFpsCapFromServer`). Failure → log and stop (fail safe).
3. **Fast path:** if `generation` equals the one saved in
   `LocalState\offlinemod_cache_state.json` → stop.
4. **Scan** LocalState once with `FindFirstFileExW(…, FindExInfoBasic, …,
   FIND_FIRST_EX_LARGE_FETCH)` — size and last-write time without per-file
   opens.
5. For each cached file whose basename **is in the manifest**, mark it stale
   when **either**:
   - its size matches **none** of the served sizes (FileCrypt keeps sizes, so
     this is valid for scrambled images too), **or**
   - **any** served copy's mtime is newer than the cached file's last-write
     time (the cached time is the download time; a newer server file means it
     changed after the download).
   Files not in the manifest are never touched.
6. Delete stale files (`DeleteFileW`), log each with its reason, then save
   `generation` to the state file. Deletion failures are logged and skipped.
7. Nothing else: the game, finding the files absent, downloads them through its
   own pipeline.

Notes:
- Get the LocalState path from the game itself:
  `CCFileUtils::sharedFileUtils()->getWriteablePath()` via the exports in §3.5,
  or the WinRT `ApplicationData` API. Do not hard-code the package family name.
- Rough cost when content changed: one ~4 MB loopback GET + one directory
  enumeration of ~71k entries — about a second. Zero when unchanged.
- Known blind spot: a server file replaced by one with the **same size and an
  older mtime** (e.g. restored with preserved timestamps). The server-side
  fix is to touch the file; `purge_before` covers bulk cases.
- Bundle fallback caveat (§3.3): a few scenes use the copy shipped in the app
  package when no local copy is registered. The re-download after deletion was
  observed for `MapVillage.sam`; **to verify** that deleted files are fetched
  again rather than replaced by the shipped copy for other scenes, before
  relying on this for every file type.

### 5.3 Build options (suggested)

Mirror the FPS-cap options in `CMakeLists`/`serverconfig.h.cmake`:
`OFFLINEMOD_CACHE_SYNC` (default 1), `OFFLINEMOD_CACHE_SYNC_DRYRUN` (log only,
delete nothing) and `OFFLINEMOD_CACHE_SYNC_DIAG` (log to
`%TEMP%\offlinemod_cache.log` + `OutputDebugString`).

## 6. Phase 2 (the maintainer's preferred end state): read straight from `game_content`

The maintainer would ultimately like the client to read assets **directly from
`game_content`** with no per-client cache at all, for development and testing.
This is more invasive; Phase 1 should land first. What it involves:

- Redirect reads, not writes: hook the exported `CCFileUtils::getFileData`,
  `fullPathForFilename` and `CCFileUtilsWinRT::isFileExist` so that a request
  for `<writable path>\<name>` resolves to the `game_content` file the server
  would serve for that basename (same exact-path-then-basename rules). **To
  verify** which of these the game's loaders actually call — some may open
  files with the CRT directly.
- The game's own "downloaded?" test is the in-memory map in the stripped exe
  (`FileLoader::isFileAlreadyDownloaded`, §3.3). It must report "present" for
  redirected names, or the game re-downloads them anyway. This needs an x86
  address (signature scan, §3.5).
- FileCrypt: the game runs `FileCrypt::decode` on cached images. Files read
  from `game_content` are raw, so either skip the decode for redirected reads,
  or have the read hook return the scrambled form
  (`c[i] = (b[i] + i*i) & 0xFF`) for images. A raw image is easy to recognise
  (`89 50 4E 47 0D 0A 1A 0A` for PNG, `FF D8 FF` for JPEG); a scrambled PNG
  starts `89 51 52 50`.
- **Sandbox:** the game is an AppContainer (UWP) process. Reading a folder
  under the user's profile (e.g. `C:\Users\…\BF-WorkingDirRust\deploy\game_content`)
  is normally denied. Granting the folder read access for "ALL APPLICATION
  PACKAGES" would be a machine security change the **maintainer** must decide
  on and apply; serving through the loopback HTTP server (Phase 1) avoids it.
- `OFFLINE_DEPLOY` builds embed the server in `offlinemod.dll`; where their
  `game_content` lives (inside the package? LocalState?) decides whether
  Phase 2 can work there at all — **to verify**.

## 7. Acceptance tests

1. **Changed file reaches the client.** With the game closed, run
   `python tools/frame_town_for_2x3.py --remove` in BF-WorkingDirRust (serves
   the original Town map, whose labels sit ~44 points above their buildings).
   Launch, open Town: the labels are back above the buildings, and the log lists
   `MapVillage.sam` as stale (mtime) and deleted. Close the game, run `--apply`,
   launch again: the labels sit on their buildings. No app reset, no manual
   deletion.
2. **Unchanged content is a no-op.** Second launch with no server change: the
   log says generation unchanged; nothing deleted; no measurable startup delay.
3. **Never touches unserved files.** In a dry run, `UserDefault.xml`,
   `XMLFileProcessor.xml`, every `Ver*.dat` and the save-key `*.dat` files never
   appear in the stale list.
4. **Scrambled images.** Replace a unit image on the server (new mtime); the
   cached copy is deleted, re-downloaded and renders correctly in battle (no
   white blob).
5. **Server down.** Launch with the server stopped: no deletions, game behaves
   as today (title/connection error as before).
6. **No regressions.** FPS cap still applies (`/offline_mod/fps_cap`);
   connection redirection and HTTP-only mode unchanged; `deploy-*` presets
   build.

## 8. Reference

- Proxy today (`fps-cap`): `OfflineMod.win32/main.cpp` (Detours on
  InternetConnectA/W + HttpOpenRequestA/W → `SERVICE_IP:SERVICE_PORT`,
  HTTPS off; `OFFLINE_DEPLOY` loads `offlinemod.dll` and calls
  `OfflineMod_startup`), `fps_cap.cpp` (Render + eglSwapBuffers hooks, one-shot
  server probe on the first Render), `curl_exp.cpp` (libcurl export shims;
  `Source.def` names the DLL `libcurl`).
- Server (BF-WorkingDirRust): `gimuserver/controller/OfflineModController.hpp`
  (`/offline_mod/fps_cap`), `gimuserver/controller/BfWebController.cpp`
  (`/content/…` serving and `findContentByName`), `deploy/game_content/content`.
- Existing stopgaps the patch replaces (local tools on the maintainer's
  machine, not in either repository): `tools/reset_client_cache.py` (deletes
  cached files whose **size** differs from the server's — blind to same-size
  changes), `tools/frame_town_for_2x3.py --clear-client-cache` (one file).
- Background notes: `docs/CLAUDE_SESSION_6_HANDOFF.md` §11–§12 (the Town map
  case and the post-reset re-download, with times and paths).
