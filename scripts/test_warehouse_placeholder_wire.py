"""#34: act on a crafted sphere before the client has been sent its real row id.

python scripts/test_warehouse_placeholder_wire.py PATH_TO_DEBUG_EXE [--port 19976]

The client applies a craft locally and names each row it adds INT_MAX - itemId
(ARM64 GameUtils::incWarehouseItem @0x118A304; Windows: helper RVA 0x6E5580 and
inline stores of 0x7FFFFFFF - itemId into the row's +0x14 index field at
0x6E546D / 0xD49AFE / 0xD703C8 / 0x4D2761), and ItemMix answers {}.  Until a reply
carrying 9wjrh74P replaces its list, a sell, lock or equip of that sphere arrives
under the placeholder (Windows ItemSellRequest 0x4B4A40 and ItemFavoriteRequest
0x4B4060 both send [row+0x14]).

The sequence below is the client's: craft, craft again, then lock / sell / equip
WITHOUT any UserInfo in between.  Requests use the client's numeric-string
encoding against an isolated SQLite backup.  Every assertion reads the rows,
aggregate stock, Zel and unit columns -- including that a placeholder never
touches a copy the client is showing under its real id (here a real high id,
2097123), and that every refusal or injected failure changes nothing.
"""
import argparse
import sys
from bf_testkit import Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir, replay_warehouse_migration  # noqa: E402

