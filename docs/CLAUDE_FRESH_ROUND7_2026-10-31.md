# Fresh Claude session — round 7: close acceptance gaps and establish content contracts

You have no previous-chat context. Work in `C:/Users/Evan/BF/BF-WorkingDirRust`, its nested `packet-generator`, and sibling `C:/Users/Evan/BF/offline-proxy`. Evan authorizes scoped fixes and isolated client testing. Preserve all existing dirty work. Do not stage, commit, push, deploy, install, reboot, reset live data or replace the user's normal client/server. Codex is not controlling the screen; you own visual checks. Never automate age/parental attestations.

Read `docs/QC_FRESH_ROUND6_2026-10-31.md`, `docs/CLAUDE_FRESH_ROUND5_HANDOFF_2026-10-30.md` **including its later addenda**, `docs/CLAUDE_FRESH_ROUND6_HANDOFF_2026-10-30.md`, `docs/BUG_ACCEPTANCE_2026-10-27.md`, and `docs/REMAINING_CONTENT_SCOPE.md`. Historical instructions to deploy are not authorization to deploy now.

Use one new bounded root `out/qa/current/fresh_round7_20261031/`; keep maintained generators/tests outside out. Reuse existing VS build trees. Check processes/ports before building or testing. Copy saves and use isolated identities/content roots; never treat warm S19/C16 caches as fresh installs. Candidate server is round-4 `out/qa/current/fresh_round4_20261030/bin/gimuserverw.exe`, SHA256 c28a8db9efacc43d5c5f4bf707d8036520f8e8df7e69e3438ba6db1e67e532fe. Round-5 popup DLL under `fresh_round5_20261030/proxy_29960_popupfit/` is 466ce6fcf29beb0c43d02167aa93f2d0a069f7d3ff0d039959e532f2e1422ec9. S19 was restored to baseline 9633061d904203c7110c375a9e5309543d88eef2f22da92c565a443c86852b0e; verify actual state first.

## 1. Fix the independent QC finding first

In offline-proxy `OfflineMod.win32/content/cp_name_fit.cpp`, make popup installation/restoration handle every `WriteCode` result. Track per-site state; partial failure must roll back completed writes, report failure accurately, and retain restoration state on rollback failure. Do not lose the independent name-fit patch's state. Add maintained failure-injection coverage for each install/restore position and a successful lifecycle, including a retry after failed restoration. Preserve the guarded supported-client identity and operand signatures. Build with existing trees, run disassembly checks and focused tests, then one long/short name and BB/SBB/UBB popup visual smoke on an isolated candidate. Record source and binary hashes.

## 2. Close the four outstanding acceptance routes

Prioritize these before broad content implementation. Existing fixture tools in round-5 `tools/` are evidence to inspect, not trusted blindly.

1. **#22-1 Lukroar 51247:** deterministic normal and special-gate pulls beside a known-good rarity control. Use copied fixture gacha/selector data only. Record scripts and actual asset requests, gate-to-unit animation sequence, absence/presence of the reported failure and log evidence. Separate missing gate art from old #24 overlay behavior. Fix only a reproduced cause and retain controls.
2. **#25 SP:** complete an actual Omni Frog fusion on an eligible unit (expected +30), plus an ineligible control; inspect displayed SP, database gain/material consumption, cap behavior and relog. Check an untouched migrated/default unit's starting SP separately from the deliberately zero-SP fixture. Do not mark selection alone as complete. Independent automated SP test already passes 36 checks.
3. **#40 UBB:** use an actual documented evolution chain, verify resulting BB/SBB levels, unlock UBB at SBB10, enter Overdrive and execute UBB in battle, then relog. Preserve SBB5/0 and lower-rarity negative controls; check no old-form skill leakage. Popup listing alone does not prove battle use. Wiki Unit Skills revision 656738 is the starting reference.
4. **#26 rewards:** plain leader versus Roglizer/Fuu and helper on matched copied fixtures. Roglizer EXP can be exact; record displayed and stored deltas. For Fuu, distinguish client-generated random drops from fixed clear rewards and server payment. Several random samples alone cannot establish correctness: use client process/packet evidence or controlled drop inputs, document sample limitations. Never add Fuu's item bonus to chest rolls.

## 3. Resolve the missing-system contract; do not invent parity

**Per-hit item drops:** trace one actual enemy item-drop definition from generator/archive through MissionStart encoding to the client consumer and MissionEnd inventory award. Determine whether the schema/encoder lacks a channel, content simply lacks records, or another existing mechanism supplies it. Supply field names, code locations and an isolated minimal example. Only implement a missing channel once the client contract and source-backed behavior are established; test zero/one/multiple items and persistence. Otherwise write a concrete bounded follow-up with the missing evidence. Do not close #26 as fully correct merely because chests are excluded.

**SP scope:** inventory ordinary-unit probabilistic SP, initial 10 SP, duplicate/material values, spent+available caps and Omni+ limits against Unit Skills revision 656738. The ordinary-unit chance is explicitly unported in UnitMix; unknown probability is a remaining gap, not license to choose a number. Keep changes bounded to evidenced reported defects; list broader missing features separately.

## 4. Correct and define remaining content scope

Use exact wiki revision links plus local client/MST evidence for each proposed batch. Codex saved API snapshots in `out/qa/current/review_round6_20261031/`.

- #17 Fire Mecha God: The Mecha Gods Awaken revision 659707 identifies boss10354, mission structure and named skills for Sanctum of Fire Lv6. Compare mission100320 to those facts. Recover missing HP/AI action values/capture rules from client data or another precise source; the wiki leaves HP/capture rate blank. Prepare a field-by-field proposed generator change and tests. Do not fabricate exact values. Expand to other elements only after their own evidence.
- #18 story bosses: replace the current recommendation to reuse Research Lab Juggernaut AI automatically. Tower of Mistral revision674494 describes the story encounter; match that variant's effects/vulnerabilities and existing 50,000 HP. Compare skill IDs before reusing anything. Do not import Trial/Rift difficulty into story content.
- #12 Vortex Trials: retain the 41-mission inventory and distinguish missing encounters from merely present menu/MST rows. Define one first encounter and required assets/AI, not an all-trials rewrite.
- #15 roster/assets: separate absent archive rows, intentional region exclusions and proven missing packaged/provider assets. The 135-path candidate list does not prove 135 broken units. Keep the Global-vs-JP decision explicit; do not silently enlarge the roster.

Keep these content items as researched implementation proposals this round unless required to reproduce/fix the four acceptance targets above. This prevents another round from ending with neither accepted fixes nor a bounded content contract.

## Delivery and stop condition

Run the focused tests for changed code; if runtime changes, rebuild and run the maintained regression runner on isolated fixtures, reporting failures honestly. Update the acceptance matrix with implemented / automated-tested / Claude visually observed / still open distinctions. Append a dated correction to stale current scope without erasing historical findings. Do not turn the cancelled heartbeat back on.

Write `docs/CLAUDE_FRESH_ROUND7_HANDOFF_2026-10-31.md` with changed files/hashes, test commands/counts, candidate identity, visual route/evidence, restored baseline state, unresolved issues and one recommended next content batch. Include a proposed release file list across all three repos: maintained runtime/generators/tests/docs only; exclude out, copied saves, screenshots and transient session/QC coordination files. Preserve everything on disk. Stop for Evan/Codex review; no deployment or automatic next session.
