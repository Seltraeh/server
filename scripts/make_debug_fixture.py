"""Make a client fixture by running debug-console commands on a scratch copy, validated as the client sees it.

    python scripts/make_debug_fixture.py SERVER_EXE SOURCE_SAVE OUT_SAVE --command "gems 50" [--command ...]
                                         [--sql "UPDATE ..." ...] [--port 19979]

SOURCE_SAVE is copied with the backup API (only read); OUT_SAVE must not exist
(a fixture is evidence, never overwritten).  Each --sql statement is applied to
the scratch copy first (test state the console has no command for, such as a
known harvest list); an isolated server on the copy then runs each --command
through its debug console (DebugCli.cpp) and reads UserInfo back: the fixture
is written only when every squad slot names an owned unit
(bf_testkit.roster_problems), no table names a deleted unit
(dangling_unit_references) and the commands' own output reports no usage
error.  A JSON manifest beside OUT_SAVE records the statements and commands,
what each said, and the units, gems, capacity, energy and Town harvest rows the
client will receive.

Built for Session 13's client check (effects need a summon, a summon needs
gems and free unit slots); scripts/make_small_roster_fixture.py is the
clearunits-specific sibling.
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
    ap.add_argument('--command', action='append', default=[], help='a debug-console command, e.g. "gems 50"')
    ap.add_argument('--sql', action='append', default=[],
                    help='a statement applied to the scratch copy before the server starts')
    ap.add_argument('--port', type=int, default=19979)
    args = ap.parse_args()
    if not args.command and not args.sql:
        ap.error('give at least one --command or --sql')
    out = Path(args.out).resolve()
    if out.exists():
        print(f'refusing: {out} exists (fixtures are evidence; choose a new name)')
        return 1
    fx = Fixture.create(qa_dir('debug_fixture'), port=args.port, source=Path(args.source))
    applied = []
    with closing(sqlite3.connect(fx.db_path)) as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        for statement in args.sql:
            applied.append({'sql': statement, 'rows': db.execute(statement).rowcount})
        db.commit()
    said = []
    with IsolatedServer(args.exe, fx) as server, DebugConsole(server) as console:
        for command in args.command:
            said.append({'command': command, 'output': console.run(command).strip()})
        client = Client(fx, user)
        reply = client.call('UserInfo')
        client.call('nJ3A7qFp', 'bGxX67KB', {})      # BadgeInfo: answered after every earlier commit
        cap = console.run('cap').strip()
    problems = roster_problems(reply)
    with closing(sqlite3.connect(fx.db_path)) as db:
        dangling = dangling_unit_references(db, user)
        gems, level = db.execute('SELECT gems, level FROM user_info WHERE id=?', (user,)).fetchone()
        units = db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=?', (user,)).fetchone()[0]
    team = (reply.get('fEi17cnx') or [{}])[0]
    manifest = {
        'source': str(Path(args.source).resolve()), 'source_sha256': sha256_file(Path(args.source)),
        'server': str(Path(args.exe).resolve()), 'server_sha256': sha256_file(Path(args.exe)),
        'user': user, 'sql': applied, 'commands': said, 'capacity': cap,
        'units': units, 'units_in_reply': len(reply.get('4ceMWH6k', [])), 'gems': gems, 'level': level,
        'energy_in_reply': team.get('0P9X1YHs'), 'max_energy_in_reply': team.get('YnM14RIP'),
        # What the Town will replay (MyTownTopScene::collectItem reads element count - tap_cnt).
        'town_in_reply': [{'location': r.get('un80kW9Y'), 'taps': r.get('mDaE3t6A'), 'drops': r.get('eip3MS5L')}
                          for r in reply.get('s8TCo2MS', [])],
        'roster_problems': problems, 'dangling': dangling,
    }
    print(json.dumps(manifest, indent=1))
    usage_error = [s for s in said if 'Usage:' in s['output'] or 'unknown' in s['output'].lower()]
    if problems or dangling or usage_error or units != manifest['units_in_reply']:
        print('NOT written: the scratch result is not a valid fixture')
        return 1
    backup_copy(fx.db_path, out)
    manifest['fixture_sha256'] = sha256_file(out)
    out.with_suffix('.json').write_text(json.dumps(manifest, indent=1), encoding='utf-8')
    print(f'fixture {out} ({manifest["fixture_sha256"][:16]}), manifest {out.with_suffix(".json").name}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
