# Offline content loading plan — October 3, 2026

Target repository: ../offline-proxy, currently fps-cap; coordinate a small
server contract in this repository if needed. Planning only: no proxy patch,
client-cache deletion, AppContainer permission change or screen bypass has been
performed. CLIENT_CACHE_PATCH_BRIEF.md supplies detailed evidence. This plan
supersedes its recommendation to make persistent-cache reconciliation the
mandatory first product: the requested destination is direct authoritative
game_content loading, with no per-client asset mirror.

## Intended behavior

deploy/game_content is the source of truth. Editing a served file, including a
same-size edit, must be visible at the next safe load boundary without resetting
the app or re-downloading tens of thousands of files. Preserve bounded RAM caches
for decoded textures/animations already in use; eliminating these would increase
I/O and frame stalls. Do not retain a second persistent LocalState asset library.

Do not remove a verification screen by skipping required work. Replace asset
download/existence checks with a fast, correct local-provider readiness result.
Keep real account/save/server/MST initialization and meaningful missing-file
errors. Fast scene entry is accepted only with complete dependent assets.

## Evidence and unresolved platform questions

- The traced ARM64 FileLoader keeps an in-memory downloaded-name map; several
  readers bypass network once a basename is marked present. HTTP Cache-Control
  headers alone cannot fix files for which no HTTP request occurs.
- LocalState flattens names. Different URLs can overwrite the same cached file.
  BfWebController serves an exact path first and has a static basename fallback
  index. Neither ambiguous basename selection nor a once-built stale index is
  suitable as the new authoritative mapping.
- Some image files are scrambled when written to the client cache, then decoded
  again by consumers; server images are raw. A direct reader must avoid double
  decoding. Verify actual Windows call sites and buffer ownership.
- offline-proxy's libcurl shim sees URLs; its WinINet detours redirect traffic,
  and its existing FPS hooks must survive. Cached file reads and the game's
  downloaded map require additional Windows-side integration.
- The running Windows client is x86/AppContainer. ARM64 addresses explain
  semantics, not patch locations. Determine accessible content paths and the
  standalone versus OFFLINE_DEPLOY storage arrangement before choosing transport.

## Phase A — Windows read-path proof, no UI shortcuts yet

Record supported EXE/DLL hashes and locate/signature-verify the actual Windows
asset loaders, existence checks, decode boundaries and completion callbacks.
Trace a SAM, layout/CGG/CGS, raw/encoded image, sound, script and MST access.
Include any direct CRT file readers; avoid broad process-wide filesystem hooks.
Use a feature flag and fail safely on unknown binaries rather than patching
unverified offsets. Preserve account files, save keys, UserDefault.xml and
XMLFileProcessor.xml. Separate assets from mutable client state explicitly.

Implement one end-to-end proof: MapVillage.sam and one unit's full asset bundle,
read through the authoritative provider, while an intentionally stale LocalState
copy exists. Change source bytes without changing their length; the next scene
load must use the new generation. Prove no persistent client asset file is written.

## Phase B — authoritative provider and content identity

Prefer direct read-only filesystem access when it works within the existing
deployment permissions. Otherwise use the loopback server as a read-only content
broker returning bytes into memory; that still eliminates the client disk mirror.
Do not silently grant broad folder permissions to make direct reads work.

Use normalized relative paths, exact-path resolution and a deterministic,
explicit legacy-name alias table. Reject/report conflicting basenames unless an
authored alias selects the intended variant. Confine reads to the configured
content root, including symlink/reparse traversal and percent-decoding cases.
Expose asset type/encoding, size, content digest and generation. Do not use mtime
or size alone as proof of equality. Build an index once and maintain it with
change detection; recover from watcher overflow with a rescan off the render
thread. No full-tree hashing or HTTP probe on every draw.

Files must be read as a coherent generation, not while a writer has half-updated
them. Prefer atomic content publication and bundle generation pins. Preserve
old in-memory objects until their scene releases them; apply changes at scene
entry/reload, not halfway through battle. Define explicit developer reload and
production update behavior. MST has its own version/parse channel: make it
authoritative too only with correct table dependency reloads; do not delete its
version checks as part of the raw-image change.

## Phase C — complete asset dependencies without bulk downloading

Teach the downloaded/existence checks about authoritative availability and route
read/completion paths through the provider. A boolean 'all files exist' patch is
not sufficient. Resolve the selected scene's dependency closure: helper portrait,
texture, CGG/CGS and animation variants; every gate transition/overlay; SAM pages;
audio and layout/script dependencies. Read/prefetch only that set asynchronously.

Do not enter battle with Zeal's layout ready but his texture absent. Missing or
corrupt dependencies get a bounded, recoverable error with their exact path;
do not render a white blob or synthesize a successful download. Coalesce duplicate
reads and define cancellation, timeout, allocation/free, reentrancy and thread
ownership. Avoid disk writes and blocking synchronous HTTP on the render thread.

Only after this works disable the legacy bulk asset download/write queue and
its stale downloaded-map authority. Account/login/save requests remain intact.
Treat old LocalState assets as ignored; optional one-time cleanup is a separate,
explicitly asset-scoped maintenance action, not required for correctness.

## Phase D — remove redundant waiting screens

Inventory each startup/scene verification screen by its actual state machine and
requests. Classify asset download, asset decode, MST parse, account/session and
gameplay transitions. Skip only the obsolete download/asset-verification wait
states whose work the provider already completed, and dispatch their normal
completion callbacks in the correct order. Never blanket-ack network operations,
erase tutorial/feature gates, skip schema migrations or suppress real errors.

Measure title-to-home and scene-ready timing before/after, plus p50/p95 frame
times, peak RAM, bytes read and disk writes. Use the same machine, content and
save; compare cold and warm OS caches. Do not promise speed improvements before
measurement. Keep FPS cap hooks, input timing and all original gameplay rules.

## Acceptance and rollout

1. Stale and empty client asset caches both work without app reset; no full-cache
   re-download; same-size source edit appears on safe reload.
2. Town labels stay aligned; Zeal 60956 has portrait, sprite and animations on
   first encounter; summon colour-change and overlays have no white flash.
3. File types, encrypted/raw boundaries, duplicate basenames, missing/corrupt
   assets, concurrent updates, broker unavailable and watcher overflow tested.
4. Preserve login/save keys, settings, progression, purchased skills, equipment,
   friends and all relevant scenes; no unexpected LocalState asset writes.
5. Server-backed standalone and embedded OFFLINE_DEPLOY variants build/test
   separately with their real x86 toolchain and packaging paths.
6. Feature flag supports rollback without a client reset. No second giant content
   mirror, no unbounded RAM growth, no worsening of measured frame pacing.

First implementation assignment should be Phases A–B plus the Town/Zeal proof,
not every hook and screen in one pass. Then Phase C dependency handling, then
Phase D. If Windows constraints block a file family, document that specific
blocker and bounded fallback; do not quietly ship cache invalidation as the
completed no-cache design.

Keep builds in the proxy's existing configured tree, test output in one reusable
QA directory, and signature/contract/unit tests in shared source. Port the Town
content transform to parameterized release tooling so clean installations can
reproduce the current local content fix without ignored tools or bundled dumps.
