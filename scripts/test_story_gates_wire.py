"""#30: compound story prerequisites are ALL-OF -- map visibility and direct entry.

python scripts/test_story_gates_wire.py PATH_TO_DEBUG_EXE [--port 19981]

The client's locked-tile popup (MissionSelectScene2::touchEnded) reads
LOCK_DUNGEON_NOTICE "The dungeon will open when the requirements below have been
met." and lists every need_mission_id entry as `Quest "<name>" Cleared`; Lizeria
("666,20067") is covered by test_lizeria_qc_wire.py.  This covers the other story
lists in deploy/mst: land 12 (dungeon/mission 10170: "10080,10165" inside area
10200, which needs 10075), land 20 (area 11000 / dungeon 10900: nine chapter
finals) and land 101 (dungeon 20250 / mission 21050: "21033,21043" inside area
20200, which needs 20705).  Each case replaces the copied save's clear list with
exactly the set named, then reads UserInfo's PermitPlace (yXNM8kL3) and tries
MissionStart the way a stale client tile would.
"""
import argparse
import sys
from bf_testkit import Fixture, IsolatedServer, Client, Checker, mission_start_body
from bf_testkit import qa_dir  # noqa: E402

LAND20 = (10080, 10170, 10270, 10370, 10470, 10570, 10670, 10771, 10870)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19981)
    args = ap.parse_args()
    fx = Fixture.create(qa_dir('story_gates'), port=args.port)
    check = Checker()
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()

        def clears(ids):
            db.execute('DELETE FROM user_campaign_missions WHERE user_id=?', (c.user,))
            for mid in ids:
                db.execute('INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,?,2)',
                           (c.user, str(mid)))
            db.execute('UPDATE user_info SET energy=20000,energy_full_ts=0 WHERE id=?', (c.user,))
            db.commit()
            permits = c.call('UserInfo').get('yXNM8kL3', [])
            return lambda key, value: any(p.get(key) == str(value) for p in permits)

        def enters(mission):
            before = db.execute('SELECT energy FROM user_info WHERE id=?', (c.user,)).fetchone()[0]
            reply = c.call('MissionStart', body=mission_start_body(mission))
            after = db.execute('SELECT energy FROM user_info WHERE id=?', (c.user,)).fetchone()[0]
            return 'error' not in reply, before == after, reply

        # ---- land 12: dungeon/mission 10170 needs 10080 AND 10165 ------------------
        for extra, open_ in (((), False), ((10080,), False), ((10165,), False), ((10080, 10165), True)):
            seen = clears((10075,) + extra)
            label = f'land 12 with {extra or "neither"}'
            check(f'{label}: area 10200 open (it needs 10075 only)', seen('VjCY7rX4', 10200))
            check(f'{label}: dungeon 10170 {"shown" if open_ else "hidden"}', seen('MHx05sXt', 10170) == open_)
            check(f'{label}: mission 10170 {"shown" if open_ else "hidden"}', seen('j28VNcUW', 10170) == open_)
            entered, energy_kept, reply = enters(10170)
            check(f'{label}: direct entry {"allowed" if open_ else "refused"}', entered == open_)
            if not open_:
                check(f'{label}: a refused entry spends no energy', energy_kept)
                # A stale tile is refused with GmeErrorCommand::ReturnToGame (6):
                # GameScene::checkResponseMessage -> notice -3992 -> HomeScene2,
                # where Close (4) would reach CommonUtils::appExit.
                error = reply.get('error') or {}
                check(f'{label}: the refusal returns the player Home instead of exiting the app',
                      str(error.get('iPD12YCr')) == '6' and 'locked' in str(error.get('ZC0msu2L', '')), error)

        # ---- land 20: all nine chapter finals ---------------------------------
        seen = clears(LAND20[:-1])
        check('land 20 with 8 of 9 finals: area 11000 hidden', not seen('VjCY7rX4', 11000))
        check('land 20 with 8 of 9 finals: dungeon 10900 hidden', not seen('MHx05sXt', 10900))
        check('land 20 with 8 of 9 finals: direct entry refused', not enters(10900)[0])
        seen = clears(LAND20)
        check('land 20 with all nine: area 11000 and land 20 shown', seen('VjCY7rX4', 11000) and seen('9C64Qwe0', 20))
        check('land 20 with all nine: dungeon 10900 and its first mission shown',
              seen('MHx05sXt', 10900) and seen('j28VNcUW', 10900))
        check('land 20 with all nine: the second mission waits for the first (single prerequisite)',
              not seen('j28VNcUW', 10901))
        check('land 20 with all nine: direct entry allowed', enters(10900)[0])
        seen = clears(LAND20 + (10900,))
        check('land 20: clearing 10900 opens 10901', seen('j28VNcUW', 10901))

        # ---- land 101: dungeon 20250 / mission 21050 needs 21033 AND 21043 ----------
        seen = clears((20705, 21033))
        check('land 101 with 21033 only: dungeon 20250 hidden', not seen('MHx05sXt', 20250))
        check('land 101 with 21033 only: direct entry to 21050 refused', not enters(21050)[0])
        seen = clears((20705, 21033, 21043))
        check('land 101 with both: dungeon 20250 and mission 21050 shown',
              seen('MHx05sXt', 20250) and seen('j28VNcUW', 21050))
        check('land 101 with both: direct entry allowed', enters(21050)[0])

        # ---- earned history is kept -------------------------------------------------
        clears((10075, 10080, 10170))      # an old any-of save that already cleared 10170
        kept = db.execute('SELECT COUNT(*) FROM user_campaign_missions WHERE user_id=? AND mission_id=?',
                          (c.user, '10170')).fetchone()[0]
        check('history: a mission cleared under the old rule stays cleared', kept == 1)
        db.close()
    return check.summary('story gates')


if __name__ == '__main__':
    sys.exit(main())
