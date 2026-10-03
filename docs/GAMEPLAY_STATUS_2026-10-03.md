# Gameplay status before mainline publication — October 3, 2026

Source: the complete CLAUDE_SESSION_6_HANDOFF.md, including evening/night and
October 3 updates; current source and QA records. Earlier handoff sections are
historical and are superseded by later observations. This status is not a claim
that every change in the full branch has received an in-game playthrough.

## Player-confirmed fixes

- Farm/Mountain harvest; Farm intro plays once; ordinary mission repeat returns
  to its stage list; Sphere House opens on the legitimate mission-21 clear.
- One-/two-slot sphere equipment, unequip and direct transfer persist.
- Vargas SP purchase persistence; one-gem reset restores available SP and clears
  skills. Two genuine Continues in mission 20 cost one gem each.
- Berdette 5→6→7-star evolution, BB/SBB progression and halving matched the
  expected data. The test supplies were delivered; do not issue them again.
- 400 Ignis Shards cost 20,000 Merit points and were granted.
- Training Grounds entry, passive dummies, settings/HP/element changes and
  reticle placement (final offset 0,17) work.
- Friends remain available after fusion; three Deemo forms fought in mission 10;
  two used potions consumed two.
- Town art/labels align after the framed content was actually downloaded.
  The reported summon white flash did not recur once the re-download finished;
  its underlying cause was not independently isolated.

## Still implementation work or known limitations

1. Permanent client asset cache and incomplete asynchronous downloads: server
   file changes are ignored; scene entry can occur before all related artwork
   arrives. See OFFLINE_CONTENT_LOADING_PLAN.md for the next project.
2. Archive-granted units may store leader_skill_id=0. Ordinary battles/screens
   use the client MST, but Training Grounds, arena/campaign/Frontier Gate paths
   may read the stored value. Trace and repair the owning path; verify old-save
   backfill and new grants without overwriting legitimate overrides.
3. Newly crafted spheres share a provisional ID until a refresh. Independent
   favorites and retries of sales can be ambiguous. A faithful client identity/
   refresh solution is needed; do not guess which copy the player meant.
4. Town harvesting refreshes on login or settled MissionEnd. A long town-only
   session still lacks a safe entry/timer refresh coordinated with queued taps.
5. Reset has no request nonce: a delayed duplicate arriving after repurchase is
   indistinguishable from a new reset. Continue fingerprints have a documented
   identical-status/blob ambiguity. Server tests do not remove protocol limits.
6. Existing Trials/boss/Fire Mecha content gaps, unverified special-boss cursor
   offsets, Tilith/other unit gaps, and GuildUpdate remain separate backlog.
   Mission validator's 3,117 recorded findings are not a clean result.

## Remaining client acceptance

- Zeal 60956 helper portrait/sprite now that all its files are present; reproduce
  with cold/partial content when testing the offline patch.
- Continue, close/relaunch mid-run, resume and finish; also verify no-gem refusal
  returns to a usable prompt without secretly reviving. Fault/retry injection
  belongs on copied test saves.
- Reset → repurchase → reconnect, SP battle effects, and relevant UBB visibility.
- First clear of a final stage (e.g. mission 23) and next-area unlock presentation;
  mission 234/Cordelica crash case and later compound story gates.
- Craft multiple identical spheres now that Sphere House is open; inspect
  favorites/equip/sales and remaining stock immediately and after reconnect.
- Training Save/Load Conditions, all special boss reticles, level-999 result and
  energy boundaries, leader/helper EXP, Fuu drops, slots/parades, long text and
  remaining summon/portrait cases from the full client checklist.
- Performance: cold/warm login, scene changes, crafting/inventory, battles and
  summons, with consistent fixtures. Current server tests do not certify FPS.

## Publication boundaries

Publish source, recovered/authored JSON, KDL, reproducible tests and documents.
Exclude out/, saves, logs, backups, extracted clients and bulk game_content.
The Town SAM change and other locally patched assets are part of the separately
distributed content set, not tracked server source. The Town generator currently
lives in ignored tools/frame_town_for_2x3.py; moving its parameterized generation
and tests into shared release tooling is a reproducibility follow-up. A source
push alone does not deliver those local asset bytes to another installation.

No automatic deletion of historical recovery files is part of publication.

## Publication validation — October 3

Reran all 41 QA entries against the normal deployed Debug executable on copied
saves: 1,615 checks passed and two failed, both in the settlement fixture. The
fixture inherited the player's zero-gem wallet but expected a paid Continue;
the server log correctly recorded a no-funds refusal. The fixture now seeds
10 gems only in its copy, clears prior mission-10 Continue history and explicitly
checks acceptance. Its targeted rerun passed 23/23 (previously 20/22), giving
1,618 passing checks when using the latest result for each suite. No unresolved
unexpected failure remains; the validator matches its 3,117-finding baseline.

The initial failure is preserved in out/qa/current/SUMMARY.md and
settlement_zero_gem_fixture_failure.log; the corrected run is in
settlement_publish_recheck.log. Do not describe the original full invocation as
having exited successfully. Source/schema staged whitespace checks passed.
Only the fixture, ignore rules and publication/planning documentation changed
in this review, plus removal of a trailing blank line in FeSkillGet.cpp.
