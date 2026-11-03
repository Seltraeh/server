"""Start, check and stop ONE isolated client run, touching only the processes that run started.

    python scripts/qa/run_ctl.py start  --qa-dir DIR --run NAME --exe SERVER_EXE [--port 29961]
                                        [--relay-port 29960 | --no-relay] [--relay-delay 0] [--relay-groups G ...]
                                        [--no-client] [--settle 30]
    python scripts/qa/run_ctl.py status --qa-dir DIR --run NAME
    python scripts/qa/run_ctl.py stop   --qa-dir DIR --run NAME

The run folder is <qa dir>/visual/saves/<NAME> (prep_run.py makes it, with config.json and gme.sqlite).

start   refuses unless the run's ports have no listener, no client of the S19 Fresh package is running and the
        run database is not held by any process.  Then it starts the server with the run's config as an
        absolute argv[1] (never the live 9960 default) and records it at once in run.json: pid, creation time
        (from the process handle), canonical image and the command-line parts it must carry.  It waits for the
        server's listener and requires its OWNER to be that server or a child it forked with the same image
        (recorded too).  The relay and the client are started and recorded the same way; the client is the one
        BraveFrontier.Windows.exe of the package created after the launch.  The database probe is reported as
        what it shows: free before the start and held after it, by some process.  If any step fails, only the
        processes this run recorded are stopped.
stop    re-opens each recorded pid, checks creation time, image and command line on the handle it then uses,
        and terminates only matches; a reused pid or a changed image is REFUSED and left alone (exit 1).  The
        client is closed with WM_CLOSE first (bf_drive.ps1 close) when it is the only S19 client, then waited
        for; it is terminated through its verified handle only if it does not exit.
status  each recorded process's check, and every listener on the run's ports with its owner.
"""
from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import qa_run as q  # noqa: E402

CREATE_NO_WINDOW = 0x08000000
S19_AUMID = 'gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w!App'
S19_PACKAGE = (q.REPO / 'out/qa/current/session19/content_client_s19/package').resolve()
CLIENT_IMAGE = 'BraveFrontier.Windows.exe'
DRIVER = q.REPO / 'out/qa/current/session19/visual/bf_drive.ps1'
RELAY = Path(__file__).resolve().parent / 'http_relay_delay.py'
ROLE_ORDER = {'client': 0, 'relay': 1, 'server-child': 2, 'server': 3}


def filetime_now() -> int:
    return int((time.time() + 11644473600) * 10 ** 7)


def s19_client_match(row: dict) -> bool:
    img = row.get('image') or ''
    return Path(img).name.lower() == CLIENT_IMAGE.lower() and S19_PACKAGE in Path(img).resolve().parents


def s19_close_client() -> None:
    subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(DRIVER),
                    '-Action', 'close'], capture_output=True, text=True, timeout=60)


def _record_entry(role: str, pid: int, cmdline_has: list[str]) -> dict:
    ident = q.identity(pid)
    if ident is None or ident['created'] is None:
        raise q.QaError(f'{role} (pid {pid}) is not running')
    return {'role': role, **ident, 'cmdline_has': cmdline_has}


