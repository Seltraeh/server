# Server completeness audit: synthesis and empty handlers

2026-09-25. Complementary to Claude's Trial Zone / Research Lab work. This pass
does not alter those encounters, their KDL or their unlock/reward implementation.

## Registry coverage

`python scripts/audit_handlers.py` currently finds **152 registrations mapping to
112 distinct handler bodies**, no duplicate GroupIds, no missing bodies, and no
unregistered HANDLEF definitions. This establishes registry/source consistency,
not feature completeness. The scanner is a lightweight source audit, not a C++
parser; review findings and use the compiler as the authority.

`--json` emits the complete registry with source paths/lines, literal empty reply
counts and await counts. It deliberately does not treat every `{}` reply as a
defect. Empty guards, client-local mutations, deferred features and actual stubs
need different treatment. The Windows workflow runs this structural check.

## Actual unfinished or intentionally limited handlers

| Handler | Current behavior | Next useful work |
|---|---|---|
| GuildUnimplemented | **39 distinct registered routes** (40 until GuildJoin was implemented on 2026-09-26, see [GUILD_INVITE_2026-09-26.md](GUILD_INVITE_2026-09-26.md)) share a log-and-empty-success body. These remain genuine feature stubs. | Identify a reachable route from client captures, map its request/response, implement its state transition and tests. Do not replace all 40 with fabricated data. |
| UserEnteredFeature | Replaced by persistent per-player visit tracking in the next pass. | See [FEATURE_VISITS.md](FEATURE_VISITS.md) for packet evidence, validation and remaining client observation. |
| UpdateEventInfo | Empty acknowledgement with no active-event refresh. | Establish which event caches the requesting scene needs and their reset/list semantics. |
| Chronology | Empty acknowledgement. | Determine whether this client needs timeline data or already supplies it locally before treating emptiness as an observed gameplay failure. |
| ArenaInfo | Deliberately deferred/feature-gated; no match lifecycle. | Implement opponent, entry, result, rank/reward and orb rules together. Arena daily tasks remain disabled until wins can be counted. |
| UserGemShardInfo | Deferred offline purchase/catalog behavior. | Needs an explicit offline economy design and verified client contract, not fake real-money success. |
| VideoAdSlotsClaimBonus | Ads disabled; no provider or claimable bonus. | Leave disabled unless an offline replacement is explicitly designed. |
| NoticeList / NoticeReadUpdate | Empty notice board/signal acknowledgements. | Low gameplay priority; no dumped notice content. |

The current SummonerJournal startup diagnostic also reports **28 of 45 objectives
without a progress source** and **18 stand-in rewards**. That is a separate high
value gameplay-completeness target; a rendered Journal is not a completed feature.

## Empty responses that are not blanket stubs

- ItemMix's generated response schema explicitly documents client-local crafting
  and an empty acknowledgement. Its persistence still needed repair (below).
- TownUpdate persists taps/sound state in a transaction before acknowledging.
- DeckEdit, ItemEdit, ItemSphereEqp and ItemSell contain actual mutations; their
  literal empty replies alone do not establish missing functionality.
- AreaInfo renders from local MST according to the existing client trace.
- NgwordCheck is the offline name/profanity-check acknowledgement.
- BannerClick records/logs a click and returns a signal; it is not a reward path.

Several registry comments still describe now-implemented achievements, Rewards
menu or Summoner handlers as probes. Inspect their bodies and helpers rather than
treating those older comments as a current work queue.

## Synthesis defect and repair

ItemMix previously read materials/Karma outside a transaction, then separately
debited materials, granted output, debited Karma and advanced counters. A fault
in the Karma update was reproduced against the earlier build: the request failed
but two crafted spheres and spent ingredients remained in the copied save.

The repair reserves a transaction before eligibility/stock reads and uses it for
every mutation and progress helper. A late statistics failure now propagates for
this caller, preventing a false success after transaction failure. Other callers
retain the shared helper's historical best-effort statistics behavior; they have
not been certified atomic by this pass.

Recipe/count parsing now rejects malformed batches rather than converting an
invalid count to one or ignoring trailing characters. Counts are parsed with
full consumption and overflow checks. Affordable-prefix behavior remains, but
calculating the affordable quantity replaces a loop proportional to an untrusted
requested count. Duplicate ingredient entries aggregate, and output overflow is
rejected before mutation. The 64 KiB CSV bound is an input-processing guard, not
a recovered gameplay batch limit.

The client-local empty success contract is preserved. Partial affordability,
unknown/locked recipe skipping, recipe date enforcement and dictionary refresh
semantics are not redefined by this repair. A broader synthesis-fidelity pass
should verify those rules against the actual scene.

## Reproduce and validate

Use an isolated server on **19962**, a copied account/save under `out/`, and the
current schema/data. Never run fixtures against the player's active database.

```powershell
python scripts/test_synthesis_wire.py out/synthesis-tests/gme.sqlite
python scripts/audit_handlers.py
```

The synthesis test injects SQLite failures at payment and final counters, checks
whole-operation rollback, rejects malformed batches, covers duplicate/large
counts, validates material/Karma limits and counters, repeats an exhausted-stock
request and submits concurrent crafts. `--baseline` instead asserts reproduction
of the earlier partial-write defect and should only be used with the old binary.

These tests do not establish real-client animation/warehouse behavior and do not
claim the server is free of stubs. Confirm ordinary and repeated synthesis in the
game after deploying the tested build.

Validation on 2026-09-25: the earlier binary reproduced the partial-write defect.
The repaired Debug library compiled and linked into the isolated test executable;
all five synthesis regression groups above passed against the copied save.
The handler audit passed with zero structural errors, and
`python scripts/test_research_lab_ai.py` reported zero failures. The first parallel
build encountered an MSVC PDB API error; a single-job rebuild completed. These are
local results, not a hosted CI run. The test server was stopped afterward; the
normal server executable was not replaced or restarted by this pass.
