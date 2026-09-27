# Exchange visibility, expiry labels and guild thumbnails

User confirmed an Honor Fang purchase through Merit Exchange on 2026-09-26.
Burst Queen, Mystery Frog and Tilith remain unconfirmed in-game because the
current save does not contain those units. Earlier API tests are separate evidence.

## Expiry correction

The offline offers are permanent. Three responses incorrectly used zero:

* EventTokenInfo.timeleft: Android getter 0x1C7E538 preserves -1; other values
  count down and clamp to zero. EventTokenTopScene::setLayout 0x1D1A2E4 skips a
  token when its time remaining is zero. This explains the empty Bazaar despite
  supported currencies and offers being emitted.
* Merit UserAchievementTradeInfo.VDKB0Y5h: reader 0x13FCD3C stores it at +0x34.
  Shop item createSprite 0x1A650A8 reads that offset: positive values render a
  countdown, zero renders RANDALL_ACHIEVEMENT_TIME_LIMIT, negatives skip the label.
  The generated member is historically named state; its actual meaning is time
  remaining. It is now explicitly -1 for permanent offers.
* GuildUserExchangeInfo uses the same field/offset. GuildExchangeItem at
  0x1DB1A84 has the equivalent branches. Stock rows now send -1.

No far-future dates or client clock changes are needed. Stock limits and purchase
ledgers retain their existing behavior; this does not introduce stock resets.

## Guild Hall thumbnail crash

The September 26 GuildCreate capture shows unit 20011 with artwork variant 2 in
its founder row. Guilds.cpp assigned unit_type_id (Anima/Breaker/etc.) to artwork
variant fields for the owner, simulated members and invitation candidates.
GameUtils::getUnitListThum 0x117CC40 and getUnitIllsImage 0x117D11C append _N to
asset names when that variant is at least 2. The stock and current assets contain
unit_ills_thum_20011.png but no unit_ills_thum_20011_2.png.

Windows dump BraveFrontier.Windows.exe.2908.dmp records a null read at cocos2d
+0x15716C, called from game +0x2BA1D7 immediately after thumbnail creation.
The caller's stack contains the 26-character thumbnail filename buffer (heap
text unavailable in the minidump), consistent with the derived alternate name.
This strongly links the crash to the bad artwork variant; visual retesting remains
necessary. No placeholder PNG was invented to conceal the incorrect packet.

All three guild card producers now use base artwork variant 1. Selected alternate
art is not persisted for those remote cards, so growth type must never stand in
for it. Another earlier dump, .17460 at game +0x855588, is a separate want_gift
roster path and is not claimed fixed by this change.

## Regression

Use a copied save under out/ containing a founded guild, invitation candidates
and unit 20011, with an isolated server on port 19960:

```powershell
python scripts/test_exchange_ui_wire.py out/exchange-ui-tests-2026-09-26/gme.sqlite
python scripts/test_fusion_merit.py out/exchange-ui-tests-2026-09-26/gme.sqlite
```

The first verifies all 241 Merit and 121 guild timers, Bazaar visibility at zero
balance, owner art for six growth types, stock thumbnail existence, and invitation
art. The second exercises payment, limits, rollback, fusion and rewards.

Validation completed: Debug build/link passed, all four targeted wire-test groups
passed, and all eight fusion/exchange regression groups passed against the copied
save. The normal Debug executable at
out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe was rebuilt.
The isolated test process was stopped afterward; no player inventory was changed.
Actual UI confirmation of the corrected badges, Bazaar tiles and Hall entry is
still pending. No new asset files are required for the diagnosed thumbnail path.

Visual Studio updated from 18.10.1 to 18.10.2 since the previous build. The old
PCH failed with C1853; removing only the generated PCH and its object, then
rebuilding, resolved it. This is separate from the earlier schema dependency fix.

Optional local Debug CLI test pack (only run when the player wants these units):

```text
addunit 750004
addunit 730302
addunit 730322
addunit 50253
addunit 10017
```

These are Burst Queen, random Mystery Frog, fixed Anima Mystery Frog, 4-star
Tilith, and an eligible BB/SBB base respectively. CLI additions obey capacity;
back up the save first. They were not run against the player's save by this pass.

## September 27 client feedback

The player confirmed that expiry timers and the reported Guild/Randall warnings
are gone. Burst Queen, Mystery Frog and Tilith fixes were also confirmed.
Bazaar tile visibility still needs an explicit observation.
