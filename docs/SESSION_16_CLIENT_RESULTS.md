# C16 native test attempt — October 21, 2026

**Current status, October 21: Session 17 repair independently QC'd and observed installed in C16. The user accepted consent. The bounded native fallback route, both restarts, missing-file recovery and provider-disabled control completed without a crash.** This is not acceptance of cache-free startup, audio, all gameplay, or deployment. The original failed attempt is retained below as history.

## Resumed native run, October 21

Only C16 and named copied save `s16_cold` are used. DLL `45a2c1b1...`, server
`b2552da2...`, C7 policy `04995161...`, manifest `3be2dae4...`; no live/C1 changes.
The user personally accepted the first-launch consent screen. They also requested
investigating removal of that old online-service screen in the offline patch.

- Actual registered-client log confirms `startup coverage path installed` with all
  nine sites checked before/after the provider and the expected lookup detour.
  The log appends launches: its old leading `DECLINED` belongs to the superseded DLL.
- Chose **No** at the background-DLC prompt, which correctly remained visible under
  incomplete coverage. Daily spin awarded Rainbow Crystal; claimed the calendar
  reward. Home shows Selena as the sole squad member, with the other slots empty.
- `snapshot_c16_home.json`: **0 owned, 105 other assets**. Text CSVs and the unowned
  `layout_home2.csv` / `layout_skill_cutin.csv` are among the fallback downloads.
- Played mission 10, Start of Adventure, with Selena and Cerulean Knight Stein.
  Unit/enemy sprites, attacks and effects were present in sampled screenshots.
  Won; results showed 336 Zel, 25 Karma, 20 XP and two captured units. Returned to
  the Mistral map, not Home. Neither Store rating nor Facebook-gem prompt appeared.
- Result-screen usability observation: no visible Next control in captured frames;
  tapping the bottom-center (window coordinates 601,903 at 1202x934) advanced both
  result pages. Tapping higher on the results and scrolling did not. Compare against
  the accepted renderer before classifying this as a regression.
- Client-only restart: title, Connecting and Checking files screens, then daily
  task notice and Home; no DLC prompt. Selena and 2,294,988 Zel / 685,227 Karma
  persisted, daily quest counter was 1/3. Reopened Prairie list successfully.
- Screenshot sampling cannot certify zero brief download screens between captures
  or exact durations; queued-name logs and snapshots are the authoritative evidence
  of actual fallback writes. Audio has not been assessed.

Evidence collected under `out/qa/current/session16/content_client_c16/collected/`:
`20261021-160130` (installed hook), `160534` (Home), `160906` (quest results),
`161224` (client restart). These are the harness's timestamp directory names.
The census was **built**, including after adoption of the incomplete coverage
report; the first prompt was **left to the player**, not suppressed.

### Additional resumed evidence

Full harness/client restart retained Selena, currencies and the new quest counter
1/3. `snapshot_c16_before_controls.json`: **0 owned, 109 other assets**. Collected
`20261021-161507` contains the full-restart evidence. No stored DLC answer was changed.

Part D: parking `action_attack1.png` published generation 2 and login unavailable=1.
C16 reached Home without a crash; `snapshot_c16_missing_control.json` contains exactly
**1 owned asset**, `action_attack1.png` (5,633 bytes, digest prefix `7ce1669678598790`),
and 109 others. Restore completed and the broker published it after 72 seconds.
Collected `20261021-161722` retains this control's client log.

Part E: broker off reported incomplete (`broker disabled`, generation 0). Native
login displayed a Downloading screen with 2,233 total files (captured at 1,319/2,233),
then reached Home and Mistral normally. No DLC prompt. Log confirms provider dormant,
census built and the stored false answer retained. Final snapshot `c16_after.json`
parses successfully: **3,147 owned, 109 other assets**, expected for this intentional
legacy-download control. C16 is no longer asset-cold; do not reuse it as a cold baseline.
Collection: `20261021-210903`. No uppercase `LEFT OUT` decision appears in the combined log.

The system/server clock crossed into October 22 UTC by this last control (local
October 21 evening); the new daily spin/calendar and refreshed tasks therefore are
not evidence of lost restart persistence. Earlier same-day restart checks retained 1/3.
No exact wall-clock performance comparison is claimed.

Game and harness are now stopped; port 29960 has no listener. The parked file was
restored, C1/live data remain untouched, and the next bounded implementation directive
is `CLAUDE_SESSION_18.md`. Result continuation cue remains a comparison task, not a
confirmed new regression. Audio and exact brief-screen timing remain unassessed.

## Original failed attempt (historical)

Setup succeeded: C16 registered, loopback exemption added and verified by harness check (zero outstanding items). Copied 179 account/master-state files, no assets. `snapshot_c16_prelaunch.json` shows zero provider-owned and zero other cached assets. The corrected candidate/server hashes remain those in `QC_SESSION_16_FINAL_REVIEW.md`. Harness save `s16_cold` was freshly seeded and broker reports expected incomplete coverage.

