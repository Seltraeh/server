"""#34: crafted copies get their own row id before any screen can act on them.

python scripts/test_craft_row_identity_wire.py PATH_TO_DEBUG_EXE [--port 19971]
       [--out out/qa/current/craft_row_identity]

Player report, 2026-10-03 21:58:01 (deploy/log/http_log_4P5GELTF_20261003_215801.log):
one ItemMix batch "2005:19" -- nineteen Famous Blades (item 30000, max stack 1)
-- answered {}, then a broken Equip Sphere list.  The consumer, from the Windows
client BraveFrontier.Windows.exe (x86, sha256 fb4f3c25...; arm64 agrees):

  craft, at the tap        MyTownSphereMixScene::createItem (x86 0xA5FC50) and
                           MyTownItemMixScene::mixItem (0xA4EAC0) spend Karma and
                           materials and call GameUtils::incWarehouseItem (0xD49790):
                           a max-stack-1 item gets one new row per copy, every one
                           named 0x7FFFFFFF - itemId (store @0xD49AFE); a stackable
                           item tops up rows below its cap, then appends overflow
  send, later              createItemMixConnectList (0x384160) queues the batch when
                           the craft scenes go invisible -- a new request per 50
                           crafts -- and StepScene (~0x829D70) sends the queue in
                           order through accessPhp
  reply                    parseTag (0x493170) gives every body key to parseBodyTag
                           (0x493770), whatever the request: 9wjrh74P replaces the
                           list, then setFavorite re-derives every lock from the
                           VSRPkdId set (0x4951B5); keys are parsed in sorted order
  Equip Sphere list        each entry's frame id is IntToString(row id) (0x8D3565),
                           and every sprite, label and visibility flag of the row is
                           keyed on it; setSphereList skips a row whose key already
                           exists (0x8D0F95) -- so N rows with one id are one row

The ItemMix reply now carries the whole warehouse; this suite checks that every
copy then has a unique id, that each copy-specific action reaches exactly the
copy chosen, and that stock, Karma, locks and equipment are conserved.  The
model below is the recovered consumer; wire tests cannot certify drawing, which
stays a client check (docs/CLIENT_SESSION_8_CHECKLIST.md).
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Client, Fixture, IsolatedServer, qa_dir  # noqa: E402

INT_MAX = 2147483647
SPHERE, MATERIAL = 30000, 10400                 # recipe 2005: 5 x 10400 + 500 Karma -> 1 x 30000
CURE, GRASS, DROP = 20000, 10000, 10300          # recipe 1009: 10000 + 10300 -> 1 x 20000 (cap 99)
WAREHOUSE, LOCKS, DICTIONARY, UNITS = '9wjrh74P', 'VSRPkdId', 'bd5Rj6pN', '4ceMWH6k'


def item_mst():
    rows = next(iter(json.loads((ROOT / 'deploy/mst/item_mst.json').read_text(encoding='utf-8')).values()))
    return {int(r['kixHbe54']): r for r in rows}


ITEMS = item_mst()


def cap(item):
    n = int(ITEMS[item].get('m9gd5h1u') or 0)
    return n if n > 0 else 10 ** 9


def is_sphere(item):
    return ITEMS.get(item, {}).get('h0K7wjeH') in ('3', '6')


class ClientWarehouse:
    """UserWarehouseInfoList and ItemFavoriteInfoList as the client keeps them."""

    def __init__(self):
        self.rows = []              # [id, item, count, locked], list order
        self.favorites = set()      # ItemFavoriteInfoList

    def receive(self, reply):
        # A zero-row array never reaches readParam; both keys end in setFavorite.
        for key in sorted(k for k in (WAREHOUSE, LOCKS) if reply.get(k)):
            if key == WAREHOUSE:
                self.rows = [[int(r['n6E8iMf3']), int(r['kixHbe54']), int(r['wgV86x1q']), False]
                             for r in reply[WAREHOUSE]]
            else:
                self.favorites = {int(e['n6E8iMf3']) for e in reply[LOCKS] if e.get('5JbjC3Pp') == '1'}
            for row in self.rows:
                row[3] = row[0] in self.favorites

    def inc(self, item, n):
        """GameUtils::incWarehouseItem: top up below the cap, then new rows named INT_MAX - item."""
        for row in self.rows:
            if n and row[1] == item and 0 < row[2] < cap(item):
                add = min(n, cap(item) - row[2])
                row[2] += add
                n -= add
        while n:
            add = min(n, cap(item))
            self.rows.append([INT_MAX - item, item, add, False])
            n -= add

    def dec(self, item, n):
        for row in self.rows:
            if n and row[1] == item and row[2] > 0:
                take = min(n, row[2])
                row[2] -= take
                n -= take

    def craft(self, product, materials, times):
        """One tap per craft, as createItem / mixItem apply it before anything is sent."""
        for _ in range(times):
            for item, qty in materials.items():
                self.dec(item, qty)
            self.inc(product, 1)

    def count(self, item):
        return sum(r[2] for r in self.rows if r[1] == item)

    def sphere_list(self, item=None):
        """ItemSphereSelectScene's entries: sphere rows above 0 (setSphereList skips a 0)."""
        return [r for r in self.rows if is_sphere(r[1]) and r[2] > 0 and (item is None or r[1] == item)]

    def aliased(self):
        """Equip Sphere keys shared by more than one entry (one sprite set for all of them)."""
        seen = {}
        for r in self.sphere_list():
            seen[str(r[0])] = seen.get(str(r[0]), 0) + 1
        return {k: n for k, n in seen.items() if n > 1}

    def lock_body(self):
        """ItemFavoriteRequest (x86 0x4B4060): a key per row, the FIRST row with it, sent if locked."""
        first = {}
        for row in self.rows:
            first.setdefault(row[0], row)
        return [row[0] for row in self.rows if first[row[0]][3]]


