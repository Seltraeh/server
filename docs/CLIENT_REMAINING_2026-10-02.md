# Remaining client acceptance — October 2, 2026

## October 3 — results after the re-download

| # | Result | What the logs show | Status |
|---|---|---|---|
| G1 | Town fixed | The client downloaded the 2:3-framed `MapVillage.sam` at 22:22:36 after a relaunch. | PASS (player-confirmed) |
| G3 | Summon gate fixed | The white flash at 21:56 happened during the post-reset re-download and is not seen any more. | PASS (player-confirmed) |
| G2 | Zeal drawn as a white blob (22:24 Training Grounds) | The battle started at 22:24:44; Zeal's frame layout was cached at 22:18:35 but the background re-download only fetched the texture `unit_anime_60956.png` at 22:25:59 and the portrait at 22:27:47, so the game drew the layout with no texture. All of Zeal's files are now cached and decode to byte-identical copies of the server's. | Expected to be fixed — retest once |

**Why the client needs a patch:** this whole round (the Town file, Zeal, the
summon flash) came from the client's asset cache, which never re-checks a file
once downloaded. The write-up for the offline-proxy patch is
[CLIENT_CACHE_PATCH_BRIEF.md](CLIENT_CACHE_PATCH_BRIEF.md).

| # | Test | Steps | Expected |
|---|---|---|---|
| H1 | Zeal | Any battle with Mauve Tenebrosity Zeal as the helper. | Sprite and portrait drawn normally. |

## Night follow-up 22:15 — your F1–F4 results

| # | Result | What the logs show | Status |
|---|---|---|---|
| F1 | Fused into a goblin, then saw friends when loading a quest | 21:42 fusion, 21:44 Training Grounds start with a helper picked from a full list. | PASS (player-confirmed) |
| F2 | Reticle on the dummy; but the helper (Mauve Tenebrosity Zeal) had no sprite or portrait; another helper was fine | Reticle: PASS (player-confirmed). Sprite: the server has all 8 of Zeal's files (unit 60956); for the 21:44 battle the client requested none of them — only the picker thumbnail at 21:43. The 21:50 battle fetched helper 20837's art at battle start and drew it. Both happened during the client's post-reset re-download (below). | Open — retest after the download finishes |
| F3 | Town labels unchanged | **My fix was on the wrong path.** After your app reset the client fetched `sam/MapVillage/MapVillage.sam` (21:36, 21:40); I had served the framed copy only at `_dlcbundle/MapVillage.sam`, which it never asks for. Measured in your screenshot: art top 86.7 design px low (the model says 87.5), so the cause stands. Both paths now serve the framed copy (22:11) and your cached town file was deleted. The Windows client keeps a 2:3 picture (black bars on a taller window), so the window shape does not matter — ignore the 9:16 remark in the evening section. | Changed again — needs a game restart |
| F4 | Double summon gate white bleed is back after the reset | Noted for QC (below). | Open |

**Your client is still re-downloading everything** after the app reset: ~1,000
files a minute since 21:36 (34,000+ so far, still going at 22:14). Missing art
while it runs is expected. Please let it finish, then **close and reopen the
game** before testing again.

QC note — F4 white bleed after the reset: summon at 21:56:03 (GachaAction).
The gate set the client fetched on demand at 21:56:05 was GachaGateChange5_6
(a gold → rainbow colour change), Open3/4/6, Open6Add, OpenAdd and Close3–5.
All the gate scripts in your cache are byte-identical to the patched server
copies (re-downloaded 21:46), so the reset did not bring back the stock
scripts. The rainbow-path animations (Change5_6, Open6, Open6Add, Close5) are
not in the client's own bundle, so after a reset they come only from the
server's copies; whatever your old cache held before is gone. Next: a still of
the white frame and a retest after the download finishes.

