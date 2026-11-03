# Later isolated-client checklist: a true first launch against the Session 19 bootstrap contract

**Not executed.** Session 19 changed no client behaviour and launched no client. This checklist is
for a later, separately authorized task. It observes whether a genuinely fresh install receives the
master tables that `scripts/check_mst_bootstrap.py` says this server delivers. An audit is not a
first run; this is how to check one. The consent and online-service screens are out of scope here:
whoever runs the client handles them as before, and nothing below automates or changes them.

## Preconditions (all the runner's, none automated)

- A **new** test package identity, never launched. Do not use C1, C1 Yes or C16: C16 is warm, and
  their packages, registration, saves and consent state stay as they are.
- An isolated server from the Session 19 candidate (`out/qa/current/session19/bin/gimuserverw.exe`)
  on a **fresh** save, config passed as argv[1]. Never the live port 9960 or the live save.
- Before launching, run the audit read-only against that server and keep its output:

  ```bash
  python scripts/check_mst_bootstrap.py --server <PORT> --json <out folder>/first_launch_prediction.json
  ```

  Expected today: `every required delivery is available`, 169 announced rows, the 29 response
  tables present, 104 dictionaries served.

## Observe (record each, with times; mark anything not seen as "not observed")

1. **Server log:** one `GET /accounts/guest/login/`, then Initialize. `mstVersions` should offer the
   21 staged tables. Then one `/mst/Ver..._....dat` request per part the prediction lists: 127
   registered tables, 142 parts.
   None may 404. With `extra_log` enabled in the test config (`"directory": "./log"`,
   `"dlc_error_log_name": "dlc_404.log"`; the test kit's configs turn it off), `dlc_404.log` should
   list no `/mst/` path.
2. **Client:** FILE_CHECK should run to its end with no "Refreshing the latest data files"
   (`REDOWNLOAD_MST_FILES`) notice. A notice means a checked list was empty, and the prediction
   names which tables could cause it.
3. **Client:** no network-error dialog right after FILE_CHECK. That would be setInitialUnits failing
   on DefineMst `21ovwqYT`, whose prediction says elements 1-4 each have a unit.
4. **Test package LocalState, read after the run:** the 142 `Ver*` files of the prediction, at the
   announced versions, and the 29 response files (`EEPBM76L2G.dat`, ...). Compare names only;
   copy nothing out of the package.
5. **A second launch:** no `/mst/` request at all, since every part is now held.

## Not this checklist

The startup screens and downloads beyond the master tables: coverage is still incomplete (5 unknown
lists, 8 absent names, 9 families), so census and prompt stay stock. Removing or shortening any screen
is not part of this. Neither is any change to consent or attestation state.
