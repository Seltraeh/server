"""Battle-run ownership: which MissionEnd settles, which MissionContinue pays.

python scripts/test_mission_run_ownership_wire.py PATH_TO_DEBUG_EXE [--port 19990]
       [--out out/qa/current/mission_ownership]

Extends scripts/test_mission_run_edges_wire.py (Codex QC, 2026-09-30, kept
unchanged) with the matrix the QC asked for.  Open-battle states:

  pre-serial, unknown   open_mission_id NULL, open_mission_serial NULL -- a save
                        older than both columns; its battle's client holds the
                        mission id as serial
  pre-serial, known     open_mission_id = M, open_mission_serial NULL -- opened
                        by the server that recorded the mission but not a serial
  issued                open_mission_serial = S >= 1,000,000,000 (MissionStart)
  settled / none        both 0

MissionEnd (A): an issued serial settles only its own open run; a mission-id
serial settles only a pre-serial battle, once; stale, legacy-after-upgrade,
other-mission and retried results pay nothing and leave the open run alone;
unknown and other players' serials are refused before anything changes.

MissionContinue (B): only the open battle is charged, marked and recorded;
stale/unknown/foreign/conflicting bodies are refused with GmeErrorCommand::Retry
(2) and change nothing -- the client revives on any ORDINARY reply
(MissionGameOverScene::loopContinue), while Retry sends it back to the Continue
prompt (noticeOK -4000), so a refusal must not be an ordinary reply (Session 6,
net/mission.kdl).  A repeat of a paid continue (same serial, status and blob)
is an ordinary reply that charges nothing, while a second revival (new blob)
pays; short on gems is refused and records nothing; a failure part way leaves
nothing; a MissionEnd racing a continue leaves one consistent outcome; restart
keeps all of it.  Delayed copies after later revivals (A/B/A) are covered by
scripts/test_continue_delayed_retry_wire.py.

The client's continue body (MissionContinueRequest::createBody @0x13A7528):
Kz7qfSs5 {k9cxD7Ba serial, j3g5P4cq 4|6} and 5PR2VmH1 {k9cxD7Ba serial,
K2gIYm0h suspend data} -- both serials from MissionInfo::getMissionSerialID.
"""
import argparse
import sqlite3
import sys
import threading
from bf_testkit import (Fixture, IsolatedServer, Client, Checker, mission_start_body, mission_end_body,
                        started_serial)
from bf_testkit import qa_dir  # noqa: E402

