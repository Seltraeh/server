# Client feedback — October 1, 2026

User reports and screenshots:

- Ignis Halcyon Vargas enhancements show Current SP 0, Used SP 0, Total SP 0/0;
  Burst Frogs cannot be selected to gain SP. Screenshot confirms the counters.
- Three supplied Deemo units were visible, could be added to the team, and
  battled successfully in mission 10. **Client pass for that scope**; this does
  not certify every evolution, skill effect or encounter.
- Replaying mission 10 still triggers the Farm unlock scene. **Observed failure**
  on the tested installation; retain mission 10 as the reproduction, not the
  previously suggested milestone 11.

Read-only environment verification:

- No gimuserverw process was running at inspection time, so the executable used
  for the screenshot cannot be identified from a current process.
- The normal launcher points to out/build/debug-win64/standalone_frontend/Debug/
  gimuserverw.exe, timestamp September 27. The independently reviewed build is
  out/build/bugfix-0929/standalone_frontend/Debug/gimuserverw.exe, September 30.
- deploy/gme.sqlite still has only legacy fe_bp/fe_max_usable_bp columns;
  fe_sp, fe_used_sp and fe_max_sp are absent. Thus the reviewed SP migrations
  have not been applied to this save.
- Owned Vargas 1034 / species 10017 is level 150, BB 10, SBB 10. Eligibility
  is not the missing piece; the new SP storage/mapping has not reached this save.
- Stored scenario_info is 0,0@0@0@0&|0,0. special_scenario_info contains
  randall,dungeonkey,achievement,guild_enter,minigame. These values alone do not
  establish the exact Farm trigger or prove the updated marker fix fails.

Conclusion: SP 0/0 is consistent with the older unmigrated installation.
Farm repetition must be retested on the reviewed build before treating it as
a regression of the new scenario code. The old executable can still read the
updated unit archive, so Deemo working does not prove the new server code ran.

Next client run should use the updated executable and matching data with a
backed-up/copied save and explicit test config. Verify nonzero SP capacity in
UserInfo and the client before consuming frogs. Retest mission 10 twice, record
scenario uploads/result fields and whether the Farm scene repeats. SP purchase
and reset were unfinished at the last QC; fusion SP gains are a separate test.
No live executable, save values or progression markers changed in this check.
