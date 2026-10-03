"""ItemSphereEqp as the Windows client actually sends it, and the unit frame repair.

    python scripts/test_sphere_equip_format_wire.py PATH_TO_DEBUG_EXE [--port 19986] [--out DIR]

Reported 2026-10-02: "I would equip a sphere and after some play there would be
no sphere(s) equipped to the unit."  Live capture 10:39:39, a2utCvs8 =
"19:-1:1000:31000:0", was answered {} and changed nothing: the handler parsed
every field as unsigned and refused, silently, the "-1" that every unit without
a Sphere Frog sends for its second slot.  The equip never persisted, and the next
HomeInfo unit rebuild (4ceMWH6k) put the unit back the way the server had it.

The fields are the unit's own strings (ItemSphereEqpRequest::createBody
@0x13A7054): frame1:frame2:unit:item1:item2.  A frame is the warehouse ROW of the
copy in the slot -- ItemSphereSelectScene::eqpSphere writes it, and
UserUnitInfoList::updateSphereEquipList keys the client's "who wears this row"
map on it -- "-1" a second slot the unit does not have, "" an id it never held.
The server used to store and send ItemMst.sphere_type there instead, which marked
the wrong rows worn and got any later batch carrying an unchanged slot refused.

On an isolated copy of the live save this checks:
  * login repairs legacy frames (sphere_type, or a worn sphere with no row at all)
    and every unit then names, by frame, a count-1 row holding its sphere;
  * the captured strings: equip with "-1", unequip with empty items;
  * an equip survives HomeInfo, UserInfo and a restart;
  * an edit to ONE slot of a two-slot unit, and a move between units;
  * malformed batches refused whole, state unchanged, and logged.
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Client, Fixture, IsolatedServer, qa_dir  # noqa: E402

UNITS = '4ceMWH6k'
WAREHOUSE = '9wjrh74P'


def sphere_types():
    """ItemMst id -> sphere category, for single-stack spheres (h0K7wjeH 3)."""
    rows = next(iter(json.loads((ROOT / 'deploy/mst/item_mst.json').read_text(encoding='utf-8')).values()))
    return {int(r['kixHbe54']): int(r['92udyUrJ']) for r in rows
            if r.get('h0K7wjeH') == '3' and r.get('m9gd5h1u') == '1' and str(r.get('92udyUrJ', '')).isdigit()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19986)
    ap.add_argument('--out', default=str(qa_dir('sphere_equip_format')))
    args = ap.parse_args()
    check = Checker()
    fx = Fixture.create(args.out, port=args.port)
    types = sphere_types()

    with fx.db() as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        held = {r[0] for r in db.execute('SELECT item_id FROM user_items WHERE user_id=?', (user,))} | \
               {r[0] for r in db.execute('SELECT item_id FROM user_warehouse_rows WHERE user_id=?', (user,))}
        # Four spheres the save has never held, three different categories.
        by_type = {}
        for item, kind in sorted(types.items()):
            if item not in held and kind not in by_type:
                by_type[kind] = item
        kinds = sorted(by_type)
        s_a, s_b, s_c = by_type[kinds[0]], by_type[kinds[1]], by_type[kinds[2]]
        s_d = next(i for i, k in sorted(types.items()) if i not in held and k == kinds[1] and i != s_b)
        template = dict(db.execute('SELECT * FROM user_units WHERE user_id=? LIMIT 1', (user,)).fetchone())
        template.pop('user_unit_id')
        template.update(eqip_item_id=0, eqip_item_id2=0, eqip_item_frame_id=0, eqip_item_frame_id2=-1,
                        sphere_ext=0, favorite_flg=0)

        def new_unit(**kw):
            row = dict(template, **kw)
            return db.execute(f"INSERT INTO user_units ({','.join(row)}) VALUES ({','.join('?' for _ in row)})",
                              tuple(row.values())).lastrowid
        unit_a = new_unit()                                      # one slot, nothing worn
        # B: two slots, both worn under the PRE-FIX frame (ItemMst.sphere_type).
        unit_b = new_unit(sphere_ext=1, eqip_item_id=s_a, eqip_item_frame_id=types[s_a],
                          eqip_item_id2=s_b, eqip_item_frame_id2=types[s_b])
        row_b1 = db.execute('INSERT INTO user_warehouse_rows(user_id,item_id,item_num,equip_unit_id,equip_slot)'
                            ' VALUES (?,?,0,?,1) RETURNING instance_id', (user, s_a, unit_b)).fetchone()[0]
        row_b2 = db.execute('INSERT INTO user_warehouse_rows(user_id,item_id,item_num,equip_unit_id,equip_slot)'
                            ' VALUES (?,?,0,?,2) RETURNING instance_id', (user, s_b, unit_b)).fetchone()[0]
        # C: a save from before warehouse rows -- the sphere lives only on the unit.
        unit_c = new_unit(sphere_ext=1, eqip_item_frame_id2=0, eqip_item_id=s_c, eqip_item_frame_id=types[s_c])
        # Free stock: two of s_a, one of s_d (rows are minted by the login sync).
        for item, count in ((s_a, 2), (s_d, 1), (s_b, 0), (s_c, 0)):
            db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,?)'
                       ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=excluded.item_num', (user, item, count))
        db.commit()
    check('setup: four unheld spheres in three categories',
          len({types[s_a], types[s_b], types[s_c]}) == 3 and s_d not in (s_a, s_b, s_c),
          (s_a, s_b, s_c, s_d))

    db = fx.db()

    def unit_row(unit):
        return dict(db.execute('SELECT eqip_item_id,eqip_item_frame_id,eqip_item_id2,eqip_item_frame_id2'
                               ' FROM user_units WHERE user_unit_id=?', (unit,)).fetchone())

    def stock(item):
        r = db.execute('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?', (user, item)).fetchone()
        return r[0] if r else 0

    def row(instance):
        r = db.execute('SELECT item_id,item_num,equip_unit_id,equip_slot FROM user_warehouse_rows'
                       ' WHERE instance_id=?', (instance,)).fetchone()
        return tuple(r) if r else None

    def snapshot():
        return ([tuple(r) for r in db.execute('SELECT * FROM user_units WHERE user_id=? ORDER BY user_unit_id',
                                              (user,))],
                [tuple(r) for r in db.execute('SELECT * FROM user_warehouse_rows WHERE user_id=?'
                                              ' ORDER BY instance_id', (user,))],
                [tuple(r) for r in db.execute('SELECT * FROM user_items WHERE user_id=? ORDER BY item_id',
                                              (user,))])

    def wire_unit(reply, unit):
        return next((u for u in reply.get(UNITS, []) if u['edy7fq3L'] == str(unit)), None)

    def free_rows(reply, item):
        """Rows of `item` the client would offer: count 1 and worn by no unit."""
        worn = worn_rows(reply)
        return sorted(int(r['n6E8iMf3']) for r in reply.get(WAREHOUSE, [])
                      if r['kixHbe54'] == str(item) and r['wgV86x1q'] == '1' and int(r['n6E8iMf3']) not in worn)

    def worn_rows(reply):
        """The client's worn map: every frame > 0 (updateSphereEquipList skips "0", -1 names no row)."""
        return {int(u[k]) for u in reply.get(UNITS, []) for k in ('0R3qTPK9', 'RXfC31FA') if int(u[k]) > 0}

    def model_agrees(reply):
        """Every worn slot names a count-1 row of its own sphere; every empty slot names none."""
        listed = {int(r['n6E8iMf3']): r for r in reply.get(WAREHOUSE, [])}
        problems, named = [], []
        for u in reply.get(UNITS, []):
            for frame_key, item_key in (('0R3qTPK9', 'Ge8Yo32T'), ('RXfC31FA', 'mZA7fH2v')):
                frame, item = int(u[frame_key]), int(u[item_key] or 0)
                if item > 0:
                    r = listed.get(frame)
                    if not r or r['kixHbe54'] != str(item) or r['wgV86x1q'] != '1':
                        problems.append((u['edy7fq3L'], frame_key, frame, item, r))
                    named.append(frame)
                elif frame > 0:
                    problems.append((u['edy7fq3L'], frame_key, frame, 'empty slot names a row'))
        if len(named) != len(set(named)):
            problems.append(('a row is named by two slots', sorted(named)))
        return not problems, problems[:6]

    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx)

        def equip(packed):
            return c.call('ItemSphereEqp', body={'wx1ZLFj9': [{'a2utCvs8': packed}]})

        # ---- login: legacy frames repaired, worn copies sent as count-1 rows ---------
        login = c.call('UserInfo')
        b, cc = wire_unit(login, unit_b), wire_unit(login, unit_c)
        row_c1 = db.execute('SELECT instance_id FROM user_warehouse_rows WHERE equip_unit_id=? AND equip_slot=1',
                            (unit_c,)).fetchone()
        check('login: a sphere stored only on a unit is given its own row', row_c1 is not None)
        row_c1 = row_c1[0] if row_c1 else -1
        check('login: legacy frames now name the worn rows (slot 1 and 2)',
              b and b['0R3qTPK9'] == str(row_b1) and b['RXfC31FA'] == str(row_b2), b and (b['0R3qTPK9'], b['RXfC31FA']))
        check('login: the row-less legacy sphere is named by its new row',
              cc and cc['0R3qTPK9'] == str(row_c1), cc and cc['0R3qTPK9'])
        check('login: the repair is persisted', unit_row(unit_b)['eqip_item_frame_id'] == row_b1
              and unit_row(unit_b)['eqip_item_frame_id2'] == row_b2
              and unit_row(unit_c)['eqip_item_frame_id'] == row_c1, (unit_row(unit_b), unit_row(unit_c)))
        a = wire_unit(login, unit_a)
        check('login: a unit without a Sphere Frog keeps frame2 -1', a and a['RXfC31FA'] == '-1')
        ok, why = model_agrees(login)
        check('login: every unit in the save agrees with the warehouse list (client worn map)', ok, why)
        check('login: worn copies are listed with count 1, still out of user_items',
              all(r['wgV86x1q'] == '1' for r in login[WAREHOUSE] if int(r['n6E8iMf3']) in (row_b1, row_b2, row_c1))
              and stock(s_b) == 0 and stock(s_c) == 0)
        free_a = free_rows(login, s_a)
        check('login: the two free copies of s_a are offered', len(free_a) == 2, free_a)
        ra = free_a[0]

        # ---- the captured repro: a one-slot unit, frame2 "-1" -------------------------
        before = stock(s_a)
        reply = equip(f'{ra}:-1:{unit_a}:{s_a}:0')
        check('equip "-1": answered', 'error' not in reply, reply)
        check('equip "-1": persisted on the unit, frame = the chosen row, slot 2 still absent',
              unit_row(unit_a) == {'eqip_item_id': s_a, 'eqip_item_frame_id': ra,
                                   'eqip_item_id2': 0, 'eqip_item_frame_id2': -1}, unit_row(unit_a))
        check('equip "-1": exactly that copy is worn and left storage',
              row(ra) == (s_a, 0, unit_a, 1) and stock(s_a) == before - 1, (row(ra), stock(s_a)))
        home = c.call('HomeInfo')
        a = wire_unit(home, unit_a)
        check('equip "-1": the HomeInfo unit rebuild keeps the sphere on the unit',
              a and a['Ge8Yo32T'] == str(s_a) and a['0R3qTPK9'] == str(ra) and a['RXfC31FA'] == '-1',
              a and (a['Ge8Yo32T'], a['0R3qTPK9'], a['RXfC31FA']))
        relog = c.call('UserInfo')
        a = wire_unit(relog, unit_a)
        check('equip "-1": still worn after the next login', a and a['Ge8Yo32T'] == str(s_a), a and a['Ge8Yo32T'])
        ok, why = model_agrees(relog)
        check('equip "-1": the worn copy stays in the list (count 1), named by the frame', ok
              and ra in worn_rows(relog) and ra not in free_rows(relog, s_a), why)

        # ---- unequip as captured: "0:-1:<unit>::" -------------------------------------
        before = stock(s_a)
        reply = equip(f'0:-1:{unit_a}::')
        check('unequip "::": answered', 'error' not in reply, reply)
        check('unequip "::": the slot is empty and frame 0, slot 2 still absent',
              unit_row(unit_a) == {'eqip_item_id': 0, 'eqip_item_frame_id': 0,
                                   'eqip_item_id2': 0, 'eqip_item_frame_id2': -1}, unit_row(unit_a))
        check('unequip "::": the same copy is back in storage', row(ra) == (s_a, 1, 0, 0) and stock(s_a) == before + 1,
              (row(ra), stock(s_a)))
        a = wire_unit(c.call('HomeInfo'), unit_a)
        check('unequip "::": HomeInfo agrees', a and a['Ge8Yo32T'] in ('0', '') and a['0R3qTPK9'] == '0')

        # ---- one slot of a two-slot unit: the frame the server now sends names slot 1 -
        listing = c.call('UserInfo')
        rd = free_rows(listing, s_d)
        check('two-slot: one free copy of s_d is offered', len(rd) == 1, rd)
        rd = rd[0] if rd else 0
        b_frame1 = wire_unit(listing, unit_b)['0R3qTPK9']
        reply = equip(f'{b_frame1}:{rd}:{unit_b}:{s_a}:{s_d}')
        check('two-slot: changing only slot 2 is accepted', 'error' not in reply
              and unit_row(unit_b) == {'eqip_item_id': s_a, 'eqip_item_frame_id': row_b1,
                                       'eqip_item_id2': s_d, 'eqip_item_frame_id2': rd}, unit_row(unit_b))
        check('two-slot: slot 1 keeps its own copy; the old slot-2 copy returns',
              row(row_b1) == (s_a, 0, unit_b, 1) and row(row_b2) == (s_b, 1, 0, 0)
              and stock(s_b) == 1 and stock(s_d) == 0, (row(row_b1), row(row_b2), stock(s_b), stock(s_d)))
        before = snapshot()
        reply = equip(f'{types[s_a]}:{rd}:{unit_b}:{s_a}:{s_d}')
        check('two-slot: a frame naming no row of that sphere is refused whole, nothing swapped',
              'error' not in reply and snapshot() == before)

        # ---- a move between units, source slot emptied with "" ------------------------
        reply = equip(f'{row_b1}:-1:{unit_a}:{s_a}:0,0:{rd}:{unit_b}::{s_d}')
        check('move: answered', 'error' not in reply, reply)
        check('move: the copy keeps its row and lands on the one-slot unit',
              unit_row(unit_a)['eqip_item_id'] == s_a and unit_row(unit_a)['eqip_item_frame_id'] == row_b1
              and row(row_b1) == (s_a, 0, unit_a, 1), (unit_row(unit_a), row(row_b1)))
        check('move: the source slot is empty, its other slot untouched',
              unit_row(unit_b) == {'eqip_item_id': 0, 'eqip_item_frame_id': 0,
                                   'eqip_item_id2': s_d, 'eqip_item_frame_id2': rd}, unit_row(unit_b))
        ok, why = model_agrees(c.call('UserInfo'))
        check('move: the client model still agrees after the move', ok, why)

        # ---- malformed or impossible batches: refused whole, logged ---------------------
        for label, packed in (('non-numeric field', f'x:-1:{unit_c}:{s_a}:0'),
                              ('negative item', f'0:-1:{unit_c}:-5:0'),
                              ('four fields', f'0:0:{unit_c}:{s_b}'),
                              ('a unit named twice', f'0:0:{unit_c}:0:0,0:0:{unit_c}:0:0'),
                              ('an item in an absent second slot', f'0:-1:{unit_a}:{s_a}:{s_b}')):
            before = snapshot()
            reply = equip(packed)
            check(f'refused, {label}: answered without an error and nothing changed',
                  'error' not in reply and snapshot() == before)
        log = server.log_text()
        check('refused: malformed batches are logged with the batch', 'refused malformed batch' in log
              and f'x:-1:{unit_c}' in log)
        before = snapshot()
        check('empty change list: a no-op', 'error' not in equip('') and snapshot() == before)
        final = snapshot()
    db.close()

    with IsolatedServer(args.exe, fx, log_name='server_restart.log'):
        c = Client(fx)
        db = fx.db()
        check('restart: units, rows and stock exactly as left', snapshot() == final)
        login = c.call('UserInfo')
        ok, why = model_agrees(login)
        check('restart: the login list agrees with every unit', ok, why)
        a = wire_unit(login, unit_a)
        check('restart: the moved copy is still worn by its unit',
              a and a['Ge8Yo32T'] == str(s_a) and a['0R3qTPK9'] == str(row_b1), a and a['0R3qTPK9'])
        db.close()
    return check.summary('sphere equip format')


if __name__ == '__main__':
    sys.exit(main())
