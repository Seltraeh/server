"""Validate a save for a player's client run, on scratch copies, and write the validated copy.

    python scripts/check_client_fixture.py SERVER_EXE SOURCE_SAVE [--out NEW_FIXTURE.sqlite]
                                           [--mission 10] [--port 19981]

SOURCE_SAVE is only read (backup API).  An isolated server on a scratch copy answers what a
client asks on a first launch and a first quest, and every check below must pass:

  launch   UserInfo: every squad slot names an owned unit, one leader each, the active squad is
           led by an owned unit (bf_testkit.roster_problems); no table names a deleted unit
           (dangling_unit_references); units within the unit cap; the item box within its cap
           (warehouse rows holding items -- the live save's 485 of 100 made Session 11's crafting
           kit unusable); the currencies the run reads.
  helpers  FriendGet (the pre-battle "choose a helper" request): the picker list (xZH6EIQ7) is
           not empty, every helper's unit exists in deploy/mst/unit_mst.json and its user unit id
           is unique; the Social list (tojMy68W) is not empty.
  quest    MissionStart of --mission with the active squad and the first helper, then a cleared
           MissionEnd echoing the battle serial: both answer without an error, the energy falls by
           the mission's cost, the helper is recorded and consumed, nothing dangles afterwards.
  relaunch the server restarted on the same scratch copy answers UserInfo with the post-quest
           energy, Zel and roster (what the client sees on a relaunch).

With --out, and only when everything passed, NEW_FIXTURE is written as a backup-API copy of
SOURCE_SAVE itself (the quest ran on a scratch copy: the fixture is the untouched save), with a
JSON manifest beside it recording every value checked.  NEW_FIXTURE must not exist (fixtures are
evidence, never overwritten).
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
from bf_testkit import (ROOT, Checker, Client, DebugConsole, Fixture, IsolatedServer, backup_copy,  # noqa: E402
                        dangling_unit_references, mission_end_body, mission_start_body, qa_dir, roster_problems,
                        started_serial)

FRIEND_GET = ('2o4axPIC', 'EoYuZ2nbImhCU1c0')
BADGE_INFO = ('nJ3A7qFp', 'bGxX67KB')
MST = ROOT / 'deploy' / 'mst'


def sha256_file(path: Path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_mst(name: str) -> list:
    text = (MST / name).read_text(encoding='utf-8')
    data = json.loads('\n'.join(l for l in text.splitlines() if not l.lstrip().startswith('//')))
    return data if isinstance(data, list) else next(v for v in data.values() if isinstance(v, list))


def team(reply: dict) -> dict:
    return (reply.get('fEi17cnx') or [{}])[0]


def state(db: sqlite3.Connection, user: str) -> dict:
    row = db.execute('SELECT level, energy, gems, zel, karma, friend_points, max_unit_count, max_warehouse_count,'
                     ' reinforce_user_id, open_mission_id FROM user_info WHERE id=?', (user,)).fetchone()
    keys = ('level', 'energy', 'gems', 'zel', 'karma', 'friend_points', 'max_unit_count', 'max_warehouse_count',
            'reinforce_user_id', 'open_mission_id')
    out = dict(zip(keys, row))
    out['units'] = db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=?', (user,)).fetchone()[0]
    out['item_box_rows'] = db.execute('SELECT COUNT(*) FROM user_warehouse_rows WHERE user_id=? AND item_num>0',
                                      (user,)).fetchone()[0]
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('exe', type=Path)
    ap.add_argument('source', type=Path)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--mission', type=int, default=10)
    ap.add_argument('--port', type=int, default=19981)
    args = ap.parse_args()
    out = args.out.resolve() if args.out else None
    if out and out.exists():
        print(f'refusing: {out} exists (fixtures are evidence; choose a new name)')
        return 1
    source_sha = sha256_file(args.source)
    check = Checker()
    units = {r['pn16CNah'] for r in load_mst('unit_mst.json')}
    mission = next((r for r in load_mst('mission_mst.json') if r['j28VNcUW'] == str(args.mission)), None)
    if mission is None:
        print(f'refusing: mission {args.mission} is not in deploy/mst/mission_mst.json')
        return 1
    cost = int(mission.get('A8DEK5ob') or 0)
    record = {'source': str(args.source.resolve()), 'source_sha256': source_sha,
              'server': str(args.exe.resolve()), 'server_sha256': sha256_file(args.exe),
              'mission': {'id': args.mission, 'name': mission.get('0iAIR2LP'), 'energy_cost': cost}}

    fx = Fixture.create(qa_dir('client_fixture_check'), port=args.port, source=args.source)
    with closing(sqlite3.connect(fx.db_path)) as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    with IsolatedServer(args.exe, fx) as server, DebugConsole(server) as console:
        client = Client(fx, user)
        info = client.call('UserInfo')
        problems = roster_problems(info)
        with closing(sqlite3.connect(fx.db_path)) as db:
            before = state(db, user)
            dangling = dangling_unit_references(db, user)
        cap = console.run('cap').strip()
        t = team(info)
        record['launch'] = dict(before, unit_cap=cap, roster_problems=problems, dangling=dangling,
                                reply={'level': t.get('D9wXQI2V'), 'energy': t.get('0P9X1YHs'),
                                       'max_energy': t.get('YnM14RIP'), 'zel': t.get('Najhr8m6'),
                                       'karma': t.get('HTVh8a65'), 'free_gems': t.get('92uj7oXB'),
                                       'paid_gems': t.get('d37CaiX1'), 'units': len(info.get('4ceMWH6k', [])),
                                       'squad_slots': len(info.get('dX7S2Lc1', []))})
        check('launch: the roster and every squad are drawable (roster_problems)', not problems, problems)
        check('launch: no table names a deleted unit', not dangling, dangling)
        check(f'launch: units within the cap ({cap})', 'free' in cap and before['units'] == len(info.get('4ceMWH6k', [])),
              (cap, before['units']))
        check(f'launch: the item box within its cap ({before["item_box_rows"]} of {before["max_warehouse_count"]})',
              before['item_box_rows'] <= before['max_warehouse_count'], before)
        check(f'launch: energy for the quest ({before["energy"]} >= {cost}) and the reply agrees',
              before['energy'] >= cost and str(t.get('0P9X1YHs')) == str(before['energy']), (before, t.get('0P9X1YHs')))
        check('launch: Zel, karma and gems are readable non-negative numbers',
              all(int(before[k]) >= 0 for k in ('zel', 'karma', 'gems', 'friend_points')), before)

        friends = client.call(*FRIEND_GET, {})
        picker = friends.get('xZH6EIQ7', [])
        social = friends.get('tojMy68W', [])
        helper_units = [str(h.get('pn16CNah')) for h in picker]
        unknown = [u for u in helper_units if u not in units]
        slots = [str(h.get('edy7fq3L')) for h in picker]
        record['helpers'] = {'picker': [{'user': h.get('h7eY3sAK'), 'name': h.get('B5JQyV8j'),
                                         'unit': h.get('pn16CNah'), 'friend_type': h.get('q9Arz8Jv')} for h in picker],
                             'social': len(social)}
        check(f'helpers: FriendGet offers a helper picker ({len(picker)} helpers)', bool(picker) and 'error' not in friends,
              friends.get('error'))
        check('helpers: every helper unit exists in the unit MST', bool(helper_units) and not unknown, unknown)
        check('helpers: every helper has its own user unit id', len(set(slots)) == len(slots), slots)
        check(f'helpers: the Social list is populated ({len(social)})', bool(social))

        helper = str(picker[0].get('h7eY3sAK')) if picker else '0'
        deck = int(t.get('Z0Y4RoD7', 0) or 0)
        body = mission_start_body(args.mission, deck=deck)
        body['9Q1Lq5FS'][0]['h7eY3sAK'] = helper
        start = client.call('MissionStart', body=body)
        with closing(sqlite3.connect(fx.db_path)) as db:
            started = state(db, user)
        check(f'quest: MissionStart {args.mission} ({mission.get("0iAIR2LP")}) with helper {helper} answers',
              'error' not in start and 'Kz7qfSs5' in start, start.get('error'))
        serial = started_serial(start) if 'Kz7qfSs5' in start else 0
        check(f'quest: the start spent the mission\'s {cost} energy and recorded the helper',
              started['energy'] == before['energy'] - cost and started['reinforce_user_id'] == helper,
              (before['energy'], started['energy'], started['reinforce_user_id']))
        end = client.call('MissionEnd', body=mission_end_body(args.mission, status=2, zel=100, karma=10, serial=serial))
        client.call(*BADGE_INFO, {})          # answered after every earlier commit
        with closing(sqlite3.connect(fx.db_path)) as db:
            after = state(db, user)
            dangling_after = dangling_unit_references(db, user)
        check('quest: the cleared MissionEnd answers', 'error' not in end, end.get('error'))
        check('quest: the result paid Zel and consumed the helper; nothing dangles',
              after['zel'] > before['zel'] and after['reinforce_user_id'] == '' and not dangling_after,
              (before['zel'], after['zel'], after['reinforce_user_id'], dangling_after))
        record['quest'] = {'serial': serial, 'helper': helper, 'started': started, 'after': after}

    with IsolatedServer(args.exe, fx, log_name='server_relaunch.log'):
        again = Client(fx, user).call('UserInfo')
    t2 = team(again)
    record['relaunch'] = {'energy': t2.get('0P9X1YHs'), 'zel': t2.get('Najhr8m6'),
                          'units': len(again.get('4ceMWH6k', [])), 'roster_problems': roster_problems(again)}
    check('relaunch: the restarted server shows the post-quest energy and Zel, and a drawable roster',
          str(t2.get('0P9X1YHs')) == str(after['energy']) and str(t2.get('Najhr8m6')) == str(after['zel'])
          and not roster_problems(again), record['relaunch'])
    check('the source save is byte-identical', sha256_file(args.source) == source_sha)

    code = check.summary('check_client_fixture')
    if out is None:
        print(json.dumps(record, indent=1))
        return code
    if code:
        print(f'NOT written: {out}')
        return code
    out.parent.mkdir(parents=True, exist_ok=True)
    backup_copy(args.source, out)
    record['fixture'] = str(out)
    record['fixture_sha256'] = sha256_file(out)
    out.with_suffix('.json').write_text(json.dumps(record, indent=1), encoding='utf-8')
    print(f'fixture {out} ({record["fixture_sha256"][:16]}), manifest {out.with_suffix(".json").name}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
