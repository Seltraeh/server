"""Town harvest: what every tap pays, the Farm against the other plots (Session 14).

    python scripts/test_town_harvest_wire.py PATH_TO_DEBUG_EXE [--port 19989] [--out DIR]

Session 13's player saw drops on the Mountain, Forest and River but none on the
Farm, and no sparkle after upgrading the Farm.  The run's own fixture and save
(section E) show why: that period's six Farm entries were all "0:0:0".

The contract (gme/common/Town.cpp, tools/TOWN_STATE_MODEL.md):
  * the server pre-rolls a period -- tap_cnt entries "<item>:<zel>:<karma>" in
    UserTownLocationDetail (s8TCo2MS: mDaE3t6A taps, eip3MS5L entries);
  * the client replays entry `count - tap_cnt` per tap and draws a flying icon
    only for an item that resolves, Zel >= 1 and Karma >= 1
    (MyTownTopScene::collectItem @0x18F244C): an all-zero entry draws nothing
    but still uses the tap and the sparkle leaves after the last one;
  * TownUpdate (CuQ5oB8U, "<location>:<taps>,...") pays the SAME entries by
    index, clamped to the taps left; nothing is rolled twice;
  * an upgrade (TownFacilityUpdate) re-rolls only the REMAINING taps at the new
    level and never refills; a period older than three hours is re-rolled.

On an isolated copy of the live save (state set on the copy):
  A. A known Farm list -- item only, Zel only, Karma only, all three, nothing,
     item -- paid entry by entry over two flushes ("3:2", then "3:4").
  B. The player's exact flush "1:3,2:8,3:6,4:10" on known lists for all four
     plots: each plot pays exactly its entries; the empty Farm entry pays
     nothing and is still counted.
  C. Exhausted and repeated: "3:2" with no taps left pays nothing; a repeated
     flush never pays more than the list holds.
  D. Upgrade and refresh (controlled time on the copy): Farm lv 1 -> 2 persists,
     the exhausted Farm is not refilled and keeps its period start, a partly
     harvested Mountain keeps its remaining tap count with entries from its
     current pool; a period older than three hours is re-rolled at login.
  E. (Read-only, skipped when absent) the Session 13 evidence: fixture
     out/qa/current/session13/fixtures/c3_effects_kit.sqlite and the copy of the
     player's final save out/qa/current/session14/evidence/c3_effects_copy.sqlite.
"""
from __future__ import annotations

import argparse
import json
import sqlite3
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Client, Fixture, IsolatedServer, qa_dir  # noqa: E402

MOUNTAIN, RIVER, FARM, FOREST = 1, 2, 3, 4
PERIOD = 3 * 60 * 60
TILES = 's8TCo2MS'
TOWN_UPDATE = ('CuQ5oB8U', 'w1eo2ZDJ')
FARM_LIST = '10200:0:0,0:150:0,0:0:20,10202:120:15,0:0:0,10200:0:0'
KNOWN = {MOUNTAIN: '10700:36:0,11001:12:14,0:51:25',
         RIVER: '10300:0:3,10300:0:10,0:0:4,10300:0:3,10300:0:3,10300:0:8,10300:0:7,10300:0:3',
         FARM: FARM_LIST,
         FOREST: '10000:30:3,10000:22:0,10000:15:5,10000:22:3,10000:17:4,10000:12:10,10000:28:6,0:18:7,10000:17:0,10000:24:4'}
EVIDENCE_FIXTURE = ROOT / 'out/qa/current/session13/fixtures/c3_effects_kit.sqlite'
EVIDENCE_FINAL = ROOT / 'out/qa/current/session14/evidence/c3_effects_copy.sqlite'


