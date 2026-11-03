# Session 15 QC — October 9, 2026

October 11 acceptance: player confirmed Parts A/B, C2/C3 replacement, C4
restoration and C5 visuals/audio. Restored banner (182,625 bytes) and original
gate sound (31,636 bytes) are confirmed in collected logs; disposable changes
are empty. C6 before/after snapshots contain the same 3,931 files with zero
new, removed or changed entries. Session 15's tested routes are accepted;
this is not complete cache-free startup or live deployment approval.
The newer post-quest prompt candidate is also player-confirmed (both prompts
absent; successful install line collected October 11):
[POST_QUEST_PROMPTS.md](POST_QUEST_PROMPTS.md).

Farm follow-up: the current s15_audio save records five Farm rolls:
`0:0:0,0:0:0,0:0:0,0:0:30,0:0:0`, with three taps remaining. The latest
TownUpdate consumed two taps from five, both empty. No item was rolled in this
batch; one Karma-only tap remains. This observation does not demonstrate a
lost payout. If guaranteed/nonempty Farm harvests are desired, that is a
separate balance change rather than the visual defect previously tested.

## October 11 client follow-up

Player reports tests before C2 passed. C2 did not show the Christmas image.
The checklist targeted the wrong banner: archive gate 99007 (Test Gate: 7 Star)
uses `gacha_rare_bg_img_20161028_5M.png`, while the original command changed
`gacha_rare_bg_img_20170524.png` (Rare Summon). The current provider log confirms
the 7-star banner was served from `gacha/gacha_rare_bg_img_20161028_5M.png`.
Corrected the checklist command and expected log filename. This failed visual
replacement is a test-instruction defect, not evidence of cache fallback or
an elevation mismatch. Retest Part C's corrected image swap and restore;
Parts A/B need not be repeated. Audio acceptance remains separately pending.

After the corrected swap, the player encountered "Refreshing the latest data
files. Application will close now." before verification. Evidence collected
under `content_client_c1/collected/20261011-120149`: provider active with 57,631
names, generation 2; NoticeScene entered after login, zero provider read failures
and zero tripwire writes in the recorded counters. The disposable record holds
the corrected image swap. Cause is not established; Part C remains blocked
pending one ordinary relaunch with the same harness and no cache reset. This
notice must not be recorded as a successful swap or dismissed as expected.

Subsequent ordinary relaunch succeeded: player confirms the Christmas banner
on Test Gate: 7 Star (C2 visual PASS), with download screens after Login and
on Home load. Collection `20261011-121242` confirms the target banner served
as 149,968 bytes; latest counters show zero provider read failures and zero
tripwire writes. The shutdown notice did not recur on that retry; its cause
is still unconfirmed. Part C can continue. The current disposable record has
only the image swap, so C3 needs the sound swap reapplied before a fresh client
launch. Image restoration, audio replacement/restoration and C5 remain pending.

