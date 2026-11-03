# Client test status for Session 8

## Session 9 update (October 4)

The provider proof no longer needs the normal server stopped: the test package
now carries an isolated-port build of the reviewed DLL (port 29960), and the
harness prints `READY` only when its server is proven ready. Use the procedure
in [CLAUDE_SESSION_9_HANDOFF.md](CLAUDE_SESSION_9_HANDOFF.md) §3; the
switch-over steps below are the fallback for the reviewed 9960 DLL. New check
after the leader-skill deployment and repair (handoff §8): in Training Grounds,
pick one of your own units as the helper — e.g. Inferno Berdette (LS 7401,
+30% Atk and max HP for all units): the party's max HP at battle start should
now include that boost (compare with a note taken before the repair); ordinary
battles should look exactly as before.

## Latest October 4 candidate acceptance

Follow-up player confirmation: the selected sphere's favorite persists,
equipment persists, and selling spheres removes only the selected copies.
Together with the fresh-craft display/equip pass, issue #34 is now
player-confirmed fixed for the reported workflow. No remaining sphere acceptance
check from this sequence blocks progress to client-provider testing.

Player confirms a fresh batch of identical spheres on the candidate: all copies
were visible/equippable, and one copy could be favorited. The repeated-craft
display defect is player-confirmed fixed. This supersedes the fresh-batch
pending status below. Remaining copy-specific regression checks: relaunch
persistence, intended spare sale without affecting others, and verifying the
favorite stayed on just the selected copy. Client-provider acceptance is separate.

## October 4 player update

Player confirms all previously crafted spheres are visible and mission completion
no longer sends them to Home. Keep these as passes. Normal server still hashes
to fb77bf6f..., while the fresh-craft fix is in candidate a2022e91.... Therefore
the immediate display of a NEW batch on the candidate remains to be checked;
existing rows can recover through login on the old build. Final-stage next-area
presentation and independent copy actions remain separate acceptance cases.
See QC_SESSION_8_REVIEW.md and CLAUDE_SESSION_9.md for the next work.

Updated 2026-10-04 by Claude Session 8 ([handoff](CLAUDE_SESSION_8_HANDOFF.md)).
Everything below separates what was **observed in the client**, what is
**server/binary evidence**, and what is still **client-pending**. Nothing here
was played by Claude; no client check below has passed yet unless it says
"player-confirmed".

## Current observation

Player report (not re-observed): crafting many identical spheres broke the Equip
Sphere list — a large blank gap, a description near the top, one row near the
bottom. Town still looks correct. This is the historical failure; see the latest
candidate acceptance above for the display fix and remaining regression checks.

Recovered from read-only logs and a backup-API copy of the save:

- The craft was ONE ItemMix batch, `4HqhTf3a = "2005:19"`, at 2026-10-03
  21:58:01 (`deploy/log/http_log_4P5GELTF_20261003_215801.log`), answered `{}`.
  Recipe 2005 = **19 × Famous Blade (item 30000, max stack 1)** for 95 × 10400
  and 9,500 Karma. Its short description is "Boosts Atk by 5%".
