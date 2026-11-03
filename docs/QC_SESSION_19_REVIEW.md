# Session 19 independent QC — October 23, 2026

Scope: Session 19 deltas over its preserved pre-edit copies, not unrelated dirty gameplay work. Nothing deployed, staged, committed or pushed. Sources remain unchanged by this review.

## Verdict

The candidate remains suitable for an isolated **fallback** visual test. It is not approved as a completed cache/download-screen solution. Independent review found one reproducible audit defect and an unresolved area-source proof prerequisite. Two conflicting image aliases also need resolution before release.

## Findings

1. **Area coverage claims need an ordering proof.** `scripts/startup_coverage.py` derives area requirements from the server's curated UserInfo rows alone. Handoff §2.3 explicitly admits that the ordering of client `F_AREA_MST` loading and UserInfo replacement relative to the census was not traced. A matching C16 trace corroborates that route but cannot establish all initial/version-refresh paths. Prove the supported x86 order, conservatively include and bind both possible sources, or retain area as unknown. Today's remaining unknown groups prevent an unsafe completeness decision; this is not a reproduced runtime bypass.
2. **Malformed staged manifest crashes the bootstrap audit.** `scripts/check_mst_bootstrap.py:staged_rows` calls `m.get` before checking that parsed JSON is an object. Independently reproduced with an isolated temporary `content/F_ITEM_MST/manifest.json` containing `[]`: `AttributeError` at line 124. Validate shape first and test list/null/scalar inputs against the server's handling. This affects audit robustness, not the observed valid-data startup.
3. **Two existing C8 aliases remain questionable.** The recorded area consumers and decoded-cache evidence select `dungeon/` for `dungeon_name_frame_red_off.png` and `frontier_rift_banner.png`; C8 selects `mission_select/` and `challeng/`. Analyze other consumers before changing a global flat-name alias. No assets or aliases were changed by QC.
4. **Release packaging needs correction.** The regression runner depends on ignored `tools/bin` scripts; include the specific durable dependencies or relocate them. The S19 proposed list also incorrectly includes `ASYNC_REVERSE_PROGRESS.md` as technical documentation. Coordination/session/checklist/QC notes stay on disk but are excluded from the proposed commit.

## Independent evidence

- Candidate SHA-256: `02b95f90c98bcf921a247cc305c0244917c3a65274546ffc7eb23a074ade9272`.
- All 23 source/document/fixture hashes in the handoff's full-hash table match: `out/qa/current/session19/qc_source_hashes.json`.
- Reviewed C++ served-projection construction, immutable `m_clientAreaMst` lifetime and UserInfo copy, broker root-aware binding, client-part binding validation, Python derivation/curation and bootstrap audit behavior.
- Manifest tests: **116 passed, 0 failed**.
- Bootstrap synthetic/real/live tests: **38 passed, 0 failed**, `qc_mst_bootstrap.log`. Earlier offline-only run: 26/0 with live cases explicitly skipped.
- Server-table snapshot/binding tests: **40 passed, 0 failed**, `qc_server_tables.log`.
- Binary startup facts: **184 passed, 0 failed**, `qc_rules_binary.log`.
- Binary bootstrap facts: **37 passed, 0 failed**, `qc_bootstrap_binary.log`.
- Recorded provider-off queue cross-check: passes, `qc_evidence.log`.
- Proxy unit tests: **677 passed, 0 failed, 5 integration skips**, `qc_proxy_unit.log`.
- Mocked harness: **86 passed, 0 failed**, `qc_harness.log`.
- Coverage-wire and live startup tests are in progress; final results will be appended below.

Claude's wider regression report (61 entries, 2,640 checks, zero failures) is prior evidence, not an independently repeated result. This review reruns the checks affected by the Session 19 changes and records reused evidence separately.

## Visual acceptance

See `SESSION_19_CLIENT_RESULTS.md` for the isolated visual run and any required human action. A fresh client must use a new package identity and empty LocalState; C16's previous warm route cannot substitute for that. Consent attestations remain human-only. A delivered-table audit is not a first-run visual pass, and screenshot sampling cannot prove audio or absence of brief intervening screens.

## Next work

The bounded Session 20 directive closes the area proof gap, reviews the two aliases, derives login-end preloads, repairs the audit crash and fixes release dependency guidance. It does not authorize blanket screen suppression or unrelated gameplay changes.
