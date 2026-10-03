"""#34: after every lock reply, the server holds what the Windows client shows.

python scripts/test_warehouse_consumer_model_wire.py PATH_TO_DEBUG_EXE [--port 19991]
       [--out out/qa/current/wh_consumer_model]

The client side is modelled from BraveFrontier.Windows.exe (x86), not assumed:

  GameUtils::incWarehouseItem   tops up rows below the item's max stack, then
                                appends new rows named INT_MAX - item_id; a
                                max-stack-1 sphere (30000: m9gd5h1u = 1) gets one
                                new row per crafted copy, all sharing that id
  ItemFavoriteRequest (RVA 0x4B4060)
                                one key per ROW (getAllKeys RVA 0x5C09C0 keeps
                                duplicates), each resolved by getObjectAtItemIndex
                                (RVA 0x20D090: the FIRST row with that id), sent as
                                {n6E8iMf3: id, 5JbjC3Pp: 1} only when that row's
                                lock flag ([row+0x40]) is set
  UserWarehouseInfoList::setFavorite (arm64 @0x12BCA34)
                                after a reply carrying VSRPkdId every row's flag is
                                "is my id in that list"; a reply without it leaves
                                the flags as the player set them
  9wjrh74P                      replaces the list outright

So a lock on the FIRST of k crafted copies goes up as the placeholder k times and
comes back as all k locked; a lock on the 2nd..kth is never sent and comes back
as none.  The server must end each exchange holding exactly what the client then
shows: every real id's lock, and as many locked unsent copies as the client
shows locked placeholder rows.  A lock request built before a warehouse list the
client never received must not cost any existing lock.
"""
import argparse
import sys
from bf_testkit import Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir  # noqa: E402