def _wait_listener(port: int, owner: dict, names: list[str], timeout: float, child_role: str | None,
                   rec: dict, run: Path) -> None:
    """Wait for a listener on the port owned by `owner` or (child_role) a child it forked with its image."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if q.identity(owner['pid']) is None:
            raise q.QaError(f'{owner["role"]} (pid {owner["pid"]}) exited before listening on {port}')
        snap = q.snapshot(names, [port])
        owners = q.listener_owners(snap, port)
        if owners:
            allowed = {owner['pid']}
            children = []
            if child_role:
                for r in snap['processes']:
                    if (r['ppid'] == owner['pid'] and q.same_image(r['image'], owner['image'])
                            and int(r['created']) >= owner['created']):
                        children.append(r)
                        allowed.add(r['pid'])
            stray = owners - allowed
            if stray:
                raise q.QaError(f'port {port} is owned by pid(s) {sorted(stray)}, not by this run\'s '
                                f'{owner["role"]} (pid {owner["pid"]})')
            for r in children:
                entry = _record_entry(child_role, r['pid'], owner['cmdline_has'] if
                                      all(n.lower() in (r.get('cmd') or '').lower() for n in owner['cmdline_has']) else [])
                rec['processes'].append(entry)
            rec['listeners'][str(port)] = sorted(owners)
            q.save_record(run, rec)
            return
        time.sleep(0.5)
    raise q.QaError(f'no listener on port {port} within {timeout:.0f} s')


def start(run: Path, *, server: list[str], config: Path, server_port: int, relay: list[str] | None = None,
          relay_port: int | None = None, relay_cmdline_has: list[str] | None = None,
          client_launch: list[str] | None = None, client_match=None, client_close=None,
          db: Path | None = None, listen_timeout: float = 90, client_timeout: float = 60, settle: float = 0) -> dict:
    exe = Path(server[0]).resolve()
    if not exe.is_file():
        raise q.QaError(f'no server executable {exe}')
    config = q.require_inside(config, run, 'config')
    if str(config) not in [str(Path(a).resolve()) for a in server[1:] if Path(a).suffix == '.json']:
        raise q.QaError('the server command must carry the run config as an argument')
    # Every destination is checked before anything is written or launched: the database the config names
    # (the server writes it), the logs opened for append below, and the record files.
    own_db = q.check_run_config(run, config)
    if db is not None and Path(db).resolve() != own_db:
        raise q.QaError(f'database {db} is not the one the run config names ({own_db})')
    for name in ('server.log', 'server.err.log', 'relay.stdout.log', 'relay.log', 'relay.log.dump.jsonl',
                 q.RECORD, q.RECORD + '.tmp',
                 'gme.sqlite-journal', 'gme.sqlite-wal', 'gme.sqlite-shm'):
        q.run_file(run, name)
    ports = [server_port] + ([relay_port] if relay else [])
    names = sorted({exe.name, Path(sys.executable).name, CLIENT_IMAGE} |
                   ({Path(relay[0]).name} if relay else set()))

    if (run / q.RECORD).is_file():
        old = q.load_record(run)
        old_snap = q.snapshot(names, ports)
        live = [e for e in old['processes'] if q.verify_recorded(e, old_snap)[0] == 'match']
        if live:
            raise q.QaError(f'this run still has live processes {[e["pid"] for e in live]}: stop it first')
        n = 1
        while (run / f'{q.RECORD}.{n}').exists():
            n += 1
        (run / q.RECORD).rename(run / f'{q.RECORD}.{n}')
    snap = q.snapshot(names, ports)
    busy = {p: sorted(q.listener_owners(snap, p)) for p in ports if q.listener_owners(snap, p)}
    if busy:
        raise q.QaError(f'ports already have listeners {busy}; nothing started')
    if client_launch and client_match and any(client_match(r) for r in snap['processes']):
        raise q.QaError('a client is already running; this run will not start a second one or close one it did '
                        'not start')
    if db is not None and q.db_probe(db) != 'free':
        raise q.QaError(f'{db} is already held by some process (or missing); nothing started')

    rec = {'format': q.FORMAT, 'run': run.name, 'run_dir': str(run), 'started': q.now_iso(),
           'server_exe': str(exe), 'server_exe_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
           'config': str(config), 'ports': {'server': server_port, 'relay': relay_port},
           'processes': [], 'listeners': {}, 'notes': []}
    q.save_record(run, rec)
    logs = []
    try:
        out, err = open(q.run_file(run, 'server.log'), 'ab'), open(q.run_file(run, 'server.err.log'), 'ab')
        logs += [out, err]
        p = subprocess.Popen([str(exe), *server[1:]], cwd=run, stdout=out, stderr=err,
                             creationflags=CREATE_NO_WINDOW)
        entry = _record_entry('server', p.pid, [str(config)])
        rec['processes'].append(entry)
        q.save_record(run, rec)
        _wait_listener(server_port, entry, names, listen_timeout, 'server-child', rec, run)
        if db is not None:
            after = q.db_probe(db)
            rec['notes'].append(f'run database {db.name}: free before the start, {after} after it -- some process '
                                f'holds it, consistent with the started server; the probe does not name the holder')
            if after != 'held':
                raise q.QaError(f'{db} is not held after the server started: it may have opened another database')
        if relay:
            rout = open(q.run_file(run, 'relay.stdout.log'), 'ab')
            logs.append(rout)
            rp = subprocess.Popen(relay, cwd=run, stdout=rout, stderr=subprocess.STDOUT,
                                  creationflags=CREATE_NO_WINDOW)
            rentry = _record_entry('relay', rp.pid, relay_cmdline_has or [])
            rec['processes'].append(rentry)
            q.save_record(run, rec)
            _wait_listener(relay_port, rentry, names, 30, None, rec, run)
        if settle:
            time.sleep(settle)       # the content broker answers 503 not-ready until it has loaded
        if client_launch:
            t0 = filetime_now()
            subprocess.run(client_launch, capture_output=True, timeout=60)
            deadline, found = time.time() + client_timeout, []
            while time.time() < deadline and not found:
                time.sleep(1)
                found = [r for r in q.snapshot(names, [])['processes']
                         if client_match(r) and int(r['created']) >= t0]
            if len(found) != 1:
                raise q.QaError(f'expected one new client process, found {len(found)}')
            rec['processes'].append(_record_entry('client', found[0]['pid'], []))
            rec['client_closer'] = 'bf_drive.ps1 close' if client_close else None
        rec['ready'] = q.now_iso()
        q.save_record(run, rec)
        return rec
    except Exception as ex:
        report, _ = stop(run, client_close=None)
        rec = q.load_record(run)
        rec['notes'].append(f'start failed ({ex}); stopped only this run\'s recorded processes: {report}')
        q.save_record(run, rec)
        raise
    finally:
        for f in logs:
            f.close()


def stop(run: Path, client_close=None, client_wait: float = 20) -> tuple[list[str], bool]:
    rec = q.load_record(run)
    names = sorted({Path(e['image']).name for e in rec['processes'] if e.get('image')})
    snap = q.snapshot(names, [])
    report, refused = [], False
    for e in sorted(rec['processes'], key=lambda e: ROLE_ORDER.get(e['role'], 9)):
        status, detail = q.verify_recorded(e, snap)
        tag = f'{e["role"]} pid {e["pid"]}'
        if status == 'gone':
            report.append(f'{tag}: already exited')
            continue
        if status == 'mismatch':
            refused = True
            report.append(f'{tag}: REFUSED, left running -- {detail}')
            continue
        if e['role'] == 'client' and client_close is not None:
            others = [r for r in snap['processes'] if r['pid'] != e['pid'] and s19_client_match(r)]
            if not others:
                client_close()
                with q.Proc(e['pid']) as p:
                    if p.wait(client_wait):
                        report.append(f'{tag}: closed (WM_CLOSE)')
                        continue
        with q.Proc(e['pid'], terminate=True) as p:
            if not p.alive:
                report.append(f'{tag}: exited')
                continue
            if p.created() != e['created'] or not q.same_image(p.image(), e['image']):
                refused = True
                report.append(f'{tag}: REFUSED at terminate time -- identity changed')
                continue
            ok = p.terminate() and p.wait(10)
            report.append(f'{tag}: {"terminated" if ok else "TERMINATE FAILED"}')
            refused |= not ok
    rec['stopped'] = q.now_iso()
    rec.setdefault('stop_reports', []).append(report)
    q.save_record(run, rec)
    return report, refused


def status(run: Path) -> list[str]:
    rec = q.load_record(run)
    ports = [p for p in rec['ports'].values() if p]
    names = sorted({Path(e['image']).name for e in rec['processes'] if e.get('image')})
    snap = q.snapshot(names, ports)
    out = [f'{e["role"]} pid {e["pid"]}: {" -- ".join(q.verify_recorded(e, snap))}' for e in rec['processes']]
    mine = {e['pid'] for e in rec['processes']}
    for p in ports:
        for o in sorted(q.listener_owners(snap, p)):
            out.append(f'port {p}: listener pid {o} ({"this run" if o in mine else "NOT this run"})')
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('action', choices=('start', 'stop', 'status'))
    ap.add_argument('--qa-dir', required=True)
    ap.add_argument('--run', required=True)
    ap.add_argument('--exe')
    ap.add_argument('--port', type=int, default=29961)
    ap.add_argument('--relay-port', type=int, default=29960)
    ap.add_argument('--no-relay', action='store_true')
    ap.add_argument('--relay-delay', type=float, default=0)
    ap.add_argument('--relay-groups', nargs='*', default=[])
    ap.add_argument('--relay-dump', nargs='*', default=[], metavar='HANDLER',
                    help='handler names (bf_testkit.KEYS, e.g. MissionStart MissionEnd) whose decrypted requests '
                         'and replies the relay appends to relay.log.dump.jsonl')
    ap.add_argument('--no-client', action='store_true')
    ap.add_argument('--settle', type=float, default=30)
    a = ap.parse_args(argv)
    try:
        run = q.run_dir(q.qa_dir(a.qa_dir), a.run, must_exist=True)
        if a.action == 'status':
            print('\n'.join(status(run)))
            return 0
        if a.action == 'stop':
            report, refused = stop(run, client_close=s19_close_client)
            print('\n'.join(report))
            return 1 if refused else 0
        if not a.exe:
            raise q.QaError('start needs --exe')
        config = run / 'config.json'
        dumps = []
        if a.relay_dump:
            sys.path.insert(0, str(q.REPO / 'scripts'))
            from bf_testkit import KEYS
            unknown = [n for n in a.relay_dump if n not in KEYS]
            if unknown:
                raise q.QaError(f'--relay-dump names no handler in bf_testkit.KEYS: {unknown}')
            dumps = [f'{KEYS[n][0]}={KEYS[n][1]}' for n in a.relay_dump]
        relay = None if a.no_relay else [sys.executable, '-I', str(RELAY), str(a.relay_port), str(a.port),
                                         str(run / 'relay.log'), str(a.relay_delay), *a.relay_groups, *dumps]
        rec = start(run, server=[a.exe, str(config)], config=config, server_port=a.port, relay=relay,
                    relay_port=None if a.no_relay else a.relay_port,
                    relay_cmdline_has=[str(RELAY), str(run / 'relay.log')],
                    client_launch=None if a.no_client else ['explorer.exe', f'shell:AppsFolder\\{S19_AUMID}'],
                    client_match=s19_client_match, client_close=s19_close_client, db=run / 'gme.sqlite',
                    settle=a.settle)
        for e in rec['processes']:
            print(f'{e["role"]}: pid {e["pid"]} created {e["created"]} {e["image"]}')
        print('\n'.join(rec['notes']))
        print(f'server exe sha256 {rec["server_exe_sha256"]}; listeners {rec["listeners"]}')
        return 0
    except q.QaError as ex:
        print(f'refused: {ex}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
