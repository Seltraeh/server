# Isolated client-run helpers (`scripts/qa/`)

Reusable helpers for driving the isolated S19 Fresh test client against a test server on copied saves. Logs, configs, saves and screenshots of each run stay under `out/qa/current/<round>/`. These helpers do not run, change or stop the live server, the live save or any other run.

| File | Purpose |
|---|---|
| `qa_run.py` | Shared checks: run names, containment, Win32 process identity, run records, database probe |
| `prep_run.py` | Copy a save and write the run's config |
| `prep_chain.py` | Clear every prerequisite of a target mission in the copy (and optionally copy a squad) |
| `run_ctl.py` | Start, check and stop one run's server, relay and client |
| `http_relay_delay.py` | Logging relay that can hold chosen replies (started by `run_ctl.py`) |
| `test_qa_run.py` | Tests with dummy processes; never touches a real client or save |

## A run, end to end

Replace `R` with a round folder, for example `out/qa/current/fresh_round3_20261029`.

```bash
python scripts/qa/prep_run.py --qa-dir R --run slots1 --save out/qa/current/session19/visual/save_after_s20_completed_check.sqlite --content-root out/qa/current/fresh_round1_20261028/content_root --system-root out/qa/current/fresh_round2_20261029/visual/system --set gems=60
```

```bash
python scripts/qa/prep_chain.py --qa-dir R --run slots1 --target 85 --units 10017,40987,830918,840968,840808
```

```bash
python scripts/qa/run_ctl.py start --qa-dir R --run slots1 --exe out/qa/current/fresh_round1_20261028/bin/gimuserverw.exe
```

```bash
python scripts/qa/run_ctl.py status --qa-dir R --run slots1
```

```bash
python scripts/qa/run_ctl.py stop --qa-dir R --run slots1
```

Drive the client with `out/qa/current/session19/visual/bf_drive.ps1`, which checks the foreground window and occlusion before every input.

To record exactly what the client sent and received for chosen handlers, add `--relay-dump MissionStart MissionEnd` (any names in `bf_testkit.KEYS`) to `start`. The relay then decrypts those requests and replies into `relay.log.dump.jsonl` in the run folder. Bytes are forwarded unchanged; a message the dump cannot decode is logged and skipped.

## What they refuse

- **A run name that is not one identifier** (`[A-Za-z0-9][A-Za-z0-9_-]{0,62}`). The run folder must be exactly `R/visual/saves/<name>`, and `R` must lie below `out/qa/current`. A link at `visual`, at `saves` or at the run folder is refused, as are traversal, rooted and drive-qualified names, before anything is written.
- **A write through a link.** Every file a helper writes in a run, and every file the server it starts writes, must be absent or a plain regular file with a single link. That covers the database and its journal files, the config, logs, `run.json` and its temporary file. A symlink, junction, other reparse point or shared hard link is refused, so a write can never reach another save.
- **A config that names another database.** `run_ctl start` requires the run's config to name exactly one database, the run's own `gme.sqlite`. A config edited to point at another save, including the live one, is refused before anything starts.
- **A bad copy.** The live save as a source; a source without exactly one account; an existing run folder (runs are never overwritten). Also an MST root or system root missing any file that `deploy/` has, because a missing file at boot leaves an unkillable server.
- **A bad fixture.** A prerequisite that names no mission row, a prerequisite cycle, or a target already cleared in the copy. Prerequisites follow the server's story rule: the mission's, its dungeon's and its area's need lists, every comma entry, transitively. Scenario markers are not written, so the client shows the implied unlock scenes on the first visit and then stores its own markers.
- **A start into a busy environment.** A listener already on the run's ports, an S19 client already running, or a run database already held open. The server's listener must be owned by the started server or a child it forked; the relay's by the relay. A failed start stops only what it recorded.
- **A stop on a changed identity.** `stop` re-opens each recorded pid and compares its creation time, image path and required command-line parts on the same handle it terminates through. A reused pid or another run's process is refused, left running and reported (exit 1). The client is closed with WM_CLOSE when it is the only S19 client, and terminated through its verified handle only if it does not exit.

## Limits

- The database probe says only that some process holds the file. The start reports it as "free before, held after", which is consistent with the started server but does not name it.
- `bf_drive.ps1` still finds the client window by title, so `stop` sends WM_CLOSE only after verifying that the recorded client is the only one.

Tests: `python scripts/qa/test_qa_run.py --work out/qa/current/<round>/helpers_selftest`. The default work folder is `out/qa/current/qa_helpers_selftest`, which is recreated on each run; pass a new folder to keep earlier evidence. Junctions and links are made only between disposable folders and files inside the work folder.
