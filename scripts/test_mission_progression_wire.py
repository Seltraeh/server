"""Mission lifecycle: what a MissionEnd says was NEWLY cleared, and what that opens.

    python scripts/test_mission_progression_wire.py PATH_TO_DEBUG_EXE [--port 19983] [--out DIR]

Isolated server on a copy of the live save under out/qa/current/mission_progression.

MissionResultFriendRequestScene::changeNextScene @0x18C301C routes the client out
of the result screens on three ids in the reward block F5Vs19mb, and trusts them:

  mauD5qZ1 clear_mission_id  plays that mission's END script whenever it has one
           (mission 10: map1-dungeon0-1end.txt, the Farm announcement, ending on
           return point 0 = Home); "12" first offers the Randall presentation.
  4sQ8vBXm clear_dungeon_id  plays the dungeon's end script (map1-ending.txt on 80)
           and animates the next dungeon; "" is the client's own empty value.
  NgPQbA46 clear_area_id     anything but "0" resets the last area so the map shows
           the newly opened one; "0" is the only neutral value.

With all three neutral a story result goes to DungeonSelectScene2, the area's own
dungeon list.  Sending all three on EVERY clear replayed the Farm scene and went
Home after each repeat of mission 10 (2026-10-02).  So this checks, per clear:
first vs repeat, dungeon completion, an area opened (Mistral -> Morgan) and a
compound area need (Lizeria: 666 AND 20067), losses, duplicate and late results,
a restart between start and result -- plus the town/feature gates the same clears
open: Farm (TownLocationMst 3, need 11), Mountain (1, need 12) and the Sphere House
(TownFacilityMst 1, need 21: its recipes join PermitRecipe 51yQrDBR), with every
first-clear reward paid once.
"""
import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import (ROOT, Checker, Client, Fixture, IsolatedServer, mission_end_body,  # noqa: E402
                        mission_start_body, permitted, qa_dir, started_serial)

NEUTRAL = ('0', '', '0')