| # | Test | Steps | Expected |
|---|---|---|---|
| G1 | Town | Open Town (your 22:22 relaunch already downloaded the corrected town file). If it looks unchanged, close and reopen the game once more. | Labels and tap areas sit on their buildings and tiles; the river glints are on the water. |
| G2 | Helper sprite | Training Grounds or any quest with Mauve Tenebrosity Zeal as the helper. | Zeal's sprite and portrait show. If not, note the time. |
| G3 | Double gate | A summon that changes gate colour. | No white screen. If it happens, a screenshot of the white frame helps. |

## Evening follow-up 21:02 — your report on the 15:17 build, and three fixes

Server redeployed 21:02 (details: [Session 6 handoff §11](CLAUDE_SESSION_6_HANDOFF.md)).
**Restart the game before opening the Town** (the Town map file was refreshed).

Your report, checked against the save and the server log:

| # | You reported | What the server recorded | Status |
|---|---|---|---|
| 1 | Sphere crafting unlocked; two Continues | Mission 21 "Wielder of the Fire" first clear 16:58 opened the Sphere House. Mission 20, 19:01:48 and 19:02:07: two revivals, 1 Gem each (Gems 4 → 3 → 2 after the reset's 5 → 4). | PASS (player-confirmed). Resume-after-relaunch not yet tried. |
| 2 | SP reset worked | 17:09, Vargas: 100 SP back, 0 used, no skills, 1 Gem. | PASS (player-confirmed). Repurchase not reported. |
| 3 | Evolution worked | On your own Berdette (not the bundle copy, which is still lv 1): 5★ → 6★ BB 5 → Burst Queen BB 10 / SBB 10 → lv 100 → 7★ BB 5 / SBB 5 → Burst Queen BB 10 / SBB 10, LS Flame Spider's Silk. | PASS (player-confirmed); matches every expected value. |
| 4 | Merit 400 shards worked | 18:30, 400 Ignis Shards for 20,000 points; 18,340 left. | PASS (player-confirmed). |
| 5 | "No friends could be found" now and then | Every fusion and evolution reply emptied the client's helper list; it refilled only after a battle or a relaunch. Your 18:31 quest (mission 22 "The Thief's Hideout") came right after six fusions/evolutions. Mission 23 has not been started yet. | FIXED, server-tested |
| 6 | Reticle below the dummy | "0,70" was an over-correction (screen pixels added to battle units one for one). Both screenshots agree on "0,17". | FIXED (data), server-tested |
| 7 | Town labels/tiles above their buildings | The game's own Town map is laid out for tall phone screens; on your ≈2:3 window the art hangs 44 points (~88 px) too low. The client ships this exact file. The server now sends a copy framed for a 2:3 window, and your cached copy was cleared so the game fetches it. | Changed, not yet seen in the client |

About "a feature in the sky": the castle at the top right of the Town is
Imperial Capital Randall, and it is already there. The padlock-with-sparkle on
it means "something new unlocked inside that you have not opened yet", not a
lock. The Town has no other sky feature in this client; the LS Spheres building
(after mission 20102, the Bectas area) and the Music House (after mission 33)
are ordinary buildings.

| # | Test | Steps | Expected |
|---|---|---|---|
| F1 | Helpers after fusion | Fuse or evolve anything, then go straight to any quest. | The helper picker lists friends and strangers; no "Reinforcements could not be found". |
| F2 | Reticle | Training Grounds: tap every enabled dummy. | The reticle sits on each dummy's body. |
| F3 | Town | Restart the game, open Town. | Building labels and tap areas sit on their buildings, the resource tiles on the Mountain/River/Farm/Forest, the river glints on the water. If you make the window tall (9:16), the labels will sit low instead — tell me if you prefer that shape and I'll switch back. |
| F4 | Still open from Session 6 | Continue, then close mid-battle, relaunch and accept the resume offer; re-buy one SP skill on Vargas; the first clear of mission 23 "The Blazing Beast" (Egor Snowfield should be presented once); whether your 7★ Berdette (now SBB 10) shows its UBB, Promethea, in battle. | As in the Session 6 table below. |

## Session 6 update ~15:17 — what changed, what to test next

Server deployed 15:17 (details: [Session 6 handoff](CLAUDE_SESSION_6_HANDOFF.md)).
Reconnect. Nothing here is client-confirmed yet; record counts and what you see
immediately and again after a relaunch. Retry/fault cases were tested on copied
saves — please do not recreate them on your account.

What changed for you:
- **Continue (revival)** now pays exactly once per revival, however many times a
  request is retried. If the server cannot accept a revival (for example the
  balance is short), you get a message and the Continue prompt again instead of
  a free revival.
- **SP Reset** on the Enhancements screen works: 1 Gem, all spent SP back,
  skills cleared. With nothing spent the button stays disabled (the client's
  own rule).
- **Evolution test bundle delivered** to your Present Box: 16 units + 2,000,000
  Zel, tagged "Client QA evolution bundle 2026-10-02". Claim all of it (16 of
  your 17 free unit slots).
- Your Gems: 5. Continue and Reset cost 1 each.

| # | Test | Steps | Expected |
|---|---|---|---|
| 1 | Continue + resume | Take a weak squad (e.g. one low-level unit) into mission 20 (already cleared, so 21's first clear stays continue-free) so it wipes. Note Gems. Tap Continue. Lose again and Continue once more. Then close the client mid-battle, relaunch, accept the resume offer, finish or give up. | Each Continue costs exactly 1 Gem (5 → 4 → 3); the battle revives each time; the HUD Gem count matches. After the relaunch the resume offer appears and resumes the battle; no extra Gem is taken at any point. |
| 2 | SP Reset + repurchase | Your call which unit: Vargas (uu 1034) is the only one with SP spent (Current 0, Used 100); a reset clears his bought skills, refunds all 100 SP, and re-buying costs SP only. Note Current/Used/Total SP, skills and Gems. Enhancements → Reset → Yes. Close and reopen Enhancements; relaunch. Buy one skill again. | Gems −1; Current SP = old Current + old Used; Used 0; Total unchanged; no skills marked. Same after reopening and after relaunch. The repurchase spends SP normally. Reset button disabled while Used is 0. |
| 3 | Evolution comparison | Claim the bundle. On the NEW Inferno Berdette (lv 1, not your own lv 5 one): fuse a Rainbow Crystal (→ lv 80), then a Burst Queen (→ BB 10). Note BB/LS. Evolve (500,000 Zel). Note BB/SBB/LS. Fuse the 2nd Rainbow Crystal (→ lv 100) and 2nd Burst Queen. Note BB/SBB. Evolve (1,500,000 Zel). Note BB/SBB/UBB/LS. Relaunch and check again. | 5★: BB Ruse Filo 10, no SBB, LS High Ruler's Chakra. 6★ Hades Flame Berdette: lv 1, BB Ruse Capture **5**, SBB Era Cremation **locked**, LS Flame Spider's Threads. After the queen: BB 10, SBB **10**. 7★ Tartarus Blaze Berdette: lv 1, BB Ruse Eclipse **5**, SBB Terrible Cremation **5**, LS Flame Spider's Silk; UBB Promethea — note whether its button shows at SBB 5 (unknown). Zel: exactly the two prices. The evolve screen may pick your own totems instead of the bundle's — totals still end where they started. |
| 3b | (optional, your call) Selena → Ice Selena | With the bundled Water Nymph, evolve your Selena (12/12, BB 10, Muramasa). | Ice Selena lv 1, BB Divine Hail 5, LS Water Spirit's Power unchanged, 2,500 Zel, Muramasa stays on her. |
| 4 | Merit Exchange | Offer 91000059, 400 Ignis Shards. | 20,000 points; 400 delivered; remaining stock drops by one; repeat until stock is used. |
| 5 | Final-stage next area | After 21 and 22, the first clear of mission 23 "The Blazing Beast" (last stage of Cave of Flames). Then repeat 23 once. | The next dungeon, Egor Snowfield (mission 30), is revealed/presented once and the result returns to the map; the repeat returns to the stage list with no presentation. |
| 6 | Training Grounds | Mission 6000000: tap every enabled dummy; buff the squad, Menu → Save Conditions, later Load Conditions. | Reticle on each dummy's body (calibrated "0,70"); saved buffs restored on every Load tap. |
| 7 | Sphere House | Clear mission 21 "Wielder of the Fire" (next stage after 20; its first clear plays a story scene). Open Town. Craft a sphere, equip, favorite, sell an unequipped copy, reconnect. | The scene plays once; the Sphere House is available after the clear; counts correct immediately and after reconnect; no duplicate or vanished copies. |

## Player results ~13:40 — client-confirmed

PASS: the training dummies no longer attack (T1); every Training Grounds Menu
setting works (T2); changing Enemy 1's HP and element reloads the battle and
applies (T3). Found: the target reticle drawn ~110 px above the dummy. Fixed
14:00 (data only: the dummies' cursor offset "0,-40" → "0,70", calibrated from
the player's screenshot; battle coordinates are y-down). Pending: T5 below.

| # | Test | Steps | Expected |
|---|---|---|---|
| T5 | Reticle | Mission 6000000: tap each dummy you have enabled. | The reticle sits on the dummy's body (not above it), for every element. |
| T6 | Special bosses | When next met (e.g. the Juggernaut, mission 85): tap it. | Reticle on the body. Every archived special boss carries the same untested "0,-40" offset. |

## Player results ~12:45 — client-confirmed

PASS (player-confirmed on the deployed 11:54 build): Farm and Mountain
harvestable in town; no Farm scene after a mission-10 clear; Muramasa on
Selena survives Home, mission 10 and a relaunch; unequip returns it to the list;
Vargas slot-2 change keeps Ragna Blade in slot 1; a direct move of Ragna Blade
from Vargas to Selena; a repeat of mission 11 returns to the stage list;
Training Grounds (6000000) entered without a crash.

Found: the six training dummies ATTACKED, frozen for the attack and then
snapping back from the player's line. Fixed at 13:07 (server restart only, no
rebuild): they now never act, per the Global wiki ("enemies will not attack
back") and the client content (idle art only). See the handoff §9.

"Load Conditions" (top-left in the training battle) restores a squad state you
SAVED earlier; with nothing saved it plays the OK sound and does nothing — that
is the client's own design. The settings live under the battle's **Menu**:
Enemy Settings, Battle Settings, Save Conditions, Reset LOG, Items, Options.

| # | Test | Steps | Expected |
|---|---|---|---|
| T1 | Dummies | Mission 6000000, attack for 3+ turns. | The dummies never move or attack; the enemy turn passes straight back to you. |
| T2 | Settings menu | In that battle tap **Menu** (bottom right). | A menu with Enemy Settings, Battle Settings, Save Conditions, Reset LOG, Items, Options. Report anything missing or not opening. |
| T3 | Enemy settings | Menu → Enemy Settings: change element/HP/DEF of Enemy 1, apply. | Enemy 1 changes accordingly (element icon, HP bar). |
| T4 | Save / Load Conditions | Buff your squad, Menu → Save Conditions → Yes; later tap **Load Conditions**. | The saved buffs come back on every tap. |

## Afternoon update — supersedes the follow-up below where they overlap

Player report ~11:15: the Farm scene now plays once (PASS, player-confirmed),
but the Farm and the Mountain stayed unusable in town, and equipped spheres
disappeared from units. Both are fixed and server-tested; details and evidence
in [the Session 5 handoff](CLAUDE_SESSION_5_HANDOFF.md). Server deployed — just
reconnect.

Ordered client checks (record before/after, immediately and after relaunch):

| # | Test | Steps | Expected |
|---|---|---|---|
| 1 | Farm + Mountain | Reconnect, open Town. | Farm (tile 3) and Mountain (tile 1) sparkle; each tap gives an item and/or Zel/Karma; leave and re-enter town: remaining sparkle/taps unchanged, nothing refilled. |
| 2 | One-slot equip | Selena (unit 1000): equip Muramasa 31000. Go Home, open her again; play mission 10 once; relaunch. | Muramasa stays on Selena every time. In the sphere list it shows with Selena's icon and still counts in "equipped / total". |
| 3 | Unequip | Take Muramasa off Selena, go Home, reopen the sphere list. | Muramasa is back in the list immediately (not missing until relaunch); Selena shows no sphere after relaunch. |
| 4 | Two-slot edit | Ignis Halcyon Vargas (unit 1034, Ragna Blade 37500 + Evil Shard 36300): change ONLY slot 2 to another sphere; Home; relaunch. | Slot 1 still Ragna Blade, slot 2 the new sphere, Evil Shard back in storage. Both worn spheres listed with Vargas' icon. |
| 5 | Move | Move a worn sphere from one unit straight onto another; Home; relaunch. | It is on the new unit only; the old slot empty; no duplicate or missing copy. |
| 6 | Ordinary clear | Repeat mission 11. | Result returns to the area's stage list, not Home; no Farm/Mountain scene. |
| 7 | Training Grounds | Mission 6000000: Begin, all six elements, fight, exit, re-enter. | No crash; no reward, capture or item loss. |
| 8 | Merit Exchange | Offer 91000059, 400 Ignis Shards. | Costs 20,000 points; 400 delivered; repeat until stock is used. |

Not deliverable yet: the evolution supply bundle's manifest is stale (no Goblin
10050 left in the save); it was not granted. Sphere crafting needs mission 21
(Sphere House), still uncleared on this save.

## Latest follow-up — morning (superseded where the update above overlaps)

- Two potions used consumed two: user-confirmed pass for this case.
- Farm remains unavailable and its cutscene repeats after every mission-10
  completion: FAIL on the updated installation.
- Mission completion returns Home instead of the expected mission-area/next-area
  flow: reported regression, investigation required.
- Training Grounds crashes after Begin: FAIL; server wire coverage was insufficient.
- Merit Exchange, 400 Ignis Shards: FAIL, invalid trade request m9LiF6P2.
  Captured offer 91000059 / quantity 400 was rejected as invalid quantity or price.
- Sphere crafting remains blocked by progression or unavailable functionality;
  distinguish these after repairing Farm/story progression.
- Evolution testing remains blocked by missing comparison units and materials.

Next implementation directive: [Claude Session 5](CLAUDE_SESSION_5.md).
The build workflow must use the shared normal build tree and bounded reusable
test artifacts. No old out/ evidence or backups were deleted for this handoff.

## Confirmed by the user

- Vargas SP purchases and persistence after client relaunch: PASS. October 2
  screenshot shows acquired Atk/Rec and Spark nodes, Current SP 0, Used SP 100,
  Total SP 100/100. This confirms acquired-node display and persisted allocation;
  it does not independently verify each battle effect or every purchase cost.
- Three supplied Deemo forms: inventory, squad selection and mission-10 battle
  passed in the earlier report. Evolution and individual effects remain open.

## Next tests, in priority order

| Test | Steps | Expected result |
|---|---|---|
| SP battle effects | Take Vargas into a known quest; inspect stats and exercise purchased Spark/critical/other applicable effects. Use a comparable unenhanced fixture for an exact comparison. | Effects agree with the selected skills; no missing animations or battle crash. Damage alone varies with enemies, buffs and critical/Spark timing. |
| SP at cap | At Current 0 / Used 100, inspect an unowned node and fusion eligibility; reopen unit/squad screens. | Cannot buy more skills or gain extra SP from frogs; owned nodes and roster remain correct. Total SP includes used SP. |
| Farm scene | Replay mission 10 twice on the updated normal server, reconnect, replay once more. | Farm unlock scene stays completed. Earlier replay failure is still open until this retest. |
| Crafting and spheres | Craft three Famous Blade 30000 copies with recipe 2005. Inspect, equip/transfer/unequip, favorite and sell selected unlocked copies, then reconnect. Also craft Cures with recipe 1009. | Correct costs/counts and visible items; no duplicates or vanished copies. Record first/second/third newly crafted copy behavior: temporary-ID lock ambiguity remains known. If a facility is locked, report it rather than changing progression. |
| Battle items and resume | Load five Cures, use two in mission 10, finish, inspect bar/storage and refill. Separately suspend/relaunch/resume and finish a run. | Three remain from the five taken; transfer/refill conserves total ownership, rewards settle once, correct battle resumes. |
| Evolution and Deemo | Evolve an eligible disposable unit; inspect destination BB/SBB/leader skill and retained equipment/upgrades. Cover Deemo 50563→50564 when materials/progression allow. | Correct evolved form, skills, art and costs; no unrelated unit disappears. Check battle and relaunch. |
| Simulator | Enter mission 6000000, inspect all six elemental dummies, fight, exit and repeat. | No crash or missing dummy; no unintended capture/reward/item loss. |
| Shops and slots | Inspect Merit Legend Stone stock/cost (4,000 Merit); exercise an affordable intended purchase and check persistence. Try slots 1 and multiple rolls when resources allow. | Exact currency/item changes, usable result screens and all prize artwork. Guild Legend Stone is not expected in the decoded guild catalogue. |

Record before/after counts, exact unit/item/mission, immediate behavior versus
after reconnect, and a screenshot/video for failures. The latest screenshot
shows zero gems: gem-priced cases need a funded test fixture first.

## Requires setup or additional progression

- Skill prerequisites on an eligible unit with prerequisite nodes and unspent
  SP; insufficient/exact-cost UI boundaries. Server tests pass; visual behavior
  remains unconfirmed. Vargas's current fully allocated SP cannot cover these.
- Mission 234 first clear/replay through Cordelica unlock; no confirmed crash fix.
- Lizeria prerequisites 666 and 20067, later compound gates and stale-tile notice.
- Level 998→999/result completion and level-999 energy ticks: copied high-level fixture.
- Roglizer leader/helper EXP and Fuu battle drops; normal/Super/Mega parades.
- Reward/equipped-unit sphere return and favorite persistence across presents,
  slots, Merit delivery, fusion/evolution/sale of disposable units.
- Lukroar summon effects, double-gate rapid taps, Amadream header, Galtier long
  description, Seria/Tilith portrait cases, other restored units.
- Burst Queen BB/SBB boundaries and Mystery Frog retained-state regressions.
- Performance: login, warehouse open/scroll/sort, craft/fusion/evolution result,
  multi-roll slots, mission entry/result/resume. Record cold/warm times and
  visible hitches; large-inventory stress and mainline comparison need matching
  copied fixtures. Server test timings do not establish client performance.

## Still implementation work, not ready for acceptance

- ShopUse type-9 SP reset: implemented and server-tested in Session 6 (see the
  top section, test 2). Superseded: it is ready for the client check now.
- Delayed Continue retry A/B/A: fixed and server-tested in Session 6 (durable
  per-run receipts). Retry/fault cases stay on copied saves; the client check is
  the ordinary Continue/resume in the top section, test 1.
- Newly crafted sphere temporary-ID lock/sale-retry ambiguities: known limits.
- Missing trial/boss/Fire Mecha content and undeployed Tilith crop remain backlog.

The historical full matrix is CLIENT_DIFF_CHECKLIST_2026-09-30.md. Its older
"SP purchase unimplemented" and "live server not updated" statements are
superseded by FE_SKILL_PURCHASE_2026-10-01.md and this status record.
