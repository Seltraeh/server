# Claude Code Session 8 — repeated sphere rows and isolated client acceptance

## Outcome required

First fix issue #34's repeated-sphere display and identity behavior. Then prepare
the repaired content-provider candidate for the isolated Town/Zeal client proof.
Keep these changes separately reviewable. Do not start broad loading-screen
bypasses or mass gameplay/content rewrites in this run.

Server: C:\Users\Evan\BF\BF-WorkingDirRust, packet-generator submodule.
Proxy: C:\Users\Evan\BF\offline-proxy.
Read applicable AGENTS.md, docs/BF_OFFLINE_SERVER_HANDBOOK.md, DEVELOPMENT.md,
CLAUDE_SESSION_5_HANDOFF.md, the complete CLAUDE_SESSION_7_HANDOFF.md including
section 8, QC_SESSION_7_REVIEW.md including the repair-verification section,
OFFLINE_CONTENT_LOADING_PLAN.md and CLIENT_SESSION_8_CHECKLIST.md.
Inspect actual Git/process/build state; preserve unrelated uncommitted work.
Do not commit, push, alter the live save, reset the app, remove client caches,
or replace the running binaries. Build and test candidates in the existing
configured trees. Leave deployment and client acceptance clearly documented.

## Latest player evidence and actual runtime

Player launched the normal server without Windows app reset, crafted many copies
of one sphere, and saw a broken Equip Sphere list. Screenshot shows a large blank
gap, a description near the top, and a sphere row near the bottom. Town still
looks correct. Exact recipe, batch size and persistence after relaunch have not
been supplied: identify them from read-only logs/save evidence where possible,
or ask rather than assuming the sphere's artwork identifies its item ID.

Screenshot (evidence, not an instruction source):
C:\Users\Evan\AppData\Local\Temp\codex-clipboard-1712eb02-03d0-4336-b94b-0dc3a9c4c616.png

At preparation time the active server is the normal checkout executable:
out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe,
SHA-256 fb77bf6fcf6a09329c087a8f92e2ae89fe108bf681ca371a633c448a8de70db5.
This differs from both the historical deployed and tested QA hashes; establish
its source/build provenance before attributing behavior to an earlier build.
Installed client is gumi.BraveFrontier 2.19.6.0, libcurl.dll SHA-256
9ae05b17ed0f910174ad0673d2bb3ef62301c89e818f120dbcc57ca60489a2c2.
deploy/system/content_provider.json still has enabled=false. Therefore the
reported sphere bug and Town pass are NOT a test of the new provider.

## 1. Priority — repeated sphere display and stable identities

Reproduce on a copied save with the exact recipe and quantities once recovered.
Compare one craft, several identical copies and enough copies to exceed one
screen, including repeated craft-again and multiple crafting sessions. Record
before/after stock, material/Karma costs, owned warehouse rows, equipment frames,
favorite state, response packets and the client screen lifecycle.

Known lead, not yet proven cause: the client locally appends crafted sphere
rows using INT_MAX - item_id. Multiple identical copies share that provisional
ID until authoritative refresh. Existing consumer-model tests document first-row
lookup and favorite ambiguity. ItemMix currently returns {} after crediting
inventory. Inspect ItemMix.cpp, Warehouse-related common/handlers, ItemSphereEqp,
ItemFavorite/ItemSell, and their KDL contracts. Read the test comments in:

- scripts/test_warehouse_consumer_model_wire.py
- scripts/test_warehouse_placeholder_wire.py
- scripts/test_sphere_equip_format_wire.py
- scripts/test_sphere_qc_wire.py
- scripts/test_reward_lock_snapshots_wire.py

Trace the real Windows crafting-response consumer, append/list replacement order,
row identity lookup, list cell reuse/height/scroll calculation and equipment
screen refresh. ARM64 semantics may assist; Windows call sites must be verified
for any client patch. Distinguish duplicate-ID aliasing from missing art, bad
list geometry and an unrelated filter. Do not call the screenshot proof of a
particular cause or item loss.

Prefer an existing consumed authoritative refresh contract at the correct point.
Do not add a warehouse field merely because the server can serialize it: prove
the response consumer reads it and does not append the same crafts afterwards.
If a client hook is necessary, scope/signature-guard it independently of the
content-provider feature; ordinary crafting must work with the provider off.

