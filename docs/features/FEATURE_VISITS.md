# Persistent feature visits

2026-09-25. Completes the former UserEnteredFeature acknowledgement stub.

## Evidence

Android arm64 libgame.so was inspected locally. This is binary-confirmed packet
behavior, not a claim of Windows-client visual confirmation.

* UserEnteredFeatureRequest::createBody, 0x1C706F0: one row in `2386Diw1`, with
  `e63D1BV0` feature ID, `MHx05sXt` dungeon ID, `Diwl3b56` third flag. Values are
  decimal strings. setParam at 0x1C70848 stores the three arguments in that order.
* UserEnteredFeatureListResponse::readParam, 0x1C7041C: clears the list on the
  first field, then appends a triple at the end of each row. Replies must contain
  the whole snapshot rather than only the latest visit.
* enteredCheckByFeatureID, 0x1CBBE68, and enteredCheckByDungeonID, 0x1CBBF74:
  a matching ID counts as entered only when the third value is zero.
* getIfUserEnteredParadeFeature, 0x1CBBD20: checks both feature and dungeon IDs
  plus that same zero flag. Different dungeons must not overwrite one another.
* All 14 direct request setParam call sites send zero for the third value.
  RandallTownScene at 0x1A08104 sends (11,0,0); RandallSummonScene2 sends feature
  IDs 3,13,7; Home also sends 5,7,14,17; Vortex has dungeon-specific calls.
* setNewForRandall, 0x1C8892C, checks feature IDs 11,3,7,13. Visiting only one
  building does not imply the entire Randall badge should disappear.

Raw local disassembly is under out/feature-visits-*.txt and is not source content.
The source schemas retain the relevant addresses and semantics.

## Implementation

An additive migration creates user_entered_features keyed by player, feature and
dungeon. Visits are insert-if-absent, so retries are idempotent. The handler
validates one nonnegative ID pair with at least one positive ID and a zero flag,
resolves the existing authenticated player identity, then inserts and reads the
full snapshot in one transaction. A failed write returns an error.

UserInfo, UpdateInfoLight and UpdateInfo emit the same persisted snapshot. Rows
are scoped to the resolved player. No existing save needs manual editing; visits
predating this migration cannot be recovered and must be reported again by the
client. No feature-release, level gate, reward, or mission-clear state changes.

The third field is named new_flg to describe the observed zero/entered behavior;
nonzero request semantics are not recovered and are rejected. Feature IDs are not
arbitrarily restricted to the current release table: feature gating and function
release use different coverage, and the binary has dungeon-specific visits.
An empty array has no fields to invoke the list reader; cross-account client
singleton reset remains the client's login lifecycle, not something this endpoint
has established. Server snapshots never include another player's visits.

## Build dependency repair

Changing KDL regenerated packets/all.hpp but MSVC/Ninja reused the old PCH,
producing unknown-type errors for the new packet types. Explicit OBJECT_DEPENDS
on both generated headers now cover the server PCH, server translation units and
standalone frontend. The generated Ninja PCH rule was inspected to confirm these
are file dependencies, not just target-order dependencies.
See [CMake OBJECT_DEPENDS](https://cmake.org/cmake/help/latest/prop_sf/OBJECT_DEPENDS.html).

## Validation

Use only an isolated server on port 19962 and a copied save under out/:

```powershell
python scripts/test_feature_visits_wire.py out/feature-visits-tests/gme.sqlite
# Restart that isolated server with the same copied save, then:
python scripts/test_feature_visits_wire.py out/feature-visits-tests/gme.sqlite --check-persisted
```

Coverage: first and duplicate visits, complete snapshots, separate dungeon visits,
invalid inputs, mismatched player identity, injected database failure, concurrent
visits, login and both refresh paths, and process-restart persistence.

Client follow-up: enter Randall buildings and Vortex/Parade entries, return Home,
and relaunch. Verify NEW badges stop recurring for those visited entries while
unvisited entries remain distinguishable. This has not yet been observed in-game.

Local validation completed: regenerated schemas, rebuilt the Debug library/PCH
and frontend objects, and linked the isolated test executable. All four initial
wire-test groups passed, followed by the restart-persistence/migration check.
All five synthesis regression groups also passed against this build. The handler
audit reports zero structural errors. A repeated build reported "ninja: no work
to do." The isolated server was stopped afterward. The normal server executable
was not replaced, and these changes have not been committed or pushed.
