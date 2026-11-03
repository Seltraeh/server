"""An outside reader of the save must never cost an acknowledged action or the connection.

    python scripts/test_db_commit_contention_wire.py EXE [--port 19997]

Drogon commits a transaction from its destructor, after the handler has
replied, and on SQLITE_BUSY neither retries nor rolls back, so one reader of
the save that outlasted a request lost that acknowledged action and left the
connection inside the transaction: every later request failed until a restart.
The server now commits explicitly before replying (gme::commitTransaction): a
commit that fails is rolled back on its own connection and answered with an
error.  A busy timeout makes short readers merely wait (standalone_frontend/
main.cpp), and gme::beginTransaction clears a connection left open anyway.

A  a 1 s reader across one craft: the reply waits for the commit, then it is
   acknowledged and in the save;
B  three more crafts succeed and another reader sees all four;
C  backup-API copies taken in a loop while crafts are sent: every craft and
   every copy succeeds;
E  QC's 6.5 s reader, beyond the 5 s busy timeout: the craft is refused, not
   acknowledged, and nothing of it -- product, materials, Karma -- is kept; a
   read-only request still works while the lock is held; the next craft
   succeeds; no "transaction begin" failure, no destructor commit failure;
F  a connection left inside an open transaction (debug console `dbwedge`, the
   state a failed destructor commit leaves): the next craft heals it and works;
G  restart: every acknowledged craft, and only those, persisted; a server
   started on the same save lists them and crafts again.
"""
from __future__ import annotations

import argparse
import sqlite3
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import Checker, Client, Fixture, IsolatedServer, backup_copy, qa_dir  # noqa: E402

