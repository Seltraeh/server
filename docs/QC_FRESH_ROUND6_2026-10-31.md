# Independent QC: fresh rounds 5–6, October 31, 2026

Verdict: focused automated checks pass; acceptance remains incomplete. Round 6 reports no runtime edits. This review also covers the round-5 correction tools and name/popup patch carried into round 6. No screen use, deployment, live-data writes, commits or pushes were performed. Claude's visual observations remain attributed to Claude.

## Actionable finding

**P2 — popup patch writes can fail while installation/removal reports success.** `../offline-proxy/OfflineMod.win32/content/cp_name_fit.cpp`, `NameFit_Attach` loops over three width operands without checking `WriteCode` and unconditionally sets `widthPatched=true` and logs installation. `NameFit_Detach` likewise ignores restore failures and clears the flag. A failed protection/write operation can leave a partial patch, falsely report success, or discard the information needed to retry restoration. Check every write; track installed sites individually; roll back successful writes on partial installation, retaining state if rollback fails. Inject failures at each write and restore in a maintained test. Successful stock-site disassembly checks do not exercise this failure path. No failure was reproduced in the actual client; this is a code-path finding.

## Independent verification

Evidence folder: `out/qa/current/review_round6_20261031/`.

| Check | Result |
|---|---:|
| `test_unit_mst_corrections.py` with separate work root | 34 pass, 0 fail |
| `test_sgtext_corrections.py` with separate work root | 12 pass, 0 fail |
| `test_enhancement_sp_wire.py`, isolated copied database, port 30977 | 36 pass, 0 fail |
| `test_leader_exp_boost_wire.py`, isolated copied database, port 30980 | 21 pass, 0 fail |
| offline-proxy `check_name_fit_site.py`, original x86 executable | 24 pass, 0 fail |
| Total | 127 pass, 0 fail |

Server tested: SHA256 `c28a8db9efacc43d5c5f4bf707d8036520f8e8df7e69e3438ba6db1e67e532fe`, round-4 isolated candidate. Round 6 made no new server build. Checked round-5 popup DLL: `466ce6fcf29beb0c43d02167aa93f2d0a069f7d3ff0d039959e532f2e1422ec9`. Reviewed name-fit source: `45fd2efe35dc8d8ccbbe3f0962c5c7b2674f1b0d2913e980bcc4e3993e462d15`.

The previous predictable temporary-file write-through finding is resolved: exclusive temporary creation and replacement preserve linked source bytes; independent regression passed. Dictionary correction preserves unrelated fields and refuses collisions; its regression passed. Name-fit stack movement and popup operand locations pass the disassembly assertions. This is not an independent runtime ABI/visual test or a rebuilt-DLL acceptance.

Claude's reported round-5 full suite (67 entries, 2755 passes) is historical evidence, not rerun here. No claim is made that these focused tests audit every pre-existing dirty change or prove all gameplay correct.

## Acceptance and research corrections

- **#25:** eligibility is visually supported by round 6; actual client fusion, displayed/stored gain, cap and relog remain required for closure. Calling fusion optional overstates acceptance. The fixture's zero SP is an authored test condition, not evidence of the original starting balance. Migration code defaults to 10. The wiki also describes ordinary-unit probabilistic gains and Omni+ capacities; UnitMix explicitly leaves ordinary-unit probability unported. Inventory these separately; do not call the entire SP system complete or invent odds.
- **#26:** rejecting a Fuu chest multiplier is correct. The server forwarding reported resources proves payment, not client rate behavior. “No server defect” does not close absent per-hit item support. Determine archive schema, packet field and client consumer before classifying the omission as content-only.
- **#22-1:** Lukroar deterministic gate comparison remains unperformed.
- **#40:** popup visibility is supported by round-5 observations, but actual evolved-unit Overdrive/UBB use and relog remain unperformed.

Sources inspected October 31; API revisions saved in this QC folder (Drop Rate/Fuu snapshots remain under round-5 `wiki/`):

- [Unit Skills, revision 656738](https://bravefrontierglobal.fandom.com/wiki/Unit_Skills?oldid=656738): Omni eligibility, SP material values, initial balance and UBB acquisition. Supports separating selection from actual fusion and combat acceptance.
- [Drop Rate Boost, revision 645616](https://bravefrontierglobal.fandom.com/wiki/Drop_Rate_Boost?oldid=645616) and [Fuu, revision 678968](https://bravefrontierglobal.fandom.com/wiki/Fuu?oldid=678968): per-hit resource effects; chest exclusion. Current indexed wiki text independently corroborates the saved drop-rate snapshot.
- [Tower of Mistral, revision 674494](https://bravefrontierglobal.fandom.com/wiki/Tower_of_Mistral?oldid=674494): story Juggernaut has 50,000 HP, causes Weakness and has listed vulnerabilities. Do not transplant a Research Lab or Rift variant based solely on its name; the local 50,000 HP itself is not a defect.
- [The Mecha Gods Awaken, revision 659707](https://bravefrontierglobal.fandom.com/wiki/The_Mecha_Gods_Awaken?oldid=659707): Sanctum of Fire Lv.6 has boss 10354, 70 energy, three battles, 7000 EXP, room-two Dragon Graven/Dalimaone and named fire AoE skills. HP and capture-rate fields are empty: these must be recovered or explicitly left unresolved, not presented as sourced numbers.

Release remains blocked by the patch failure-path fix, open visual acceptance and unresolved content scope (#12/#15/#17/#18). No defensible completion percentage or release date follows from the current evidence.
