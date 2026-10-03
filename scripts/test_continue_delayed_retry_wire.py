"""MissionContinue retry identity: every revival a run has seen is remembered.

    python scripts/test_continue_delayed_retry_wire.py PATH_TO_DEBUG_EXE [--port 19996]

A copied save on an isolated server (out/qa/current/continue_delayed).  The
client body is MissionContinueRequest::createBody @0x13A7528 -- Kz7qfSs5
{k9cxD7Ba serial, j3g5P4cq 4|6} and 5PR2VmH1 {k9cxD7Ba serial, K2gIYm0h suspend
blob} -- and a client retry re-sends that same body byte for byte (the request
object is built once; see gme/handlers/MissionBreak.cpp).  So a revival is
named by its run serial plus status and blob, and the server keeps a durable
receipt per run in user_mission_continue_receipts.

Replies follow the decoded client (net/mission.kdl): an ordinary reply IS the
revival (loopContinue), so only an accepted or repeated -- paid -- revival gets
one; anything refused gets GmeErrorCommand::Retry (2), which sends the game-over
scene back to its Continue prompt.

Cases: A/A, A/B/A, concurrent copies, restart between attempts, distinct runs
of one mission, superseded/closed/foreign/never-issued runs, insufficient gems
(and that the refusal does not poison the funded try or let a stale copy rewind
a newer revival), and an injected transaction fault followed by its retry.
"""
import argparse
import concurrent.futures
import sys

from bf_testkit import Fixture, IsolatedServer, Client, Checker, mission_start_body, mission_end_body, started_serial
from bf_testkit import qa_dir  # noqa: E402