RECIPE, PRODUCT, MATERIAL = 2005, 30000, 10400          # Famous Blade: 5 x 10400 + 500 Karma
COST_MATERIAL, COST_KARMA = 5, 500
BUSY_TIMEOUT = 5.0                                       # standalone_frontend/main.cpp


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19997)
    args = ap.parse_args()
    check = Checker()
    fx = Fixture.create(qa_dir('db_commit_contention'), port=args.port)
    with fx.db() as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,1000)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=1000', (user, MATERIAL))
        db.execute('DELETE FROM user_items WHERE user_id=? AND item_id=?', (user, PRODUCT))
        db.execute('DELETE FROM user_warehouse_rows WHERE user_id=? AND item_id=?', (user, PRODUCT))
        db.execute('UPDATE user_units SET eqip_item_id=0,eqip_item_frame_id=0 WHERE user_id=? AND eqip_item_id=?',
                   (user, PRODUCT))
        db.execute('UPDATE user_units SET eqip_item_id2=0,eqip_item_frame_id2=0 WHERE user_id=? AND eqip_item_id2=?',
                   (user, PRODUCT))
        db.execute('UPDATE user_info SET karma=1000000, max_warehouse_count=3200 WHERE id=?', (user,))
        db.execute('INSERT INTO user_town_facilities(user_id,facility_id,lv) VALUES (?,1,1)'
                   ' ON CONFLICT(user_id,facility_id) DO UPDATE SET lv=1', (user,))
        db.execute("INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,'21',2)"
                   ' ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2', (user,))
        db.commit()

    def state(conn):
        """(Famous Blades, materials, Karma) as stored."""
        def one(sql, *a):
            row = conn.execute(sql, a).fetchone()
            return row[0] if row else 0
        return (one('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?', user, PRODUCT),
                one('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?', user, MATERIAL),
                one('SELECT karma FROM user_info WHERE id=?', user))

    def peek():
        """state() as another reader sees it, or why it could not read."""
        try:
            with sqlite3.connect(fx.db_path, timeout=10) as probe:
                return state(probe)
        except sqlite3.OperationalError as e:
            return f'unreadable: {e}'

    def reader(seconds: float):
        """Hold a read transaction on the save for `seconds` from now; returns its thread."""
        started = threading.Event()

        def hold():
            conn = sqlite3.connect(fx.db_path, timeout=10, isolation_level=None)
            conn.execute('BEGIN')
            conn.execute('SELECT COUNT(*) FROM user_items').fetchone()
            started.set()
            time.sleep(seconds)
            conn.execute('COMMIT')
            conn.close()
        t = threading.Thread(target=hold, daemon=True)
        t.start()
        started.wait(10)
        return t

    acknowledged = 0
    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx, user)

        def craft():
            return c.call('ItemMix', body={'JTf2jY5o': [{'4HqhTf3a': f'{RECIPE}:1'}]})

        def settle():
            # BadgeInfo reads without a transaction on the server's one
            # connection, so its reply arrives only after anything queued there.
            return c.call('nJ3A7qFp', 'bGxX67KB', {})

        # A. a short reader: the reply waits for the commit
        t = reader(1.0)
        started = time.monotonic()
        reply = craft()
        waited = time.monotonic() - started
        t.join(15)
        acknowledged += 'error' not in reply
        check('A: a craft overlapped by a 1 s reader is acknowledged', 'error' not in reply, reply.get('error'))
        check(f'A: its reply came only after the commit, i.e. after the reader let go ({waited:.2f} s)',
              0.8 <= waited < BUSY_TIMEOUT, waited)
        settle()
        seen = peek()
        check('A: the craft is in the save', isinstance(seen, tuple) and seen[0] == 1, seen)

        # B. the connection keeps working
        replies = [craft() for _ in range(3)]
        acknowledged += sum('error' not in r for r in replies)
        check('B: three later crafts succeed', all('error' not in r for r in replies), [r.get('error') for r in replies])
        settle()
        seen = peek()
        check('B: another reader sees all four', isinstance(seen, tuple) and seen[0] == 4, seen)

        # C. backup copies racing a burst of crafts
        copies, stop = [], threading.Event()

        def copier():
            n = 0
            while not stop.is_set():
                target = fx.dir / f'race_copy_{n % 3}.sqlite'
                try:
                    backup_copy(fx.db_path, target)
                    copies.append('ok')
                except sqlite3.Error as e:
                    copies.append(f'error: {e}')
                n += 1
        ct = threading.Thread(target=copier, daemon=True)
        ct.start()
        replies = [craft() for _ in range(15)]
        stop.set()
        ct.join(30)
        settle()
        acknowledged += sum('error' not in r for r in replies)
        check(f'C: 15 crafts during {len(copies)} concurrent backup copies all succeed',
              len(copies) >= 3 and all('error' not in r for r in replies), [r.get('error') for r in replies][:5])
        check('C: every backup copy completed', copies and all(s == 'ok' for s in copies),
              [s for s in copies if s != 'ok'][:3])

        # E. QC's 6.5 s reader, beyond the busy timeout
        before = peek()
        t = reader(6.5)
        started = time.monotonic()
        refused = craft()
        waited = time.monotonic() - started
        during = settle()
        still_locked = t.is_alive()
        t.join(15)
        acknowledged += 'error' not in refused
        check(f'E: the craft outlasted by the reader is refused, not acknowledged ({waited:.1f} s)',
              'error' in refused, refused)
        check('E: a read-only request still answers while the reader holds its lock',
              'error' not in during and still_locked, (during.get('error'), still_locked))
        after_refusal = peek()
        check('E: nothing of the refused craft was kept (product, materials, Karma)', after_refusal == before,
              (before, after_refusal))
        later = craft()
        acknowledged += 'error' not in later
        check('E: the next craft succeeds -- the connection is usable', 'error' not in later, later.get('error'))
        settle()
        seen = peek()
        check('E: and it is in the save', isinstance(seen, tuple) and isinstance(before, tuple)
              and seen[0] == before[0] + 1 and seen[1] == before[1] - COST_MATERIAL
              and seen[2] == before[2] - COST_KARMA, (before, seen))
        log = server.log_text()
        check('E: the refused commit was rolled back on its own connection (no failed destructor commit)',
              'Transaction submission failed' not in log, '')
        check('E: no later "transaction begin" failure (no wedge)', 'Error occurred in transaction begin' not in log, '')

        # F. a connection left inside an open transaction heals at the next transaction
        with open(r'\\.\pipe\gimudebug_' + str(server.proc.pid), 'r+b', buffering=0) as pipe:
            def response():
                data = bytearray()
                while True:
                    b = pipe.read(1)
                    if not b:
                        raise RuntimeError('debug pipe disconnected')
                    if b == b'\x01':
                        return data.decode(errors='replace')
                    data.extend(b)
            response()
            pipe.write(b'dbwedge\n')
            said = response()
        check('F: the debug console left the connection inside a transaction',
              'left inside an open transaction' in said, said.strip())
        healed = craft()
        acknowledged += 'error' not in healed
        check('F: the next craft heals the connection and succeeds', 'error' not in healed, healed.get('error'))
        settle()
        check('F: the server logged the recovery',
              'beginTransaction: the connection was still inside a transaction' in server.log_text(), '')

    # G. restart persistence
    with sqlite3.connect(fx.db_path) as final:
        stored = state(final)
    check(f'G: after the server stopped, exactly the {acknowledged} acknowledged crafts persisted',
          stored[0] == acknowledged, (acknowledged, stored))
    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx, user)
        info = c.call('UserInfo')
        rows = [r for r in info.get('9wjrh74P', []) if str(r.get('kixHbe54')) == str(PRODUCT)]
        check('G: the restarted server lists every persisted copy', len(rows) == acknowledged, len(rows))
        again = c.call('ItemMix', body={'JTf2jY5o': [{'4HqhTf3a': f'{RECIPE}:1'}]})
        check('G: and crafts again', 'error' not in again, again.get('error'))
        c.call('nJ3A7qFp', 'bGxX67KB', {})
    with sqlite3.connect(fx.db_path) as final:
        check('G: that craft persisted too', state(final)[0] == acknowledged + 1, state(final))
    return check.summary('db commit contention')


if __name__ == '__main__':
    sys.exit(main())
