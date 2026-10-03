"""Town tiles opened mid-session: the Farm (mission 11) and the Mountain (12).

    python scripts/test_town_unlock_wire.py PATH_TO_DEBUG_EXE [--port 19988] [--out DIR]

Reported 2026-10-02, after the once-only Farm scene fix: "farm isn't unlocked when
it says it was inside of town.  The same thing with the mountain tile."  The
player's save had 11 and 12 cleared at 10:32 and 10:35, and the town visited at
11:05 -- with no UserInfo in between.

MyTownTopScene::setLocationInfo @0x18F1574 draws a tile only when the client's
cleared list holds its TownLocationMst need_mission_id (MissionEnd's UT1SVg59
already updates that list), and sparkles it only when its UserTownLocationDetail
(s8TCo2MS) has tap_cnt >= 1; collectItem @0x18F24A8 returns SILENTLY on a tile
with no taps.  A locked tile is sent with no taps, and nothing but UserInfo ever
re-sent s8TCo2MS, so the opened Farm passed its gate and stayed empty and inert
until the next login.  MissionEnd now carries the complete tile list, rolled
against the post-clear cleared set.  On an isolated copy of the live save:

  * a loss, and a repeat of 10, open nothing; River/Forest are not re-rolled;
  * the first clear of 11 hands the Farm its first period IN THAT REPLY (taps in
    the level-1 range, one pre-rolled entry per tap, items from its pool);
  * a repeat does not refill it; taps reported by TownUpdate stay spent;
  * 12 opens the Mountain the same way; a period past 3 hours re-rolls;
  * the player's exact shape (11/12 cleared, tiles never rolled) is repaired by
    the next clear of anything; a result that settles nothing carries no tiles;
  * the next login shows the same tiles the last MissionEnd did.
"""
import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import (ROOT, Checker, Client, Fixture, IsolatedServer, mission_end_body,  # noqa: E402
                        mission_start_body, qa_dir, started_serial)

FARM, MOUNTAIN, RIVER, FOREST = 3, 1, 2, 4
PERIOD = 3 * 60 * 60
TILES = 's8TCo2MS'