def mst(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


LEVELS = {(int(r['un80kW9Y']), int(r['D9wXQI2V'])): r for r in mst('town_location_lv')}


def pool(location: int, lv: int) -> set[int]:
    out = set()
    for (loc, level), r in LEVELS.items():
        if loc == location and level <= lv:
            for pair in (r.get('8fjZN6Mx') or '').split(','):
                item, _, rate = pair.partition(':')
                if item.strip() and rate.strip() and int(rate) > 0:
                    out.add(int(item))
    return out


def worth(entries: list[str]) -> tuple[dict, int, int]:
    items, zel, karma = {}, 0, 0
    for e in entries:
        i, z, k = (int(x) for x in e.split(':'))
        if i:
            items[i] = items.get(i, 0) + 1
        zel, karma = zel + z, karma + k
    return items, zel, karma


def evidence(check: Checker) -> None:
    if not (EVIDENCE_FIXTURE.is_file() and EVIDENCE_FINAL.is_file()):
        print('SKIP  E: Session 13 evidence copies not on this machine')
        return
    a = sqlite3.connect(f'file:{EVIDENCE_FIXTURE.as_posix()}?mode=ro', uri=True)
    b = sqlite3.connect(f'file:{EVIDENCE_FINAL.as_posix()}?mode=ro', uri=True)
    lists = {r[0]: (r[1], r[2]) for r in a.execute('SELECT location_id, tap_cnt, drop_info FROM user_town_locations')}
    check('E: the Session 13 fixture rolled the Farm as six all-empty entries',
          lists.get(FARM) == (6, ','.join(['0:0:0'] * 6)), lists.get(FARM))
    check('E: the player spent every tap of every plot (final tap counts 0)',
          all(r[0] == 0 for r in b.execute('SELECT tap_cnt FROM user_town_locations')))
    want_items, zel, karma = {}, 0, 0
    for loc, (_taps, drops) in lists.items():
        items, z, k = worth(drops.split(','))
        for i, n in items.items():
            want_items[i] = want_items.get(i, 0) + n
        zel, karma = zel + z, karma + k
    ia = {r[0]: r[1] for r in a.execute('SELECT item_id, item_num FROM user_items')}
    ib = {r[0]: r[1] for r in b.execute('SELECT item_id, item_num FROM user_items')}
    got_items = {i: ib.get(i, 0) - ia.get(i, 0) for i in set(ia) | set(ib) if ib.get(i, 0) != ia.get(i, 0)}
    check('E: item credits equal the plots\' entries exactly (the Farm added none)', got_items == want_items,
          (got_items, want_items))
    cols = [c[1] for c in a.execute('PRAGMA table_info(user_team_archive)')]
    ta = dict(zip(cols, a.execute('SELECT * FROM user_team_archive').fetchone()))
    tb = dict(zip(cols, b.execute('SELECT * FROM user_team_archive').fetchone()))
    (za, ka), (zb, kb) = a.execute('SELECT zel, karma FROM user_info').fetchone(), b.execute('SELECT zel, karma FROM user_info').fetchone()
    other_zel = tb['zel_get'] - ta['zel_get']
    other_karma = (tb['karma_get'] - ta['karma_get']) - (tb['karma_use'] - ta['karma_use'])
    check(f'E: Zel credit = {zel} from the plots + {other_zel} recorded elsewhere (the quest)',
          zb - za == zel + other_zel, (zb - za, zel, other_zel))
    check(f'E: Karma = {karma} from the plots + quest Karma - the 500-Karma Farm upgrade',
          kb - ka == karma + other_karma and tb['karma_use'] - ta['karma_use'] == 500, (kb - ka, karma, other_karma))
    check('E: the harvest counter counted all 27 taps, the Farm\'s six included',
          tb['town_harvest_cnt'] - ta['town_harvest_cnt'] == 27, (ta['town_harvest_cnt'], tb['town_harvest_cnt']))
    check('E: the Farm was upgraded to lv 2 only after its taps were spent (no re-roll: lv 1 entries kept)',
          b.execute('SELECT lv, tap_cnt, drop_info, period_lv FROM user_town_locations WHERE location_id=3').fetchone()
          == (2, 0, ','.join(['0:0:0'] * 6), 1))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19989)
    ap.add_argument('--out', default=str(qa_dir('town_harvest')))
    args = ap.parse_args()
    check = Checker()
    evidence(check)

    fx = Fixture.create(args.out, port=args.port)
    db = fx.db()

    def q(sql, params=()):
        r = db.execute(sql, params)
        db.commit()
        return r

    user = q('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    for mission in ('11', '12'):           # the Farm and Mountain gates
        if not q('SELECT 1 FROM user_campaign_missions WHERE user_id=? AND mission_id=? AND state=2',
                 (user, mission)).fetchone():
            q('INSERT OR REPLACE INTO user_campaign_missions (user_id, mission_id, state, attain_percent, clear_count,'
              ' last_cleared_at, reward_claimed, rewards_got, no_continue) VALUES (?,?,2,100,1,?,0,\'\',1)',
              (user, mission, int(time.time())))
    now = int(time.time())

    def plant(lists: dict, lv: int = 1):
        for loc, drops in lists.items():
            n = len(drops.split(',')) if drops else 0
            q('UPDATE user_town_locations SET lv=?, period_lv=?, karma=0, period_start=?, tap_cnt=?, drop_info=?'
              ' WHERE user_id=? AND location_id=?', (lv, lv, now, n, drops, user, loc))

    def items():
        return {r[0]: r[1] for r in q('SELECT item_id, item_num FROM user_items WHERE user_id=?', (user,))}

    def wallet():
        zel, karma = q('SELECT zel, karma FROM user_info WHERE id=?', (user,)).fetchone()
        harvest = q('SELECT town_harvest_cnt FROM user_team_archive WHERE user_id=?', (user,)).fetchone()
        return zel, karma, harvest[0] if harvest else 0

    def stored(loc):
        return tuple(q('SELECT lv, period_start, tap_cnt, drop_info, period_lv FROM user_town_locations'
                       ' WHERE user_id=? AND location_id=?', (user, loc)).fetchone())

    def tiles(reply):
        return {int(r['un80kW9Y']): (int(r.get('mDaE3t6A') or 0), r.get('eip3MS5L') or '')
                for r in reply.get(TILES) or []}

    q('UPDATE user_info SET karma=100000 WHERE id=?', (user,))
    plant(KNOWN)
    with IsolatedServer(args.exe, fx):
        c = Client(fx)

        def flush(log: str):
            reply = c.call(*TOWN_UPDATE, {'EuY6L7AX': [{'0mRaAo39': log}]})
            c.call('nJ3A7qFp', 'bGxX67KB', {})          # BadgeInfo: answered after the commit
            return reply

        def paid(before_items, before_wallet):
            after_items, after_wallet = items(), wallet()
            d = {i: after_items.get(i, 0) - before_items.get(i, 0)
                 for i in set(after_items) | set(before_items) if after_items.get(i, 0) != before_items.get(i, 0)}
            return d, after_wallet[0] - before_wallet[0], after_wallet[1] - before_wallet[1], after_wallet[2] - before_wallet[2]

        login = tiles(c.call('UserInfo'))
        check('setup: the login sends the planted lists, not a re-roll',
              all(login.get(loc) == (len(d.split(',')), d) for loc, d in KNOWN.items()), login)

        # ---- A. the Farm, entry by entry ---------------------------------------------
        entries = FARM_LIST.split(',')
        i0, w0 = items(), wallet()
        r = flush(f'{FARM}:2')
        check('A: TownUpdate answers', 'error' not in r, r)
        d, z, k, h = paid(i0, w0)
        want = worth(entries[:2])
        check('A: "3:2" pays entries 1-2 (Don Nut; 150 Zel) and nothing else', (d, z, k) == want, ((d, z, k), want))
        check('A: two taps counted, four left', h == 2 and stored(FARM)[2] == 4, (h, stored(FARM)))
        i1, w1 = items(), wallet()
        flush(f'{FARM}:4')
        d, z, k, h = paid(i1, w1)
        want = worth(entries[2:])
        check('A: "3:4" pays entries 3-6 (20 Karma; Buru Nut+120 Zel+15 Karma; nothing; Don Nut)',
              (d, z, k) == want, ((d, z, k), want))
        check('A: the empty fifth entry still used a tap (four counted, none left)', h == 4 and stored(FARM)[2] == 0,
              (h, stored(FARM)))
        check('A: the list itself is untouched (the client indexes it until the period ends)',
              stored(FARM)[3] == FARM_LIST)

        # ---- B. the player's flush ---------------------------------------------------
        plant(KNOWN)
        i0, w0 = items(), wallet()
        flush('1:3,2:8,3:6,4:10')
        d, z, k, h = paid(i0, w0)
        total_items, total_zel, total_karma = {}, 0, 0
        for loc, drops in KNOWN.items():
            it, zz, kk = worth(drops.split(','))
            for i, n in it.items():
                total_items[i] = total_items.get(i, 0) + n
            total_zel, total_karma = total_zel + zz, total_karma + kk
        check('B: "1:3,2:8,3:6,4:10" pays every plot exactly its entries',
              (d, z, k) == (total_items, total_zel, total_karma), ((d, z, k), (total_items, total_zel, total_karma)))
        check('B: all 27 taps counted, every plot exhausted', h == 27 and all(stored(l)[2] == 0 for l in KNOWN), h)

        # ---- C. exhausted and repeated -----------------------------------------------
        i0, w0 = items(), wallet()
        r = flush(f'{FARM}:2')
        d, z, k, h = paid(i0, w0)
        check('C: taps beyond the list pay nothing and are not counted', 'error' not in r and (d, z, k, h) == ({}, 0, 0, 0),
              (r, d, z, k, h))
        plant({FARM: FARM_LIST})
        i0, w0 = items(), wallet()
        flush(f'{FARM}:4')
        flush(f'{FARM}:4')                               # the same request again (a retry)
        d, z, k, h = paid(i0, w0)
        check('C: a repeated "3:4" pays the list once at most: 4 then the remaining 2 entries',
              (d, z, k) == worth(entries) and h == 6 and stored(FARM)[2] == 0, (d, z, k, h))

        # ---- D. upgrade and refresh --------------------------------------------------
        plant({MOUNTAIN: KNOWN[MOUNTAIN], FARM: FARM_LIST})
        flush(f'{FARM}:6,{MOUNTAIN}:1')                  # Farm exhausted, Mountain 2 left
        start_farm, start_mountain = stored(FARM)[1], stored(MOUNTAIN)[1]
        facilities = [{'y9ET7Aub': str(r[0]), 'D9wXQI2V': str(r[1]), 'HTVh8a65': str(r[2])}
                      for r in q('SELECT facility_id, lv, karma FROM user_town_facilities WHERE user_id=?', (user,))]
        locations = [{'un80kW9Y': str(loc), 'D9wXQI2V': '2' if loc in (FARM, MOUNTAIN) else '1', 'HTVh8a65': '0'}
                     for loc in (MOUNTAIN, RIVER, FARM, FOREST)]
        karma_before = wallet()[1]
        reply = c.call('TownFacilityUpdate', body={'EuY6L7AX': [{'HTVh8a65': '1000'}], 'YRgx49WG': facilities,
                                                   'yj46Q2xw': locations})
        c.call('nJ3A7qFp', 'bGxX67KB', {})
        check('D: TownFacilityUpdate answers', 'error' not in reply, reply.get('error'))
        t = tiles(reply)
        check('D: Farm and Mountain levels persist (lv 2)', stored(FARM)[0] == 2 and stored(MOUNTAIN)[0] == 2,
              (stored(FARM), stored(MOUNTAIN)))
        check('D: the karma paid is debited', karma_before - wallet()[1] == 1000, (karma_before, wallet()[1]))
        check('D: the exhausted Farm is NOT refilled by the upgrade (0 taps, period start kept)',
              t.get(FARM, (None,))[0] == 0 and stored(FARM)[2] == 0 and stored(FARM)[1] == start_farm,
              (t.get(FARM), stored(FARM)))
        mt = t.get(MOUNTAIN) or (0, '')
        mentries = mt[1].split(',') if mt[1] else []
        check('D: the partly harvested Mountain keeps exactly its 2 remaining taps, re-rolled at lv 2, same period',
              mt[0] == 2 and len(mentries) == 2 and stored(MOUNTAIN)[1] == start_mountain and stored(MOUNTAIN)[4] == 2
              and {int(e.split(':')[0]) for e in mentries} <= pool(MOUNTAIN, 2) | {0}, (mt, stored(MOUNTAIN)))
        q('UPDATE user_town_locations SET period_start=? WHERE user_id=? AND location_id=?',
          (int(time.time()) - PERIOD - 60, user, FARM))
        t = tiles(c.call('UserInfo'))
        farm = t.get(FARM) or (0, '')
        level = LEVELS[(FARM, 2)]
        lo, hi = sorted((int(level['Q43bK0zN']), int(level['dDYX75JR'])))
        fentries = farm[1].split(',') if farm[1] else []
        check('D: three hours on, the next login rolls a fresh Farm period at its lv 2',
              max(lo, 1) <= farm[0] <= hi and len(fentries) == farm[0]
              and {int(e.split(':')[0]) for e in fentries} <= pool(FARM, 2) | {0}
              and abs(stored(FARM)[1] - time.time()) < 120, (farm, stored(FARM)))
    return check.summary('test_town_harvest_wire')


if __name__ == '__main__':
    raise SystemExit(main())
