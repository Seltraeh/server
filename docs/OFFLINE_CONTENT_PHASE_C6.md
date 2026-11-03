# Offline client, Phase C6: the Home and Summon screens' images

October 9, 2026 (Session 15, priority 2). A candidate **policy only** (no DLL or
server change beyond Session 15's C5). Evidence: `out/qa/current/session15/`.

## 1. Which legacy download path it removes

The Session 11 first-visit trace (fresh package, no-prompt branch) shows Home's
entry (`ConfirmScene -> HomeScene2`, a TransitionalDLCScene) queueing **761**
downloads and the first mission's entry **1,487**. With C5 owned, 704 and 467 of
them were still legacy -- almost all single images of a few families:

| Family | Images | Home entry (legacy under C5) | Note |
|---|---:|---:|---|
| `item/` | 803 | 218 | 9 also packaged with other bytes |
| `dungeon/` | 888 | 236 | 3 flat-name collisions excluded |
| `gacha/` | 1,501 (+18 `.txt` scripts, excluded) | 157 | summon banners, buttons, door |
| `home/` | 77 | 39 | 3 collisions excluded, 5 packaged with other bytes |
| `summon/skill_icon/` | 25 | 18 | |
| `area/` | 120 | 9 | `area_sp.jpg` already owned by C3 |
| `banner/` | 32 | 4 | |

## 2. Why they may be owned

- Every member is a lone PNG or JPEG (magic checked by the generator) with no
  file depending on it or it depending on: no sprite-frame plist, layout or font
  in these folders. `footer/` (frame plists) and `common/`, `common/button/` (21
  flat-name collisions, 39 packaged copies with other bytes) are left out.
- Images reach the hooked `CCFileUtils::getFileData` through
  `CommonUtils::getTexture` 0x2353D0 / `CCTextureCache::addImage`; the provider
  already makes the FileCrypt decisions per reader (client-confirmed for the unit
  families C1/C2, and Session 12's visible swap through the same texture path),
  and every cocos-level existence check of a LocalState path is hooked.
- A name the app package also ships with other bytes is drawn from the package
  only until the Home entry downloads the server's copy; owning it serves the
  server's copy from the first draw -- what every client that visited Home
  already draws. Listed in the report, not excluded.
- Flat-name collisions (differing copies elsewhere in game_content) are
  excluded: the client's flat LocalState holds only one of them.

## 3. The policy

`scripts/gen_ui_family_policy.py --base <C5> --out <C6>`: one bundle per file
(`ui_<family>_<file name>`), exact scope paths.

| Policy | Names | SHA-256 |
|---|---:|---|
| `out/qa/current/session15/policies/family_policy_c6.json` | 57,631 (C5 + 3,439) | `e3d29e25…` |

## 4. Verification (server-tested; existing-package client confirmed October 11)

Player confirmed the corrected Test Gate: 7 Star banner swap and restoration,
audio swap/restoration and ordinary route visuals. The visible target was
`gacha_rare_bg_img_20161028_5M.png`; the older smoke below used a different
banner and proved publication only. Owned-file snapshots were unchanged.
Cold-package download elimination remains pending. See QC_SESSION_15_REVIEW.md.

| Check | Result |
|---|---|
| Trace reduction (`content_inventory.py --policy C6 --trace …`) | first-visit demand downloads owned: 3,938 of 5,630 (C5 2,745, C3 2,542); Home entry 734 of 761 (27 left), first mission 1,476 of 1,487; transitional windows of the no-prompt branch: 3,927 of 5,612 files owned, 5 windows gone (C3: 3) |
| Broker dry run (`check_content_policy.py --policy C6`) | **8/0**: every declared member served, nothing else, no incomplete bundle or unresolved collision, no client state name, digests match |
| Real broker + hooks (`test_content_family_live.py … C6`) | **driver 9/0, proxy 6,073/0**: all 3,938 traced owned names served byte-exact, plus the 36,321 C3 game reads; provider index 36 MB for 57,631 names |
| Harness smoke of Part C (`harness_s15_audio_smoke.py … C6`) | **18/0**: READY with 57,631 names; the summon background `gacha_rare_bg_img_20170524.png` swapped to `gacha_rare_bg_img_20151223_xmas.png` and restored, published each time |

Client check: Part C of [CLIENT_SESSION_15_STEPS.md](CLIENT_SESSION_15_STEPS.md)
(Home, Summon, items, quests drawn as usual; the swapped Test Gate background
proves the summon screen draws the provider's bytes). On the existing test
package these images are cached, so the run shows correctness, not the removed
downloads; the reduction itself needs a cold package (plan D5).

## 5. Limits

- The 27 names Home's entry keeps (collisions, `guild/`, `common/` art, its
  layout `layout_home2.csv`, `tuto101.txt`) still bring up a short transitional
  screen on an uncached client.
- The login check list (2,272 priority requests) is mostly other families
  (`guild`, `common/button`, `common`, `_dlcbundle` layouts and fonts): C6
  changes it by 10 names.
