"""#19: one settlement per battle -- battle serials, late/repeated results, supplied bars.

python scripts/test_mission_settlement_wire.py PATH_TO_DEBUG_EXE [--port 19979]

MissionStart's Kz7qfSs5.k9cxD7Ba is the client's per-battle token: the client
stores it (MissionInfo::setMissionSerialID) and echoes it in MissionEnd and
MissionContinue (see gimuserver/gme/common/MissionRuns.hpp).  These requests echo
the serial exactly as the client does, then check stored Zel/EXP/item bar,
the open-battle columns and the run table -- not just HTTP success:

* consecutive starts issue distinct serials above every mission id;
* a late result from an earlier run of the SAME mission, a repeated result and
  an unknown serial all change nothing;
* a result that fails mid-settlement rolls back and settles on retry, once;
* MissionContinue records the mission, not the serial;
* an open battle survives a server restart and settles afterwards;
* a challenge (supplied item set) and the simulator never spend the owned bar,
  and a newer start supersedes an older battle;
* a battle opened before serials existed (NULL open columns) settles once.
"""
import argparse
import sys
from bf_testkit import (Fixture, IsolatedServer, Client, Checker, mission_start_body,
                        mission_end_body, started_serial)
from bf_testkit import qa_dir  # noqa: E402

FLOOR = 1_000_000_000
ITEM = 20000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19979)
    args = ap.parse_args()
    fx = Fixture.create(qa_dir('mission_settlement'), port=args.port)
    check = Checker()
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()

        def execute(sql, params=()):
            r = db.execute(sql, params)
            db.commit()
            return r

        def info():
            return dict(db.execute('SELECT * FROM user_info WHERE id=?', (c.user,)).fetchone())

        def bar():
            return [tuple(r) for r in db.execute('SELECT disp_order,item_id,item_num FROM user_equip_items'
                                                  ' WHERE user_id=? ORDER BY disp_order', (c.user,))]

        def money():
            i = info()
            return (i['zel'], i['karma'], i['exp'], i['level'])

        def start(mid=10):
            execute('UPDATE user_info SET energy=20000,energy_full_ts=0 WHERE id=?', (c.user,))
            reply = c.call('MissionStart', body=mission_start_body(mid))
            assert 'error' not in reply, reply
            return started_serial(reply)

        def end(mid, serial=None, **kw):
            return c.call('MissionEnd', body=mission_end_body(mid, serial=serial, **kw))

        def state():
            return (money(), bar(), info().get('open_mission_id'), info().get('open_mission_serial'))

        execute('DELETE FROM user_equip_items WHERE user_id=?', (c.user,))
        execute('DELETE FROM user_equip_bonus_items WHERE user_id=?', (c.user,))
        execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,20)'
                ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=20', (c.user, ITEM))
        execute('UPDATE user_info SET level=50 WHERE id=?', (c.user,))
        c.call('ItemEdit', body={'71U5wzhI': [{'XuJL4pc5': '0', 'kixHbe54': str(ITEM), 'wgV86x1q': '9'}],
                                 'nAligJSQ': []})
        check('setup: nine owned items on the bar', bar() == [(0, ITEM, 9)], bar())

        # ---- serials ---------------------------------------------------------------
        s1 = start()
        s2 = start()
        try:
            runs = {r[0]: r[1] for r in db.execute('SELECT serial,mission_id FROM user_mission_runs WHERE user_id=?',
                                                   (c.user,))}
        except Exception as e:       # an executable from before battle serials
            runs = {'unavailable': str(e)}
        check('serial: MissionStart issues a serial above every mission id', s1 >= FLOOR, s1)
        check('serial: each battle gets its own', s2 > s1, (s1, s2))
        check('serial: both recorded against mission 10', runs.get(s1) == 10 and runs.get(s2) == 10, runs)
        check('serial: the newest start is the open battle', info().get('open_mission_serial') == s2
              and info()['open_mission_id'] == 10, (info().get('open_mission_serial'), info()['open_mission_id']))

        # ---- a late result from the superseded run of the same mission ---------------
        before = state()
        reply = end(10, s1, used=f'{ITEM}:1', zel=100)
        check('late: the earlier run of the same mission pays and spends nothing',
              'error' not in reply and state() == before, (reply.get('error'), state(), before))

        # ---- the open battle settles, once -----------------------------------------
        zel0 = money()[0]
        first_clear = db.execute("SELECT 1 FROM user_campaign_missions WHERE user_id=? AND mission_id='10'"
                                 ' AND state=2', (c.user,)).fetchone() is None
        reply = end(10, s2, used=f'{ITEM}:2', zel=100)
        check('settle: the open battle pays and spends its log', 'error' not in reply and money()[0] > zel0
              and bar() == [(0, ITEM, 7)], (reply.get('error'), money()[0] - zel0, bar()))
        # clear_mission_id names the mission on its FIRST clear and nothing ("0")
        # on a repeat -- never the serial (test_mission_progression_wire.py).
        check('settle: the reply names the mission only on a first clear, never the serial',
              reply.get('F5Vs19mb', [{}])[0].get('mauD5qZ1') == ('10' if first_clear else '0'),
              (first_clear, reply.get('F5Vs19mb')))
        check('settle: nothing is open afterwards', info().get('open_mission_serial') == 0 and info()['open_mission_id'] == 0,
              (info().get('open_mission_serial'), info()['open_mission_id']))
        before = state()
        end(10, s2, used=f'{ITEM}:2', zel=100)
        check('repeat: the same serial again pays and spends nothing', state() == before, (state(), before))
        before = state()
        reply = end(10, s2 + 50000)
        check('unknown: a serial never issued is refused without effect', 'error' in reply and state() == before,
              reply.get('error'))

        # ---- a failure mid-settlement, then the retry ------------------------------
        s3 = start()
        execute("CREATE TRIGGER qc_settle_fail BEFORE UPDATE OF zel ON user_info BEGIN SELECT RAISE(ABORT,'QC'); END")
        before = state()
        reply = end(10, s3, used=f'{ITEM}:1', zel=50)
        check('retry: a failed settlement rolls back entirely and stays open', 'error' in reply and state() == before,
              (reply.get('error'), state(), before))
        execute('DROP TRIGGER qc_settle_fail')
        zel0 = money()[0]
        reply = end(10, s3, used=f'{ITEM}:1', zel=50)
        check('retry: the retried result settles once', 'error' not in reply and money()[0] > zel0
              and bar() == [(0, ITEM, 6)], (reply.get('error'), bar()))

        # ---- MissionContinue maps the serial back to the mission --------------------
        # This is an affordable-revival fixture, independent of the player's
        # current wallet and prior mission-10 continues. No-gem refusals have
        # dedicated coverage in test_continue_delayed_retry_wire.py.
        execute('UPDATE user_info SET gems=10 WHERE id=?', (c.user,))
        execute('DELETE FROM user_mission_continues WHERE user_id=? AND mission_id=?', (c.user, '10'))
        s4 = start()
        reply = c.call('p8B2i9rJ', 'G3FwvQfy5hcxHMen', {
            '6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
            'Kz7qfSs5': [{'k9cxD7Ba': str(s4), 'j3g5P4cq': '4'}],
            '5PR2VmH1': [{'k9cxD7Ba': str(s4), 'j0Uszek2': '4', 'K2gIYm0h': 'qc-suspend-blob'}]})
        check('continue: funded revival is accepted', 'error' not in reply, reply.get('error'))
        continued = {r[0] for r in db.execute('SELECT mission_id FROM user_mission_continues WHERE user_id=?',
                                              (c.user,))}
        check('continue: recorded against mission 10, not the serial', '10' in continued and str(s4) not in continued,
              continued)
        check('continue: the break record keeps the serial the client resumes with',
              info().get('mission_break_serial') == str(s4), info().get('mission_break_serial'))
        db.close()

    # ---- an open battle survives a restart --------------------------------------------
    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx)
        db = fx.db()
        check('restart: the open serial persisted', info().get('open_mission_serial') == s4, info().get('open_mission_serial'))
        zel0 = money()[0]
        reply = end(10, s4, used=f'{ITEM}:1', zel=10)
        check('restart: the resumed battle settles after a restart', 'error' not in reply and money()[0] > zel0
              and bar() == [(0, ITEM, 5)], (reply.get('error'), bar()))

        # ---- supplied item lists never touch the owned bar ---------------------------
        s5 = start(1000010)
        before_bar = bar()
        reply = end(1000010, s5, used=f'{ITEM}:3')
        check('challenge: a supplied item set spends nothing owned', 'error' not in reply and bar() == before_bar,
              (reply.get('error'), bar()))
        s6 = start(6000000)
        s7 = start()
        before = state()
        end(6000000, s6, used=f'{ITEM}:1')
        check('supersede: a simulator battle replaced by a newer start settles nothing', state() == before,
              (state(), before))
        zel0 = money()[0]
        end(10, s7, used=f'{ITEM}:1', zel=10)
        check('supersede: the newer battle still settles', money()[0] > zel0 and bar() == [(0, ITEM, 4)], bar())

        # ---- a battle opened before serials existed ---------------------------------
        try:
            execute('UPDATE user_info SET open_mission_id=NULL, open_mission_serial=NULL WHERE id=?', (c.user,))
        except Exception:            # an executable from before battle serials
            execute('UPDATE user_info SET open_mission_id=NULL WHERE id=?', (c.user,))
        zel0 = money()[0]
        end(10, used=f'{ITEM}:1', zel=10)
        check('legacy: a pre-serial battle (mission-id serial) settles once', money()[0] > zel0
              and bar() == [(0, ITEM, 3)], (money()[0] - zel0, bar()))
        before = state()
        end(10, used=f'{ITEM}:1', zel=10)
        check('legacy: and only once', state() == before, (state(), before))
        db.close()
    return check.summary('mission settlement')


if __name__ == '__main__':
    sys.exit(main())
