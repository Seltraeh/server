"""#34 persistent-row integration: Merit sphere delivery, sphere returns, lock refresh.

python scripts/test_warehouse_integration_wire.py PATH_TO_DEBUG_EXE [--port 19978]

* Merit sphere delivery sends WAREHOUSE ROW ids (RandallAchievementDedicateSphereScene
  ::sell @0x1A3D5D0 joins getItemIndex()), so a locked copy must stay while an
  unlocked copy of the same sphere is delivered; a crafted copy may arrive under
  its INT_MAX - itemId placeholder.
* A sphere equipped on a unit that is sold, fused away or used up by evolution
  comes back as the SAME row with its lock (gme::returnEquippedSpheres).
* Every reply that replaces the warehouse (9wjrh74P) now also carries the lock set
  (VSRPkdId), because GameResponseParser::parseBodyTag re-derives row locks from it.

Isolated SQLite backup under out/, client request encoding (quoted numbers).
"""
import argparse
import json
import sys
from bf_testkit import (ROOT, Fixture, IsolatedServer, Client, Checker, mission_start_body, mission_end_body,
                        started_serial)
from bf_testkit import qa_dir  # noqa: E402

INT_MAX = 2147483647
SPHERE = 30000


def mst(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19978)
    args = ap.parse_args()
    units = {int(r['pn16CNah']): r for r in mst('unit')}
    recipes = {int(r['pn16CNah']): r for r in mst('unit_evo')}
    evo_keys = ['85X6JHQA', 'wh3YRU08', '7MxucW2J', 'j7fTS3ca', 'Hb8yfmv7', 'Voht18AP', '3g8brFoq', 'agp4CKEV', 'eKPWNoLn']
    evo_kinds = ['Xyt6rhx2', '0tna4Idu', '6GwnugW3', 'hdF8ND2H', 'nB7pFdR0', 'IZUvR489', 'bNRUuatB', '2BFgYLjg', '18Oz7z8k']

    fx = Fixture.create(qa_dir('wh_integration'), port=args.port)
    check = Checker()
    with fx.db() as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        db.execute('INSERT INTO user_items(user_id,item_id,item_num,favorite_flg) VALUES (?,?,3,0)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=3,favorite_flg=0', (user, SPHERE))
        db.execute('UPDATE user_items SET favorite_flg=0 WHERE user_id=?', (user,))
        db.execute('UPDATE user_units SET eqip_item_id=0,eqip_item_id2=0,eqip_item_frame_id=0,'
                   'eqip_item_frame_id2=-1,sphere_ext=0 WHERE user_id=?', (user,))
        db.execute('INSERT INTO user_town_facilities(user_id,facility_id,lv) VALUES (?,1,1)'
                   ' ON CONFLICT(user_id,facility_id) DO UPDATE SET lv=1', (user,))
        db.execute("INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,'21',2)"
                   ' ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2', (user,))
        db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,10400,5)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=5', (user,))
        db.execute('UPDATE user_info SET karma=99999999, zel=50000000, achieve_point=0 WHERE id=?', (user,))
        db.execute('DELETE FROM user_achievement_deliver WHERE user_id=?', (user,))
        db.commit()

    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        template = dict(db.execute('SELECT * FROM user_units WHERE user_id=? LIMIT 1', (user,)).fetchone())
        template.pop('user_unit_id')

        def unit(mid, **kw):
            data = units[mid]
            row = dict(template, user_id=user, unit_id=str(mid), unit_lvl=int(data['EI1DF8Yt']), favorite_flg=0,
                       bb_id=data['nj9Lw7mV'], sbb_id=data['iEFZ6H19'], bb_lvl=1, sbb_lvl=0,
                       eqip_item_id=0, eqip_item_id2=0, eqip_item_frame_id=0, eqip_item_frame_id2=-1, sphere_ext=0)
            row.update(kw)
            uid = db.execute(f"INSERT INTO user_units ({','.join(row)}) VALUES ({','.join('?' for _ in row)})",
                             tuple(row.values())).lastrowid
            db.commit()
            return uid

        def rows():
            return {r[0]: (r[1], r[2], r[3]) for r in db.execute(
                'SELECT instance_id,item_num,favorite_flg,equip_unit_id FROM user_warehouse_rows'
                ' WHERE user_id=? AND item_id=? ORDER BY instance_id', (user, SPHERE))}

        def stock():
            r = db.execute('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?', (user, SPHERE)).fetchone()
            return r[0] if r else 0

        def merit():
            return db.execute('SELECT achieve_point FROM user_info WHERE id=?', (user,)).fetchone()[0]

        def state():
            return {t: [tuple(r) for r in db.execute(
                f'SELECT * FROM {t} WHERE ' + ('id' if t == 'user_info' else 'user_id') + '=?', (user,))]
                for t in ('user_items', 'user_units', 'user_warehouse_rows', 'user_info')}

        def lock(values):
            return c.call('I8il6EiI', 'aRoIftRy',
                          {'VSRPkdId': [{'n6E8iMf3': str(v), '5JbjC3Pp': '1'} for v in values]})

        def equip(ops):
            return c.call('ItemSphereEqp', body={'wx1ZLFj9': [
                {'a2utCvs8': ','.join(':'.join(map(str, x)) for x in ops)}]})

        def deliver(row_ids):
            # Trade category 3, sphere flow 6; the client's own point claim is ignored.
            return c.call('vsaXI4M0', '2Lj5hIEG', {
                '1Q2pGAXE': [{'KT71m8Ae': '3', '3rhygS9K': '6', 'M7SXoc31': ''}],
                'dE79UwNE': [{'Rs7bCE3t': '0', 'idfCDG70': '0', 'edy7fq3L': '',
                              'n6E8iMf3': ','.join(str(r) for r in row_ids)}]})

        def locks_in(reply):
            return {int(e['n6E8iMf3']) for e in reply.get('VSRPkdId', []) if e.get('5JbjC3Pp') == '1'}

        def listed(reply):
            return {int(r['n6E8iMf3']): int(r['wgV86x1q']) for r in reply.get('9wjrh74P', [])
                    if r['kixHbe54'] == str(SPHERE)}

        view = c.call('UserInfo')
        ids = sorted(int(r['n6E8iMf3']) for r in view['9wjrh74P'] if r['kixHbe54'] == str(SPHERE)
                     and int(r['wgV86x1q']) > 0)
        check('setup: three one-count copies', len(ids) == 3, ids)
        r1, r2, r3 = ids if len(ids) == 3 else (0, 0, 0)
        lock([r1])
        check('setup: only the first copy is locked', rows().get(r1, (0, 0))[1] == 1 and rows().get(r2, (0, 0))[1] == 0)

        # ---- Merit delivery by warehouse row ----------------------------------------
        m0 = merit()
        reply = deliver([r2])
        now = rows()
        check('merit: an unlocked copy is delivered by its row id', 'error' not in reply and now[r2][0] == 0
              and stock() == 2 and merit() > m0, (reply.get('error'), now, stock(), merit()))
        check('merit: the locked copy of the same sphere is untouched', now[r1] == (1, 1, 0), now[r1])
        check('merit: reply replaces the warehouse AND carries the lock set',
              listed(reply).get(r1) == 1 and locks_in(reply) == {r1}, (listed(reply), locks_in(reply)))
        before = state()
        reply = deliver([r1])
        check('merit: a locked copy alone is refused and nothing moves', 'error' in reply and state() == before,
              reply.get('error'))
        m1 = merit()
        reply = deliver([r3, r3])
        check('merit: a repeated row id is handed over once', rows()[r3][0] == 0 and stock() == 1
              and merit() - m1 == m1 - m0, (rows()[r3], stock(), merit() - m1, m1 - m0))

        # A craft the client applied itself, delivered before any refresh.
        c.call('ItemMix', body={'JTf2jY5o': [{'4HqhTf3a': '2005:1'}]})
        check('merit: craft added a copy', stock() == 2, stock())
        m2 = merit()
        reply = deliver([INT_MAX - SPHERE])
        check('merit: the crafted copy is delivered under its placeholder', 'error' not in reply and stock() == 1
              and merit() > m2 and rows()[r1] == (1, 1, 0), (reply.get('error'), stock(), rows()))

        # ---- sphere returns keep the row and its lock ------------------------------
        seller = unit(10011)
        equip([(r1, 0, seller, SPHERE, 0)])
        check('return: the locked copy is equipped on the unit to sell', rows()[r1] == (0, 1, seller) and stock() == 0,
              rows()[r1])
        db.execute("CREATE TRIGGER qc_sell_unit_fail BEFORE DELETE ON user_units BEGIN SELECT RAISE(ABORT,'QC'); END")
        db.commit()
        before = state()
        c.call('Ri3uTq9b', '92VqcGFWuPkmT60U', {'Km35HAXv': [{'edy7fq3L': str(seller)}]})
        check('return: a failed unit sale leaves the sphere equipped and the unit owned', state() == before)
        db.execute('DROP TRIGGER qc_sell_unit_fail')
        db.commit()
        reply = c.call('Ri3uTq9b', '92VqcGFWuPkmT60U', {'Km35HAXv': [{'edy7fq3L': str(seller)}]})
        check('return (sale): the same row comes back to storage with its lock',
              'error' not in reply and rows()[r1] == (1, 1, 0) and stock() == 1, (reply.get('error'), rows()[r1]))
        check('return (sale): reply lists the row and its lock', listed(reply).get(r1) == 1 and r1 in locks_in(reply),
              (listed(reply), locks_in(reply)))

        fodder, fusion_base = unit(10011), unit(10011, unit_lvl=1, total_exp=0)
        equip([(r1, 0, fodder, SPHERE, 0)])
        reply = c.call('UnitMix', body={'60subGk3': [{'81GjwoWy': '1', '2vnqRIr3': '2'}],
                                        'mCE3rUu5': [{'Rs7bCE3t': '0'}],
                                        'Km35HAXv': [{'edy7fq3L': str(fusion_base), 'mnZ5K4Ii': '1'},
                                                     {'edy7fq3L': str(fodder), 'mnZ5K4Ii': '2'}]})
        check('return (fusion): the row comes back with its lock', 'error' not in reply and rows()[r1] == (1, 1, 0)
              and r1 in locks_in(reply), (reply.get('error'), rows()[r1], locks_in(reply)))

        recipe = recipes[10011]
        evo_base = unit(10011)
        nodes, first_material = [], None
        for key, kind in zip(evo_keys, evo_kinds):
            material = int(recipe[key])
            if not material:
                continue
            if recipe[kind] == '2':
                db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,1) ON CONFLICT(user_id,item_id)'
                           ' DO UPDATE SET item_num=item_num+1', (user, material))
                db.commit()
                nodes.append({'inU8Q4gL': '0', 'mnZ5K4Ii': '2', '29MgiJIQ': '2'})
            else:
                uid = unit(material)
                first_material = first_material or uid
                nodes.append({'inU8Q4gL': str(uid), 'mnZ5K4Ii': '2', '29MgiJIQ': '1'})
        if first_material:
            equip([(r1, 0, first_material, SPHERE, 0)])
            reply = c.call('UnitEvo', body={
                '8Z2NQrx1': [{'inU8Q4gL': str(evo_base), 'mnZ5K4Ii': '1', '29MgiJIQ': '1'}] + nodes,
                'I82p0wCL': [{'pn16CNah': recipe['74VFwuTd']}],
                'mCE3rUu5': [{'Rs7bCE3t': recipe['Rs7bCE3t']}]})
            check('return (evolution): the row comes back with its lock', 'error' not in reply
                  and rows()[r1] == (1, 1, 0) and r1 in locks_in(reply), (reply.get('error'), rows()[r1]))
        else:
            check('return (evolution): recipe 10011 has a unit material', False, recipe)

        # ---- the lock set rides MissionEnd too -------------------------------------
        started = c.call('MissionStart', body=mission_start_body(10))
        reply = c.call('MissionEnd', body=mission_end_body(10, serial=started_serial(started)))
        check('mission end: warehouse and lock set both present', listed(reply).get(r1) == 1 and r1 in locks_in(reply),
              (listed(reply).get(r1), locks_in(reply)))
        db.close()

    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        db = fx.db()
        check('restart: the returned copy keeps its id, count and lock', rows()[r1] == (1, 1, 0), rows())
        dup = db.execute('SELECT instance_id,COUNT(*) n FROM user_warehouse_rows GROUP BY instance_id HAVING n>1').fetchall()
        check('restart: row ids stay unique', not dup, dup)
        db.close()
    return check.summary('warehouse integration')


if __name__ == '__main__':
    sys.exit(main())
