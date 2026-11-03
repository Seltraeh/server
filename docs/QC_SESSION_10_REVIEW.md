# Session 10 QC — completed October 5, 2026

Review targets the final server `270832d6d8957f1327f71a3009c7ca4d6ad80142c086d5d3114af5f77924a764`,
C1 isolated DLL `523c77ed23479174f6f9ce2d222331f6209159048f8372987dbdd156aa21ef76`,
and proxy tests `3e1df7bbf192ea8db1113344ecaaa97791469b744108982193effa515a103a40`.
Hashes were checked independently. No server/proxy production source was newer
than its respective candidate binary when checked; QC uses those existing builds.

## Scope and findings

- Reviewed rollback-on-release ownership and callbacks, explicit commits,
  MissionStart/Guild/CampaignReceipt and reward/login transaction conversions,
  and exception/trigger regressions. Reviewed the actual dirty trees, not just
  the 19-file screenshot.
- Reviewed digest-specific live-test synchronization and deterministic unrelated
  generation case: the exact-byte and pinned-version assertions remain intact.
- Reviewed enqueue hook ownership and string destruction, signature guards,
  exact-scope broker matching, family-policy generation and persistent fixture
  selection. The new x86 entry bytes were independently inspected at RVAs
  0x634FA0 and 0x1049A0; actual game execution remains a client check.
- **Corrected an acceptance-tool gap:** snapshot/progress/check previously used
  only the 21-name baseline even when the server used the C1 family policy.
  They now accept `--policy`. The configured target resolver was indexed rather
  than repeatedly scanning all scope entries; baseline resolves 21, C1 16,639.
  C1 file preflight finds zero missing target paths. These are configured
  names; dynamic serving still needs provider-log evidence.

## Independent verification

The candidate is ready for isolated visual acceptance, subject to the limits
below. No new blocking defect was found in the reviewed code. Logs stay in
`out/qa/current/session10/qc_*.log`, using the existing QA/build directories.

- Full independent regression run: 50 entries, **1,972 checks passed, zero
  failed**. The mission validator matches its **3,117 unresolved findings**;
  this remains a baseline, not clean mission validation. Included: contention
  21/0, exception atomicity 70/0, broker 78/0, generation 17/0, policy 8/0.
- Final C1 proxy against the real broker: **341/0, zero skipped**, driver 5/0.
  This completes the live integration check Claude left unfinished, including
  the corrected CSV readiness regression and enqueue-filter tests.
- Additional independent broker suite repeat: **78/0**, including replacement,
  delete-pending, missing-file and bundle withdrawal/recovery cases.

- Standalone proxy: 311 passed, zero failed; live broker case explicitly skipped
  there and run separately. BF_APPX_DIR was supplied for real binary guards.
- Family-policy/persistent-save harness: 9/0; 16,639 owned names at runtime,
  save marker survives server restart, live save byte-identical.
- Parent and packet-generator diff whitespace checks pass. Proxy's ordinary
  check flags existing CRLF additions; with `core.whitespace=cr-at-eol` it
  passes. No broad line-ending rewrite was made.

## Client preparation and limits

### October 5 player results and collected evidence

#### Yes branch completed

The separately prepared `c1yes` package was tested with background DLC enabled.
Player reports Vargas detail and initial Archive/training entry downloads,
correct assets throughout Training, and later Archive visits without download
screens or lag. After restarting BOTH the named harness and client, the DLC
prompt did not reappear and previously visited screens remained visually
correct without downloads. Mark this scoped Yes-branch visual/restart sequence
confirmed alongside the No branch; exact currency/inventory accounting was not
independently established by this report.

All four Yes snapshots (prelaunch, after_prompt, after, after_restart) contain
zero of the 16,639 configured names. Player's progress output shows total legacy
cache growth from 3,360 files / 0.19 GB to 41,322 files / 1.65 GB, while the
configured-name cache count stayed zero. QC inspected the collected provider
log: active serving, zero recorded fetch failures, zero write-tripwire counts,
up to 27 suppressed enqueues and 192 refused remove attempts per process; no
warnings/errors other than those expected remove-guard warnings. Per-process
maxima are not summed totals. This supports coexistence of background legacy
downloads with provider-backed C1 assets, not cache elimination for the whole
game. Initial Vargas/Archive/training requests still need file-level tracing;
enhancements or unfinished background work remain hypotheses, not findings.

