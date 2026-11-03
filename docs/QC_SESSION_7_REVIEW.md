# Session 7 quality review — October 3, 2026

## Repair verification — October 3, evening

**Current verdict: the three reported blockers are resolved; ready for isolated
client acceptance, not yet approved as a completed cache-free client.** The
original review below is retained as historical evidence and its initial
deployment-blocking verdict is superseded by this verification.

Independently inspected the repaired broker publication path, protocol v2 digest
binding, identity-keyed cache/flights, scene-token insertion guard and expanded
regressions. The original failing assertions remain intact and pass. Fetches
validate the pinned SHA-256; unchanged bytes may safely cross a broker epoch,
while changed bytes are refused. Old-version refusal is the bounded fallback
accepted by the review; old versions are not retained by the broker.

Independently reran against the repaired binaries:

| Check | Result |
|---|---|
| VS Win32 RelWithDebInfo proxy build | Passed |
| Proxy offline tests, real binary signature fixtures enabled | 287 passed, 0 failed, 1 live-only skip |
| Generation QC including concurrent readers/writers | 17 passed, 0 failed |
| Broker wire tests | 70 passed, 0 failed |
| Real broker/proxy integration | 313 passed, 0 failed, 0 skipped; driver 5/5 |
| Policy against the real content tree | 8 passed, 0 failed |
| Source whitespace | Passed (proxy with CRLF-aware whitespace setting) |

These runs overlap; do not add their counts together as unique checks. Logs:
`out/qa/current/session7/qc2_{build,proxy,generation,broker,live,policy}.log`.

Verified DLL SHA-256:
`e24234c11765275f775a093e7c47e12a44e6a6ba05591962474c7b466eef3f7b`.
Verified QA server SHA-256:
`3c1a1a564c5c8f698e06940f9fe6febb27ce4ef40d21188993888ff21b91644f`.
Both match the repair handoff. The generation QC script is now in the shared
runner. The full gameplay runner's 1,712-passing result is Claude's reported
run, not independently rerun here. Provider-off and embedded variants were not
rebuilt in this verification; that limitation is accurately noted in the handoff.

No additional correction was identified in the reviewed repair scope. Remaining
acceptance is unchanged: actual AppContainer hook behavior, Town/Zeal rendering,
safe reload and recovery when a pinned version disappears, persistent asset
writes, account preservation and measured frame pacing. Synchronous reads and
the unintercepted legacy download queue remain explicit Phase C limitations.
Use the isolated test package and fixture before updating the installed client.

Only review documentation changed in this verification. No deployment, live
save/content mutation, runtime-policy change, commit or push was performed.

## Original review

**Verdict: corrections required before deployment or client acceptance.**
The existing tests pass, but additional regression cases expose three content
consistency defects. No production implementation was changed in this review.
The candidate DLL and QA server hashes still match Claude's handoff. The live
client, live save, deployed server and runtime policy were not changed.

## Findings

### P1 — broker reads can swallow a content generation change

Location: `gimuserver/content/ContentBroker.cpp:1157–1177`.

`read()` notices a digest difference, but overwrites the stored digest and size
before calling `applyChanged()`. That function hashes the file again and now
compares it against the just-written digest. For a same-size edit with a
preserved timestamp, neither this call nor the later watcher advances the
generation. A client holding the old RAM bytes will never receive invalidation.

Reproduced against the handoff QA server using a one-file temporary root:
initial read `old-bytes`, overwrite with `new-bytes` preserving size/mtime,
immediate read before the watcher debounce. The new bytes were returned with
generation 1, and the generation remained 1 after waiting for the watcher.

Fix: publish digest/size and generation/owned mapping changes atomically through
the writer path. Preserve the old identity until the change comparison completes;
do not repair this by unconditionally increasing generations on every read.
Concurrent reader/watcher updates must not overwrite newer metadata or attach
the wrong generation to a response. Cover immediate reads before watcher flush,
identical rewrites, timestamp restoration and concurrent readers.

Regression: `scripts/test_content_generation_qc.py` (3 passed, 1 failed).

### P1 — stale in-flight reads repopulate the cache after a scene change

Locations: proxy `OfflineMod.win32/content/cp_provider.cpp:471–483,519–536`
and `cp_core.cpp:583–590`.

The cache and outstanding fetches use only the relative path. `SafePoint()`
evicts previously served paths, but an unfinished first read has not entered
`m_servedRel` yet. After the new index is adopted, the old request finishes and
unconditionally inserts its old bytes. Subsequent reads under the new scene
index return those bytes, with no further delta guaranteed to remove them.
Even when a path was served earlier, eviction does not cancel or invalidate an
outstanding flight's eventual insertion.

Reproduced deterministically with a broker that pauses after capturing the old
response: publish new bytes, poll/adopt the new generation, release the old
request, then read again. The last read incorrectly returns the old bytes.

Fix: key caches/flights by immutable content identity (epoch and per-file
version/digest as well as path), or use generation-aware invalidation tokens
that prevent stale publication. A read must keep its original snapshot identity
through completion. Merely clearing the cache at every scene change is not
sufficient. Add explicit tests for outstanding first reads, waiters sharing a
flight, eviction and broker restart overlapping adoption.

