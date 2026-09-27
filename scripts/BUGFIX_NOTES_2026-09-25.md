# Fusion, exchanges, and Tilith Home display

## Changes

- Merit Exchange: 241 distinct supported offers selected from the newest original
  records per reward. Includes Legend Stone (4,000 Merit Points, limit 99).
- Guild Exchange: 121 supported offers with original prices and limits. Added the
  purchase handler, persistent per-player stock ledger, Guild Token wallet and
  present type 8002 payout. Historical mystery-chest offers without supported
  rewards are omitted.
- Event Bazaar: 51 curated offers across Rift Tokens (8), Brave Insignias (13),
  and Brave Tokens (61). Added listing/purchase handlers and zero-balance shop
  visibility. These are a documented subset, not a recovered full historical
  Bazaar dump. Every offer carries its source URL in event_exchange.json.
- Purchases validate quantity, currency, reward and stock. Payment, inventory
  grants and stock updates share a transaction; a failed cart rolls back.
- Burst materials apply their level budget across BB and SBB. A Burst Queen
  raises an eligible BB1/SBB0 unit to BB10/SBB10 in one fusion. Existing unit
  eligibility still governs SBB/UBB availability.
- Mystery Frogs use MST kinds 10010–10016. Fixed frogs select their type; random
  frogs exclude the current type. Fusion resets level/experience and type growth,
  preserves BB/SBB, imps, spheres, SP and Omni upgrades, and sends three Rainbow
  Crystals to Presents. Subsequent type growth follows UnitTypeMst ranges.
- Random frog odds are an offline policy: weight 1 for each ordinary type and
  0.25 for Rex, excluding the old type. Exact live probabilities were unavailable.

Shop dates run through 2037; purchase limits persist and currently do not reset
on a daily/monthly schedule. Wallet creation does not invent prior Guild Token
earnings or implement unfinished Guild reward sources.

## Tilith evidence and correction

Unit 50253's original Home rectangle `245,-5,135,369` was unchanged in the decoded
source, server MST, downloadable version 1085 and the local Windows client cache.
The cached artwork, after undoing the client's byte obfuscation, exactly matched
the served stock PNG (906 × 606). No server hardcoded override was found.

Android `HomeScene2::unitDraw` at 0x16F0108 obtains `getHomeImgPos`, parses four
floats, constructs a texture rectangle and scales it to the Home panel. Its crop
explains the screenshot: Tilith's face lies at/beyond the old rectangle's right
edge. The authored correction is `303,50,180,492`: shifted right/down and widened,
retaining the original aspect ratio. Only the reported 4-star unit is changed.
UnitMst download version 1086 is generated and its encryption round trip verified.
**Confirmed in the Windows client on September 27: Tilith is no longer off the panel.**

## Validation

Debug Windows server build succeeded. Encrypted GME tests against a separate
SQLite save on port 19960 passed:

- Five BB/SBB boundary cases and consumed-material replay rejection.
- Six fixed frogs, random exclusion for all six old types, preserved upgrades,
  level reset, result animation row and compensation presents.
- Invalid mixed, duplicate and self-fusions without material consumption.
- Legend Stone delivery/payment, stock limits and insufficient-funds rollback.
- All three Bazaar lists; unit/item delivery; incorrect currency and counts;
  atomic failure of a multi-entry cart; refreshed balances/remaining stock.
- Guild stock, delivery, payment rollback and refreshed member wallet.
- Guild Token present payout once only.
- Anima HP/REC growth bounds and unchanged ATK/DEF after a frog reset.

No live player save was used for these mutations. In-game exchange UI and Tilith
framing are not established by API tests.

## Regeneration

Run from the repository root; Python scripts need pycryptodome where noted:

```powershell
python scripts/gen_achievement_mst.py --source 'PATH_TO_DECODED_MST'
python scripts/gen_exchange_catalogs.py --guild-source 'PATH_TO_F_GUILD_POINT_EXCHANGE_MST_Ver233.json'
python scripts/refresh_unit_mst.py
```

The last command publishes the next UnitMst version from current source data and
retains earlier files. Include the generated F_UNIT_MST folder when distributing
the updated server assets; the content directory is ignored by Git.

Regression command (only with the isolated server/config and copied save):

```powershell
python scripts/test_fusion_merit.py out/bugfix-tests-2026-09-23/gme.sqlite
```

Mechanics: [Unit Types](https://bravefrontierglobal.fandom.com/wiki/Unit_Types).
Bazaar references: [Brave Insignia](https://bravefrontierglobal.fandom.com/wiki/Event_Bazaar/Brave_Insignia),
[Brave Bazaar](https://bravefrontierglobal.fandom.com/wiki/Event_Bazaar/Brave_Bazaar).

## September 26 client feedback

The player confirmed a Merit Exchange Honor Fang purchase. Burst Queen, Mystery
Frog and Tilith still await in-game testing because those units are absent from
the fresh save. Follow-up [exchange UI and guild thumbnail fixes](../docs/features/EXCHANGE_UI_2026-09-26.md)
correct zero expiry values and growth-type/artwork confusion; see that report for
binary evidence and isolated-save validation.

## September 27 client confirmation

Burst Queen now reaches BB/SBB 10 and unlocks UBB. Mystery Frog resets level
and changes type. Tilith framing is corrected; exchange expiry labels and the
reported Guild/Randall warnings are gone. See [release notes](../docs/PATCH_NOTES_2026-09-27.md)
for the source-build and publication scope.
