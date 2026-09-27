# Gameplay fidelity work queue

Updated 2026-09-27. This is an implementation queue, not a declaration that every
listed mechanic is absent. Audit before changing it. Use the handbook's evidence
labels and preserve player saves. Take one bounded slice through implementation
and verification before expanding it across the game.

Current release status: [September 27 patch notes](PATCH_NOTES_2026-09-27.md).
Fusion, Tilith and exchange fixes have player confirmation. Trials 001–003 and
The Creation God are implemented and model/server-tested; client playthroughs
are still needed. Read their feature reports before starting overlapping work.

## Suggested starting assignment for Claude

> Read docs/BF_OFFLINE_SERVER_HANDBOOK.md, docs/DEVELOPMENT.md and this queue.
> Inspect the actual checkout and unfinished changes. Improve gameplay fidelity
> using the Brave Frontier Global wiki and local client/MST evidence. Begin with
> boss AI and one documented Trial Zone encounter after confirming its IDs and
> current behavior. Research the complete mechanic, implement it in the correct
> schema/data/helper/handler layer, and test against an isolated save. Do not stop
> at listing discrepancies or filling packet fields. Work in sequential, reviewable
> slices; continue independently useful work when a client observation is pending.
> Expand the audit to progression, fusion and other wiki feature families instead
> of treating this queue as exhaustive. Record sources, authored compromises,
> tests, client confirmation and the next slice. Do not label placeholders or
> unobserved client behavior complete. Preserve unrelated changes; follow the
> current user's publication/commit instructions.

## 1. Boss AI and representative trials — highest priority

**Original September 25 baseline:** ai.json contained one random-attack record
(now 11 records after the Research Lab work). MissionArchiver
already translates AI records, and archive/ai.kdl describes condition/action
strings with some unnamed trailing arguments. The existing schema is a starting
point, not proof that skill targeting and phase flags are understood.

**First slice:** choose a documented Trial Zone encounter, verify its mission,
monster, skill and reward IDs, and implement one phase mechanic end to end. Then
finish that encounter before generalizing. Trial No. 003 is a candidate, not an
instruction to invent Maxwell IDs or copy another variant's fight.

**Research record:** stage order and stats; action budget; turn/HP triggers;
precedence and once-only flags; targets; preemptives; counters; buffs/debuffs,
ailment resistance and removal; dialogue; death/revival; retreat; first/repeat
clear rewards and unlocks. Include exact links/revisions and unresolved values.

**Acceptance:** test just above/at/below each threshold, conflicting turn/HP
triggers, several thresholds crossed together, repeated eligibility, phase death
and next-wave state. Confirm named skills actually execute in-game. Confirm first
clear and replay pay the correct rewards once and that failure grants nothing.

**Expand next:** Trial Zone -> Strategy Zone -> special Vortex encounters -> raid
bosses, once each family’s entry/result semantics are established. Maintain an
encounter matrix with faithful / partial / generic-fallback / unavailable states.

**Status 2026-09-26** (detail and sources: [features/TRIAL_003.md](features/TRIAL_003.md),
[features/RESEARCH_LAB.md](features/RESEARCH_LAB.md),
[features/CREATION_GOD.md](features/CREATION_GOD.md)):

* The AI grammar is decoded from the client and documented in
  `packet-generator/assets/archive/ai.kdl`: condition terms, target pools,
  action types, the three formerly unknown action arguments, flags, action
  budgets and evaluation order. `scripts/ai_model.py` models the selector for
  tests. Mission scripts that swap monsters mid-battle are expressed by the new
  `script_monsters` archive field. The ailment-resistance slot order is now
  binary-confirmed.
* Trials No. 001, 002 and 003 are built from the client's own mission scripts
  and original skills plus the wiki's phase tables (`scripts/gen_research_lab.py`);
  server- and model-tested and deployed, **not yet played**. HP is the wiki's
  except Juggernaut/Abaddon; ATK/DEF are authored.
* The Research Lab unlocks in order per player and pays only on the first
  clear (dungeon types 2 and 8), per the wiki. Free replays remain a gap.