No repetition of these prompt branches or restart sequences is needed absent
relevant changes. Next priorities: trace remaining demand/background downloads,
prepare a capacity-valid crafting fixture, and complete the remaining scoped
asset replacement/recovery, small-roster, leader comparison and performance
checks before expanding coverage or claiming the full offline patch complete.

- Fresh C1 launch displayed the background-DLC prompt. Player selected No.
  Daily spin and advent-calendar screens precede Home; the wheel was not spun
  for the initial baseline. Two `c1_before` snapshots both reported zero names;
  the second overwrote the first file, but the console report preserves both
  observations. Prelaunch, before, after and after-restart snapshot files all
  contain `{}`. Existence of the restart snapshot alone is not a player
  confirmation of saved-state persistence.
- Player reports the requested screens rendered correctly. First interactions
  downloaded resources; repeated visits did not download again. Ten C1 unit
  presents and their Unit List details required no download. Archive animation
  views did download; Touka showed a seven-asset download. A mission with three
  newly received units showed no missing assets. Reward receipt worked.
- Crafting was blocked by the copied save's over-capacity material inventory
  (player reports every material at quantity 50 and a 100-item limit). Player
  sold the materials. Treat this as a fixture-preparation defect, not a failed
  crafting implementation or a crafting pass. Future copies should provide a
  small, capacity-valid recipe kit; do not reset or rewrite the player's active
  test save without a concrete need.
- QC collected both provider log segments. Recorded provider-failure and
  write-tripwire counters stay at zero. Maximum per-process counters include
  43 suppressed enqueues and 192 refused remove attempts; all 192 warning lines
  are the remove guard, not fetch failures. The counters restart per process.
- Touka 840767's seven configured image/CGG/CGS names are offered by the provider;
  logs show provider existence decisions and served art. This does not identify
  the seven names shown in the Archive download UI. Do not equate its count with
  those seven covered files or declare all remaining downloads out of scope
  without tracing the actual download requests. This is the next content-flow
  investigation, alongside the remaining login/transition download screens.

Player follow-up confirms that relaunching both the client and named harness
retains the same visuals with no additional downloads. Mark the visual restart
check confirmed; no repeat of this sequence is needed. This observation does
not independently establish exact inventory quantities or currency balances.

Prepared and registered a separate **[ContentTest C1] Brave Frontier** package
with 1,361 hard links to source assets (not a second extracted-content copy),
the C1 port-29960 DLL, and account-state files only. QC did not launch it or set
its background-DLC choice. Prelaunch snapshot tracks all 16,639 configured
names and contains zero cached names. Original clients, their Settings, the
normal server binary and live save were not changed. The new package still
needs its own elevated loopback exemption.

Use [CLIENT_SESSION_10_STEPS.md](CLIENT_SESSION_10_STEPS.md). The initial run
uses a persistent copy of the prepared new-unit-present fixture. User tests
must report startup/first-visit art, new units, battle and both restart types.
Do not use fixture edit/missing-file commands against the family-policy root:
that root is the actual game_content tree, read-only in normal broker use.

Known limitations remain: 136 incomplete units and 4,880 variant files stay
legacy; this is not all asset families, cache-free startup, or removal of the
DLC prompt. Some single-statement handlers still swallow database errors
(UnitFavorite/Scenario per the handoff); the transaction audit is lint, not a
universal correctness proof. Fault-injection coverage and existing regressions
do not guarantee every client screen or rare race is correct.

Follow-up: incorporate client feedback before widening coverage; finish the
hook-contract documentation and VA/RVA plan corrections; test a fresh Yes-prompt
branch, small roster, leader before/after and controlled performance; consider
the remaining swallowed-error handlers separately. No commit, push or normal
deployment was performed by QC.
