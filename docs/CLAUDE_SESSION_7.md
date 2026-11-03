# Claude Code Session 7 — authoritative offline content loading proof

## Objective and bounded scope

Implement the first working Windows x86 path that reads Town and Zeal assets
from the authoritative game_content tree without using or writing a persistent
client asset mirror. Complete Phases A–B of OFFLINE_CONTENT_LOADING_PLAN.md with
an end-to-end proof. Broader dependency handling and removal of loading screens
are later phases. Do not replace this objective with cache purging or a startup
cache reconciliation job.

Primary checkout: C:\Users\Evan\BF\offline-proxy (currently fps-cap).
Server checkout: C:\Users\Evan\BF\BF-WorkingDirRust (main).
Authoritative content: server checkout's deploy/game_content.

Work in these existing checkouts and preserve unrelated work. Do not commit,
push, deploy over the running client, reset the app, alter the live save, or
delete caches/backups in this session. Produce a tested candidate and exact
activation/rollback instructions for the maintainer's client acceptance.

## Read and establish the starting state

Read applicable AGENTS.md files, proxy README/CMake configuration, and these
server documents before editing:

- docs/BF_OFFLINE_SERVER_HANDBOOK.md and docs/DEVELOPMENT.md.
- docs/OFFLINE_CONTENT_LOADING_PLAN.md (current design authority).
- docs/CLIENT_CACHE_PATCH_BRIEF.md (reverse-engineering evidence; its older
  permanent-cache reconciliation recommendation is superseded).
- docs/CLAUDE_SESSION_6_HANDOFF.md in full, especially sections 11–13.
- docs/GAMEPLAY_STATUS_2026-10-03.md and docs/CLIENT_REMAINING_2026-10-02.md.

Inspect status, branches, remotes, submodules and active build/client processes.
Published baseline: server e8cdefc, packet-generator a46b7df, both mine/main;
main and audit-campaign matched in both repositories at publication. Check the
current state rather than resetting to these hashes. No new schema work is
expected unless the actual implementation requires a recovered contract.

Latest per-suite server results total 1,618 passed checks. The first publication
run had two zero-gem settlement-fixture failures; the corrected isolated rerun
passed 23/23. The mission validator still has its 3,117-finding baseline. Do not
describe that baseline as zero defects or the first invocation as successful.

## 1. Recover the real Windows loader contract

Record supported Windows EXE/DLL hashes and the actual x86 signatures/call sites
for asset existence, reads, decode, downloaded-name bookkeeping and completion
callbacks. ARM64 symbols explain semantics but are not Windows patch offsets.
Trace SAM and its pages, unit CGG/CGS and textures/portraits, and identify separate
audio, script/layout and MST readers. Include direct CRT reads that bypass curl.

Document callback order, owning thread, allocation/free ownership, cancellation
and raw-versus-encoded bytes. The cache currently encodes image data; a raw
server PNG must not be decoded twice. Preserve the current curl/WinINet redirect
and FPS hooks. Avoid a process-wide filesystem hook affecting account/state files.

Use signature guards and a default-off feature flag. Unknown binaries must
decline the new hooks safely, with a concise actionable diagnostic.

## 2. Implement the authoritative provider

Prefer direct read-only filesystem access if the existing AppContainer and
deployment permissions allow it. If they do not, use the loopback server as a
read-only byte broker into memory. Both approaches must avoid a second durable
client asset library. Do not silently grant broad filesystem permissions.

Implement exact normalized relative-path lookup under the configured content
root. Resolve legacy basename requests through explicit deterministic aliases;
report collisions rather than choosing an arbitrary file. Reject traversal,
malformed/encoded paths and reparse/symlink escape. Keep save keys, settings,
UserDefault.xml and XMLFileProcessor.xml outside asset interception.

Use content digests/generations to recognize same-size edits. Maintain the
index/change detection off the render thread; no whole-tree hashing per request
or frame. Handle watcher overflow and updates atomically. A loaded scene retains
its coherent generation until release; new bytes become visible at a safe scene
entry/reload. Keep bounded decoded RAM caches and coalesce duplicate reads.

Readiness and existence must reflect real provider results. Missing/corrupt
files produce bounded recoverable errors with their paths; never return a fake
successful download or enter a scene with an incomplete tested bundle. Preserve
MST parsing/version initialization; do not fold its protocol into an image hook.

