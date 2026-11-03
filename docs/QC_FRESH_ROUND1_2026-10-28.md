# Independent review of fresh round 1

October 28, 2026. Scope: review Claude's completed first fresh-profile round and prepare round 2. No computer use, runtime edits, deployment, staging, commits or pushes. Claude visual findings below are reviewed written evidence, not independent screen observations.

## Verdict

The reviewed fixes are suitable to carry into the next isolated candidate. No new runtime defect was established in the reviewed round. This is not an all-bugs release approval. Missing content, client acceptance and packaging gates remain.

## Verification

- Read CLAUDE_FRESH_ROUND1_HANDOFF_2026-10-28.md and updated acceptance matrix; inspected content patch tool, manifest validation, energy change and its Mission caller, ignore exceptions and release instructions.
- Candidate SHA256 independently matches 03101975f6ba7a1c919b063eaeab627a004f6f497a03d0aba11ffd2b4bde9e03.
- Current tracked server diff SHA256 independently matches the full report: 88c473d5e9042b0223f7b11f1bcb3201fe2c1e71a1d4b347d50e53383380ebad. This comparison covers tracked diffs, not every untracked artifact.
- Claude's saved complete report at out/qa/current/fresh_round1_20261028/regressions_full_03101975/SUMMARY.json records 63 entries, 2,692 passing checks, zero failures, finished October 28 19:39:58 America/New_York. This is reviewed evidence, not a new full rerun by Codex.
- Independent focused reruns: content patch 13/0; offline bootstrap 36/0 (live portion skipped); gameplay wire 43/0 against the round-1 binary, isolated port 29974. Fixture roots redirected under out/qa/current/review_round1_20261028 to preserve earlier evidence. No need to rebuild or repeat the full suite for this documentation-only review.

## Findings and limits

The energy fix reads the timestamp using the stored old level and adds the new cap. Both the 998 to 999 rate transition and an ordinary cap increase pass the focused regression. The manifest reader now rejects non-objects and invalid field types before member access. The tutorial patch validates exact input/output hashes and replaces the file to preserve hard-linked controls; tests verify idempotence and refusal of unknown content.

The tutorial delayed-response A/B and mission 234/level 999 visual routes are Claude-reported. A cold quest-select asset route remains untested. Mission 234 did not crash on synthetic progression; that does not establish the original report fixed under all conditions. Level 999 still needs the on-screen cap completion check.

Packaging remains a concrete gate: apply_content_patches.py intentionally allows an absent target to exit 0, and the release builder does not invoke it. Therefore generic success is not proof a release contains corrected tuto8. The eventual release check must require the specific target to exist and have the patched hash, and reject absent/original/unknown states. This is a release integration requirement, not a newly introduced defect in the report-only tool.

No defensible percentage or calendar date for an all-fixes release yet. Four content-heavy areas remain (#12, #15, #17, #18), alongside presentation and reward acceptance. Server tests cannot establish those client/content outcomes. A limited release would require Evan explicitly accepting a narrower scope; this review does not change the requested all-fixes scope.

## Tracker

Updated C:/Users/Evan/Downloads/Bug fixes BF.docx in place with a release overview and dated per-issue notes, preserving original paragraphs. Backup: out/qa/current/review_round1_20261028/Bug fixes BF before progress update.docx. OOXML reopened successfully and original text preservation was checked. Packaged render_docx.py failed because soffice.exe is unavailable; page layout is not visually verified. No desktop application was opened.

## Next action

Use CLAUDE_FRESH_ROUND2_2026-10-28.md. Do not restart the cancelled automation. Keep the original fresh-session handoff and round-1 evidence as history.