def mst(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


MISSIONS = {int(r['j28VNcUW']): r for r in mst('mission')}
LOCATION_NEED = {int(r['un80kW9Y']): int(r['HSRhkf70'] or 0) for r in mst('town_location')}
FACILITY_NEED = {int(r['y9ET7Aub']): int(r['HSRhkf70'] or 0) for r in mst('town_facility')}


def facility_recipes(facility, lv):
    out = set()
    for r in mst('town_facility_lv'):
        if int(r['y9ET7Aub']) == facility and int(r['D9wXQI2V']) <= lv:
            out |= {int(x) for x in (r.get('rGoJ6Ty9') or '').split(',') if x.strip()}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19983)
    ap.add_argument('--out', default=str(qa_dir('mission_progression')))
    args = ap.parse_args()
    check = Checker()
    fx = Fixture.create(args.out, port=args.port)

    # The data facts the routing below depends on, read from the tables served.
    check('data: the Farm is town location 3, gated on mission 11', LOCATION_NEED.get(3) == 11, LOCATION_NEED)
    check('data: the Mountain is town location 1, gated on mission 12', LOCATION_NEED.get(1) == 12, LOCATION_NEED)
    check('data: the Sphere House is facility 1, gated on mission 21', FACILITY_NEED.get(1) == 21, FACILITY_NEED)
    check('data: mission 10 ends on the Farm script, 11 on the Mountain one',
          MISSIONS[10].get('N4XVE1uA') == '0,map1-dungeon0-1end.txt'
          and MISSIONS[11].get('N4XVE1uA') == '0,map1-dungeon0-2end.txt')

    db = fx.db()

    def execute(sql, params=()):
        r = db.execute(sql, params)
        db.commit()
        return r

    # A known starting point: the player's own state (1, 2 and 10 cleared).
    execute('DELETE FROM user_campaign_missions WHERE user_id=(SELECT id FROM user_info LIMIT 1)'
            " AND mission_id NOT IN ('1','2','10')")
    user = execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    # ...and the tiles those clears open unopened with them.  The player's own
    # Farm and Mountain have been rolled and harvested since (2026-10-02 12:44),
    # and a copied period that has not expired yet left this "first clear of
    # 11" with nothing to hand out: a rolled tile behind a closed gate is a
    # state real play cannot reach (Session 6 handoff, fixture drift).
    execute("UPDATE user_town_locations SET period_start=0, tap_cnt=0, drop_info=''"
            " WHERE user_id=? AND location_id IN (1, 3)", (user,))
    sphere_lv = (execute('SELECT lv FROM user_town_facilities WHERE user_id=? AND facility_id=1', (user,)).fetchone()
                 or [1])[0]
    # Only recipes the master carries reach the menu (Town::permittedRecipes drops
    # a release-list id RecipeMstList could not resolve, e.g. 99999).
    known = {int(r['4HqhTf3a']) for r in mst('recipe')}
    sphere_only = (facility_recipes(1, max(sphere_lv, 1)) - facility_recipes(2, 99)) & known
    check('data: the Sphere House releases recipe 2005 (Famous Blade) at its level', 2005 in sphere_only,
          sorted(sphere_only)[:8])

    def clear(*ids):
        for mid in ids:
            execute('INSERT INTO user_campaign_missions(user_id,mission_id,state,attain_percent,clear_count)'
                    ' VALUES (?,?,2,100,1) ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2',
                    (user, str(mid)))

    def gems():
        return execute('SELECT gems FROM user_info WHERE id=?', (user,)).fetchone()[0]

    def presents():
        return execute('SELECT COUNT(*) FROM user_presents WHERE user_id=?', (user,)).fetchone()[0]

    def clear_count(mid):
        row = execute('SELECT clear_count FROM user_campaign_missions WHERE user_id=? AND mission_id=?',
                      (user, str(mid))).fetchone()
        return row[0] if row else 0

    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        check('fixture: the account under test', c.user == user)

        def start(mid):
            execute('UPDATE user_info SET energy=20000,energy_full_ts=0 WHERE id=?', (user,))
            reply = c.call('MissionStart', body=mission_start_body(mid))
            assert 'error' not in reply, (mid, reply.get('error'))
            return started_serial(reply)

        def end(mid, serial, status=2):
            return c.call('MissionEnd', body=mission_end_body(mid, serial=serial, status=status))

        def ids(reply):
            r = (reply.get('F5Vs19mb') or [{}])[0]
            return (r.get('mauD5qZ1'), r.get('4sQ8vBXm'), r.get('NgPQbA46'))

        def cleared(reply):
            return {int(r['j28VNcUW']) for r in reply.get('UT1SVg59') or []}

        def recipes(reply):
            return {int(r['4HqhTf3a']) for r in reply.get('51yQrDBR') or []}

        def tile(reply, location):
            """(tap count, drop list) of one town tile in a UserInfo reply."""
            row = {int(r['un80kW9Y']): r for r in reply.get('s8TCo2MS') or []}.get(location)
            return None if row is None else (int(row.get('mDaE3t6A') or 0), row.get('eip3MS5L') or '')

        def run(mid, label, expect, status=2):
            serial = start(mid)
            g0, p0, c0 = gems(), presents(), clear_count(mid)
            reply = end(mid, serial, status)
            check(f'{label}: MissionEnd answers', 'error' not in reply, reply.get('error'))
            check(f'{label}: clear ids {expect}', ids(reply) == expect, ids(reply))
            return reply, gems() - g0, presents() - p0, clear_count(mid) - c0

        # ---- before: Farm locked, its tile present but unrolled ----------------------
        login = c.call('UserInfo')
        farm = tile(login, 3)
        check('before: the Farm tile row is sent (a missing row would hide it forever)', farm is not None)
        check('before: nothing to harvest behind the Farm gate', farm == (0, ''), farm)
        check('before: 11 is not cleared', 11 not in cleared(login))

        # ---- repeat clear of mission 10: nothing new, no Farm scene, no Home trip -----
        reply, dg, dp, dc = run(10, 'repeat 10', NEUTRAL)
        check('repeat 10: counted as one more clear, nothing first-clear paid', dc == 1 and dg == 0 and dp == 0,
              (dc, dg, dp))
        check('repeat 10: mission 11 is the permitted next stage', 11 in permitted(reply, 'j28VNcUW'))

        # ---- first clear of 11: names 11, no dungeon or area yet; the Farm opens ------
        reply, dg, dp, dc = run(11, 'first 11', ('11', '', '0'))
        check('first 11: recorded once', dc == 1)
        check('first 11: the Farm gate (11) is in the cleared list the client checks', 11 in cleared(reply))
        # In-session, not only after a relog: a Farm opened with no taps is
        # drawn but inert (collectItem returns silently), as reported 2026-10-02.
        farm = tile(reply, 3)
        check('first 11: the same MissionEnd hands the Farm its first harvest',
              farm is not None and farm[0] > 0 and farm[1].count(',') + 1 == farm[0], farm)
        relog = c.call('UserInfo')
        farm = tile(relog, 3)
        check('first 11: after the next login the Farm has a harvest rolled',
              farm is not None and farm[0] > 0 and farm[1].count(',') + 1 >= farm[0], farm)
        check('first 11: the Mountain (needs 12) is still shut', tile(relog, 1) == (0, ''), tile(relog, 1))
        reply, dg, dp, dc = run(11, 'repeat 11', NEUTRAL)
        check('repeat 11: no first-clear payment', dg == 0 and dp == 0, (dg, dp))

        # ---- first clear of 12: completes dungeon 10 ---------------------------------
        reply, dg, dp, dc = run(12, 'first 12', ('12', '10', '0'))
        check('first 12: the dungeon clear Gem is paid once', dg == 1, dg)
        check('first 12: its first-clear reward (8:0:1) is queued once', dp == 1, dp)
        check('first 12: the Mountain gate (12) is cleared', 12 in cleared(reply))
        check('first 12: the next dungeon (20) is permitted', 20 in permitted(reply, 'MHx05sXt'))
        reply, dg, dp, dc = run(12, 'repeat 12', NEUTRAL)
        check('repeat 12: no Gem, no present, no dungeon replay', dg == 0 and dp == 0, (dg, dp))

        # ---- duplicate and late results ------------------------------------------------
        serial = start(20)
        first = end(20, serial)
        check('first 20: names 20 only', ids(first) == ('20', '', '0'), ids(first))
        g0, p0 = gems(), presents()
        dup = end(20, serial)
        check('duplicate 20: the same serial again is neutral and pays nothing',
              'error' not in dup and ids(dup) == NEUTRAL and gems() == g0 and presents() == p0, ids(dup))
        old = start(21)
        newer = start(21)
        late = end(21, old)
        check('late 21: a superseded run settles nothing and names nothing',
              'error' not in late and ids(late) == NEUTRAL and 21 not in cleared(late), ids(late))

        # ---- a loss clears nothing ------------------------------------------------------
        lost = end(21, newer, status=3)
        check('loss 21: neutral ids, not cleared', 'error' not in lost and ids(lost) == NEUTRAL
              and 21 not in cleared(lost), ids(lost))
        check('loss 21: the Sphere House stays shut (no sphere recipe)', not (recipes(lost) & sphere_only),
              sorted(recipes(lost) & sphere_only)[:5])

        # ---- first clear of 21, across a restart: the Sphere House opens ----------------
        serial21 = start(21)
    with IsolatedServer(args.exe, fx, log_name='server_restart.log'):
        c = Client(fx)
        reply = c.call('MissionEnd', body=mission_end_body(21, serial=serial21))
        check('restart: the open 21 settles after a restart', 'error' not in reply, reply.get('error'))
        check('restart: first 21 names 21', ids(reply) == ('21', '', '0'), ids(reply))
        check('first 21: the Sphere House recipes join the permit list (2005 Famous Blade)',
              2005 in recipes(reply) and sphere_only <= recipes(reply),
              sorted(sphere_only - recipes(reply))[:5])

        def start2(mid):
            execute('UPDATE user_info SET energy=20000,energy_full_ts=0 WHERE id=?', (user,))
            r = c.call('MissionStart', body=mission_start_body(mid))
            assert 'error' not in r, (mid, r.get('error'))
            return started_serial(r)

        again = c.call('MissionEnd', body=mission_end_body(21, serial=start2(21)))
        check('restart: a repeat of 21 is neutral', ids(again) == NEUTRAL, ids(again))

        # ---- the area's final stage: 85 completes dungeon 80 and opens Morgan ------------
        clear(*range(22, 24), *range(30, 34), *range(40, 44), *range(50, 54), *range(60, 64),
              *range(70, 74), *range(81, 85))
        g0, p0 = gems(), presents()
        final = c.call('MissionEnd', body=mission_end_body(85, serial=start2(85)))
        check('final 85: names 85, dungeon 80 (map1-ending) and area 100', ids(final) == ('85', '80', '100'),
              ids(final))
        check('final 85: Morgan (area 200) and its first stage are permitted',
              200 in permitted(final, 'VjCY7rX4') and 200 in permitted(final, 'j28VNcUW'))
        check('final 85: the dungeon clear Gem is paid once', gems() - g0 == 1, gems() - g0)
        repeat = c.call('MissionEnd', body=mission_end_body(85, serial=start2(85)))
        check('repeat 85: neutral -- no ending replay, the map is not reset', ids(repeat) == NEUTRAL, ids(repeat))

        # ---- a compound area need: Lizeria needs 666 AND 20067 --------------------------
        clear(565, 655, *range(660, 666), 234, 20056, *range(20060, 20067))
        first = c.call('MissionEnd', body=mission_end_body(666, serial=start2(666)))
        check('666 alone: dungeon 660 completes, Lizeria stays shut', ids(first) == ('666', '660', '0'), ids(first))
        check('666 alone: Lizeria (700) not permitted', 700 not in permitted(first, 'VjCY7rX4'))
        second = c.call('MissionEnd', body=mission_end_body(20067, serial=start2(20067)))
        check('then 20067: dungeon 20060 completes and Lizeria opens from Cordelica (20000)',
              ids(second) == ('20067', '20060', '20000'), ids(second))
        check('then 20067: Lizeria (700) is permitted', 700 in permitted(second, 'VjCY7rX4'))

        # ---- special content: named on its first clear, no dungeon/area events ----------
        sim = c.call('MissionEnd', body=mission_end_body(6000000, serial=start2(6000000)))
        check('simulator: a special mission carries no dungeon or area event',
              ids(sim)[1:] == ('', '0'), ids(sim))
    db.close()
    return check.summary('mission progression')


if __name__ == '__main__':
    sys.exit(main())
