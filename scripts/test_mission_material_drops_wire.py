"""#26 §3 (round 8): per-hit material drops -- MissionStart's BattleMonsterGroupMst i30R8TAs and MissionEnd's award.

python scripts/test_mission_material_drops_wire.py PATH_TO_DEBUG_EXE [--port 19987]

Client contract (arm64 libgame.so; x86 2.19.6.0 reads the same key, reference at 0x4CEA55):
BattleMonsterGroupMstResponse::readParam 0x13CEDA4 stores i30R8TAs with setItemDrop; MonsterParty::setMaterialDrop
0x10C2354 splits it on ',' then ':' into BattleUnitDropMaterial {dropPer = StrToFloat, itemID, dropRest = StrToInt}.
Each hit rolls BattleUnit::getDropMaterialPer 0x111F7B0 (a percent).  What dropped comes back in MissionEnd's item
list (4T0Q2Bh5, "itemId:count,...") and is credited.  Live-format captures carry e.g. "7:10000:3,7:10300:3".

A disposable archive built here (hard links to deploy/archive; mission.json with every generated list stripped and
mission 10 authored, so this tests the channel whatever content deploy carries) checks:
  - one drop, many drops (authored order, a fractional percent), no field, an empty list: the key is sent only
    when there are drops, exactly "per:itemId:maxCount" joined by ',', between 3g8PW6x0 and hw3L0uVj;
  - five invalid lists (per 0 / 101, item 0, max_count 0 / 100; one mixes a valid entry) are refused whole at
    load and logged, and the mission still starts;
  - production missions send no i30R8TAs (the wire is unchanged where nothing is authored);
  - MissionEnd credits zero, one and many reported items; a repeated settlement adds nothing; a malformed or
    unknown item report is refused with nothing credited.
"""
import argparse
import json
import os
import shutil
import sys
from pathlib import Path

from bf_testkit import (ROOT, Fixture, IsolatedServer, Client, Checker, mission_start_body, mission_end_body,
                        started_serial)
from bf_testkit import qa_dir  # noqa: E402

MISSION = 10
ONE = [{'per': 100, 'item_id': 10603, 'max_count': 2}]
MANY = [{'per': 7, 'item_id': 10000, 'max_count': 3}, {'per': 7, 'item_id': 10300, 'max_count': 3},
        {'per': 2.5, 'item_id': 10301, 'max_count': 5}]
INVALID = [
    [{'per': 50, 'item_id': 10000, 'max_count': 1}, {'per': 0, 'item_id': 10300, 'max_count': 1}],
    [{'per': 101, 'item_id': 10000, 'max_count': 1}],
    [{'per': 50, 'item_id': 0, 'max_count': 1}],
]
INVALID_STAGE3 = [[{'per': 50, 'item_id': 10000, 'max_count': 0}], [{'per': 50, 'item_id': 10000, 'max_count': 100}]]
# What the client's parse must keep, per (wave, order); None = no key.
EXPECT = {(1, 0): '100:10603:2', (1, 1): '7:10000:3,7:10300:3,2.5:10301:5', (1, 2): None, (1, 3): None}
ROW_KEYS = ['Qzhp8B40', 'o49dYfpH', 'hZtF1s8B', '3g8PW6x0', 'hw3L0uVj']


def build_archive(folder: Path) -> Path:
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    src = ROOT / 'deploy' / 'archive'
    rows = json.loads((src / 'mission.json').read_text(encoding='utf-8'))
    for row in rows:   # the content (scripts/gen_material_drops.py) has its own test
        for stage in row['stages']:
            for monster in stage['battle_monsters']:
                monster.pop('material_drops', None)
    stages = next(r for r in rows if r['id'] == MISSION)['stages']
    assert [len(s['battle_monsters']) for s in stages][:3] == [4, 3, 3], 'mission 10 waves changed'
    w1, w2, w3 = (s['battle_monsters'] for s in stages[:3])
    w1[0]['material_drops'] = ONE
    w1[1]['material_drops'] = MANY
    w1[3]['material_drops'] = []
    for monster, drops in zip(w2, INVALID):
        monster['material_drops'] = drops
    for monster, drops in zip(w3, INVALID_STAGE3):
        monster['material_drops'] = drops
    for p in src.iterdir():
        if p.is_file() and p.name != 'mission.json':
            os.link(p, folder / p.name)
    (folder / 'mission.json').write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding='utf-8')
    return folder