RETRY = '2'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19996)
    args = ap.parse_args()
    fx = Fixture.create(qa_dir('continue_delayed'), port=args.port)
    check = Checker()
    db = fx.db()
    user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    db.execute("INSERT OR IGNORE INTO user_campaign_missions (user_id, mission_id, state) VALUES (?, '10', 2)",
               (user,))
    db.commit()
    # FIXTURE: start from no Continue marks.  A lost or abandoned run on the live
    # save keeps its mark until that mission's next MissionStart (by design), so
    # the copy can carry one -- the player's mission-20 run on 2026-10-02 19:01
    # did -- and these checks compare the whole set.
    db.execute('DELETE FROM user_mission_continues WHERE user_id=?', (user,))
    db.commit()

    def execute(sql, params=()):
        db.execute(sql, params)
        db.commit()

    def fund(gems):
        execute('UPDATE user_info SET energy=20000, energy_full_ts=0, gems=? WHERE id=?', (gems, user))

    def state():
        row = db.execute('SELECT gems, mission_break_serial, mission_break_info, mission_break_state'
                         ' FROM user_info WHERE id=?', (user,)).fetchone()
        return tuple(row)

    def marks():
        return sorted(r[0] for r in db.execute('SELECT mission_id FROM user_mission_continues WHERE user_id=?',
                                               (user,)))

    def receipts(serial):
        return [tuple(r) for r in db.execute(
            'SELECT seq, accepted, gems_charged, deliveries FROM user_mission_continue_receipts'
            ' WHERE user_id=? AND run_serial=? ORDER BY seq', (user, str(serial)))]

    def start(c, mission=10):
        reply = c.call('MissionStart', body=mission_start_body(mission))
        check(f'MissionStart {mission} succeeds', 'error' not in reply, reply.get('error'))
        return started_serial(reply)

    def revive(c, serial, blob, status='4', break_serial=None):
        return c.call('MissionContinue', body={
            '6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
            'Kz7qfSs5': [{'k9cxD7Ba': str(serial), 'j3g5P4cq': status}],
            '5PR2VmH1': [{'k9cxD7Ba': str(serial if break_serial is None else break_serial),
                          'K2gIYm0h': blob}]})

    def ordinary(reply):
        return 'error' not in reply and 'fEi17cnx' in reply

    def refused(reply):
        err = reply.get('error') or {}
        return str(err.get('iPD12YCr')) == RETRY and bool(err.get('ZC0msu2L'))

    def header_gems(reply):
        """team_info brave_coin (03UGMHxF), the HUD's gem count."""
        team = reply['fEi17cnx'][0] if ordinary(reply) else {}
        return int(team['03UGMHxF']) if '03UGMHxF' in team else None

    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx, user_id=user)

        # ---- A/A and A/B/A in one run --------------------------------------------------
        fund(10)
        run = start(c)
        r1 = revive(c, run, 'qc-revival-A')
        check('A: first revival pays one gem and records A', ordinary(r1) and state() == (9, str(run), 'qc-revival-A', 4),
              (r1.get('error'), state()))
        r2 = revive(c, run, 'qc-revival-A')
        check('A/A: the immediate copy is an ordinary reply, not charged, record unchanged',
              ordinary(r2) and state() == (9, str(run), 'qc-revival-A', 4), (r2.get('error'), state()))
        r3 = revive(c, run, 'qc-revival-B')
        check('B: a genuine later revival pays again and records B',
              ordinary(r3) and state() == (8, str(run), 'qc-revival-B', 4), (r3.get('error'), state()))
        before = state()
        r4 = revive(c, run, 'qc-revival-A')
        check('A/B/A: the delayed copy of A charges nothing and leaves B\'s resume data',
              ordinary(r4) and state() == before, {'before': before, 'after': state(), 'error': r4.get('error')})
        check('A/B/A: the reply header carries the true balance', header_gems(r4) == 8, header_gems(r4))
        check('receipts: A and B in first-seen order, both paid once; A delivered three times',
              receipts(run) == [(1, 1, 1, 3), (2, 1, 1, 1)], receipts(run))
        check('the run is marked continued once', marks() == ['10'], marks())
        r5 = revive(c, run, 'qc-revival-A', status='6')
        check('same blob under auto-continue status 6 is a different revival (pays)',
              ordinary(r5) and state()[0] == 7 and state()[3] == 6, (r5.get('error'), state()))

        # ---- concurrent copies -----------------------------------------------------
        before = state()
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
            replies = list(pool.map(lambda _: revive(c, run, 'qc-revival-C'), range(3)))
        check('three concurrent copies of C: one charge, all ordinary replies',
              all(ordinary(r) for r in replies) and state() == (before[0] - 1, str(run), 'qc-revival-C', 4),
              ([r.get('error') for r in replies], before, state()))
        check('concurrent copies: one receipt row with three deliveries',
              [x for x in receipts(run) if x[0] == 4] == [(4, 1, 1, 3)], receipts(run))

        # ---- the run closes; copies of its revivals are refused ------------------------
        end = c.call('MissionEnd', body=mission_end_body(10, serial=run))
        check('the continued run settles', 'error' not in end and state()[3] == 0, (end.get('error'), state()))
        before, before_marks = state(), marks()
        r6 = revive(c, run, 'qc-revival-C')
        check('closed run: a late copy is refused with Retry, nothing changes',
              refused(r6) and state() == before and marks() == before_marks, (r6.get('error'), state()))

        # ---- distinct runs of the same mission ------------------------------------------
        fund(10)
        first = start(c)
        revive(c, first, 'same-blob')
        second = start(c)
        before = state()
        r7 = revive(c, first, 'same-blob')
        check('superseded run: refused with Retry, no charge, the new run\'s record untouched',
              refused(r7) and state() == before, (r7.get('error'), before, state()))
        r8 = revive(c, second, 'same-blob')
        check('the same blob in the next run of the same mission is a new revival and pays',
              ordinary(r8) and state() == (before[0] - 1, str(second), 'same-blob', 4), (r8.get('error'), state()))

        # ---- foreign, never-issued and two-battle bodies ---------------------------------
        foreign = db.execute("INSERT INTO user_mission_runs (user_id, mission_id, started_at)"
                             " VALUES ('qc-foreign-player', 10, 0)").lastrowid
        db.commit()
        before, before_marks = state(), marks()
        for label, reply in (("another player's serial", revive(c, foreign, 'f-1')),
                             ('never-issued serial', revive(c, 1_999_999_999, 'f-2')),
                             ('body naming two battles', revive(c, second, 'f-3', break_serial=first)),
                             ('status 0', revive(c, second, 'f-4', status='0'))):
            check(f'{label}: refused with Retry, nothing charged, recorded or marked',
                  refused(reply) and state() == before and marks() == before_marks, (reply.get('error'), state()))

        # ---- insufficient gems ----------------------------------------------------------
        fund(0)
        poor = start(c)
        before, before_marks = state(), marks()
        r9 = revive(c, poor, 'poor-A')
        check('no gems: refused with Retry; not charged, not recorded, not marked continued',
              refused(r9) and state() == before and marks() == before_marks, (r9.get('error'), state(), marks()))
        check('no gems: the refusal is remembered as unaccepted', receipts(poor) == [(1, 0, 0, 1)], receipts(poor))
        fund(5)
        r10 = revive(c, poor, 'poor-A')
        check('funded later, the same revival is accepted -- the refusal did not poison it',
              ordinary(r10) and state() == (4, str(poor), 'poor-A', 4) and '10' in marks(), (r10.get('error'), state()))
        r11 = revive(c, poor, 'poor-A')
        check('and a copy of it is then a paid repeat', ordinary(r11) and state()[0] == 4, (r11.get('error'), state()))

        fund(0)
        stale = start(c)
        revive(c, stale, 'stale-A')                     # refused: no gems
        fund(5)
        r12 = revive(c, stale, 'stale-B')               # a newer revival, paid
        before = state()
        r13 = revive(c, stale, 'stale-A')               # late copy of the refused try
        check('a late copy of a refused try after a newer revival: Retry, no charge, B kept',
              ordinary(r12) and refused(r13) and state() == before == (4, str(stale), 'stale-B', 4),
              (r12.get('error'), r13.get('error'), before, state()))

        # ---- transaction fault -------------------------------------------------------
        fund(5)
        faulty = start(c)
        execute("CREATE TRIGGER qc_receipt_fail BEFORE INSERT ON user_mission_continue_receipts"
                " BEGIN SELECT RAISE(ABORT, 'qc injected'); END")
        before, before_marks = state(), marks()
        r14 = revive(c, faulty, 'fault-A')
        check('injected fault: Retry error, nothing kept (gems, record, mark, receipt)',
              refused(r14) and state() == before and marks() == before_marks and receipts(faulty) == [],
              (r14.get('error'), state(), receipts(faulty)))
        execute('DROP TRIGGER qc_receipt_fail')
        r15 = revive(c, faulty, 'fault-A')
        check('the retry after the fault is accepted once', ordinary(r15) and state() == (4, str(faulty), 'fault-A', 4),
              (r15.get('error'), state()))
        log = server.log_text()
        check('the server log names the repeat and the refusals',
              'repeated a revival already paid for' in log and 'refused' in log)
        holder = {'run': faulty}

    # ---- restart between attempts --------------------------------------------------
    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx, user_id=user)
        before = state()
        r16 = revive(c, holder['run'], 'fault-A')
        check('after a restart the paid revival is still a repeat', ordinary(r16) and state() == before,
              (r16.get('error'), before, state()))
        r17 = revive(c, holder['run'], 'fault-B')
        check('after a restart a new revival still pays', ordinary(r17) and state() == (3, str(holder['run']), 'fault-B', 4),
              (r17.get('error'), state()))
    db.close()
    return check.summary('delayed Continue retry')


if __name__ == '__main__':
    sys.exit(main())