- The save holds all 19 (`user_items` 30000 = 19; 10400 = 5; recipe 2005 counted
  19; Karma 700,367 and Zel 2,242,846, the values in the screenshot's header).
  The server had not yet given them warehouse rows; that happens at the next
  sync. **No inventory loss is established.**
- No request followed the craft, so persistence after a relaunch was not
  observed.

Mechanism, from the Windows client binary (not from the screenshot): the client
names every copy it crafts `INT_MAX - 30000` until a warehouse list replaces
them, and the Equip Sphere list keys each row's sprites, labels and visibility on
that id, so 19 copies share one row's drawing. The screenshot is consistent with
that; it is not by itself the proof. Details: handoff §2–3.

Runtime at the time: the normal server binary `fb77bf6f…` (same code as the
reviewed QA server; ItemMix replied `{}`), installed proxy DLL `9ae05b17…`
(no provider), provider policy `enabled: false`. Ordinary play therefore tested
server gameplay and the existing art cache, not the new provider.

## Useful checks now (current server, no crafting needed)

1. **Relaunch, then Equip Sphere.** Close and relaunch normally (no Windows app
   reset). The server's login list now gives the 19 Famous Blades 19 distinct row
   ids (server-side evidence:
   `out/qa/current/session8/evidence/craft_prefix.json`, `next_login_lists`), so
   the list should draw 19 normal rows and scroll normally. Record: rows visible,
   gap or not, the count shown. A gap that survives a relaunch would contradict
   the mechanism — report it with a screenshot.
2. Progression: on the next natural final-stage clear, verify return to the map,
   next-area reveal and first stage selection rather than an unexplained Home
   jump. Repeat a cleared stage: no repeated unlock scene. Record mission IDs.
3. Training Grounds: Save/Load Conditions, changed elements/HP, and Zeal helper
   appearance if available. A Zeal pass here verifies current cached art only.
4. Existing purchased SP buffs: verify the expected stats/effects in battle and
   reconnect persistence. Reset/repurchase needs a funded test fixture; do not
   spend resources solely to repeat already-confirmed reset/persistence checks.
5. Continue/resume needs a funded test fixture: revive, close mid-mission,
   relaunch/resume and finish; separately ensure no-gem refusal is recoverable.
   Ordinary mission item checks: five Cures taken, two used, three remain;
   refilling must conserve total stock. Never use Windows app reset for resume.

Already confirmed (earlier sessions): SP purchase persistence/reset, ordinary
two-potion consumption, three Deemo forms in battle, training entry/passive
dummies/reticle, 400 shards, Berdette BB/SBB evolution behavior, friends after
fusion, sphere equip/transfer on previously refreshed copies, Farm harvest/intro,
Sphere House gating and Town. These passes do not certify the repeated-craft list.

## After the sphere fix is deployed

The fix is **server-only** (no client change): the ItemMix reply now carries the
whole warehouse, so every crafted copy has its own row id before any unit or
sphere screen exists. It is built and wire-tested in the QA candidate
`out/qa/current/session8/bin/gimuserverw.exe` (`a2022e91…`) and **not deployed**;
deployment steps are in the handoff §9. Check the running server first: after
deployment the in-tree `gimuserverw.exe` must be newer than 2026-10-03 21:55.

Use a craft you can afford (e.g. Famous Blade, recipe 2005: 5 × 10400 + 500
Karma each). The box holds 100; stay well under it.

1. **Craft enough identical copies to fill more than one screen** (≥ 8), in one
   session and with "craft again" held, then a second session of a few more
   before leaving. Leave the synthesis screens (Back to Town or the footer) —
   that is when the client sends the batch.
2. Open a unit's Equip Sphere: **every copy is its own row** — no blank gap, no
   floating description, normal spacing. Scroll to the bottom and back to the top
   slowly, then fast. Sort (Sort/Filter) by another order and back.
3. Leave Equip Sphere and re-enter; then relaunch normally and re-enter. Same rows
   each time.
4. Lock (favorite) the **first** and **last** copy from the item detail; equip the
   **middle** copy on a one-slot unit, the locked last copy into a Sphere-Frog
   unit's second slot, then move the middle copy directly to another unit. Sell
   one **unlocked spare**. Other copies must stay untouched (locks, owner, count).
5. Compare immediately, after reopening, and after a reconnect: total Famous
   Blades (sold one fewer), material and Karma spent (5 × 10400 and 500 per
   craft), locks on exactly the two copies, both units still wearing theirs.
6. Regression: craft a stackable item (e.g. Cure at Synthesis) past 99 — one full
   stack plus the remainder; a normal sale of it still works.

Known limits, expected (not regressions): a byte-identical ItemMix retry after a
lost reply crafts again (the protocol has no request identity); if the server
ever crafts fewer than the client applied (a stale client), the inventory is
corrected at once but the Karma figure only at the next login.

Wire tests cannot certify drawing; only this pass can close #34.

## After isolated provider candidate activation

Separate from the sphere fix. Uses a **side-by-side test package**
(`gumi.BraveFrontier.ContentTest`, its own LocalState and loopback exemption) and
an **isolated server with a copied save** on port 9960 — never the installed
client, its LocalState or the live save. The candidate DLL is built for 9960, so
this is a **switch-over**: the normal server is stopped for the test and
restarted afterwards (handoff §6.4 has the exact commands).

Prepared by Claude (only `out/qa/current/content_client` written): the test
package layout (1,361 hard links to the installed package's own unpacked layout,
identity `gumi.BraveFrontier.ContentTest` / `CN=BraveFrontier`, candidate
`libcurl.dll` `e24234c1…`) and the 21-file broker fixture. Still the
maintainer's: register the package (Developer Mode is on), grant its loopback
exemption (elevated), launch it once, then run the checks.
`python scripts/content_provider_client_test.py check` lists what is outstanding.

First launch: the test LocalState starts with only the copied account files, so
the client downloads its own asset cache through the legacy path (~71,000 files,
~2.5 GB, roughly half an hour). That download is not a provider result; take the
`snapshot before` only after it finishes.

Checks (each with the log line named in the handoff §6.5):

- Town with the deliberately stale `MapVillage.sam` planted in the TEST package
  only: labels on their buildings (authoritative framed bytes).
- Zeal 60956's first battle: portrait, sprite and animations.
- Same-size fixture edit while a scene is loaded, then a safe reload (leave and
  re-enter Town): the new bytes appear.
- Missing/changed dependency: useful error and recovery after restoration; no
  white blob, crash or repeated disk download accepted as success.
- Account/settings/purchased skills/equipment preserved across relaunch.
- Town, battles and summons: visible stalls, input timing and measured frame
  pacing; compare provider off/on under the same conditions.
- Snapshots before/after prove no persistent writes for owned assets.

Do not reset the installed app or edit the live content for these tests. The
candidate covers Town and Zeal only. Broader asset families, download-queue
interception, background prefetch, old-version retention and obsolete
loading-screen removal remain later phases. A warm-cache pass does not certify
the no-cache objective, and desktop broker tests are not game evidence.

## Later backlog acceptance

Mission 234/Cordelica and compound unlock gates; level-999 results and doubled
energy recovery; SP/UBB visibility; leader/helper EXP and Fuu rewards; slots and
parades; long names/descriptions, summon gates, missing-unit art; special-boss
reticles and encounter mechanics. Use appropriate supplied/progressed fixtures.
Missing Trials, bosses, Fire Mecha drops, EU units, stored leader skills and
GuildUpdate still require scoped implementation/investigation as applicable.
