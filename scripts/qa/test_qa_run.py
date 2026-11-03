"""Tests for scripts/qa (no real client, server or save is touched).

    python scripts/qa/test_qa_run.py [--work out/qa/current/<round>/helpers_selftest]

Everything happens in the work folder (default out/qa/current/qa_helpers_selftest, recreated each run; pass
a new folder to keep an earlier run's evidence).  Links are made only between folders and files inside it.  Process control is tested
with dummy Python processes that listen (themselves or through a forked child, as gimuserverw does), never
listen, exit at once, or bind a port late; an "unrelated" dummy with the SAME command line must survive every
stop and every failed start.  Saves are copies made here; the live save is only opened read-only by the
refusal check.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sqlite3
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))
import qa_run as q  # noqa: E402
import prep_chain  # noqa: E402
import prep_run  # noqa: E402
import run_ctl  # noqa: E402
from bf_testkit import Checker, free_port  # noqa: E402

WORK = q.QA_PARENT / 'qa_helpers_selftest'
COMPLETED = q.REPO / 'out/qa/current/session19/visual/save_after_s20_completed_check.sqlite'
CONTENT = q.REPO / 'out/qa/current/fresh_round1_20261028/content_root'
SYSTEM = q.REPO / 'out/qa/current/fresh_round2_20261029/visual/system'
DUMMY = '''import socket, subprocess, sys, time
mode, port = sys.argv[1], int(sys.argv[2])
def listen():
    s = socket.socket(); s.bind(("127.0.0.1", port)); s.listen(); time.sleep(300)
if mode == "self": listen()
elif mode == "child":
    subprocess.Popen([sys.executable, __file__, "self", str(port)] + sys.argv[3:]); time.sleep(300)
elif mode == "late": time.sleep(2.0); listen()
elif mode == "exit": time.sleep(1.0); sys.exit(3)
else: time.sleep(300)
'''
spawned: list[subprocess.Popen] = []


def raises(fn, *args, **kw) -> str | None:
    try:
        fn(*args, **kw)
    except q.QaError as ex:
        return str(ex)
    return None


def unrelated(mode: str, port: int, marker: Path) -> subprocess.Popen:
    p = subprocess.Popen([sys.executable, str(WORK / 'dummy.py'), mode, str(port), str(marker)],
                         creationflags=run_ctl.CREATE_NO_WINDOW)
    spawned.append(p)
    return p


def new_run(name: str) -> Path:
    """A dummy run: an empty database and a config naming it (run_ctl.start checks both)."""
    run = q.run_dir(WORK, name, must_exist=False)
    run.mkdir(parents=True)
    (run / 'gme.sqlite').write_bytes(b'')
    write_config(run / 'config.json', run / 'gme.sqlite')
    return run


def write_config(path: Path, db: Path) -> None:
    path.write_text(json.dumps({'db_clients': [{'filename': q.posix_abs(db)}]}), encoding='utf-8')


def junction(link: Path, target: Path) -> bool:
    return subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(target)], capture_output=True).returncode == 0


def unjunction(link: Path) -> None:
    subprocess.run(['cmd', '/c', 'rmdir', str(link)], capture_output=True)    # removes the link only


def server_cmd(run: Path, mode: str, port: int) -> list[str]:
    return [sys.executable, str(WORK / 'dummy.py'), mode, str(port), str(run / 'config.json')]


def alive(pid: int) -> bool:
    return q.identity(pid) is not None


def entries(run: Path) -> list[dict]:
    return q.load_record(run)['processes']


def test_names(check: Checker) -> None:
    check('a plain identifier is a valid run name', q.validate_run_name('round3_slots-1') == 'round3_slots-1')
    bad = ['', '.', '..', '../escape', 'a/b', 'a\\b', 'C:\\x', 'C:x', '/rooted', '\\\\host\\share', 'x' * 64,
           ' lead', 'dot.name', 'nul\0x', None, 7]
    check(f'{len(bad)} rooted, traversal, separator, drive, overlong and non-string names are refused',
          all(raises(q.validate_run_name, n) for n in bad), [n for n in bad if not raises(q.validate_run_name, n)])
    for path in (q.QA_PARENT, q.REPO / 'deploy', q.QA_PARENT / '..' / '..' / 'deploy', Path('C:/Windows')):
        check(f'qa_dir refuses {path}', raises(q.qa_dir, path) is not None)
    check('qa_dir accepts a folder below out/qa/current', q.qa_dir(WORK) == WORK.resolve())
    before = sorted(p.name for p in q.QA_PARENT.iterdir())
    saves_before = sorted(p.name for p in q.saves_root(WORK).iterdir())
    codes = [prep_run.main(['--qa-dir', str(WORK), '--run', n, '--save', str(COMPLETED), '--content-root',
                            str(CONTENT), '--system-root', str(SYSTEM)]) for n in ('../escape', 'C:\\x', 'a/b')]
    check('prep_run refuses those names with exit 2 and writes nothing anywhere',
          codes == [2, 2, 2] and sorted(p.name for p in q.QA_PARENT.iterdir()) == before
          and sorted(p.name for p in q.saves_root(WORK).iterdir()) == saves_before, codes)
    outside = WORK / 'outside'
    outside.mkdir()
    link = q.saves_root(WORK) / 'jx'
    made = subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(outside)], capture_output=True).returncode == 0
    if made:
        check('a junction in the saves root that leads elsewhere is refused as a run folder',
              raises(q.run_dir, WORK, 'jx', must_exist=True) is not None)
        subprocess.run(['cmd', '/c', 'rmdir', str(link)], capture_output=True)    # removes the link only
    else:
        print('SKIP  junction case: mklink /J failed')


def test_prep(check: Checker) -> None:
    common = dict(qa=WORK, content_root=CONTENT, system_root=SYSTEM)
    check('the live save is refused as a source',
          'live save' in (raises(prep_run.prepare, name='live', save=q.LIVE_SAVE, **common) or ''))
    two = WORK / 'two_accounts.sqlite'
    src = sqlite3.connect(f'file:{COMPLETED.as_posix()}?mode=ro', uri=True)
    dst = sqlite3.connect(two)
    src.backup(dst)
    src.close()
    cols = [r[1] for r in dst.execute('PRAGMA table_info(user_info)')]
    picks = [f"{c} || 'b'" if c in ('id', 'gumi_user_id') else c for c in cols]
    dst.execute(f'INSERT INTO user_info ({",".join(cols)}) SELECT {",".join(picks)} FROM user_info')
    dst.commit()
    dst.close()
    msg = raises(prep_run.prepare, name='two', save=two, **common)
    check('a source with two accounts is refused before any write', msg and 'exactly one' in msg
          and not (q.saves_root(WORK) / 'two').exists(), msg)
    msg = raises(prep_run.prepare, name='badset', save=COMPLETED, sets=['tutorial_status=x'], **common)
    check('a non-integer --set is refused before any write', msg and not (q.saves_root(WORK) / 'badset').exists(), msg)
    msg = raises(prep_run.prepare, name='badcol', save=COMPLETED, sets=['gumi_user_id=1'], **common)
    check('a --set column outside the allowlist is refused', msg is not None, msg)
    msg = raises(prep_run.prepare, name='badmst', save=COMPLETED, mst_root=WORK, **common)
    check('an MST root missing tables is refused', msg and 'lacks' in msg, msg)
    msg = raises(prep_run.prepare, name='badarch', save=COMPLETED, archive_root=WORK, **common)
    check('an archive root missing files is refused', msg and 'lacks' in msg and not (q.saves_root(WORK) / 'badarch').exists(), msg)
    run = prep_run.prepare(name='good', save=COMPLETED, sets=['level=150', 'energy=200'], **common)
    cfg = json.loads((run / 'config.json').read_text(encoding='utf-8'))
    paths = [cfg['app']['document_root'], cfg['db_clients'][0]['filename'],
             *(cfg['plugins'][0]['config']['server'][k] for k in ('system_root', 'mst_root', 'archive_root'))]
    check('a good copy: config paths absolute with forward slashes, the database is the copy',
          all('\\' not in p and Path(p).is_absolute() for p in paths)
          and Path(cfg['db_clients'][0]['filename']) == (run / 'gme.sqlite').resolve(), paths)
    db = sqlite3.connect(run / 'gme.sqlite')
    check('the copy has the --set values; the source is unchanged',
          db.execute('SELECT level, energy FROM user_info').fetchone() == (150, 200))
    db.close()
    check('preparing the same run name again is refused (never overwritten)',
          'already exists' in (raises(prep_run.prepare, name='good', save=COMPLETED, **common) or ''))


def test_chain(check: Checker) -> None:
    g = {1: set(), 2: {1}, 3: {2}, 4: {2, 3}, 5: {4, 99}, 6: {7}, 7: {6}, 8: {8}, 9: {4, 1}}
    check('a compound need is followed on every branch, prerequisites first',
          prep_chain.closure(g, 4) == [1, 2, 3] and prep_chain.closure(g, 9) == [1, 2, 3, 4])
    check('a need naming no mission row is refused', 'no mission row' in (raises(prep_chain.closure, g, 5) or ''))
    check('a cycle is refused', 'cycle' in (raises(prep_chain.closure, g, 6) or ''))
    check('a self-need is refused as a cycle', 'cycle' in (raises(prep_chain.closure, g, 8) or ''))
    graph = prep_chain.need_graph(
        [{'j28VNcUW': '10', 'HSRhkf70': '', 'MHx05sXt': '1'}, {'j28VNcUW': '11', 'HSRhkf70': '10', 'MHx05sXt': '1'},
         {'j28VNcUW': '20', 'HSRhkf70': '11', 'MHx05sXt': '2'}, {'j28VNcUW': '30', 'HSRhkf70': '0', 'MHx05sXt': '3'}],
        [{'MHx05sXt': '1', 'VjCY7rX4': '1', 'HSRhkf70': '0'}, {'MHx05sXt': '2', 'VjCY7rX4': '1', 'HSRhkf70': '11'},
         {'MHx05sXt': '3', 'VjCY7rX4': '2', 'HSRhkf70': ''}],
        [{'VjCY7rX4': '1', 'HSRhkf70': '0'}, {'VjCY7rX4': '2', 'HSRhkf70': '20,11'}])
    check('dungeon and area need lists count too (the server\'s rule): 30 needs area 2\'s "20,11"',
          prep_chain.closure(graph, 30) == [10, 11, 20], prep_chain.closure(graph, 30))
    real = prep_chain.need_graph(prep_chain.rows('mission_mst'), prep_chain.rows('dungeon_mst'),
                                 prep_chain.rows('area_mst'))
    lz = prep_chain.closure(real, 700)
    check('real data: Lizeria 700 needs both 666 and 20067; 234 keeps the 53-mission chain',
          666 in lz and 20067 in lz and len(prep_chain.closure(real, 234)) == 53)
    run = q.saves_root(WORK) / 'good'
    db = sqlite3.connect(run / 'gme.sqlite')
    user = db.execute('SELECT id FROM user_info').fetchone()[0]
    db.execute("INSERT INTO user_campaign_missions (user_id, mission_id, state) VALUES (?, '85', 2)", (user,))
    db.commit()
    before = db.execute('SELECT COUNT(*) FROM user_campaign_missions').fetchone()[0]
    msg = raises(prep_chain.apply_clears, db, prep_chain.closure(real, 85), 85)
    check('an already-cleared target is refused and nothing is written', msg and 'already cleared' in msg
          and db.execute('SELECT COUNT(*) FROM user_campaign_missions').fetchone()[0] == before, msg)
    db.close()
    two = sqlite3.connect(WORK / 'two_accounts.sqlite')
    msg = raises(prep_chain.apply_clears, two, [1, 2], 10)
    check('a copy with two accounts is refused', msg and 'exactly one' in msg, msg)
    two.close()


def test_processes(check: Checker) -> None:
    marker = WORK / 'unrelated_config.json'
    marker.write_text('{}', encoding='utf-8')

    # Happy path: the server forks a child that listens (as gimuserverw does); a relay; a client.
    run = new_run('happy')
    sport, rport = free_port(), free_port()
    twin = unrelated('none', sport, run / 'config.json')          # same command line as the run's server
    cmark = 'client-marker-' + run.name
    launcher = [sys.executable, '-c', f'import subprocess,sys; subprocess.Popen([sys.executable, r"{WORK / "dummy.py"}", '
                                      f'"none", "0", "{cmark}"], creationflags=0x08000000)']
    rec = run_ctl.start(run, server=server_cmd(run, 'child', sport), config=run / 'config.json', server_port=sport,
                        relay=[sys.executable, str(WORK / 'dummy.py'), 'self', str(rport), 'relay-marker'],
                        relay_port=rport, relay_cmdline_has=['relay-marker'], client_launch=launcher,
                        client_match=lambda r: cmark in (r.get('cmd') or ''), listen_timeout=30, client_timeout=30)
    roles = [e['role'] for e in rec['processes']]
    server = next(e for e in rec['processes'] if e['role'] == 'server')
    child = next((e for e in rec['processes'] if e['role'] == 'server-child'), None)
    check('start records server, its listening child, relay and client with creation times and images',
          roles == ['server', 'server-child', 'relay', 'client'] and all(e['created'] for e in rec['processes'])
          and child and rec['listeners'][str(sport)] == [child['pid']], (roles, rec['listeners']))
    check('status names the listener owners as this run', all('NOT this run' not in s for s in run_ctl.status(run)))
    report, refused = run_ctl.stop(run)
    check('stop terminates exactly the recorded processes', not refused
          and not any(alive(e['pid']) for e in rec['processes']), report)
    check('the unrelated process with the same command line survives the stop', alive(twin.pid))
    report, refused = run_ctl.stop(run)
    check('stopping again only reports them exited', not refused and all('already exited' in r for r in report), report)

    # Identity tampering: a reused pid (other creation time) or another image is refused and left alone.
    for what, change in (('creation time', lambda e: e.update(created=e['created'] + 1)),
                         ('image', lambda e: e.update(image=r'C:\Windows\notepad.exe'))):
        run = new_run('tamper_' + what.replace(' ', '_'))
        port = free_port()
        rec = run_ctl.start(run, server=server_cmd(run, 'self', port), config=run / 'config.json', server_port=port,
                            listen_timeout=30)
        good = json.loads((run / q.RECORD).read_text(encoding='utf-8'))
        bad = json.loads(json.dumps(good))
        change(bad['processes'][0])
        q.save_record(run, bad)
        report, refused = run_ctl.stop(run)
        pid = good['processes'][0]['pid']
        check(f'a record whose {what} differs is REFUSED and the process left running', refused and alive(pid), report)
        q.save_record(run, good)
        report, refused = run_ctl.stop(run)
        check(f'... with the true record restored it stops ({what})', not refused and not alive(pid), report)

    # A recorded command line that the process does not carry is refused too.
    run = new_run('tamper_cmdline')
    port = free_port()
    run_ctl.start(run, server=server_cmd(run, 'self', port), config=run / 'config.json', server_port=port,
                  listen_timeout=30)
    good = q.load_record(run)
    bad = json.loads(json.dumps(good))
    bad['processes'][0]['cmdline_has'] = [str(marker)]
    q.save_record(run, bad)
    report, refused = run_ctl.stop(run)
    check('a record naming another config is REFUSED', refused and alive(good['processes'][0]['pid']), report)
    q.save_record(run, good)
    run_ctl.stop(run)

    # Failed starts clean up only their own processes.
    for mode, why in (('none', 'no listener'), ('exit', 'exited before listening')):
        run = new_run('fail_' + mode)
        port = free_port()
        twin = unrelated('none', port, run / 'config.json')
        msg = raises(run_ctl.start, run, server=server_cmd(run, mode, port), config=run / 'config.json',
                     server_port=port, listen_timeout=5)
        own = entries(run)
        check(f'a server that {why} fails the start; its own process is stopped, the twin survives',
              msg and why in msg and own and not alive(own[0]['pid']) and alive(twin.pid), (msg, own))
    run = new_run('stolen')
    port = free_port()
    thief = unrelated('late', port, marker)
    msg = raises(run_ctl.start, run, server=server_cmd(run, 'none', port), config=run / 'config.json',
                 server_port=port, listen_timeout=15)
    own = entries(run)
    check('a listener owned by another process is refused; only the run\'s server is stopped',
          msg and 'not by this run' in msg and not alive(own[0]['pid']) and alive(thief.pid), msg)
    run = new_run('busy')
    port = free_port()
    holder = unrelated('self', port, marker)
    time.sleep(1.5)
    msg = raises(run_ctl.start, run, server=server_cmd(run, 'self', port), config=run / 'config.json',
                 server_port=port, listen_timeout=5)
    check('a port that already has a listener refuses the start before anything is launched',
          msg and 'already have listeners' in msg and not (run / q.RECORD).exists() and alive(holder.pid), msg)


def test_links(check: Checker) -> None:
    """Round-3 QC P2: links ABOVE the run (at visual or saves) and links INSIDE it (database, config, logs,
    record).  Every link target is a disposable folder or file inside WORK, and is checked unchanged."""
    args = lambda qa, run: ['--qa-dir', str(qa), '--run', run, '--save', str(COMPLETED), '--content-root',
                            str(CONTENT), '--system-root', str(SYSTEM)]
    for where in ('visual', 'saves'):
        qa = WORK / f'jqa_{where}'
        away = WORK / f'away_{where}'
        (away / 'saves' if where == 'visual' else away).mkdir(parents=True)
        (qa if where == 'visual' else qa / 'visual').mkdir(parents=True)
        link = qa / 'visual' if where == 'visual' else qa / 'visual' / 'saves'
        if not junction(link, away):
            print(f'SKIP  junction at {where}: mklink /J failed')
            continue
        msg = raises(q.run_dir, qa, 'r1', must_exist=False)
        check(f'a junction at {where} is refused by run_dir (the resolved saves root must be <qa>/visual/saves)',
              msg and 'link' in msg, msg)
        code = prep_run.main(args(qa, 'r1'))
        check(f'prep_run through a junction at {where} exits 2 and writes nothing at the junction\'s target',
              code == 2 and not any(away.rglob('*.sqlite')) and not any(away.rglob('config.json')), code)
        unjunction(link)

    other = WORK / 'other_save.sqlite'
    other.write_bytes(b'other save bytes')
    other_sha = hashlib.sha256(other.read_bytes()).hexdigest()

    def refused_start(run: Path, expect: str, what: str) -> None:
        port = free_port()
        msg = raises(run_ctl.start, run, server=server_cmd(run, 'self', port), config=run / 'config.json',
                     server_port=port, listen_timeout=5)
        check(f'{what}: start refused before writing or launching anything',
              msg and expect in msg and not (run / q.RECORD).exists() and not (run / 'server.log').exists(), msg)

    run = new_run('redir_db')
    write_config(run / 'config.json', other)
    refused_start(run, 'names database', 'a config naming another save')
    run = new_run('redir_live')
    write_config(run / 'config.json', q.LIVE_SAVE)
    refused_start(run, 'names database', 'a config naming the live save')
    run = new_run('hl_db')
    (run / 'gme.sqlite').unlink()
    os.link(other, run / 'gme.sqlite')
    refused_start(run, 'hard links', 'a run database hard-linked to another save')
    code = prep_chain.main(['--qa-dir', str(WORK), '--run', 'hl_db', '--target', '85'])
    check('prep_chain refuses to write a hard-linked run database (exit 2)', code == 2, code)
    run = new_run('hl_cfg')
    twin = WORK / 'cfg_twin.json'
    write_config(twin, run / 'gme.sqlite')
    (run / 'config.json').unlink()
    os.link(twin, run / 'config.json')
    refused_start(run, 'hard links', 'a config hard-linked to a file outside the run')
    run = new_run('hl_log')
    os.link(other, run / 'server.log')
    port = free_port()
    msg = raises(run_ctl.start, run, server=server_cmd(run, 'self', port), config=run / 'config.json',
                 server_port=port, listen_timeout=5)
    check('a server log hard-linked to another file: start refused before launching',
          msg and 'hard links' in msg and not (run / q.RECORD).exists(), msg)
    run = new_run('hl_record')
    os.link(other, run / (q.RECORD + '.tmp'))
    msg = raises(q.save_record, run, {'format': q.FORMAT})
    check('save_record refuses a hard-linked temporary record', msg and 'hard links' in msg, msg)
    run = new_run('sym_db')
    (run / 'gme.sqlite').unlink()
    try:
        os.symlink(other, run / 'gme.sqlite')
    except OSError as ex:
        print(f'SKIP  symlink database: {ex}')
    else:
        refused_start(run, 'is a link', 'a run database that is a symlink to another save')
    check('every link target outside the runs is unchanged',
          hashlib.sha256(other.read_bytes()).hexdigest() == other_sha and json.loads(twin.read_text())
          ['db_clients'][0]['filename'].endswith('hl_cfg/gme.sqlite'))
    check('run_file refuses names with separators, drive colons or dots',
          all(raises(q.run_file, run, n) for n in ('a/b', 'a\\b', 'C:x', '..', '.', '')))


def test_db_probe(check: Checker) -> None:
    db = WORK / 'probe.sqlite'
    sqlite3.connect(db).close()
    check('db_probe: a closed database is free', q.db_probe(db) == 'free')
    con = sqlite3.connect(db)
    con.execute('CREATE TABLE t (x)')
    check('db_probe: an open connection (shared read/write, as SQLite opens it) reads as held',
          q.db_probe(db) == 'held')
    con.close()
    check('db_probe: a missing file is reported missing', q.db_probe(WORK / 'nope.sqlite') == 'missing')


def main(argv=None) -> int:
    global WORK
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--work', default=str(WORK),
                    help='a NEW folder below out/qa/current for this run (default qa_helpers_selftest, recreated)')
    a = ap.parse_args(argv)
    WORK = q.qa_dir(a.work)
    check = Checker()
    if WORK.exists():
        shutil.rmtree(WORK)     # removes junctions as links, never their targets (Python 3.8+ on Windows)
    q.saves_root(WORK).mkdir(parents=True)
    (WORK / 'dummy.py').write_text(DUMMY, encoding='utf-8')
    try:
        test_names(check)
        test_prep(check)
        test_chain(check)
        test_db_probe(check)
        test_links(check)
        test_processes(check)
    finally:
        for p in spawned:
            p.kill()
        for run in q.saves_root(WORK).iterdir():
            if (run / q.RECORD).is_file():
                try:
                    run_ctl.stop(run)
                except q.QaError:
                    pass
    return check.summary('test_qa_run')


if __name__ == '__main__':
    raise SystemExit(main())
