# Post-quest prompt removal — October 11, 2026

Acceptance update: player reports neither prompt appeared in testing. The
installed C1 DLL hash matches the candidate below, and collected `.log.1`
contains the successful `post-quest prompts disabled` installation line
(`collected/20261011-182622`). Prompt suppression is player-confirmed. This
does not imply live deployment or additional tests beyond those reported.

Implemented in the current sibling `offline-proxy` checkout. Removes the
Windows Store rating invitation and the Facebook-for-one-gem invitation after
quest/friend handling. No reward, account, friend or mission data is changed.
This isolated candidate is installed in C1 and player-confirmed.

## Implementation and QC

`OfflineMod.win32/content/cp_post_quest.cpp` changes two conditional branches
to unconditional branches to their existing no-dialog destinations. It checks
the supported EXE identity and relocation-masked signatures for both contexts
before writing either opcode. Unsupported code is left unchanged and logged
as DECLINED. Detach restores the original opcodes.

- Rating: x86 RVA A25CD6 `JE +5` becomes `JMP +5`, skipping the sole direct
  call at A25CD8 to PTRateThisAppPopup::showPopupIfRequired (E50400). Existing
  result initialization continues at A25CDD. The callee uses APP_RATE_KEY and
  APP_RATE_MISSION_KEY, then RATE_APP_WINDOWS_BODY. ARM64 reference: rating
  function E1415C, caller MissionResultScene::initialize at 18CAA58.
- Facebook: x86 RVA A1F92E `JE +5A` becomes `JMP +5A`, following the existing
  no-promotion route at A1F98A. This skips BINDFB_FACEBOOK_REWARD_MSG/TITLE,
  dialog creation, and the input-disable write. Both ordinary scene-navigation
  calls and the common string cleanup at A1F99C still run. It does not emulate
  successful Facebook linking or grant a gem.

Validation: Visual Studio RelWithDebInfo DLL/test build succeeded in the existing
build tree. `tests/fixtures/check_post_quest_sites.py` passed against the actual
EXE: original bytes, mutation rejection, exact old/new branch targets, rating
call scope and Facebook navigation/cleanup. Proxy suite: 411 passed, zero failed,
four opt-in skips. No server/packet change; no live deployment or client reset.
The build cache port and pre-build bin DLL/PDB were restored after staging.

Candidate: `out/qa/current/session15/postquest/libcurl.dll`
SHA256: `09056ee55d52936ad584a1ec79e044369c817da0b904901a9e3deb7947ba9c24`
Port 29960; includes the accepted Session 15 gate/audio changes. Registered in
the harness's known-DLL list with audio capability. Evidence stays under that
same bounded postquest directory. Future normal proxy builds include this source
when OFFLINEMOD_CONTENT_PROVIDER is enabled; no live-port candidate was built here.

## Client steps (no repeat of the sound/banner swaps)

1. Close [ContentTest C1] and stop its harness. In PowerShell at the repo root:

   ```powershell
   python scripts\content_provider_client_test.py --identity c1 dll --file out\qa\current\session15\postquest\libcurl.dll
   ```

   Keep the printed rollback command. This changes only the test package DLL.

2. Restart the same persistent save, without reseeding:

   ```powershell
   python scripts\content_provider_client_test.py --identity c1 server --exe out\qa\current\session13\bin\gimuserverw.exe --policy out\qa\current\session15\policies\family_policy_c6.json --disposable --save s15_audio --mst-root out\qa\current\session14\mst_gate_test --timeout 300
   ```

3. Wait for READY, launch [ContentTest C1]. Clear four short quests, including
   one with a helper who is not already a friend so the normal friend-add flow
   is exercised. Keep your usual friend-add choice. Expect normal results,
   rewards and map return, with no rating or Facebook invitation and no frozen
   input between screens. Ordinary friend handling must still work.
4. Close and relaunch once; clear another quest. Confirm persistence and no
   recurrence. No new Facebook gem should be credited.
5. Close the client and stop the harness; collect:

   ```powershell
   python scripts\content_provider_client_test.py --identity c1 collect
   ```

QC should verify the install log says `post-quest prompts disabled: Windows
Store rating and Facebook gem promotion; stock no-prompt continuations`.
Absence of a popup alone is insufficient if installation was DECLINED.
