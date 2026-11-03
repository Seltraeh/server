# S19 Fresh client: visual walkthrough

October 23, 2026, for Evan. Package **[ContentTest S19 Fresh]** (`gumi.BraveFrontier.ContentTestS19Fresh`), on
the isolated Session 19 server (`out/qa/current/session19/bin/gimuserverw.exe`, port 29960, PID 2868), blank
save `out/qa/current/session19/content_client_s19/saves/s19_first_launch_blank`. Setup:
[CLIENT_SESSION_19_STEPS.md](CLIENT_SESSION_19_STEPS.md). Never the live save or the live port.

## What Session 19 changes on screen

Nothing is meant to look different from the normal game. Session 19 changed the server, its coverage
report and the audits, not the client. Coverage is still incomplete, so the game keeps its stock checks,
its download screens and the background-download prompt. This run's point is that a **brand-new install**
gets every master table from this server and then plays normally: real names and English text, the four
starter choices, item and area art.

## Already checked, read only (07:23)

You don't need to re-verify these:
- The agreement and consent screens were passed by hand at about 07:16. Initialize then ran: 169 table
  versions announced, 21 of them staged.
- The client downloaded exactly the predicted master tables: **142 of 142** table parts and **29 of 29**
  response tables. It also extracted **103 of 103** text dictionaries. No file was missing, none was
  extra, and the server logged no 404.
- The game got past its login table checks: **no "Refreshing the latest data files" notice and no
  network error**. It is now waiting at the **Background DLC Warning Prompt** (captured:
  `out/qa/current/session19/visual/s19fresh_current.png`).

## Steps

At any step you can tell me "capture N" (the step number). I grab the game window (read only, nothing is
clicked) into `out/qa/current/session19/visual/`. Note anything unexpected even if it isn't asked for.

| # | Do | Expect | Report |
|---|---|---|---|
| 1 | At **Background DLC Warning Prompt**, tap **No** (recommended: it matches the C16 runs and keeps the background list off; Yes is fine if you want to try that branch, just say so) | the prompt closes and the game goes on | Yes or No; how long until the next screen |
| 2 | Follow the new-player opening (story scenes, name entry: any test name) | English text everywhere, readable dialog boxes | any Japanese, garbled or empty text; any missing picture |
| 3 | **Starter choice** | four elements, each with its starter: Fire **Fencer Vargas**, Water **Selena**, Earth **Pikeman Lance**, Thunder **Warrior Eze** (each also gives Burny and Sparky) | names and portraits of all four; then pick one (Selena matches C16) |
| 4 | Tutorial battle(s) and the tutorial summon | units, enemies, effects and the summon gate animate; the summoned unit is revealed | anything blank, a white screen that stays, missing sound if you notice it |
| 5 | Reach **Home** | your starter on Home; the menu buttons. Locked features (Arena, Raid, Co-op) show locked icons | Home renders fully; locked icons are drawn, not empty boxes (these icons are a list Session 19 newly tracks) |
| 6 | **Items** menu: your item box, then the craft / synthesis recipe list (it lists items you don't own yet) | every item has a thumbnail inside a frame; frames differ by item type | any item with a blank thumbnail or no frame (name the item) |
| 7 | **Quest → Grand Gaia → Mistral** | the area art and each quest's banner | the banners render. If **Egor Mountains** is listed (it may still be locked), compare its banner with image **1** of the reference sheet below: **A** or **B** |
| 8 | **Vortex**, if it is open on a new account | the event list with banners | which entries show and whether their banners render. *Corrected after the run:* New Frontier Gate (area 3000001) and King of Fighters (800142) are never Vortex tiles. Image **2** is the banner of New Frontier Gate's dungeons inside **Frontier Gate** mode, so compare it there when that mode is reachable |
| 9 | Close the game normally, then start it again from the Start menu: **[ContentTest S19 Fresh] Brave Frontier** | no consent screen and no background-download prompt this time (both answered); a quick file check; Home | what appeared, and roughly how long the file check took |

## The reference sheet for steps 7 and 8

`out/qa/current/session19/visual/collision_reference.png`. The game holds two different copies of each of these
two images:
- **A** is the copy the quest code asks for, and what the installed client has cached.
- **B** is the copy the C8 policy serves the fresh client today.

Which one you see, and whether it looks right in that spot, decides a change planned for the next session.
Please don't change anything yourself.

## What not to do

- **Consent, age or online-service screens are yours alone**, with one exception you made on
  October 23: the Terms and Conditions screen is the project's own "Brave Frontier Offline Mod"
  disclaimer, and I may pass it on this test client (I say so each time). This checklist doesn't
  ask you to change any of them.
- Don't use C1, C1 Yes, C16 or the live game for this, and don't reset or reinstall the S19 package.
- Leave the server running (PID 2868). If it stops, the restart command is in
  [CLIENT_SESSION_19_STEPS.md](CLIENT_SESSION_19_STEPS.md). Don't recreate the save.

## Report (copy and fill)

```
Step 1  prompt answer: ____   time to next screen: ____
Step 2  text OK? ____   anything odd: ____
Step 3  four starters shown (names / portraits): ____   picked: ____
Step 4  tutorial battle / summon OK? ____   notes: ____
Step 5  Home OK? ____   locked icons drawn? ____
Step 6  item thumbnails + frames OK? ____   blank items: ____
Step 7  Mistral banners OK? ____   Egor Mountains banner: A / B / not listed
Step 8  Vortex open? ____   New Frontier Gate: A / B / not listed   King of Fighters: ____
Step 9  second launch: consent? ____ prompt? ____ file check time: ____ reached Home? ____
Other:  ____
```

After step 9 I'll check from the server log that the second launch fetched no master table (it should
not). I'll then append your report and that check to
[SESSION_19_CLIENT_RESULTS.md](SESSION_19_CLIENT_RESULTS.md).