UNKNOWN = 1_999_999_999
REPORTED_ZEL = 123


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19990)
    ap.add_argument('--out', default=str(qa_dir('mission_ownership')))
    args = ap.parse_args()
    fx = Fixture.create(args.out, port=args.port)
    check = Checker()

    db = fx.db()
    user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    # FIXTURE: start from no Continue marks.  A lost or abandoned run on the live
    # save keeps its mark until that mission's next MissionStart (by design), so
    # the copy can carry one -- the player's mission-20 run on 2026-10-02 19:01
    # did -- and these checks compare the whole set.
    db.execute('DELETE FROM user_mission_continues WHERE user_id=?', (user,))
    db.commit()

    def info():
        return dict(db.execute('SELECT * FROM user_info WHERE id=?', (user,)).fetchone())

    def continues():
        return sorted(tuple(r) for r in db.execute('SELECT * FROM user_mission_continues WHERE user_id=?', (user,)))

    def execute(sql, params=()):
        db.execute(sql, params)
        db.commit()

    def top_up():
        execute('UPDATE user_info SET energy=20000, energy_full_ts=0, gems=100 WHERE id=?', (user,))

    def set_open(mission, serial):
        execute('UPDATE user_info SET open_mission_id=?, open_mission_serial=?,'
                " mission_break_serial='', mission_break_info='', mission_break_state=0 WHERE id=?",
                (mission, serial, user))

    def run(body):
        with IsolatedServer(args.exe, fx):
            body(Client(fx, user_id=user))

    # The copied save is migrated by the first start (user_mission_runs and
    # open_mission_serial do not exist before it).
    run(lambda c: None)

    # The fixture save must be able to start both missions used here.
    cleared = {r[0] for r in db.execute("SELECT mission_id FROM user_campaign_missions WHERE user_id=? AND state=2",
                                        (user,))}
    for mission in ('10', '11'):
        if mission not in cleared:
            execute("INSERT OR IGNORE INTO user_campaign_missions (user_id, mission_id, state) VALUES (?,?,2)",
                    (user, mission))
    foreign = db.execute("INSERT INTO user_mission_runs (user_id, mission_id, started_at)"
                         " VALUES ('qc-foreign-player', 10, 0)").lastrowid
    db.commit()

    def start(c, mission=10):
        top_up()
        reply = c.call('MissionStart', body=mission_start_body(mission))
        check(f'MissionStart {mission} succeeds', 'error' not in reply, reply.get('error'))
        return started_serial(reply)

    def end(c, mission, serial, zel=REPORTED_ZEL):
        return c.call('MissionEnd', body=mission_end_body(mission, serial=serial, zel=zel))

    def cont(c, serial, blob, break_serial=None, status='4'):
        return c.call('MissionContinue', body={
            '6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
            'Kz7qfSs5': [{'k9cxD7Ba': str(serial), 'j3g5P4cq': status}],
            '5PR2VmH1': [{'k9cxD7Ba': str(serial if break_serial is None else break_serial), 'K2gIYm0h': blob}]})

    def settles(label, c, mission, serial, expect, open_after=None):
        before = info()
        reply = end(c, mission, serial)
        after = info()
        paid = after['zel'] - before['zel']
        if expect:
            check(f'{label}: settles and pays', 'error' not in reply and paid >= REPORTED_ZEL
                  and after['open_mission_serial'] == 0 and after['open_mission_id'] == 0,
                  (reply.get('error'), paid, after['open_mission_id'], after['open_mission_serial']))
        else:
            check(f'{label}: pays nothing, open battle untouched',
                  'error' not in reply and paid == 0 and after['open_mission_id'] == before['open_mission_id']
                  and after['open_mission_serial'] == before['open_mission_serial'],
                  (reply.get('error'), paid, before['open_mission_serial'], after['open_mission_serial']))
        return reply

    def unchanged(after, before, marks_before):
        return (after['gems'] == before['gems'] and continues() == marks_before
                and (after['mission_break_serial'], after['mission_break_info'], after['mission_break_state'])
                == (before['mission_break_serial'], before['mission_break_info'], before['mission_break_state']))

    def no_change(label, reply, before, marks_before):
        """A refused continue: Retry error (cmd 2) and nothing charged, recorded or marked."""
        after = info()
        check(f'{label}: refused with Retry, nothing charged or recorded',
              str((reply.get('error') or {}).get('iPD12YCr')) == '2' and unchanged(after, before, marks_before),
              (reply.get('error'), after['gems'] - before['gems'], after['mission_break_serial']))

    def repeated(label, reply, before, marks_before):
        """A repeat of a paid continue: an ordinary reply that changes nothing."""
        after = info()
        check(f'{label}: ordinary reply, nothing charged or recorded',
              'error' not in reply and 'fEi17cnx' in reply and unchanged(after, before, marks_before),
              (reply.get('error'), after['gems'] - before['gems'], after['mission_break_serial']))

    def charged(label, reply, before, serial, blob, delta=-1):
        after = info()
        check(f'{label}: charged {-delta}, resume record names this battle',
              'error' not in reply and after['gems'] - before['gems'] == delta
              and after['mission_break_serial'] == str(serial) and after['mission_break_info'] == blob
              and after['mission_break_state'] == 4,
              (reply.get('error'), after['gems'] - before['gems'], after['mission_break_serial'],
               after['mission_break_info']))

    # ---- A. MissionEnd ---------------------------------------------------------------
    def part_a(c):
        set_open(None, None)
        settles('A1 pre-serial save (both NULL), mission-id serial', c, 10, 10, True)
        settles('A1 the same result again', c, 10, 10, False)

        set_open(10, None)
        settles('A2 pre-serial battle of mission 10, result for mission 11', c, 11, 11, False)
        settles('A2 pre-serial battle of mission 10, its own result', c, 10, 10, True)
        settles('A2 repeated', c, 10, 10, False)

        start(c)
        current = start(c)
        settles('A3 legacy result after two issued starts (QC P1)', c, 10, 10, False)
        check('A3 the issued run is still the open battle', info()['open_mission_serial'] == current)
        settles('A3 the genuine result', c, 10, current, True)

        stale, current = start(c), start(c)
        settles('A4 superseded run of the same mission', c, 10, stale, False)
        settles('A4 current run', c, 10, current, True)
        settles('A4 current run retried', c, 10, current, False)

        current = start(c, 11)
        settles('A5 legacy result for another mission while 11 is open', c, 10, 10, False)
        settles('A5 run of mission 11', c, 11, current, True)

        current = start(c)
        for label, serial in (('A6 never-issued serial', UNKNOWN), ("A7 another player's serial", foreign)):
            before = info()
            reply = end(c, 10, serial)
            after = info()
            check(f'{label}: refused before anything changes', 'error' in reply
                  and after['zel'] == before['zel'] and after['open_mission_serial'] == current, reply.get('error'))
        settles('A8 the open run still settles after the refusals', c, 10, current, True)

    run(part_a)

    # A9: restart between start and end, for an issued run and a pre-serial one.
    holder = {}
    run(lambda c: holder.__setitem__('serial', start(c)))
    run(lambda c: settles('A9 issued run settles after a restart', c, 10, holder['serial'], True))
    set_open(None, None)
    run(lambda c: (settles('A9 pre-serial battle settles once after a restart', c, 10, 10, True),
                   settles('A9 and only once', c, 10, 10, False)))

    # ---- B. MissionContinue ------------------------------------------------------------
    def part_b(c):
        stale, current = start(c), start(c)
        before, marks = info(), continues()
        no_change('B1 superseded run (QC P1)', cont(c, stale, 'stale-blob'), before, marks)

        before = info()
        charged('B2 open run, first revival', cont(c, current, 'blob-1'), before, current, 'blob-1')
        check('B2 marked continued against mission 10', ('10',) == tuple(r[1] for r in continues()) or
              any(str(r[1]) == '10' for r in continues()), continues())

        before, marks = info(), continues()
        repeated('B3 the same request again (retry after a lost reply)', cont(c, current, 'blob-1'), before, marks)

        before = info()
        charged('B4 a second revival later in the same battle', cont(c, current, 'blob-2'), before, current, 'blob-2')

        before, marks = info(), continues()
        no_change('B5 body naming two battles', cont(c, current, 'blob-3', break_serial=stale), before, marks)
        no_change('B6 never-issued serial', cont(c, UNKNOWN, 'blob-4'), before, marks)
        no_change("B7 another player's serial", cont(c, foreign, 'blob-5'), before, marks)
        no_change('B7 status 0', cont(c, current, 'blob-6', status='0'), before, marks)
        settles('B the continued run still settles', c, 10, current, True)
        check('B settling cleared the resume record and the continue mark',
              info()['mission_break_state'] == 0 and continues() == [], (info()['mission_break_state'], continues()))

        # Pre-serial battles.
        set_open(10, None)
        before = info()
        charged('B8 pre-serial battle of mission 10', cont(c, 10, 'legacy-1'), before, 10, 'legacy-1')
        before, marks = info(), continues()
        no_change('B8 pre-serial battle of 10, continue for 11', cont(c, 11, 'legacy-2'), before, marks)
        set_open(None, None)
        before = info()
        charged('B8 pre-serial save, both NULL', cont(c, 10, 'legacy-3'), before, 10, 'legacy-3')
        issued = start(c)
        before, marks = info(), continues()
        no_change('B8 mission-id serial once a run has been issued', cont(c, 10, 'legacy-4'), before, marks)

        # Short on gems: refused with Retry -- the client then shows its Continue
        # prompt again instead of reviving -- and nothing is charged, recorded
        # or marked (Session 6; it used to be a free, recorded revival).
        execute('UPDATE user_info SET gems=0 WHERE id=?', (user,))
        before, marks = info(), continues()
        no_change('B9 no gems', cont(c, issued, 'poor'), before, marks)
        check('B9 not marked continued', not any(str(r[1]) == '10' for r in continues()), continues())

        # A failure part way undoes the charge and the mark with it.
        top_up()
        failing = start(c)
        execute("CREATE TRIGGER qc_break_fail BEFORE UPDATE OF mission_break_serial ON user_info"
                " BEGIN SELECT RAISE(ABORT, 'qc injected'); END")
        before, marks = info(), continues()
        reply = cont(c, failing, 'fail-blob')
        after = info()
        check('B10 injected failure: Retry error, nothing kept',
              str((reply.get('error') or {}).get('iPD12YCr')) == '2' and after['gems'] == before['gems']
              and continues() == marks and after['mission_break_state'] == before['mission_break_state'],
              (reply.get('error'), after['gems'] - before['gems'], continues()))
        execute('DROP TRIGGER qc_break_fail')
        before = info()
        charged('B10 the retry after the fault', cont(c, failing, 'fail-blob'), before, failing, 'fail-blob')

        # A MissionEnd racing a continue for the same run.
        racing = start(c)
        before = info()
        replies = {}
        threads = [threading.Thread(target=lambda: replies.__setitem__('end', end(c, 10, racing))),
                   threading.Thread(target=lambda: replies.__setitem__('cont', cont(c, racing, 'race')))]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        after = info()
        # Either order is legitimate: a continue that ran first was charged and
        # then settled away; one that ran second found the run closed and was
        # refused with Retry.  Never both, never neither.
        cont_paid = 'error' not in replies['cont']
        cont_refused = str((replies['cont'].get('error') or {}).get('iPD12YCr')) == '2'
        check('B12 racing end + continue: one settlement, the continue paid xor refused, nothing left open',
              'error' not in replies['end'] and cont_paid != cont_refused
              and after['zel'] - before['zel'] >= REPORTED_ZEL
              and after['gems'] - before['gems'] == (-1 if cont_paid else 0)
              and after['open_mission_serial'] == 0 and after['mission_break_state'] == 0 and continues() == [],
              (after['zel'] - before['zel'], after['gems'] - before['gems'], replies['cont'].get('error'),
               after['open_mission_serial'], after['mission_break_state'], continues()))
        settles('B12 the raced result cannot settle twice', c, 10, racing, False)
        holder['restart'] = start(c)
        before = info()
        charged('B11 continue before a restart', cont(c, holder['restart'], 'r-1'), before, holder['restart'], 'r-1')

    run(part_b)

    def part_b_restart(c):
        serial = holder['restart']
        before, marks = info(), continues()
        repeated('B11 after restart: the recorded continue repeated', cont(c, serial, 'r-1'), before, marks)
        before = info()
        charged('B11 after restart: a new revival', cont(c, serial, 'r-2'), before, serial, 'r-2')
        settles('B11 after restart: the run settles', c, 10, serial, True)

    run(part_b_restart)
    db.close()
    return check.summary('mission run ownership')


if __name__ == '__main__':
    sys.exit(main())