## 3. Prove Town and Zeal end to end

Route MapVillage.sam, its referenced images, and the full Zeal 60956 bundle
through the provider, including portrait, battle texture and animation/layout
dependencies. Town has two known request paths:

- content/sam/MapVillage/MapVillage.sam (observed client request).
- content/_dlcbundle/MapVillage.sam (legacy/binary-derived path).

Use temporary fixture roots and isolated test client state. An intentionally
stale LocalState asset must not win over game_content. Prove the client consumes
the provider's bytes; an HTTP fetch or hash comparison alone is insufficient.
Change valid source content without changing byte length and verify the next
safe reload sees it. Verify no persistent client asset writes for these paths.
Missing Zeal texture must not reproduce the white-blob scene entry.

If a Windows call site blocks this proof, report its exact evidence, attempted
bounded approaches and next step. Do not call a standalone provider unit test
an integrated client pass. Keep unsupported file families explicitly scoped.

## 4. Build and QC without artifact sprawl

Use the proxy's existing configured Visual Studio Win32 build tree and installed
toolchain. Inspect CMakeCache before configuring: the local deploy-vs18 preset
currently sets OFFLINEMOD_DEPLOY=false despite its name. Verify the actual
standalone/embedded mode from cache and target definitions. Do not copy the
server's x64 Ninja cache into this x86 project or create timestamped build trees.

Compile the affected proxy target in the normal tree. Validate standalone and
embedded OFFLINEMOD_DEPLOY modes when affected; record any unavailable mode and
why instead of claiming coverage. If server source changes, use its documented
normal checkout build and run relevant server suites. Keep runtime-output
overrides temporary and restore them; never relink over a running executable.

Keep reusable tests and small signature/contract fixtures in tracked source.
Use one reusable QA directory (server out/qa/current with a small session-7
subdirectory is sufficient). Do not copy the full content tree, extracted client,
save archive or previous QA directories. Preserve existing recovery files.

Meaningful tests must cover exact paths/alias collisions; root confinement;
same-size changes; raw/encoded boundaries; missing/corrupt dependencies; coherent
concurrent reads/updates; cancellation/provider failure; supported/unknown binary
guards; bounded cache lifetime; and preservation of state-file paths. Test with
copied saves only. Record client-only checks as pending if not actually played.

Measure comparable cold/warm title-to-home and scene-ready times, frame-time
p50/p95, peak RAM, bytes read and persistent asset writes where instrumentation
permits. Keep machine/content/save consistent and note OS cache conditions.
Do not claim improved speed or smoothness from successful compilation alone.

## Explicitly deferred

- Broad rollout across every asset family and full scene dependency scheduling.
- Skipping login/verification/download screens: first inventory their state
  transitions; bypass only obsolete waits after real readiness is guaranteed.
- Permanent cache cleanup, mass download, progression/save edits and new grants.
- Gameplay backlog: archive-unit stored leader skills, crafted-sphere temporary
  identity, long-session town refresh, Trials/bosses/GuildUpdate and other content.
- General Town transform tooling cleanup unless needed for a reproducible proof.

Preserve confirmed fixes: Farm intro/harvest, SP purchase/reset, evolution,
friends after fusion, Training Grounds/reticle, sphere equipment, shards and
mission item accounting. Do not reopen them without a new reproduction.

## Completion and handoff

Write docs/CLAUDE_SESSION_7_HANDOFF.md in the server checkout with:

1. Exact files changed per repository and why; Windows hashes/signatures and
   recovered reader contracts, without committing binaries or large dumps.
2. What is integrated versus scaffolded, tested versus inferred, and unsupported.
3. Build configuration/commands, test results, measurements and concise log paths.
4. Candidate binary path/hash, feature flag/configuration, safe activation and
   rollback instructions; leave actual deployment to the maintainer.
5. A short client checklist: Town alignment with stale cache, Zeal's first battle
   appearance/animations, same-size edit after safe reload, missing-file error,
   restart/account preservation, normal battles/summons and FPS behavior.
6. The next bounded Phase C task based on evidence, then Phase D screen work.

Finish with both repository statuses. Exclude out/, saves, content dumps and
build products from source changes. Leave work reviewable and uncommitted.