def queue(times, recipe=2005):
    """createItemMixConnectList: one ItemMixRequest per 50 crafts, itemCsvData per request."""
    out = []
    while times:
        n = min(50, times)
        out.append(f'{recipe}:{n}')
        times -= n
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19971)
    ap.add_argument('--out', default=str(qa_dir('craft_row_identity')))
    args = ap.parse_args()
    check = Checker()
    fx = Fixture.create(args.out, port=args.port)

    with fx.db() as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        for item, n in ((SPHERE, 3), (MATERIAL, 1000), (CURE, 95), (GRASS, 50), (DROP, 50)):
            db.execute('INSERT INTO user_items(user_id,item_id,item_num,favorite_flg) VALUES (?,?,?,0)'
                       ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=excluded.item_num,favorite_flg=0',
                       (user, item, n))
        db.execute('UPDATE user_warehouse_rows SET favorite_flg=0 WHERE user_id=? AND item_id IN (?,?)',
                   (user, SPHERE, CURE))
        db.execute('UPDATE user_units SET eqip_item_id=0,eqip_item_id2=0,eqip_item_frame_id=0,'
                   'eqip_item_frame_id2=-1 WHERE user_id=? AND (eqip_item_id=? OR eqip_item_id2=?)',
                   (user, SPHERE, SPHERE))
        for facility, lv in ((1, 1), (2, 1)):
            db.execute('INSERT INTO user_town_facilities(user_id,facility_id,lv) VALUES (?,?,?)'
                       ' ON CONFLICT(user_id,facility_id) DO UPDATE SET lv=excluded.lv', (user, facility, lv))
        db.execute("INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,'21',2)"
                   ' ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2', (user,))
        db.execute('UPDATE user_info SET karma=1000000, zel=1000000, max_warehouse_count=3200 WHERE id=?', (user,))
        template = dict(db.execute('SELECT * FROM user_units WHERE user_id=? LIMIT 1', (user,)).fetchone())
        template.pop('user_unit_id')
        template.update(eqip_item_id=0, eqip_item_id2=0, eqip_item_frame_id=0, eqip_item_frame_id2=-1,
                        sphere_ext=0, favorite_flg=0)

        def new_unit(**kw):
            row = dict(template, **kw)
            return db.execute(f"INSERT INTO user_units ({','.join(row)}) VALUES ({','.join('?' for _ in row)})",
                              tuple(row.values())).lastrowid
        unit_a = new_unit()                                      # one slot
        unit_b = new_unit(sphere_ext=1, eqip_item_frame_id2=0)   # two slots (Sphere Frog)
        unit_c = new_unit()                                      # one slot
        db.commit()

    db = fx.db()

    def stock(item):
        r = db.execute('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?', (user, item)).fetchone()
        return r[0] if r else 0

    def karma():
        return db.execute('SELECT karma FROM user_info WHERE id=?', (user,)).fetchone()[0]

    def zel():
        return db.execute('SELECT zel FROM user_info WHERE id=?', (user,)).fetchone()[0]

    def crafts():
        r = db.execute('SELECT craft_count FROM user_recipe_crafts WHERE user_id=? AND recipe_id=2005',
                       (user,)).fetchone()
        return r[0] if r else 0

    def rows(item=SPHERE):
        """Live rows: id -> (count, locked, unit, slot)."""
        return {r[0]: (r[1], r[2], r[3], r[4]) for r in db.execute(
            'SELECT instance_id,item_num,favorite_flg,equip_unit_id,equip_slot FROM user_warehouse_rows'
            ' WHERE user_id=? AND item_id=? AND (item_num>0 OR equip_unit_id!=0) ORDER BY instance_id',
            (user, item))}

    def worn():
        return sum(1 for v in rows().values() if v[2])

    def unit_row(unit):
        return tuple(db.execute('SELECT eqip_item_id,eqip_item_frame_id,eqip_item_id2,eqip_item_frame_id2'
                                ' FROM user_units WHERE user_unit_id=?', (unit,)).fetchone())

    def snapshot():
        return {t: [tuple(r) for r in db.execute(
            f'SELECT * FROM {t} WHERE ' + ('id' if t == 'user_info' else 'user_id') + '=? ORDER BY 1,2', (user,))]
            for t in ('user_items', 'user_units', 'user_warehouse_rows', 'user_info', 'user_recipe_crafts')}

    def reply_matches_db(reply):
        """The list is the server's live warehouse, row for row; the lock set is its locks."""
        live = {r[0]: (r[1], r[2] != 0, r[3]) for r in db.execute(
            'SELECT instance_id,item_num,favorite_flg,equip_unit_id FROM user_warehouse_rows'
            ' WHERE user_id=? AND (item_num>0 OR equip_unit_id!=0)', (user,))}
        listed = {int(r['n6E8iMf3']): int(r['wgV86x1q']) for r in reply.get(WAREHOUSE, [])}
        shown = {i: (1 if u else n) for i, (n, f, u) in live.items()}
        locks = {int(e['n6E8iMf3']) for e in reply.get(LOCKS, []) if e.get('5JbjC3Pp') == '1'}
        return listed == shown and locks == {i for i, (n, f, u) in live.items() if f}, \
            {'listed - shown': sorted(set(listed.items()) - set(shown.items()))[:5],
             'shown - listed': sorted(set(shown.items()) - set(listed.items()))[:5],
             'locks': sorted(locks)[:8]}

    def worn_ok(reply, model):
        """Every worn slot names one listed count-1 row of its sphere; no row named twice."""
        listed = {r[0]: r for r in model.rows}
        named, problems = [], []
        for u in reply.get(UNITS, []):
            for fk, ik in (('0R3qTPK9', 'Ge8Yo32T'), ('RXfC31FA', 'mZA7fH2v')):
                frame, item = int(u[fk]), int(u[ik] or 0)
                if item:
                    r = listed.get(frame)
                    if not r or r[1] != item or r[2] != 1:
                        problems.append((u['edy7fq3L'], fk, frame, item))
                    named.append(frame)
        if len(named) != len(set(named)):
            problems.append('a row is worn twice')
        return not problems, problems[:5]

    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx, user_id=user)
        model = ClientWarehouse()

        def call(name, key=None, body=None):
            return c.call(name, key, body)

        def mix(csv):
            return call('ItemMix', body={'JTf2jY5o': [{'4HqhTf3a': csv}]})

        def lock(ids):
            return call('I8il6EiI', 'aRoIftRy', {LOCKS: [{'n6E8iMf3': str(v), '5JbjC3Pp': '1'} for v in ids]})

        def sell(pairs):
            return call('qDQerU74', '73aFNjPu', {'M73i1c5U': [{'n6E8iMf3': str(v), 'wgV86x1q': str(q)}
                                                              for v, q in pairs]})

        def equip(packed):
            return call('ItemSphereEqp', body={'wx1ZLFj9': [{'a2utCvs8': packed}]})

        # ---- login: three real copies; lock one, wear one ---------------------------------
        login = call('UserInfo')
        model.receive(login)
        base = sorted(r[0] for r in model.sphere_list(SPHERE))
        check('setup: three real copies of 30000 listed with their own ids', len(base) == 3, base)
        r_lock, r_worn, r_free = base
        model.rows[[r[0] for r in model.rows].index(r_lock)][3] = True
        reply = lock(model.lock_body())
        model.receive(reply)
        equip(f'{r_worn}:-1:{unit_a}:{SPHERE}:0')
        model.receive(call('UserInfo'))
        check('setup: one copy locked, one worn by unit A, one free',
              rows()[r_lock][1] == 1 and rows()[r_worn][2] == unit_a and rows()[r_free][:3] == (1, 0, 0), rows())
        start = {'sphere': stock(SPHERE), 'material': stock(MATERIAL), 'karma': karma(), 'crafts': crafts()}

        # ---- the captured batch: nineteen crafts, then the queue goes out -----------------
        model.craft(SPHERE, {MATERIAL: 5}, 19)
        before_reply = model.aliased()
        check('model: before any reply the nineteen local rows share one Equip Sphere key (the defect)',
              before_reply == {str(INT_MAX - SPHERE): 19}, before_reply)
        sent = queue(19)
        check('queue: nineteen crafts go out as one request, as captured', sent == ['2005:19'], sent)
        reply = mix(sent[0])
        check('craft reply: accepted', 'error' not in reply, reply.get('error'))
        check('craft reply: carries the warehouse, the lock set and the dictionary',
              all(k in reply for k in (WAREHOUSE, LOCKS, DICTIONARY)), sorted(reply)[:8])
        ok, why = reply_matches_db(reply)
        check('craft reply: the list and lock set are exactly the committed warehouse', ok, why)
        model.receive(reply)
        new19 = sorted(set(r[0] for r in model.sphere_list(SPHERE)) - set(base))
        check('craft reply: nineteen new copies, each under its own row id', len(new19) == 19
              and INT_MAX - SPHERE not in new19, new19)
        check('model: after the reply no two Equip Sphere entries share a key', not model.aliased(),
              model.aliased())
        if len(new19) != 19 or INT_MAX - SPHERE in new19:
            # A server without the list leaves the placeholders until the next
            # UserInfo; take ids from there so the later sections still report.
            model.receive(call('UserInfo'))
            new19 = sorted(set(r[0] for r in model.sphere_list(SPHERE)) - set(base))
        check('craft: the new rows are count-1 rows in storage, unlocked',
              all(rows()[i] == (1, 0, 0, 0) for i in new19), {i: rows().get(i) for i in new19[:3]})
        check('craft: stock, material and Karma conserved (19 x 1, 19 x 5, 19 x 500)',
              stock(SPHERE) == start['sphere'] + 19 and stock(MATERIAL) == start['material'] - 95
              and karma() == start['karma'] - 9500 and crafts() == start['crafts'] + 19,
              (stock(SPHERE), stock(MATERIAL), karma(), crafts()))
        check('craft: the earlier copies kept their ids, lock and unit',
              rows()[r_lock][:3] == (1, 1, 0) and rows()[r_worn][2] == unit_a and rows()[r_free][:3] == (1, 0, 0),
              {i: rows().get(i) for i in base})
        check('model: the client count equals the server stock plus worn copies',
              model.count(SPHERE) == stock(SPHERE) + worn(), (model.count(SPHERE), stock(SPHERE), worn()))

        # ---- craft-again past 50, two sessions before one flush ---------------------------
        before = {'sphere': stock(SPHERE), 'material': stock(MATERIAL), 'karma': karma()}
        model.craft(SPHERE, {MATERIAL: 5}, 70)        # session 1: 70 taps
        model.craft(SPHERE, {MATERIAL: 5}, 50)        # session 2: 50 more before the StepScene
        sent = queue(70) + queue(50)
        check('queue: 120 crafts in two sessions go out as 50, 20 and 50',
              sent == ['2005:50', '2005:20', '2005:50'], sent)
        replies = [mix(csv) for csv in sent]
        check('flush: every request accepted and answered with the whole list',
              all('error' not in r and WAREHOUSE in r and LOCKS in r for r in replies), [sorted(r)[:4] for r in replies])
        for r in replies:
            model.receive(r)
        ok, why = reply_matches_db(replies[-1])
        check('flush: the last reply is the committed warehouse', ok, why)
        check('flush: 120 more copies, every key unique', len(model.sphere_list(SPHERE)) == 3 + 19 + 120
              and not model.aliased(), (len(model.sphere_list(SPHERE)), model.aliased()))
        if model.aliased():
            model.receive(call('UserInfo'))             # as above, for a server without the list
        check('flush: stock, material and Karma conserved (120 x 1, 600, 60000)',
              stock(SPHERE) == before['sphere'] + 120 and stock(MATERIAL) == before['material'] - 600
              and karma() == before['karma'] - 60000, (stock(SPHERE), stock(MATERIAL), karma()))

        # ---- first, middle and last copy of the nineteen, by their own ids -----------------
        first, second, middle, last = new19[0], new19[1], new19[9], new19[18]
        for row in model.rows:
            if row[0] in (first, last):
                row[3] = True
        body = model.lock_body()
        sphere_ids = {r[0] for r in model.rows if r[1] == SPHERE}
        check('lock: the request names exactly the locked copies (no placeholder, no repeats)',
              sorted(i for i in body if i in sphere_ids) == sorted([r_lock, first, last])
              and INT_MAX - SPHERE not in body, body)
        reply = lock(body)
        model.receive(reply)
        locked_now = {i for i, v in rows().items() if v[1]}
        check('lock: the server locked exactly the first and last crafted copy (and kept the old one)',
              locked_now == {r_lock, first, last}, sorted(locked_now))
        check('lock: the client shows the same locks after the reply',
              {r[0] for r in model.rows if r[3] and r[1] == SPHERE} == {r_lock, first, last})

        s0 = stock(SPHERE)
        equip(f'{middle}:0:{unit_b}:{SPHERE}:0')
        check('equip: the middle copy, and only it, is worn by unit B slot 1, frame = its row',
              rows()[middle][2:] == (unit_b, 1) and unit_row(unit_b)[:2] == (SPHERE, middle) and stock(SPHERE) == s0 - 1,
              (rows()[middle], unit_row(unit_b)))
        equip(f'{middle}:{last}:{unit_b}:{SPHERE}:{SPHERE}')
        check('equip: the locked last copy goes into slot 2 and keeps its lock',
              rows()[last] == (0, 1, unit_b, 2) and unit_row(unit_b) == (SPHERE, middle, SPHERE, last),
              (rows()[last], unit_row(unit_b)))
        equip(f'{middle}:-1:{unit_c}:{SPHERE}:0,0:{last}:{unit_b}::{SPHERE}')
        check('transfer: the middle copy moves to unit C directly, slot 2 of B untouched',
              rows()[middle][2:] == (unit_c, 1) and unit_row(unit_c)[:2] == (SPHERE, middle)
              and unit_row(unit_b) == (0, 0, SPHERE, last), (rows()[middle], unit_row(unit_b), unit_row(unit_c)))

        z0, s1, others = zel(), stock(SPHERE), {i: rows()[i] for i in new19 if i != second}
        reply = sell([(second, 1)])
        check('sell: the second copy, and only it, is sold', 'error' not in reply and second not in rows()
              and stock(SPHERE) == s1 - 1 and zel() > z0 and all(rows()[i] == v for i, v in others.items()),
              (rows().get(second), stock(SPHERE)))
        for label, pairs in (('the locked first copy', [(first, 1)]), ('a worn copy', [(middle, 1)]),
                             ('the sold copy again', [(second, 1)]), ('two from one row', [(new19[2], 2)])):
            before_state = snapshot()
            sell(pairs)
            check(f'sell refused: {label}; nothing changed', snapshot() == before_state)
        before_state = snapshot()
        equip(f'{last}:-1:{unit_a}:{SPHERE}:0')
        check('equip refused: a copy worn by another unit cannot be equipped again', snapshot() == before_state)

        # ---- mixed worn / locked / free copies after a reconnect-style list ----------------
        view = call('UserInfo')
        model.receive(view)
        ok, why = worn_ok(view, model)
        check('list: every worn copy is a count-1 row named by its unit frame', ok, why)
        check('list: no aliased key with worn, locked and free copies mixed', not model.aliased(), model.aliased())
        shown_locks = {r[0] for r in model.rows if r[3] and r[1] == SPHERE}
        check('list: the client shows exactly the server locks', shown_locks == {i for i, v in rows().items() if v[1]},
              sorted(shown_locks))

        # ---- sorting and filtering are the client's; the ids do not move -------------------
        orders = {
            'by id': sorted(model.sphere_list(SPHERE), key=lambda r: r[0]),
            'reversed': sorted(model.sphere_list(SPHERE), key=lambda r: -r[0]),
            'locked first': sorted(model.sphere_list(SPHERE), key=lambda r: (not r[3], r[0])),
            'type filter': [r for r in model.sphere_list() if ITEMS[r[1]].get('92udyUrJ') == ITEMS[SPHERE]['92udyUrJ']],
        }
        check('sort/filter: every order keeps one entry per key',
              all(len({str(r[0]) for r in v}) == len(v) for v in orders.values()),
              {k: len(v) for k, v in orders.items()})
        pick = next(r for r in orders['locked first'] if not r[3] and r[0] in new19 and rows()[r[0]][2] == 0)
        for row in model.rows:
            if row[0] == pick[0]:
                row[3] = True
        reply = lock(model.lock_body())
        model.receive(reply)
        check('sort/filter: a copy picked from a sorted view is the copy that gets locked',
              rows()[pick[0]][1] == 1 and sum(1 for v in rows().values() if v[1]) == 4, (pick, rows()[pick[0]]))

        # ---- a stackable result: cap 99, overflow on its own row ----------------------------
        cure0 = stock(CURE)
        model.craft(CURE, {GRASS: 1, DROP: 1}, 10)
        reply = mix('1009:10')
        model.receive(reply)
        cure_rows = sorted(r[2] for r in model.rows if r[1] == CURE)
        cure_ids = {r[0] for r in model.rows if r[1] == CURE}
        check('stackable: 95 + 10 Cures are one full stack and one of 6, under two real ids',
              'error' not in reply and stock(CURE) == cure0 + 10 and cure_rows == [6, 99]
              and len(cure_ids) == 2 and INT_MAX - CURE not in cure_ids, (stock(CURE), cure_rows, cure_ids))
        check('stackable: materials conserved (10 + 10)', stock(GRASS) == 40 and stock(DROP) == 40,
              (stock(GRASS), stock(DROP)))

        # ---- insufficient material: the server crafts what is affordable --------------------
        db.execute('UPDATE user_items SET item_num=7 WHERE user_id=? AND item_id=?', (user, MATERIAL))
        db.commit()
        model.receive(call('UserInfo'))
        s2, k2 = stock(SPHERE), karma()
        model.craft(SPHERE, {MATERIAL: 5}, 3)          # a stale client that believed it had 15
        reply = mix('2005:3')
        model.receive(reply)
        check('short: one craft is affordable and one is made', 'error' not in reply and stock(SPHERE) == s2 + 1
              and stock(MATERIAL) == 2 and karma() == k2 - 500, (stock(SPHERE), stock(MATERIAL), karma()))
        check('short: the reply removes the copies the client made but the server did not',
              model.count(SPHERE) == stock(SPHERE) + worn() and model.count(MATERIAL) == 2 and not model.aliased(),
              (model.count(SPHERE), stock(SPHERE), worn(), model.count(MATERIAL)))
        before_state = snapshot()
        model.craft(SPHERE, {MATERIAL: 5}, 2)
        reply = mix('2005:2')
        model.receive(reply)
        check('short: nothing affordable -- nothing changes, and the list still corrects the client',
              'error' not in reply and WAREHOUSE in reply and snapshot() == before_state
              and model.count(SPHERE) == stock(SPHERE) + worn() and model.count(MATERIAL) == 2,
              (model.count(SPHERE), stock(SPHERE), worn(), model.count(MATERIAL)))

        # ---- a full box: the client has no craft-time box gate, nor does the server ---------
        db.execute('UPDATE user_items SET item_num=200 WHERE user_id=? AND item_id=?', (user, MATERIAL))
        active = db.execute('SELECT COUNT(*) FROM user_warehouse_rows WHERE user_id=?'
                            ' AND (item_num>0 OR equip_unit_id!=0)', (user,)).fetchone()[0]
        db.execute('UPDATE user_info SET max_warehouse_count=? WHERE id=?', (active, user))
        db.commit()
        model.receive(call('UserInfo'))
        s3 = stock(SPHERE)
        model.craft(SPHERE, {MATERIAL: 5}, 3)
        reply = mix('2005:3')
        model.receive(reply)
        ok, why = reply_matches_db(reply)
        check('full box: three crafts past a full box are made, as the client already showed',
              'error' not in reply and stock(SPHERE) == s3 + 3, (reply.get('error'), stock(SPHERE)))
        check('full box: the reply lists every row, keys unique', ok and not model.aliased(), why)

        # ---- a request built before a list the client never got -----------------------------
        db.execute('UPDATE user_info SET max_warehouse_count=3200 WHERE id=?', (user,))
        db.commit()
        stale = ClientWarehouse()
        stale.receive(call('UserInfo'))
        stale.craft(SPHERE, {MATERIAL: 5}, 2)
        lost = mix('2005:2')                            # the reply is built, then lost
        check('lost reply: the craft itself was applied and answered', 'error' not in lost and WAREHOUSE in lost)
        before_state = snapshot()
        sell([(INT_MAX - SPHERE, 1)])
        check('stale: a sale under the placeholder names no copy the server knows unsent; nothing sold',
              snapshot() == before_state)
        for row in stale.rows:
            if row[0] == INT_MAX - SPHERE:
                row[3] = True
                break
        lock(stale.lock_body())
        check('stale: a lock set naming the placeholder is refused whole; every lock kept', snapshot() == before_state)
        equip(f'{INT_MAX - SPHERE}:{last}:{unit_b}:{SPHERE}:{SPHERE}')
        check('stale: an equip under the placeholder into an empty slot is refused; nothing worn or spent',
              snapshot() == before_state)
        stale.receive(call('UserInfo'))
        check('stale: the next list gives the stale client unique ids again', not stale.aliased(), stale.aliased())

        # ---- a byte-identical retry is a second craft (no request identity exists) ----------
        s4, m4, k4, c4 = stock(SPHERE), stock(MATERIAL), karma(), crafts()
        retry = mix('2005:2')
        check('retry (protocol limit, documented): the same body crafts again, every cost paid once per craft',
              'error' not in retry and stock(SPHERE) == s4 + 2 and stock(MATERIAL) == m4 - 10
              and karma() == k4 - 1000 and crafts() == c4 + 2, (stock(SPHERE), stock(MATERIAL), karma(), crafts()))
        final = snapshot()
        final_rows = rows()
        log = server.log_text()
        check('server: no handler error logged for ItemMix', 'ItemMix: serialization error' not in log)
    db.close()

    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx, user_id=user)
        db = fx.db()
        check('restart: every row, lock, unit and stock exactly as left', snapshot() == final)
        model = ClientWarehouse()
        view = c.call('UserInfo')
        model.receive(view)
        ids = {r[0] for r in model.sphere_list(SPHERE)}
        check('restart: the login list names the same copies by the same ids', ids == set(final_rows),
              (len(ids), len(final_rows)))
        ok, why = worn_ok(view, model)
        check('restart: worn copies still named by their units, keys unique', ok and not model.aliased(), why)
        dup = db.execute('SELECT instance_id,COUNT(*) n FROM user_warehouse_rows GROUP BY instance_id HAVING n>1').fetchall()
        check('restart: persistent row ids are unique', not dup, dup)
        db.close()
    return check.summary('craft row identity')


if __name__ == '__main__':
    sys.exit(main())
