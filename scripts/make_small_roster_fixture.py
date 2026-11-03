"""Make a one-unit roster fixture with the server's own clearunits, validated as the client sees it.

    python scripts/make_small_roster_fixture.py SERVER_EXE SOURCE_SAVE OUT_SAVE [--port 19978]

SOURCE_SAVE is copied with the backup API (it is only read); OUT_SAVE must not
exist (an existing fixture is evidence, never overwritten).  An isolated server
on a scratch copy runs `clearunits` through its debug console, then UserInfo is
read back and checked with bf_testkit.roster_problems -- every squad slot names
an owned unit, one leader per squad, the active squad led by an owned unit --
and the database with dangling_unit_references.  Only a fixture that passes is
written to OUT_SAVE; a JSON manifest beside it records what the client will
receive (units, squads, warehouse rows against the cap).

Session 11's small-roster fixture passed a login and still crashed Manage Squad:
a successful HTTP reply is not validation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
import sys
from contextlib import closing
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import (Client, DebugConsole, Fixture, IsolatedServer, backup_copy,  # noqa: E402
                        dangling_unit_references, qa_dir, roster_problems)


def sha256_file(path: Path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('source')
    ap.add_argument('out')
    ap.add_argument('--port', type=int, default=19978)
    args = ap.parse_args()
    out = Path(args.out).resolve()
    if out.exists():
        print(f'refusing: {out} exists (fixtures are evidence; choose a new name)')
        return 1
    fx = Fixture.create(qa_dir('small_roster_fixture'), port=args.port, source=Path(args.source))
    with closing(sqlite3.connect(fx.db_path)) as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    with IsolatedServer(args.exe, fx) as server, DebugConsole(server) as console:
        said = console.run('clearunits').strip()
        client = Client(fx, user)
        reply = client.call('UserInfo')
        client.call('nJ3A7qFp', 'bGxX67KB', {})      # BadgeInfo: after every earlier commit
    problems = roster_problems(reply)
    with closing(sqlite3.connect(fx.db_path)) as db:
        dangling = dangling_unit_references(db, user)
        cap = db.execute('SELECT max_warehouse_count FROM user_info WHERE id=?', (user,)).fetchone()[0]
    live_rows = sum(1 for w in reply.get('9wjrh74P', []) if int(w.get('wgV86x1q', 0)) > 0)
    manifest = {
        'source': str(Path(args.source).resolve()), 'server': str(Path(args.exe).resolve()),
        'server_sha256': sha256_file(Path(args.exe)), 'user': user, 'console': said,
        'units': [{'user_unit_id': u.get('edy7fq3L'), 'unit_id': u.get('pn16CNah')} for u in reply.get('4ceMWH6k', [])],
        'squads': sorted(([s.get('U9ABSYEp'), s.get('zsiAn9P1'), s.get('XuJL4pc5'), s.get('gr48vsdJ'), s.get('edy7fq3L')]
                          for s in reply.get('dX7S2Lc1', [])), key=lambda r: [int(x) for x in r]),
        'active_squad': (reply.get('fEi17cnx') or [{}])[0].get('Z0Y4RoD7'),
        'warehouse_live_rows': live_rows, 'warehouse_cap': cap,
        'roster_problems': problems, 'dangling': dangling,
    }
    print(json.dumps({k: v for k, v in manifest.items() if k != 'squads'}, indent=1))
    if problems or dangling or len(manifest['units']) != 1 or live_rows > cap:
        print('NOT written: the scratch result is not a valid one-unit roster')
        return 1
    backup_copy(fx.db_path, out)
    manifest['fixture_sha256'] = sha256_file(out)
    out.with_suffix('.json').write_text(json.dumps(manifest, indent=1), encoding='utf-8')
    print(f'fixture {out} ({manifest["fixture_sha256"][:16]}), manifest {out.with_suffix(".json").name}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