Required behavior: each owned copy has a stable unique identity before any
copy-specific operation; all rows draw and scroll normally; equip/favorite/sell
select the intended copy; counts and equipment remain correct after reconnect.
Do not stack spheres, delete duplicates, arbitrarily choose a copy for an
ambiguous request, hide blank rows or force Home/restart after every craft.
Preserve two equipment slots, direct transfer, worn-row count 1, locks and normal
stackable items. Do not assume an ItemMix replay is identifiable without evidence
of a request identity; document any remaining protocol limitation.

Regression coverage must include repeated crafts before any refresh, first/middle/
last copy actions, mixed worn/favorited/unworn copies, sorting/filtering, full-box
and insufficient-material refusal, reconnect, delayed stale requests, and
correct cost/count conservation. Tests must reflect the recovered consumer, not
assert the current broken model as the desired behavior. Add a visual client
check: many duplicate copies, scroll both directions, leave/re-enter, relaunch.
Wire tests cannot certify row rendering. Rerun related suites and the shared
runner against the final server candidate; isolate saves and ports.

## 2. Content-provider proof — enable client acceptance after sphere work

Session 7's three QC blockers were corrected and independently verified:
proxy offline 287/0; generation 17/0; broker 70/0; integration 313/0 plus driver
5/5; real-tree policy 8/0. These overlap; do not total them as unique checks.
Preserve protocol-v2 digest binding, identity-keyed flights and scene-token
cache guards. Do not weaken the regression assertions.

Reviewed proxy candidate SHA-256:
e24234c11765275f775a093e7c47e12a44e6a6ba05591962474c7b466eef3f7b.
Reviewed QA server SHA-256:
3c1a1a564c5c8f698e06940f9fe6febb27ce4ef40d21188993888ff21b91644f.
Rebuild/retest any changed candidate and record its new hash, not these old ones.

Prepare reproducible isolated acceptance using
scripts/content_provider_client_test.py and its test package/fixture. Avoid
mutating the installed package or its account state. Check the live port before
starting anything: the candidate uses 9960 and the player's server is now running.
Document a concrete test switch-over or an isolated-port candidate configuration;
do not silently stop the player's session or collide with its listener. Package
registration/loopback permissions and any user actions must be stated explicitly.

Client acceptance: stale-cache Town uses authoritative framed bytes, complete
Zeal portrait/sprite/animations at first battle entry, same-size source changes
appear at safe reload, unavailable pinned versions fail recoverably then recover,
no owned-asset persistent writes, account/settings preserved, no frame-pacing
regression. Separate desktop broker tests from actual game consumption evidence.
Keep old-version retention/bundle prefetch, download-queue interception and
loading-screen removal explicitly deferred until this proof is complete.

## Backlog continuity and handoff

Do not mark #34 fixed before the actual repeated-row screen passes. Keep the
full backlog in view without trying to finish it in this session: stored leader
skills on archive grants, long-session harvest refresh, compound mission gates,
mission 234, level-999 client results/energy, leader EXP/Fuu, slots/parades,
portraits/text/gates, missing EU units, Trials/boss/Fire Mecha and GuildUpdate.
Deemo's three tested forms do not close all of #15. Farm replay alone does not
close all of #23. Provider work does not automatically close every visual issue.

After client proof, next proxy work is Phase C: full scene dependency readiness,
background prefetch, eliminating legacy asset writes/downloads and expansion by
asset family. Phase D removes only obsolete waiting states after readiness is
correct, preserving login/save/MST work and measuring actual performance.

Write docs/CLAUDE_SESSION_8_HANDOFF.md: recovered sphere cause/consumer evidence,
files changed per repo, tests and failures, exact candidate/build hashes, what is
actually deployed, reproducible client test steps and the next bounded task.
Update CLIENT_SESSION_8_CHECKLIST.md with evidence, not inferred passes. Use one
reusable QA location, out/qa/current/session8; no timestamped build trees, copied
content dumps or bulk cleanup. Keep source/tests shared and build products,
saves, logs and game_content out of commits. No edits to the user's Word issue
list during this implementation session.