def client_parse(field: str | None) -> list[tuple[float, str, int]]:
    """MonsterParty::setMaterialDrop: ',' entries, each ':' into (StrToFloat, item id, StrToInt); none when absent."""
    out = []
    for entry in field.split(',') if field else []:
        per, item, rest = entry.split(':')
        out.append((float(per), item, int(rest)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19987)
    args = ap.parse_args()
    archive = build_archive(qa_dir('mission_material_drops_archive'))
    fx = Fixture.create(qa_dir('mission_material_drops'), port=args.port, archive_root=archive)
    check = Checker()
    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx)
        db = fx.db()
        db.execute('UPDATE user_info SET level=50, exp=0, energy=20000, energy_full_ts=0 WHERE id=?', (c.user,))
        db.commit()

        def items() -> dict[int, int]:
            return {r[0]: r[1] for r in db.execute(
                'SELECT item_id, SUM(item_num) FROM user_items WHERE user_id=? GROUP BY item_id', (c.user,))}

        log = server.log_text()
        refused = log.count(f'Mission {MISSION} monster ')
        check('load: the five invalid lists are refused whole, each logged, and counted',
              refused == 5 and 'with per-hit material drops; 5 invalid list(s) refused' in log, refused)
        check('load: two monsters keep their material drops (the empty list counts as none)',
              ' 2 monster(s) with per-hit material drops' in log)

        # ---- the wire ------------------------------------------------------------------------------------
        reply = c.call('MissionStart', body=mission_start_body(MISSION))
        check('mission 10 starts with the refused lists stripped', 'error' not in reply, reply.get('error'))
        rows = reply.get('75t0sx9z', [])
        sent = {(int(r['Qzhp8B40']), int(r['hZtF1s8B'])): r.get('i30R8TAs') for r in rows}
        for at, expect in EXPECT.items():
            check(f'wave {at[0]} slot {at[1]}: i30R8TAs {expect!r}', sent.get(at) == expect, sent.get(at))
        check('waves 2-5 (invalid or nothing authored) send no i30R8TAs',
              all(v is None for (w, _), v in sent.items() if w >= 2), {k: v for k, v in sent.items() if v})
        check('the client parse keeps exactly the authored entries, in order (2.5 stays a fraction)',
              client_parse(sent.get((1, 1))) == [(7.0, '10000', 3), (7.0, '10300', 3), (2.5, '10301', 5)]
              and client_parse(sent.get((1, 0))) == [(100.0, '10603', 2)], sent.get((1, 1)))
        orders = [list(r) for r in rows if 'i30R8TAs' in r]
        check('a row with drops orders its keys as the captures do: ...3g8PW6x0, i30R8TAs, hw3L0uVj',
              orders and all(o == ROW_KEYS[:4] + ['i30R8TAs'] + ROW_KEYS[4:] for o in orders), orders[:1])
        check('every row without drops keeps exactly the five keys it always had',
              all(list(r) == ROW_KEYS for r in rows if 'i30R8TAs' not in r), [list(r) for r in rows][:1])
        serial = started_serial(reply) if 'error' not in reply else 0

        # ---- production missions are unchanged -----------------------------------------------------------
        prod = {}
        for mission in (11, 12, 100600, 100601):
            r = c.call('MissionStart', body=mission_start_body(mission))
            prod[mission] = sum('i30R8TAs' in g for g in r.get('75t0sx9z', [])) if 'error' not in r else r['error']
            if 'error' not in r:
                c.call('MissionEnd', body=mission_end_body(mission, serial=started_serial(r)))
        check('missions with nothing authored (11, 12, 100600, 100601) send no i30R8TAs',
              all(v == 0 for v in prod.values()), prod)

        # ---- the award -----------------------------------------------------------------------------------
        if not serial:
            check('the award checks need mission 10 to start (it did not)', False, reply.get('error'))
            db.close()
            return check.summary('mission material drops')
        before = items()
        end = c.call('MissionEnd', body=mission_end_body(MISSION, serial=serial, items=''))
        check('zero items: the clear settles and no item changes', 'error' not in end and items() == before,
              end.get('error'))
        r = c.call('MissionStart', body=mission_start_body(MISSION))
        before = items()
        end = c.call('MissionEnd', body=mission_end_body(MISSION, serial=started_serial(r), items='10603:2'))
        after = items()
        check('one item: 10603 x2 is credited', 'error' not in end and after.get(10603, 0) - before.get(10603, 0) == 2
              and {k: v for k, v in after.items() if k != 10603} == {k: v for k, v in before.items() if k != 10603},
              (end.get('error'), before.get(10603), after.get(10603)))
        r = c.call('MissionStart', body=mission_start_body(MISSION))
        serial = started_serial(r)
        before = items()
        claim = '10000:3,10300:3,10301:1'
        end = c.call('MissionEnd', body=mission_end_body(MISSION, serial=serial, items=claim))
        after = items()
        delta = {k: after.get(k, 0) - before.get(k, 0) for k in set(after) | set(before) if after.get(k, 0) != before.get(k, 0)}
        check('many items: each reported item is credited exactly', 'error' not in end
              and delta == {10000: 3, 10300: 3, 10301: 1}, (end.get('error'), delta))
        c.call('MissionEnd', body=mission_end_body(MISSION, serial=serial, items=claim))
        check('a repeated settlement of the same battle credits nothing', items() == after)
        for bad in ('10603', '10603:2,', '999999:1'):
            r = c.call('MissionStart', body=mission_start_body(MISSION))
            before = items()
            end = c.call('MissionEnd', body=mission_end_body(MISSION, serial=started_serial(r), items=bad))
            check(f'a malformed or unknown report {bad!r} is refused with nothing credited',
                  'error' in end and items() == before, (end.get('error'), bad))
        db.close()
    return check.summary('mission material drops')


if __name__ == '__main__':
    sys.exit(main())