* The Creation God (2100004) is rebuilt from its own mission script: two
  battles, four forms with the script's monster ids, the re-issue skill band,
  both Maxwell turn tables, and Inception God Maxwell's Overdrive/Existence
  and UBB answers (the client's `ubb_use` last-action term, now decoded with
  its `before_turn_*` siblings in `ai.kdl`). Server- and model-tested, **not
  yet played**. Missing: the 9-BB "Eyes of Ends" rule and heal-on-kill.
* Every other archived monster still uses AI 1, "attack random", and no other
  mission's boss can cast a skill.

| Family | Encounters | State |
|---|---|---|
| Trial Zone (2000000) | Trials No. 001, 002, 003 | Partial: original structure/skills, authored ATK/DEF; awaiting play |
| Trial Zone | 17 others | Unavailable (no archive record, no tile) |
| Strategy Zone (2100000) | The Creation God (2100004) | Partial: original structure, skills and phase AI; BB-count rule and heal-on-kill missing; awaiting play |
| Strategy Zone | 9 others | Unavailable |
| Trial of the Gods (800051) | 6 trials | Generic fallback: attack-random, no skills |
| Story, Vortex, event bosses | all | Generic fallback |

Next: Trial No. 004 (decrypt its `F_MISSION_SCRIPT_MST` row first), or decode
the `flg_cntup`/`flg_tmp_cntup` counters to see whether one can count Brave
Bursts for Maxwell's "Eyes of Ends and Beginning".