Launch the game through the registered **[ContentTest C16]** shortcut. The computer-use app inventory initially omitted it; Explorer's Applications folder search for ContentTest exposed it. Directly launching BraveFrontier.Windows.exe failed to load AppX runtime dependencies (`msvcp120_app.dll`, then `vccorlib120_app.dll`); this was a launch-method error, not evidence those dependencies are absent from the registered package. The failed direct-launch process was stopped. Registered activation successfully displayed the game.

## Actual runtime defect

Collected log: `out/qa/current/session16/content_client_c16/collected/20261021-055758/offlinemod_content.log`.

```
content provider hooks installed ... waiting for the server's broker at 127.0.0.1:29960
startup coverage path DECLINED: censusLookup: censusLookup: byte +0 is E9, expected 55. The LocalState census and the background-download prompt stay the game's own.
```

Source evidence: `main.cpp` calls `ContentProvider_Attach()` before `Startup_Attach()`. `kStartupSignatures` includes censusLookup at RVA 0xE417E0, a function the content provider hooks. `Startup_Attach` validates signatures against the already-modified live image. Investigate the expected Detours installation overlap rather than relaxing the signature to accept any jump. Automated decision/static-on-disk tests did not exercise this real attach sequence.

The first visible game screen is a combined terms/privacy and legal-age/parental-consent checkbox. Computer Use cannot submit age verification; the user must handle this screen. No checkbox or I Agree was pressed. No quest, rewards, restart persistence, missing-file control or broker-off visual test has passed. Audio was not assessed.

## Resume conditions

Fix and QC the attach-order/signature integration defect, with a real registered-client installation log showing the startup path installed. The user must handle the first-launch consent gate. Then continue the full checklist, preserving the distinction between stock fallback due to incomplete coverage and stock behavior caused by a declined hook. Do not report either as proof of a cache-free startup.

No live save, normal installed-client cache or accepted C1 DLL was changed. Registration/exemption and the new C16 test state are intentional setup changes.

The game and isolated harness were stopped after collecting evidence; port 29960 is free.
The Session 17 repair directive was sent to the existing Claude desktop Code session
"Session 16 implementation and QC handoff" and its response started. A thread follow-up
checks every 30 minutes for completion, then performs QC without touching the consent gate.

## Session 18 reading of the C16 evidence — October 22, 2026 (Claude; no client run)

No client was launched, installed into or reset: C16 (still `45a2c1b1…`, warm) and C1
(`09056ee5…`) are as Codex left them. The October 21 collections were read again with the client's
own startup rules ([CLAUDE_SESSION_18_HANDOFF.md](CLAUDE_SESSION_18_HANDOFF.md)):

- **The login priority window is exact.** In the provider-off control (pid 13488 in
  `collected/20261021-210903`) the priority listing is complete (3,150 distinct names, under the
  4,000-name cap). Every one of the 2,257 names of the Session 18 login set was queued on the
  priority list. None of the 192 distinct names the client's own exclusion table drops (203 slots at
  VA 0x184DF00, `DLCManagerComplementor::loadBundleResourceMap` 0x16F7F0) was queued on it. The 62
  "absent" login names of Session 16 are all such names. (The oracle,
  `out/qa/current/session18/queued_oracle.py`, records every list a name went to; a name can reach
  the main list first and the priority list later.)
- **The main list is capped, so it gives positive evidence only:** 116,770 queue calls, of which
  the first 4,000 distinct names (up to t=62,785) are listed. Every one of the 408 distinct names the
  Session 18 derived lists ask for was among them: 12 dungeon-key, 106 stamp / stampset, 1 medal,
  52 Grand Quest, 4 banner, 136 map-tile, 111 raid-point and 6 DBB names (428 per list, 20 shared).
- **Why the route downloaded layouts.** In every provider-on launch the game itself downloaded
  `layout_home2.csv`, and in most also `layout_dungeon_select.csv`, `layout_mission_result_top.csv` /
  `_unit.csv` and `layout_skill_cutin.csv`, plus 17 more layouts. C7 did not own them because their
  resourceMap entries name files that exist nowhere; the client drops exactly those names itself.
  The C8 candidate owns all 22. Still left to the game's downloader on that route: the text
  dictionaries (client state), `effect_prt_1011_white.plist` and the 8 other roots whose dependency
  exists in no source.
- **The result screen's missing Next control is the stock screen, not a regression.** The result top
  (fn 0xA25610) creates its only button inside `if (MissionRepeatConfigInfo flag +0x14)`, and only the
  quest-repeat option screen sets that flag; outside repeat mode the player taps to advance. Proof:
  `out/qa/current/session18/check_result_cue.py`, 15/0.
- **Master tables.** C16 (both its LocalState and its own package folder), C1 and the installed client
  hold byte-identical copies of all 11 client files the Session 18 manifest names
  (`scripts/check_client_inputs.py`, read only).

Audio and brief-screen timings remain unassessed. No `LEFT OUT` decision has occurred on any client.