INT_MAX = 2147483647
SPHERE, OTHER_SPHERE, MATERIAL, REGULAR = 30000, 30001, 10400, 20000
HIGH_REAL_ID = 2097123


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19976)
    args = ap.parse_args()
    fx = Fixture.create(qa_dir('wh_placeholder'), port=args.port)
    check = Checker()
    with fx.db() as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        # The live save is migrated; seed the PRE-migration state this suite tests
        # (bf_testkit.replay_warehouse_migration).
        replay_warehouse_migration(db)
        # One copy of sphere 30000 under a REAL high id the client will be
        # shown, nothing locked, nothing equipped.
        db.execute('INSERT INTO user_items(user_id,item_id,item_num,favorite_flg) VALUES (?,?,1,0)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=1,favorite_flg=0', (user, SPHERE))
        db.execute('UPDATE user_items SET instance_id=? WHERE user_id=? AND item_id=?', (HIGH_REAL_ID, user, SPHERE))
        db.execute('UPDATE user_items SET favorite_flg=0 WHERE user_id=?', (user,))
        db.execute('UPDATE user_units SET eqip_item_id=0,eqip_item_id2=0,eqip_item_frame_id=0,'
                   'eqip_item_frame_id2=-1,sphere_ext=0 WHERE user_id=?', (user,))
        unit_a = db.execute('SELECT user_unit_id FROM user_units WHERE user_id=? LIMIT 1', (user,)).fetchone()[0]
        template = dict(db.execute('SELECT * FROM user_units WHERE user_unit_id=?', (unit_a,)).fetchone())
        template.pop('user_unit_id')
        unit_b = db.execute(f"INSERT INTO user_units ({','.join(template)}) VALUES ({','.join('?' for _ in template)})",
                            tuple(template.values())).lastrowid
        # Real recipe 2005: sphere 30000 from five 10400 and 500 Karma (the
        # synthesis prerequisites test_sphere_qc_wire.py uses); enough for 3.
        db.execute('INSERT INTO user_town_facilities(user_id,facility_id,lv) VALUES (?,1,1)'
                   ' ON CONFLICT(user_id,facility_id) DO UPDATE SET lv=1', (user,))
        db.execute("INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,'21',2)"
                   ' ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2', (user,))
        db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,15)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=15', (user, MATERIAL))
        # A regular item (MST cap 99) four short of a full stack.
        db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,95)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=95', (user, REGULAR))
        db.execute('UPDATE user_info SET karma=5000 WHERE id=?', (user,))
        db.commit()

    placeholder = INT_MAX - SPHERE
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()

        def stock(item=SPHERE):
            r = db.execute('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?', (user, item)).fetchone()
            return r[0] if r else 0

        def rows(item=SPHERE):
            return {r[0]: (r[1], r[2], r[3]) for r in db.execute(
                'SELECT instance_id,item_num,favorite_flg,equip_unit_id FROM user_warehouse_rows'
                ' WHERE user_id=? AND item_id=? ORDER BY instance_id', (user, item))}

        def seen():
            try:
                return db.execute('SELECT warehouse_seen_id FROM user_info WHERE id=?', (user,)).fetchone()[0]
            except Exception as e:          # an executable without the column
                return f'unavailable: {e}'

        def zel():
            return db.execute('SELECT zel FROM user_info WHERE id=?', (user,)).fetchone()[0]

        def karma():
            return db.execute('SELECT karma FROM user_info WHERE id=?', (user,)).fetchone()[0]

        def equipped(unit):
            return db.execute('SELECT eqip_item_id FROM user_units WHERE user_unit_id=?', (unit,)).fetchone()[0]

        def state():
            return {t: [tuple(r) for r in db.execute(
                f'SELECT * FROM {t} WHERE ' + ('id' if t == 'user_info' else 'user_id') + '=?', (user,))]
                for t in ('user_items', 'user_units', 'user_warehouse_rows', 'user_info')}

        def lock(values):
            return c.call('I8il6EiI', 'aRoIftRy',
                          {'VSRPkdId': [{'n6E8iMf3': str(v), '5JbjC3Pp': '1'} for v in values]})

        def sell(values):
            return c.call('qDQerU74', '73aFNjPu',
                          {'M73i1c5U': [{'n6E8iMf3': str(v), 'wgV86x1q': str(q)} for v, q in values]})

        def equip(ops):
            return c.call('ItemSphereEqp', body={'wx1ZLFj9': [
                {'a2utCvs8': ','.join(':'.join(map(str, x)) for x in ops)}]})

        def craft(count):
            return c.call('ItemMix', body={'JTf2jY5o': [{'4HqhTf3a': f'2005:{count}'}]})

        # ---- the client is shown its warehouse (login) --------------------------
        view = c.call('UserInfo')
        sent = view['9wjrh74P']
        known = [int(r['n6E8iMf3']) for r in sent if r['kixHbe54'] == str(SPHERE) and int(r['wgV86x1q']) > 0]
        check('setup: the client is shown one sphere copy, under its real high id', known == [HIGH_REAL_ID], known)
        mark = seen()
        check('seen mark: covers the newest row the client was sent',
              isinstance(mark, int) and mark >= max(int(r['n6E8iMf3']) for r in sent), mark)
        regular_known = [int(r['n6E8iMf3']) for r in sent if r['kixHbe54'] == str(REGULAR)]
        check('setup: regular item shown as one row of 95',
              [r['wgV86x1q'] for r in sent if r['kixHbe54'] == str(REGULAR)] == ['95'], regular_known)

        # ---- the client's own model of its list ---------------------------------
        # The Windows client (BraveFrontier.Windows.exe), as disassembled:
        #   GameUtils::incWarehouseItem  a max-stack-1 sphere gets ONE row per
        #                                crafted copy, each id INT_MAX - item_id
        #   ItemFavoriteRequest (0x4B4060)  one key per ROW (getAllKeys 0x5C09C0,
        #                                duplicates kept), each looked up with
        #                                getObjectAtItemIndex (0x20D090, FIRST
        #                                row with that id), sent only if locked
        #   setFavorite (arm64 0x12BCA34)  after the reply, every row's flag =
        #                                "is my id in the reply's VSRPkdId"
        # Town crafts go through MyTownItemMixConnectScene (sent at once) and
        # sells through ItemSellConnectScene (at once), while a lock is QUEUED by
        # ItemDetailScene::touchEnded and built when StepScene sends the queue.
        client_rows = [[HIGH_REAL_ID, False]]          # [id, locked] in list order

        def client_craft(n):
            client_rows.extend([placeholder, False] for _ in range(n))

        def client_lock_request():
            keys = [row[0] for row in client_rows]
            first = {}
            for row in client_rows:
                first.setdefault(row[0], row)
            return [k for k in keys if first[k][1]]

        def client_apply_reply(reply):
            listed = {int(e['n6E8iMf3']) for e in reply.get('VSRPkdId', []) if e.get('5JbjC3Pp') == '1'}
            if 'VSRPkdId' in reply:
                for row in client_rows:
                    row[1] = row[0] in listed

        def client_shows_server():
            """Client-shown locked copies per id == server-locked copies it stands for."""
            server = rows()
            unsent = [i for i in server if i > mark]
            shown_real = {row[0]: row[1] for row in client_rows if row[0] != placeholder}
            shown_new = sum(1 for row in client_rows if row[0] == placeholder and row[1])
            real_ok = all(server[i][1] == int(v) for i, v in shown_real.items() if i in server)
            new_locked = sum(1 for i in unsent if server[i][1] and server[i][0] > 0)
            return real_ok and shown_new == new_locked, (shown_real, shown_new, new_locked)

        # ---- one craft, then one more: the reply stays {} -------------------------
        zel_start, karma_start = zel(), karma()
        r1 = craft(1)
        r2 = craft(1)
        client_craft(2)
        check('craft: single and repeated batches accepted, replies do not replace the list',
              all('error' not in r and '9wjrh74P' not in r for r in (r1, r2)), (r1, r2))
        check('craft: two spheres for exactly 10 material and 1000 Karma',
              stock() == 3 and stock(MATERIAL) == 5 and karma() == karma_start - 1000,
              (stock(), stock(MATERIAL), karma()))

        # ---- an injected failure mid-sale changes nothing -------------------------
        db.execute("CREATE TRIGGER qc_placeholder_sell_fail BEFORE UPDATE OF zel ON user_info"
                   " BEGIN SELECT RAISE(ABORT,'QC sell'); END")
        db.commit()
        before = state()
        sell([(placeholder, 1)])
        check('rollback: a failed placeholder sale restores rows, stock and Zel', state() == before)
        db.execute('DROP TRIGGER qc_placeholder_sell_fail')
        db.commit()

        # ---- sell one new copy (sent at once) --------------------------------------
        z = zel()
        r = sell([(placeholder, 1)])
        client_rows.pop()                                  # one placeholder row fewer
        now = rows()
        # Crafted copies get their persistent rows at the next warehouse access
        # (gme::syncWarehouse), so the unsent ids are read after it.
        first_two = sorted(i for i in now if isinstance(mark, int) and i > mark)
        check('sell: accepted', 'error' not in r, r)
        check('sell: one sphere leaves stock', stock() == 2)
        check('sell: the oldest unsent copy was sold',
              len(first_two) == 2 and now[first_two[0]] == (0, 0, 0) and now[first_two[1]] == (1, 0, 0), now)
        check('sell: the real high-id copy is untouched', now[HIGH_REAL_ID] == (1, 0, 0), now[HIGH_REAL_ID])
        check('sell: Zel credited', zel() > z, (z, zel()))

        # ---- lock the new copy (queued, built at send time) ------------------------
        client_rows[1][1] = True                           # the first placeholder row
        body = client_lock_request()
        check('lock: the client sends the placeholder once per placeholder row it holds',
              body == [placeholder], body)
        r = lock(body)
        client_apply_reply(r)
        now = rows()
        check('lock: accepted and echoed under the client\'s own id',
              'error' not in r and {e['n6E8iMf3'] for e in r.get('VSRPkdId', [])} == {str(placeholder)}, r)
        check('lock: the one unsent copy the client holds is locked', now[first_two[1]] == (1, 1, 0), now)
        check('lock: the real high-id copy is untouched', now.get(HIGH_REAL_ID) == (1, 0, 0), now.get(HIGH_REAL_ID))
        ok, detail = client_shows_server()
        check('lock: what the client shows after the reply is what the server holds', ok, detail)

        # ---- craft once more: a new, unlocked copy beside the locked one -----------
        r3 = craft(1)
        client_craft(1)
        check('craft: a third copy for 5 material and 500 Karma', 'error' not in r3 and stock() == 3
              and stock(MATERIAL) == 0 and karma() == karma_start - 1500, (stock(), stock(MATERIAL), karma()))
        ok, detail = client_shows_server()
        check('craft: the new copy shows unlocked on both sides until the next lock reply', ok, detail)

        # ---- equip new copies on two units in one request ---------------------------
        r = equip([(placeholder, 0, unit_a, SPHERE, 0), (placeholder, 0, unit_b, SPHERE, 0)])
        now = rows()
        unseen = sorted(i for i in now if isinstance(mark, int) and i > mark)
        check('equip: accepted', 'error' not in r, r)
        check('equip: both units hold the sphere', equipped(unit_a) == SPHERE and equipped(unit_b) == SPHERE,
              (equipped(unit_a), equipped(unit_b)))
        check('equip: the unlocked copy went first, the locked one second (lock stays with its copy)',
              len(unseen) == 3 and now[unseen[2]] == (0, 0, unit_a) and now[unseen[1]] == (0, 1, unit_b), now)
        check('equip: stock falls by exactly two, the real high-id copy untouched',
              stock() == 1 and now[HIGH_REAL_ID] == (1, 0, 0), (stock(), now[HIGH_REAL_ID]))
        locked_copy = unseen[1] if len(unseen) == 3 else None

        before = state()
        equip([(placeholder, 0, unit_a, SPHERE, 0)])
        check('equip: resending a unit under the placeholder is a no-op', state() == before)

        # ---- refusals change nothing -------------------------------------------
        before = state()
        sell([(placeholder, 1)])
        check('refuse: no free unsent copy left -- the real copy is never sold instead', state() == before)
        before = state()
        sell([(HIGH_REAL_ID, 1), (placeholder, 1)])
        check('refuse: a mixed sale with an unresolvable placeholder rolls back entirely', state() == before)
        before = state()
        sell([(INT_MAX - MATERIAL, 1)])
        check('refuse: a placeholder for a species with no unsent copy', state() == before)
        before = state()
        equip([(placeholder, 0, unit_a, OTHER_SPHERE, 0)])
        check('refuse: a placeholder cannot equip a different species', state() == before)
        before = state()
        sell([(INT_MAX - 647, 1)])
        check('refuse: INT_MAX minus a non-item id is not a placeholder', state() == before)

        # ---- the real high id still sells exactly --------------------------------
        z = zel()
        sell([(HIGH_REAL_ID, 1)])
        check('exact: the real high id sells itself', rows()[HIGH_REAL_ID][0] == 0 and stock() == 0 and zel() > z,
              (rows()[HIGH_REAL_ID], stock()))

        # ---- regular stacks: the overflow of a grant ----------------------------
        db.execute('UPDATE user_items SET item_num=item_num+10 WHERE user_id=? AND item_id=?', (user, REGULAR))
        db.commit()
        # The client tops the 95 up to 99 and names the overflow 6 by placeholder.
        before = state()
        sell([(INT_MAX - REGULAR, 7)])
        after = state()
        check('regular: cannot sell more than the unsent overflow',
              after['user_items'] == before['user_items'] and stock(REGULAR) == 105)
        r = sell([(INT_MAX - REGULAR, 6)])
        reg = rows(REGULAR)
        check('regular: the unsent overflow of 6 sells, the known stack stays full',
              'error' not in r and stock(REGULAR) == 99 and reg.get(regular_known[0], (None,))[0] == 99, reg)

        # ---- a warehouse list goes out: placeholders are spent -------------------
        view = c.call('UserInfo')
        favorites = {int(e['n6E8iMf3']) for e in view.get('VSRPkdId', []) if e['5JbjC3Pp'] == '1'}
        listed = {int(r['n6E8iMf3']): r['wgV86x1q'] for r in view['9wjrh74P'] if r['kixHbe54'] == str(SPHERE)}
        check('refresh: the locked copy is listed as locked under its real id',
              locked_copy in favorites and listed.get(locked_copy) == '1', (favorites, listed))
        # A worn copy goes out as an ordinary count-1 row, named by its unit's
        # frame: the client's sphere picker skips a 0-count sphere row, and its
        # worn map is keyed on the frame (item_num / equipitem_frame_id in
        # net/user.kdl).  Listed "at zero" until 2026-10-02.
        frames = {int(u['edy7fq3L']): int(u['0R3qTPK9']) for u in view.get('4ceMWH6k', [])}
        check('refresh: both worn copies are listed as count-1 rows named by their units\' frames',
              len(unseen) == 3 and listed.get(locked_copy) == '1' and listed.get(unseen[2]) == '1'
              and frames.get(unit_b) == locked_copy and frames.get(unit_a) == unseen[2],
              (listed, frames.get(unit_a), frames.get(unit_b)))
        check('refresh: seen mark advanced past every row', isinstance(seen(), int) and seen() >= max(listed))
        before = state()
        lock([placeholder])
        check('stale: after refresh a placeholder resolves to nothing, locks survive', state() == before)
        before = state()
        equip([(0, 0, unit_b, 0, 0)])
        after_rows = rows()
        check('unequip: the locked copy returns to storage with its lock and id',
              locked_copy is not None and after_rows[locked_copy] == (1, 1, 0) and stock() == 1, after_rows)
        db.close()

    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx)
        db = fx.db()
        persisted = seen()
        check('restart: seen mark persists', isinstance(persisted, int) and persisted >= max(rows()), persisted)
        before = state()
        sell([(placeholder, 1)])
        check('restart: a stale placeholder still sells nothing', state() == before)
        check('restart: the returned copy kept its id and lock',
              locked_copy is not None and rows()[locked_copy] == (1, 1, 0), rows())
        db.close()
    return check.summary('warehouse placeholder')


if __name__ == '__main__':
    sys.exit(main())