Regression: `TestGenerationIsolationQC()` in proxy `tests/test_main.cpp`,
the in-flight completion case.

### P1 — scene generation pinning does not constrain fetched bytes

Location: proxy `OfflineMod.win32/content/cp_provider.cpp:401–452`.

`Fetch()` asks for the current path only, validates bytes against the digest
provided in that same response, logs a newer generation and returns it anyway.
The response epoch is not enforced. A first read after an edit, an LRU miss, or
a broker restart can therefore mix new texture/animation/layout bytes with old
assets in the same scene. The broker protocol does not offer versioned reads.

Three added checks reproduce acceptance of newer bytes on a cold read, after
LRU eviction, and from a different broker epoch. Returning a clear error rather
than incompatible bytes is an acceptable bounded fallback; silently mixing is
not. True pinning needs snapshot/version-bound reads or a fully materialized,
coherent bundle retained for its scene lifetime. Compare expected identity,
not just the response's self-reported digest, and handle unavailable versions.

The proxy contract's known-limits section acknowledges the first-read behavior,
but its coherent-generation guarantee and the assignment require stronger
behavior. The original `TestProvider()` even asserts acceptance of newer bytes;
replace that expectation when implementing the correction rather than weakening
the new isolation assertions.

## Checks performed

| Check | Review result |
|---|---|
| Normal proxy VS Win32 RelWithDebInfo build | Passed, including rebuilt regression tests |
| Original proxy checks with real binary signature fixtures | 226 passed, 0 failed, 1 live-only skip |
| Broker wire suite against handoff QA server | 67 passed, 0 failed |
| Real-content policy check | 7 passed, 0 failed; runtime policy remains disabled |
| Proxy suite including new regression cases, offline | 234 passed, 4 failed, 1 live-only skip |
| Real C++ broker + proxy integration including new cases | 254 passed, 4 failed, 0 skipped; the four failures are the new isolation checks |
| Integration Python driver | 4 passed, 1 failed because the proxy suite correctly exits nonzero |
| New immediate-read broker regression | 3 passed, 1 failed |
| Whitespace | Server passes; proxy passes with `core.whitespace=cr-at-eol` (ordinary diff check flags its CRLF additions) |

The five new failed assertions describe the three findings above, not five
separate defects. The driver failure is an aggregate result, not another defect.
The full 43-entry gameplay runner was not repeated during this review: no gameplay
implementation changed. Its 1,692-passing result remains Claude's earlier run,
not certification of the new scenarios. Do not mark the new tests expected-fail
or omit them from the repair's validation.

Commands from the server checkout:

```powershell
cmake --build ../offline-proxy/build --config RelWithDebInfo
$env:BF_APPX_DIR='C:/Users/Evan/BF/BraveFrontierAppxClient'
../offline-proxy/build/tests/content_provider_tests.exe
python scripts/test_content_broker_wire.py out/qa/current/bin/gimuserverw.exe
python scripts/test_content_generation_qc.py out/qa/current/bin/gimuserverw.exe
python scripts/test_content_provider_live.py out/qa/current/bin/gimuserverw.exe ../offline-proxy/build/tests/content_provider_tests.exe
python scripts/check_content_policy.py out/qa/current/bin/gimuserverw.exe
```

Run the broker-based scripts sequentially: they use port 19960. The new broker
regression is currently a standalone script; include it in the shared runner
as part of the repair. QA logs are in `out/qa/current/session7/qc_*.log`; its
small new fixture is in `out/qa/current/session7/qc_generation`.

Verified candidate hashes:

- Proxy DLL: `3d6d45e649af26d6b01f1570ad35286992b3dabd7eda8b93cb6984b41a86a5ee`.
- QA server: `d6a8d59ebd9ba368208bb9bb5b719b7fb34c1ade902681d30b8d4d2f4800932c`.

## Remaining client and performance gates

The handoff correctly labels actual game integration as pending. Mock hook ABI
checks and the real broker harness do not prove hooks run correctly inside the
AppContainer or that Town and Zeal draw correctly. After repairing these defects,
use the separate test package/fixture first, with stale assets and preserved
account state, before considering an in-place package update.

Still require: Town/Zeal rendering and same-size safe reload; missing-texture
recovery; observed persistent asset writes; login/account preservation; gameplay
regressions and measured frame pacing. The download queue is not intercepted,
so absent owned assets can still trigger legacy downloads/writes. Synchronous
network reads can still block the render thread. These are acknowledged Phase C
limitations, not evidence of improved speed or a completed cache-free client.
Loading-screen bypass remains deferred until real dependency readiness works.

## Review changes

- Added four deterministic proxy regression assertions plus their setup checks
  in `offline-proxy/tests/test_main.cpp`.
- Added `scripts/test_content_generation_qc.py` using an isolated server/save/root.
- Wrote this review and linked it prominently from the Session 7 handoff.
- No production fix, deployment, cache cleanup, save mutation, commit or push.

Next work should repair the three findings and update the contracts/tests, then
rerun the targeted checks and affected shared suites. Only afterward proceed to
the integrated Town/Zeal acceptance proof and wider Phase C work.