def mst(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


LEVELS = {(int(r['un80kW9Y']), int(r['D9wXQI2V'])): r for r in mst('town_location_lv')}
NEED = {int(r['un80kW9Y']): int(r['HSRhkf70'] or 0) for r in mst('town_location')}


def pool(location, lv):
    """Item ids a tile can drop at a level (item_info is cumulative)."""
    out = set()
    for (loc, level), r in LEVELS.items():
        if loc == location and level <= lv:
            for pair in (r.get('8fjZN6Mx') or '').split(','):
                item, _, rate = pair.partition(':')
                if item.strip() and rate.strip() and int(rate) > 0:
                    out.add(int(item))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19988)
    ap.add_argument('--out', default=str(qa_dir('town_unlock')))
    args = ap.parse_args()
    check = Checker()
    check('data: the Farm needs 11 and the Mountain 12; River and Forest are open',
          NEED == {MOUNTAIN: 12, RIVER: 0, FARM: 11, FOREST: 0}, NEED)
    fx = Fixture.create(args.out, port=args.port)
    db = fx.db()

    def execute(sql, params=()):
        r = db.execute(sql, params)
        db.commit()
        return r

    user = execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    now = int(time.time())
    # Before the milestone: 1, 2 and 10 cleared.  Farm/Mountain never rolled (the
    # player's own rows look like this); River/Forest mid-period with known loot.
    execute("DELETE FROM user_campaign_missions WHERE user_id=? AND mission_id NOT IN ('1','2','10')", (user,))
    for location in (FARM, MOUNTAIN):
        execute("UPDATE user_town_locations SET period_start=0, tap_cnt=0, drop_info='', period_lv=0"
                ' WHERE user_id=? AND location_id=?', (user, location))
    for location in (RIVER, FOREST):
        execute("UPDATE user_town_locations SET period_start=?, tap_cnt=2, drop_info='0:0:1,0:0:2', period_lv=lv"
                ' WHERE user_id=? AND location_id=?', (now, user, location))
    levels = {r[0]: r[1] for r in execute('SELECT location_id, lv FROM user_town_locations WHERE user_id=?',
                                          (user,))}
    check('fixture: all four tiles provisioned', set(levels) == {MOUNTAIN, RIVER, FARM, FOREST}, levels)

    def stored(location):
        r = execute('SELECT period_start, tap_cnt, drop_info, period_lv FROM user_town_locations'
                    ' WHERE user_id=? AND location_id=?', (user, location)).fetchone()
        return tuple(r)

    def tiles(reply):
        return {int(r['un80kW9Y']): (int(r.get('mDaE3t6A') or 0), r.get('eip3MS5L') or '')
                for r in reply.get(TILES) or []}

    def rolled(tile, location):
        """A fresh period: taps in the level's range, one entry per tap, items from the pool."""
        if not tile:
            return False
        taps, drops = tile
        level = LEVELS[(location, levels[location])]
        lo, hi = sorted((int(level['Q43bK0zN']), int(level['dDYX75JR'])))
        entries = drops.split(',') if drops else []
        items = {int(e.split(':')[0]) for e in entries}
        return (max(lo, 1) <= taps <= hi and len(entries) == taps
                and all(len(e.split(':')) == 3 for e in entries) and items <= pool(location, levels[location]) | {0})

    with IsolatedServer(args.exe, fx):
        c = Client(fx)

        def start(mid):
            execute('UPDATE user_info SET energy=20000, energy_full_ts=0 WHERE id=?', (user,))
            reply = c.call('MissionStart', body=mission_start_body(mid))
            assert 'error' not in reply, (mid, reply.get('error'))
            return started_serial(reply)

        def run(mid, status=2):
            reply = c.call('MissionEnd', body=mission_end_body(mid, serial=start(mid), status=status))
            assert 'error' not in reply, (mid, reply.get('error'))
            return reply

        def cleared(reply):
            return {int(r['j28VNcUW']) for r in reply.get('UT1SVg59') or []}

        login = tiles(c.call('UserInfo'))
        check('before: the Farm and the Mountain are sent locked (no taps, no loot)',
              login.get(FARM) == (0, '') and login.get(MOUNTAIN) == (0, ''), login)
        quiet = {RIVER: (2, '0:0:1,0:0:2'), FOREST: (2, '0:0:1,0:0:2')}

        lost = run(11, status=3)
        t = tiles(lost)
        check('loss 11: the reply carries all four tiles', set(t) == {MOUNTAIN, RIVER, FARM, FOREST}, t)
        check('loss 11: nothing cleared and the Farm stays shut', 11 not in cleared(lost) and t.get(FARM) == (0, ''),
              t.get(FARM))
        check('loss 11: River and Forest are left mid-period, not re-rolled',
              {k: t.get(k) for k in quiet} == quiet, t)

        t = tiles(run(10))
        check('repeat 10: the Farm stays shut', t.get(FARM) == (0, ''), t.get(FARM))

        first = run(11)
        t = tiles(first)
        check('first 11: the gate opens in the client list (UT1SVg59)', 11 in cleared(first))
        check('first 11: THE SAME REPLY hands the Farm its first harvest', rolled(t.get(FARM), FARM), t.get(FARM))
        farm = t.get(FARM) or (0, '')
        s = stored(FARM)
        check('first 11: the period is stored as rolled now at the tile\'s level',
              abs(s[0] - time.time()) < 120 and (s[1], s[2]) == farm and s[3] == levels[FARM], s)
        check('first 11: the Mountain (12) stays shut', t.get(MOUNTAIN) == (0, ''), t.get(MOUNTAIN))
        check('first 11: River and Forest untouched', {k: t.get(k) for k in quiet} == quiet, t)

        t = tiles(run(11))
        check('repeat 11: no refill -- the same period comes back', t.get(FARM) == farm, (t.get(FARM), farm))

        # Two taps on the Farm, reported the way the client flushes them.
        entries = farm[1].split(',') if farm[1] else []
        items0 = {r[0]: r[1] for r in execute('SELECT item_id, item_num FROM user_items WHERE user_id=?', (user,))}
        zk0 = execute('SELECT zel, karma FROM user_info WHERE id=?', (user,)).fetchone()
        reply = c.call('CuQ5oB8U', 'w1eo2ZDJ', {'EuY6L7AX': [{'0mRaAo39': f'{FARM}:2'}]})
        check('taps: TownUpdate answers', 'error' not in reply, reply)
        gains, zel, karma = {}, 0, 0
        for e in entries[:2]:
            item, z, k = (int(x) for x in e.split(':'))
            if item:
                gains[item] = gains.get(item, 0) + 1
            zel, karma = zel + z, karma + k
        items1 = {r[0]: r[1] for r in execute('SELECT item_id, item_num FROM user_items WHERE user_id=?', (user,))}
        zk1 = execute('SELECT zel, karma FROM user_info WHERE id=?', (user,)).fetchone()
        check('taps: exactly the first two pre-rolled entries are paid',
              all(items1.get(i, 0) - items0.get(i, 0) == n for i, n in gains.items())
              and (zk1[0] - zk0[0], zk1[1] - zk0[1]) == (zel, karma), (gains, zel, karma))
        t = tiles(run(10))
        check('taps: the next MissionEnd shows them spent, same loot list',
              t.get(FARM) == (farm[0] - 2, farm[1]), (t.get(FARM), farm))

        first12 = run(12)
        t = tiles(first12)
        check('first 12: the Mountain gets its first harvest in the same reply',
              12 in cleared(first12) and rolled(t.get(MOUNTAIN), MOUNTAIN), t.get(MOUNTAIN))
        check('first 12: the Farm keeps its partly harvested period', t.get(FARM) == (farm[0] - 2, farm[1]),
              t.get(FARM))
        mountain = t.get(MOUNTAIN)

        # Three hours on, the next clear re-rolls -- in-session, not only at login.
        execute('UPDATE user_town_locations SET period_start=? WHERE user_id=? AND location_id=?',
                (int(time.time()) - PERIOD - 60, user, FARM))
        t = tiles(run(10))
        check('expiry: a Farm period past 3 hours is re-rolled by the next clear',
              rolled(t.get(FARM), FARM) and abs(stored(FARM)[0] - time.time()) < 120, (t.get(FARM), stored(FARM)))
        check('expiry: the unexpired Mountain is left alone', t.get(MOUNTAIN) == mountain, (t.get(MOUNTAIN), mountain))

        # The player's own save: 11 and 12 cleared, both tiles never rolled.
        for location in (FARM, MOUNTAIN):
            execute("UPDATE user_town_locations SET period_start=0, tap_cnt=0, drop_info='', period_lv=0"
                    ' WHERE user_id=? AND location_id=?', (user, location))
        t = tiles(run(10))
        check('live-save shape: the next clear of anything opens both tiles',
              rolled(t.get(FARM), FARM) and rolled(t.get(MOUNTAIN), MOUNTAIN), t)
        last = t

        # A duplicate result settles nothing and must not touch the tiles.
        serial = start(10)
        c.call('MissionEnd', body=mission_end_body(10, serial=serial))
        dup = c.call('MissionEnd', body=mission_end_body(10, serial=serial))
        check('duplicate: a result that settles nothing carries no tiles', 'error' not in dup and TILES not in dup,
              sorted(dup)[:12])

        settled = tiles(c.call('UserInfo'))
        check('login: the next UserInfo shows the tiles the client already holds',
              settled.get(FARM) == last.get(FARM) and settled.get(MOUNTAIN) == last.get(MOUNTAIN),
              (settled, last))
    db.close()
    return check.summary('town unlock')


if __name__ == '__main__':
    sys.exit(main())
