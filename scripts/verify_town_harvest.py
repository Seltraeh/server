"""Did a client run receive exactly the Town harvest its fixture planted?  Read only.

    python scripts/verify_town_harvest.py FIXTURE_SAVE FINAL_SAVE

FIXTURE_SAVE is the save the run was seeded from (it holds the pre-claim
balances and the pre-rolled lists, UserTownLocationDetail "<item>:<zel>:<karma>"
per tap); FINAL_SAVE is the run's save afterwards (copy it first with the
SQLite backup API if a server may still have it open).  For every plot whose
taps were spent, the entries it consumed are the first (planted - remaining)
ones -- the client replays entry `count - tap_cnt` per tap
(MyTownTopScene::collectItem @0x18F244C) and TownUpdate pays the same indexes.
Zel and Karma from other sources are taken from the save's own counters
(user_team_archive zel_get / karma_get / karma_use), so a quest or an upgrade
in the same run does not break the comparison; Town credits do not touch them.

Prints, per plot, what each consumed tap should have shown (item icon / Zel /
Karma / nothing) and then the totals; exits 0 only when items, Zel, Karma and
the harvest counter all match.  A plot re-rolled during the run (period
expired, or upgraded with taps left) is reported and not judged.
"""
from __future__ import annotations

import json
import sqlite3
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NAMES = {1: 'Mountain', 2: 'River', 3: 'Farm', 4: 'Forest'}


def item_names() -> dict[int, str]:
    try:
        rows = next(iter(json.loads((ROOT / 'deploy/mst/item_mst.json').read_text(encoding='utf-8')).values()))
        return {int(r['kixHbe54']): r['c7Z6xDB2'] for r in rows}
    except (OSError, ValueError, KeyError, StopIteration):
        return {}


def ro(path: str) -> sqlite3.Connection:
    return sqlite3.connect(f'file:{Path(path).resolve().as_posix()}?mode=ro', uri=True)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    a, b = ro(sys.argv[1]), ro(sys.argv[2])
    users_a = a.execute('SELECT id FROM user_info').fetchall()
    users_b = b.execute('SELECT id FROM user_info').fetchall()
    if len(users_a) != 1 or users_a != users_b:
        print('INCONCLUSIVE: requires the same single-user fixture in both saves')
        return 2
    names = item_names()
    before = {r[0]: r[1:] for r in a.execute('SELECT location_id, tap_cnt, drop_info, lv FROM user_town_locations')}
    after = {r[0]: r[1:] for r in b.execute('SELECT location_id, tap_cnt, drop_info, lv FROM user_town_locations')}
    want_items, want_zel, want_karma, taps_used = {}, 0, 0, 0
    ok = True
    for loc in sorted(before):
        taps0, drops0, lv0 = before[loc]
        taps1, drops1, lv1 = after.get(loc, (None, None, None))
        label = f'{NAMES.get(loc, loc)} (location {loc})'
        if drops1 != drops0:
            print(f'INCONCLUSIVE: {label} list changed (period expired or upgraded with taps left); '
                  'cannot reconstruct the harvest from these snapshots')
            return 2
        if taps1 is None or not 0 <= taps1 <= taps0:
            print(f'INCONCLUSIVE: {label} remaining taps are missing or increased')
            return 2
        used = taps0 - taps1
        entries = drops0.split(',')[len(drops0.split(',')) - taps0:][:used] if taps0 else []
        taps_used += used
        print(f'{label}: {used} of {taps0} taps used' + (f'; level {lv0} -> {lv1}' if lv1 != lv0 else ''))
        for n, e in enumerate(entries, 1):
            item, zel, karma = (int(x) for x in e.split(':'))
            shown = []
            if item:
                shown.append(f'item {names.get(item, item)}')
                want_items[item] = want_items.get(item, 0) + 1
            if zel:
                shown.append(f'{zel} Zel')
            if karma:
                shown.append(f'{karma} Karma')
            want_zel, want_karma = want_zel + zel, want_karma + karma
            print(f'    tap {n}: {" + ".join(shown) if shown else "NOTHING (no icon; the tap still counts)"}')
    ia = {r[0]: r[1] for r in a.execute('SELECT item_id, item_num FROM user_items')}
    ib = {r[0]: r[1] for r in b.execute('SELECT item_id, item_num FROM user_items')}
    got_items = {i: ib.get(i, 0) - ia.get(i, 0) for i in set(ia) | set(ib) if ib.get(i, 0) != ia.get(i, 0)}
    town_items = {i: got_items.get(i, 0) for i in want_items}
    cols = [c[1] for c in a.execute('PRAGMA table_info(user_team_archive)')]
    ta = dict(zip(cols, a.execute('SELECT * FROM user_team_archive').fetchone()))
    tb = dict(zip(cols, b.execute('SELECT * FROM user_team_archive').fetchone()))
    (za, ka), (zb, kb) = a.execute('SELECT zel, karma FROM user_info').fetchone(), b.execute('SELECT zel, karma FROM user_info').fetchone()
    other_zel = tb['zel_get'] - ta['zel_get']
    other_karma = (tb['karma_get'] - ta['karma_get']) - (tb['karma_use'] - ta['karma_use'])
    harvest = tb['town_harvest_cnt'] - ta['town_harvest_cnt']
    checks = [
        ('items from the plots', town_items == want_items, f'{town_items} (want {want_items})'),
        ('Zel', zb - za == want_zel + other_zel, f'{zb - za} = {want_zel} from the plots + {other_zel} elsewhere?'),
        ('Karma', kb - ka == want_karma + other_karma,
         f'{kb - ka} = {want_karma} from the plots + {other_karma} elsewhere (karma_get - karma_use)?'),
        ('harvest counter', harvest == taps_used, f'{harvest} (want {taps_used})'),
    ]
    other = {i: n for i, n in got_items.items() if i not in want_items}
    if other:
        print(f'other item changes in the run (not from the Town): {other}')
    for label, good, detail in checks:
        print(f'{"OK  " if good else "BAD "} {label}: {detail}')
        ok = ok and good
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