INT_MAX = 2147483647
SPHERE, MATERIAL = 30000, 10400
P = INT_MAX - SPHERE


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19991)
    ap.add_argument('--out', default=str(qa_dir('wh_consumer_model')))
    args = ap.parse_args()
    fx = Fixture.create(args.out, port=args.port)
    check = Checker()
    with fx.db() as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        db.execute('INSERT INTO user_items(user_id,item_id,item_num,favorite_flg) VALUES (?,?,3,0)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=3,favorite_flg=0', (user, SPHERE))
        db.execute('UPDATE user_items SET favorite_flg=0 WHERE user_id=?', (user,))
        db.execute('UPDATE user_units SET eqip_item_id=0,eqip_item_id2=0,eqip_item_frame_id=0,'
                   'eqip_item_frame_id2=-1,sphere_ext=0 WHERE user_id=?', (user,))
        unit = db.execute('SELECT user_unit_id FROM user_units WHERE user_id=? LIMIT 1', (user,)).fetchone()[0]
        # Recipe 2005: sphere 30000 from five 10400 and 500 Karma, enough for 8.
        db.execute('INSERT INTO user_town_facilities(user_id,facility_id,lv) VALUES (?,1,1)'
                   ' ON CONFLICT(user_id,facility_id) DO UPDATE SET lv=1', (user,))
        db.execute("INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,'21',2)"
                   ' ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2', (user,))
        db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,40)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=40', (user, MATERIAL))
        db.execute('UPDATE user_info SET karma=100000 WHERE id=?', (user,))
        db.commit()

    client_rows = []                     # the client's list: [id, locked], list order

    def server_rows(db):
        return {r[0]: (r[1], r[2], r[3]) for r in db.execute(
            'SELECT instance_id,item_num,favorite_flg,equip_unit_id FROM user_warehouse_rows'
            ' WHERE user_id=? AND item_id=? ORDER BY instance_id', (user, SPHERE))}

    def run(body):
        with IsolatedServer(args.exe, fx):
            c = Client(fx, user_id=user)
            db = fx.db()
            try:
                body(c, db)
            finally:
                db.close()

    def receive_list(reply):
        """9wjrh74P replaces the list; VSRPkdId then sets every row's flag."""
        locked = {int(e['n6E8iMf3']) for e in reply.get('VSRPkdId', []) if e.get('5JbjC3Pp') == '1'}
        client_rows[:] = [[int(r['n6E8iMf3']), int(r['n6E8iMf3']) in locked]
                          for r in reply.get('9wjrh74P', []) if r['kixHbe54'] == str(SPHERE)]

    def lock_body():
        first = {}
        for row in client_rows:
            first.setdefault(row[0], row)
        return [row[0] for row in client_rows if first[row[0]][1]]

    def apply_lock_reply(reply):
        if 'VSRPkdId' in reply:
            locked = {int(e['n6E8iMf3']) for e in reply['VSRPkdId'] if e.get('5JbjC3Pp') == '1'}
            for row in client_rows:
                row[1] = row[0] in locked

    def send_locks(c):
        body = lock_body()
        reply = c.call('I8il6EiI', 'aRoIftRy', {'VSRPkdId': [{'n6E8iMf3': str(v), '5JbjC3Pp': '1'} for v in body]})
        apply_lock_reply(reply)
        return body, reply

    def agrees(db, mark):
        """Server holds what the client shows: per real id, and in count for the placeholders."""
        server = server_rows(db)
        real = {row[0]: row[1] for row in client_rows if row[0] != P}
        wrong = {i: (server.get(i), v) for i, v in real.items() if i in server and server[i][1] != int(v)}
        shown = sum(1 for row in client_rows if row[0] == P and row[1])
        held = sum(1 for i, (n, f, u) in server.items() if i > mark and f and n > 0 and not u)
        return not wrong and shown == held, {'real mismatches': wrong, 'placeholders shown locked': shown,
                                             'unsent copies locked': held}

    state = {}

    def part_one(c, db):
        # The client logs in and is shown three real copies.
        receive_list(c.call('UserInfo'))
        real = [row[0] for row in client_rows]
        check('setup: three real copies shown', len(real) == 3, real)
        r_lock, r_free, r_eq = real
        # Lock one and equip another under their real ids.
        for row in client_rows:
            row[1] = row[0] in (r_lock, r_eq)
        body, reply = send_locks(c)
        check('setup: real-id locks sent and applied', sorted(body) == sorted([r_lock, r_eq])
              and server_rows(db)[r_lock][1] == 1 and server_rows(db)[r_eq][1] == 1, (body, reply))
        c.call('ItemSphereEqp', body={'wx1ZLFj9': [{'a2utCvs8': f'{r_eq}:0:{unit}:{SPHERE}:0'}]})
        check('setup: the locked copy is equipped and keeps its lock', server_rows(db)[r_eq] == (0, 1, unit),
              server_rows(db)[r_eq])
        receive_list(c.call('UserInfo'))
        mark = db.execute('SELECT warehouse_seen_id FROM user_info WHERE id=?', (user,)).fetchone()[0]

        # A: craft three; lock the FIRST new row.
        crafted = c.call('ItemMix', body={'JTf2jY5o': [{'4HqhTf3a': '2005:3'}]})
        client_rows.extend([P, False] for _ in range(3))
        check('A: three crafted in one batch, reply leaves the list alone', 'error' not in crafted
              and '9wjrh74P' not in crafted, crafted)
        next(row for row in client_rows if row[0] == P)[1] = True
        body, reply = send_locks(c)
        check('A: the request carries the placeholder once per crafted row', body.count(P) == 3, body)
        check('A: the client now shows all three new copies locked',
              sum(1 for row in client_rows if row[0] == P and row[1]) == 3, client_rows)
        ok, detail = agrees(db, mark)
        check('A: the server holds exactly that -- all three unsent copies locked', ok, detail)
        check('A: the real copies kept their own locks', server_rows(db)[r_lock][1] == 1
              and server_rows(db)[r_free][1] == 0 and server_rows(db)[r_eq] == (0, 1, unit), server_rows(db))
        receive_list(c.call('UserInfo'))
        ok, detail = agrees(db, db.execute('SELECT warehouse_seen_id FROM user_info WHERE id=?', (user,)).fetchone()[0])
        check('A: after the next list every copy has its own id and the same lock on both sides', ok, detail)
        state['a_locked'] = sorted(i for i, (n, f, u) in server_rows(db).items() if f)

        # B: craft three more; lock the SECOND new row, then the THIRD.
        mark = db.execute('SELECT warehouse_seen_id FROM user_info WHERE id=?', (user,)).fetchone()[0]
        c.call('ItemMix', body={'JTf2jY5o': [{'4HqhTf3a': '2005:3'}]})
        client_rows.extend([P, False] for _ in range(3))
        for position in (1, 2):
            new_rows = [row for row in client_rows if row[0] == P]
            for row in new_rows:
                row[1] = False
            new_rows[position][1] = True
            body, reply = send_locks(c)
            check(f'B: a lock on new row {position + 1} is not in the request', P not in body, body)
            check(f'B: the reply shows it gone on the client (a client limitation, see the handler)',
                  not any(row[1] for row in client_rows if row[0] == P), client_rows)
            ok, detail = agrees(db, mark)
            check(f'B: row {position + 1}: the server locked none of the new copies either', ok, detail)
            check(f'B: row {position + 1}: every earlier lock survived',
                  all(server_rows(db)[i][1] == 1 for i in state['a_locked']), server_rows(db))

        # C: a warehouse list the client never receives.
        before_locks = {i: f for i, (n, f, u) in server_rows(db).items()}
        c.call('UserInfo')                                   # reply lost: client_rows unchanged
        new_rows = [row for row in client_rows if row[0] == P]
        new_rows[0][1] = True
        body, reply = send_locks(c)
        after_locks = {i: f for i, (n, f, u) in server_rows(db).items()}
        check('C: a lock set naming copies from before a lost list is refused whole',
              P in body and 'VSRPkdId' not in reply and after_locks == before_locks, (body, reply))
        before = server_rows(db)
        sold = c.call('qDQerU74', '73aFNjPu', {'M73i1c5U': [{'n6E8iMf3': str(P), 'wgV86x1q': '1'}]})
        check('C: a sale of such a copy is refused, nothing sold or duplicated',
              'error' not in sold and server_rows(db) == before, sold)
        receive_list(c.call('UserInfo'))
        ok, detail = agrees(db, db.execute('SELECT warehouse_seen_id FROM user_info WHERE id=?', (user,)).fetchone()[0])
        check('C: once a list arrives the two agree again, earlier locks intact', ok and
              all(server_rows(db)[i][1] == 1 for i in state['a_locked']), detail)
        state['final'] = server_rows(db)

    run(part_one)

    def part_two(c, db):
        check('restart: every copy keeps its id, count, lock and unit', server_rows(db) == state['final'],
              server_rows(db))
        receive_list(c.call('UserInfo'))
        ok, detail = agrees(db, db.execute('SELECT warehouse_seen_id FROM user_info WHERE id=?', (user,)).fetchone()[0])
        check('restart: the client list shows the same locks', ok, detail)

    run(part_two)
    return check.summary('warehouse consumer model')


if __name__ == '__main__':
    sys.exit(main())