C3 follow-up: player did not hear the replacement, initially interpreting
"title voice" as title music. Collection `20261011-121951` confirms the sound
swap is active and `bf253_se_gousei02.mp3` was served to the RAM audio reader
at generation 2, 55,428 bytes (the replacement clip's size). Latest counters
remain at zero read failures/tripwire writes. Delivery is evidenced, audible
playback is NOT yet accepted. Clarify the short clip and gate-appearance timing
before asking for one focused replay; do not repeat Parts A/B.

C3 accepted: player subsequently confirmed hearing the replacement clip as
the black gate spawned. This completes audible replacement confirmation in
addition to the recorded RAM delivery. C4 restoration and C5 ordinary playback/
visual checks plus final snapshot remain pending.

Verdict: the isolated candidate is suitable for the bounded client test in
[CLIENT_SESSION_15_STEPS.md](CLIENT_SESSION_15_STEPS.md). This is not production
acceptance. Summon bug #24 stays open until the required final white transition
and absence of premature white are observed in the client. Audio and expanded
UI ownership also need the audible/visible replacement checks in Part C.

## Scope and findings

Reviewed Session 15's gate decision and x86 hook, CRT audio reader hooks,
signature checks, policy generators, candidate installation and audio-policy
guard, tests and handoff. Earlier dirty server and packet-generator work is not
new Session 15 work and is not recertified by this review. No server C++ changed
in this session; the server remains the Session 13 candidate.

No blocking production-code defect was identified in this scope. The gate hook
now follows an observed final opening animation rather than the global animation
count. It preserves the expected final flash instead of merely suppressing early
white. The audio hooks serve owned sounds from memory and reject missing owned
content rather than silently using a stale cached copy. Static checks verify the
supported executable's hook site and its fopen_s/fread/fclose reader contract;
these are not substitutes for running the actual client.

Corrected during QC:

- Part C said to allow five minutes while the command retained a three-minute
  timeout. Its command now explicitly uses `--timeout 300`.
- Part C now takes `s15_c6_before` after Parts A/B. Those earlier C3 runs may
  legitimately cache sounds/images; their writes must not be attributed to C6.
- The mocked harness startup test now supplies its fake broker's owned count
  instead of scanning the actual content tree. The original run was stalled in
  `owned_targets` filesystem enumeration, not the server lifecycle.
- The gate-model test resolves SAM paths in one directory scan instead of
  repeating the recursive scan for every animation. Its assertions are unchanged.

## Independent verification

Evidence is bounded to `out/qa/current/session15/qc_*.log`.

- Visual Studio CMake target `content_provider_tests`, existing proxy build
  tree, `RelWithDebInfo`: successful build.
- Proxy standalone suite: **411 passed, 0 failed**, four opt-in skips.
- Gate patch-site validation against the actual client executable: passed.
- Audio reader contract validation against that executable: passed.
- Gate overlay model: **68 passed, 0 failed**, including premature/missing
  white negative controls and mutations of the replacement decision.
- Audio live driver: **12 passed, 0 failed**, including cold reads and stale
  sound rejection.
- C6 family live driver: **9 passed, 0 failed**; its proxy run reported
  **6,073 passed, 0 failed**, three opt-in skips. All **36,321** traced game reads
  were byte-exact (SAM definitions/pages and particle plists).
- Harness startup suite after the test isolation correction: **49 passed,
  0 failed**.
- Candidate DLL swap/rollback check in a throwaway layout: passed. That check
  exercises the known gate-only candidate; the combined candidate's SHA was
  independently verified. No registered package was changed by QC.

Full gameplay regression suite and actual in-game visuals/audio were not run
by QC. The client checklist is the remaining acceptance step.

## Candidate and client acceptance

- Test identity: **[ContentTest C1]**, isolated port **29960**.
- Combined DLL: `out/qa/current/session15/proxy_29960_s15/libcurl.dll`, SHA256
  `af96f41b2df3359cc2326bb57e58bb9c34610251e07d3a54bd7de2ab8f6e5200`.
- Server: `out/qa/current/session13/bin/gimuserverw.exe`, SHA256
  `90e3eff97739b0d5d5713cae983db9fc4cadaa7e105b8bdf009a81b1444b5ebc`.
- C6 policy: `out/qa/current/session15/policies/family_policy_c6.json`, SHA256
  `e3d29e253a4330d1abbb6a49ba5e93bd3d09910a172e46ca05b112f86c2ce6fb`.

Install the candidate using checklist step 0 with the test client closed.
Parts A/B compare normal and rapid summon input with/without energy overflow.
Part C swaps one sound and one background in the disposable content copy,
restores both, and checks ordinary sound/art plus cache snapshots. Keep the
named saves and collect each part before starting the next.

Do not require every flash log to show exactly frame 45: a render update can
observe a later frame after crossing the threshold. The required behavior is
one final transition after opening, never an overlay hiding a waiting gate.

This candidate does **not** remove all download or login verification screens.
The startup coverage work remains a plan, with legacy files still outside C6.
The live-port twin is not approved for deployment by this isolated review.
No live save, installed client, registered test DLL or deployment was changed
during QC.
