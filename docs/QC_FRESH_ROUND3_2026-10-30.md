# Independent QC of fresh round 3

October 30 2026. Reviewed round-3 handoff, maintained QA helpers and tests, the portrait staging script/proposal, and MissionArchiver's capture encoder. No computer use, client launch, deployment or runtime edit. Visual conclusions remain Claude observations.

## Verdict and independent checks

The helper corrections substantially address the previous broad process-stop and unvalidated-name problems. Independent helper suite: 43 passed, zero failed, using dummy processes and redirected fixtures under out/qa/current/review_round3_20261030/helpers. Log: helpers.log. Earlier evidence was preserved. This is not a full gameplay suite rerun.

Round-1 server candidate independently hashes to 03101975f6ba7a1c919b063eaeab627a004f6f497a03d0aba11ffd2b4bde9e03. It remains the runtime baseline; round 3 did not supply a new runtime build. Prior 63-suite/2692-check evidence is historical, not fresh execution.

Compared parsed deploy/mst/unit_mst.json against the disposable portrait proposal: both have 2291 rows, and exactly one value differs: unit 50254, key 1W9CxaFK, from 245,-5,135,369 to 298,48,180,492. This independently confirms the data delta, not visual quality. Claude reports the client download and accepted Home/info-screen observations. No other portrait field changed.

## Findings

### P2 Ancestor junction containment remains incomplete

scripts/qa/qa_run.py run_dir resolves <qa>/visual/saves, then checks the run is a direct child of that resolved root. It never checks the resolved saves root remains within the validated QA folder. A junction at visual or saves can therefore redirect the entire root outside the intended folder while the child check succeeds. Existing tests cover a junction at the run child only.

A read-only mocked Path.resolve reproduction returned C:/Users/Evan/Downloads/probe for a valid QA folder. No junction or external file was created. Evidence: out/qa/current/review_round3_20261030/containment_probe.py. Add explicit resolved-root containment checks and ancestor-junction tests before further helper reuse. Also resolve/validate existing config/database paths before mutation so an in-run symlink cannot redirect a writer. No such escape was observed in the actual round-3 runs.

### P2 Portrait fix is not yet a maintained release input

The accepted 50254 proposal and staging tool currently live only under out. Do not solve this by deploying to the normal client. Add a maintained, narrow and idempotent correction/generation path that produces isolated proposed data with exact input/output checks and verifies only the intended unit field changes. Preserve live data. Generated download versions must avoid silently overwriting already hard-linked files: the historical staging script excludes the manifest but links all old data files, so reuse with a source already holding v1087 needs explicit refusal or replacement semantics before generator writes.

### Capture finding needs supported-client confirmation before a policy conclusion

MissionArchiver::encodeMissionDropInfo does join multiple unit records with '-'. The handoff's client experiment plus arm64 parsing trace is credible evidence of a mismatch, but this review has not independently traced the supported x86 parser. Calling it an original-game one-per-wave rule is stronger than the evidence establishes. Check the x86 parser and settlement path, including unit type/extra-skill fields, before deciding a correction. A clean supported wire format is a correctness task; changing odds or choosing a new per-wave policy is separate. Do not increase odds as a workaround or silently lose claimed rewards.

## Assessment of the recommendation

Do not deploy the portrait or require a reboot as the first next step. Preserve it as an accepted isolated candidate and make it reproducible. At this review's process check no gimuserverw.exe process was returned, so the previously reported pid 16196 is not an established current blocker. Recheck actual activity next session; never kill a stale PID or reboot automatically.

Retain Super parade formation, EXP/version differences and capture odds as unresolved behavior questions. Produce an evidence-backed recommendation before asking Evan to select policy. The current task authorizes investigation and isolated corrections without publishing.

A provider-off slots run is lower priority: the provider-enabled candidate's acceptance is the release target. Test provider-off only if that configuration is still intended to be supported or needed to distinguish a real defect. Keep the live rollout dependency explicit.

## Next action

Use CLAUDE_FRESH_ROUND4_2026-10-30.md: containment fix, durable portrait correction, supported-x86 capture investigation/correction. Stop with evidence and a bounded next recommendation. No claim that all reports or release gates are closed. The Word tracker remains at its October 28 update; the acceptance matrix carries later progress.