Sources: [Research Lab index](https://bravefrontierglobal.fandom.com/wiki/Summoners%27_Research_Lab),
[Trial No. 003](https://bravefrontierglobal.fandom.com/wiki/Trial_No._003),
[Trial No. 005](https://bravefrontierglobal.fandom.com/wiki/Trial_No._005).
These are entry points; read each selected encounter's complete behavior table.

## 2. Fusion and unit lifecycle

**Implemented, server-tested:** cross-BB/SBB material budgets and Mystery Frogs;
see scripts/BUGFIX_NOTES_2026-09-25.md and scripts/test_fusion_merit.py.
Do not replace those changes based on older “unimplemented” notes.

**Audit next:** ordinary-material BB chance, same-unit/element exceptions,
Great/Super probabilities, success XP rounding, material limits and costs;
imps/caps; sphere slots and equipped-item returns; type growth through resets,
evolution and Omni boosts; SP assignment/reset; ordinary and Omni recipes;
locked/favorite/material restrictions; no-BB/no-SBB units and maximum rarity.

**Acceptance:** test every special material class, already-maxed and unlocking
cases, partial/invalid batches, resource shortage, one-time consumption and save
reload. Check both resulting stats and animation/result data. Record official
values separately from current authored rates. Do not infer UBB availability
solely from the presence of its skill ID.

Sources: [Unit Skills](https://bravefrontierglobal.fandom.com/wiki/Unit_Skills),
[Unit Types](https://bravefrontierglobal.fandom.com/wiki/Unit_Types),
[Fusion Units index](https://bravefrontierglobal.fandom.com/wiki/Category%3AFusion_Units).

## 3. Battle skills and bonded mechanics

**Audit:** BB/SBB/UBB input and unlock prerequisites; OD consumption; DBB partner
eligibility, unlock and bond-level progression; bonded SBB/DSBB terminology in
this client; burst resource costs/recharge; synergy effects; auto/manual parity;
leader/extra/passive skills; buff stacking, durations, ailments, mitigation and
damage limits. Much combat may already run inside the client: establish ownership
before implementing server-side equivalents.

**Acceptance:** eligible/ineligible pairs, each relevant unlock boundary,
insufficient resources, consumed resources, changed partner, repeat use, and
manual versus auto. Verify the wiki’s term maps to the actual client feature;
do not create a separate mechanic merely because community names differ.

Source: [Bonding](https://bravefrontierglobal.fandom.com/wiki/Bonding), then the
selected unit pair and synergy pages. Current radial-menu operation is only an
input observation, not complete bonded-mechanics coverage.

## 4. Economy, availability and progression

**Implemented, partial:** Merit/Guild/Bazaar listing and purchasing. The catalogs
are intentionally finite; offline availability and non-resetting stock are
documented choices. Guild Token payout via Presents is not a complete earning loop.

**Audit:** every necessary evolution/awakening material has an obtainable source;
quest/trial unlock chains; first-clear and repeat rewards; event currencies and
earning sources; daily/monthly limits; guild earnings; shop costs; crafting and
sphere recipes; inventory limits/overflow; presents and claim expiry; energy,
orbs and recovery clocks; quests/achievements counting the actual successful action.

**Acceptance:** trace one scarce material from its legitimate source to spending
and the unlocked unit/feature. Test insufficient balance, limits, retries,
inventory persistence and calendar boundaries. Offline calendar/reset policy
must be explicit; never silently enable all paid or event-only rewards.

Sources: [Brave Insignia Bazaar](https://bravefrontierglobal.fandom.com/wiki/Event_Bazaar/Brave_Insignia),
[Brave Bazaar](https://bravefrontierglobal.fandom.com/wiki/Event_Bazaar/Brave_Bazaar),
and each material’s acquisition page. Record missing sources as blockers for that
item, not a reason to invent costs.

## 5. Other feature families to seek out

These are audit candidates, not confirmed defects:

| Family | Questions and smallest useful slice |
|---|---|
| Quest/Vortex/Grand Quest | Entry gates, waves, stories, branching objectives, map state, drops and completion; implement one complete route. |
| Arena/Colosseum | Opponent generation, squad restrictions, scoring, rank rewards, orbs and failure; verify one match lifecycle. |
| Raid/Guild | Room/party model suitable for offline play, raid maps, boss phases, crafting drops and token earnings; label simulated multiplayer policy. |
| Frontier Hunter/Gate/Rift/Spire | Scoring, stage restrictions, checkpoints, defeat/retry and reward tiers; separate each mode. |
| Summoner mode | Weapon/element leveling, avatar skills, SP, journal and mission eligibility; verify one progression chain. |
| Town/crafting | Facility upgrades, harvest timers, recipe unlocks, crafting quantities and rare ingredients; distinguish missing textures from missing rules. |
| Summoning/collections | Pool/rate data, guarantees, selectors, duplicate units, dictionary completion and capacity; document authored availability. |
| Daily/event systems | Login chains, daily tasks, timezones, reset boundaries, reward expiry and replay; use a controllable clock in tests. |
| Friends/support | Helper eligibility, leader-skill application, honor rewards and synthetic-friend scaling; distinguish simulation from original online behavior. |

Use the wiki navigation/category indexes to expand this table. Compare code and
data against the selected page before adding a feature to the confirmed-gap list.

## Audit and reporting templates

**Complementary server audit (2026-09-25):** see
[features/SERVER_STUB_AUDIT.md](features/SERVER_STUB_AUDIT.md) for the synthesis
transaction repair and an evidence-based inventory of empty handlers. All 152
registered routes resolve to handler bodies, but 40 guild routes share a stub;
the Summoner Journal reports 28 objectives without progress sources. Prioritize
one reachable state transition from that inventory, rather than treating empty
acknowledgements as proof that a feature is unimplemented. Run
`python scripts/audit_handlers.py` after changing handler registrations.
The follow-up [feature-visit implementation](features/FEATURE_VISITS.md) replaces
the NEW-badge acknowledgement stub with persistent visits and complete snapshots.
Its in-game badge behavior still needs client confirmation.

Keep a small feature ledger, for example:

| Feature / IDs | Expected rule + source | Current behavior | Owning files | Evidence | Next test |
|---|---|---|---|---|---|
| Selected encounter | URL + revision/date | Partial/random attack | archive + MissionArchiver + KDL | Server/binary/client labels | Threshold/skill test |

For each completed slice report: concrete before/after; affected files and data;
source references and conflicts; test results; client observations; authored
choices; outstanding gaps; next bounded slice. “Build passed” and “handler exists”
are not gameplay acceptance criteria.

Use future dated feature notes rather than growing the handbook into another
chronological transcript. Keep one authoritative current status per mechanic.
