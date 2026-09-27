"""Smoke-test a built Debug server with a new isolated save, without game assets.

Usage: python scripts/test_debug_startup.py PATH_TO_DEBUG_EXE
Requires Windows and the shared developer console (enabled by default).
"""
import json
from pathlib import Path
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
exe = Path(sys.argv[1]).resolve()
assert exe.is_file(), exe
assert exe.with_suffix('.pdb').is_file(), 'Debug symbols are missing'
(ROOT / 'out').mkdir(exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='debug-startup-', dir=ROOT / 'out'))
with socket.socket() as s:
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
config = json.loads((ROOT / 'packaging/config.json').read_text(encoding='utf-8'))
config['listeners'][0]['port'] = port
config['db_clients'][0]['filename'] = (work / 'gme.sqlite').as_posix()
settings = config['plugins'][0]['config']['server']
for folder in ('mst', 'system', 'archive'):
    settings[folder + '_root'] = (ROOT / 'deploy' / folder).as_posix()
(work / 'config.json').write_text(json.dumps(config), encoding='utf-8')
with (work / 'server.log').open('w', encoding='utf-8') as log:
    p = subprocess.Popen([str(exe), str(work / 'config.json')],
                         stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                         creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        for _ in range(120):
            assert p.poll() is None, f'Server exited; see {work}/server.log'
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{port}/offline_mod/fps_cap', timeout=1) as r:
                    assert r.status == 200
                break
            except OSError:
                time.sleep(.5)
        else:
            raise RuntimeError(f'Server did not start; see {work}/server.log')
        result = subprocess.run([str(exe), '--debug-cli', rf'\\.\pipe\gimudebug_{p.pid}'],
                                input='cap\n', capture_output=True, text=True,
                                encoding='utf-8', timeout=20,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        assert result.returncode == 0, result.stderr
        assert 'No user exists yet' in result.stdout, result.stdout
        with sqlite3.connect(work / 'gme.sqlite') as db:
            assert db.execute('SELECT COUNT(*) FROM user_info').fetchone()[0] == 0
            db.execute('SELECT unit_lvl, bb_lvl, sbb_lvl FROM user_units LIMIT 1').fetchall()
        print('PASS: Debug symbols, HTTP startup, fresh database migration and developer console')
        print(f'Isolated log/save: {work}')
    finally:
        p.terminate()
        p.wait(timeout=15)
