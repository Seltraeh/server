"""#25: max-level Omni units gain enhancement SP from frogs (and nothing else does).

python scripts/test_enhancement_sp_wire.py PATH_TO_DEBUG_EXE [--port 19977]

Before the fix every unit went out with SP 0 / used 0 / limit 0, so the
client's own check (GameUtils::mixFeBpCheck: sp + gain + used vs limit) left
no room and greyed out every SP-only material.  The per-material amounts are
the client's (UserState::getUserUnitListEfPoint @0x1247888), which match the
Global wiki's Unit Skills page (rev 656738).  Everything runs on an isolated
SQLite backup under out/; requests use the client's quoted-number encoding.
"""
import argparse
import json
import sys
from bf_testkit import ROOT, Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir  # noqa: E402

BURST_FROG, LAWSON_FROG, BURST_EMPEROR = 10312, 10522, 10313
BURST_QUEEN, OMNI_FROG, OMNI_EMPEROR, SPHERE_FROG = 750004, 750003, 750005, 20302


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19977)
    args = ap.parse_args()
    mst = {int(r['pn16CNah']): r for r in json.loads(
        (ROOT / 'deploy/mst/unit_mst.json').read_text(encoding='utf-8'))['2r9cNSdt']}
    # An Omni species that has an SBB, and a lower form of the same line.
    omni = next(r for r in mst.values() if r['7ofj5xa1'] == '8' and r['EI1DF8Yt'] == '150'
                and r.get('iEFZ6H19', '0') not in ('', '0'))
    omni_id = int(omni['pn16CNah'])
    sibling = next(i for i, r in mst.items() if r['9PsmH7tz'] == omni['9PsmH7tz'] and i != omni_id)
    seven = next(r for r in mst.values() if r['7ofj5xa1'] == '7' and r['EI1DF8Yt'] == '120'
                 and r.get('iEFZ6H19', '0') not in ('', '0'))
    # Plain exp fodder from ANOTHER line: no burst boost, not a frog, not a
    # duplicate of the base (a same-line unit is worth 5 as a duplicate).
    ordinary = next(i for i, r in mst.items() if r['7ofj5xa1'] == '2' and r['9PsmH7tz'] != omni['9PsmH7tz']
                    and r.get('PXD4v2KY', '0') == '0' and i not in (BURST_FROG, LAWSON_FROG, BURST_EMPEROR,
                                                                   BURST_QUEEN, OMNI_FROG, OMNI_EMPEROR, SPHERE_FROG))

    fx = Fixture.create(qa_dir('enhancement_sp'), port=args.port)
    # Exercise the migration on an explicitly pre-SP fixture. A copied live
    # save may already contain legitimately earned/spent SP; asserting that
    # every existing unit still has defaults would reject correct persistence.
    # This schema rewind applies ONLY to the disposable copied database.
    with fx.db() as setup:
        columns = {r[1] for r in setup.execute('PRAGMA table_info(user_units)')}
        for column in ('fe_sp', 'fe_used_sp', 'fe_max_sp'):
            if column in columns:
                setup.execute(f'ALTER TABLE user_units DROP COLUMN {column}')
        setup.execute("DELETE FROM migration_status WHERE hash='29092026_UnitEnhancementPoints'")
    check = Checker()
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        template = dict(db.execute('SELECT * FROM user_units WHERE user_id=? LIMIT 1', (user,)).fetchone())
        template.pop('user_unit_id')

        cols = {r[1] for r in db.execute('PRAGMA table_info(user_units)')}
        migrated = {'fe_sp', 'fe_used_sp', 'fe_max_sp'} <= cols
        check('migration: SP columns exist', migrated, 'fe_* columns missing')
        if migrated:
            defaults = tuple(db.execute('SELECT MIN(fe_sp),MAX(fe_sp),MAX(fe_used_sp),MIN(fe_max_sp),'
                                        'MAX(fe_max_sp) FROM user_units').fetchone())
        else:
            defaults = None
        check('migration: existing units start at 10 SP / 0 spent / limit 100', defaults == (10, 10, 0, 100, 100),
              defaults)

        def unit(species, **updates):
            data = mst[species]
            row = dict(template, user_id=user, unit_id=str(species), unit_type_id=1, unit_lvl=1,
                       total_exp=0, exp=0, bb_lvl=1, sbb_lvl=0, favorite_flg=0,
                       sphere_ext=1, eqip_item_id=0, eqip_item_id2=0,
                       eqip_item_frame_id=0, eqip_item_frame_id2=0,
                       bb_id=data['nj9Lw7mV'], sbb_id=data['iEFZ6H19'])
            for col in ('fe_sp', 'fe_used_sp', 'fe_max_sp'):
                row.pop(col, None)          # a new unit takes the column defaults
            row.update(updates)
            cur = db.execute(f"INSERT INTO user_units ({','.join(row)}) VALUES ({','.join('?' for _ in row)})",
                             tuple(row.values()))
            db.commit()
            return cur.lastrowid

        def maxed(species=omni_id, **updates):
            # total_exp far past the pattern's end: the server derives level 150.
            return unit(species, **dict(dict(unit_lvl=150, total_exp=999999999, bb_lvl=10, sbb_lvl=10), **updates))

        def sp(uid):
            if not migrated:            # an executable from before the fix
                return None
            return tuple(db.execute('SELECT fe_sp,fe_used_sp,fe_max_sp FROM user_units WHERE user_unit_id=?',
                                    (uid,)).fetchone())

        def set_sp(uid, fe_sp, used):
            if migrated:
                db.execute('UPDATE user_units SET fe_sp=?, fe_used_sp=? WHERE user_unit_id=?', (fe_sp, used, uid))
                db.commit()

        def exists(uid):
            return db.execute('SELECT 1 FROM user_units WHERE user_unit_id=?', (uid,)).fetchone() is not None

        def mix(base, materials):
            return c.call('Mw08CIg2', 'JnegC7RrN3FoW8dQ', {
                '60subGk3': [{'81GjwoWy': '1', '2vnqRIr3': '2'}],
                'mCE3rUu5': [{'Rs7bCE3t': '0'}],
                'Km35HAXv': [{'edy7fq3L': str(base), 'mnZ5K4Ii': '1'}]
                            + [{'edy7fq3L': str(m), 'mnZ5K4Ii': '2'} for m in materials]})

        def result(reply):
            ope = (reply.get('1ZbHB6Im') or [{}])[0]
            return tuple(int(ope.get(k, -1)) for k in ('0xhLsnDv', '3YVEeL2a', 'kuHWF7yc', 'ksED59z9'))

        def roster(reply, uid):
            row = next((u for u in reply.get('4ceMWH6k', []) if u.get('edy7fq3L') == str(uid)), {})
            return tuple(int(row.get(k, -1)) for k in ('bFQbZh3x', '3RgneFpP', 'GIO9DTif'))

        # ---- the wire: every unit now carries its SP ------------------------------
        view = c.call('UserInfo')
        units = view.get('4ceMWH6k', [])
        check('wire: UserInfo units carry SP 10 / used 0 / limit 100',
              units and all((u.get('bFQbZh3x'), u.get('3RgneFpP'), u.get('GIO9DTif')) == ('10', '0', '100')
                            for u in units), units[:1])

        # ---- each material class, on one eligible base, in sequence --------------
        base = maxed()
        steps = [(BURST_FROG, 1), (LAWSON_FROG, 2), (BURST_EMPEROR, 5), (BURST_QUEEN, 20),
                 (OMNI_FROG, 30), (sibling, 5), (SPHERE_FROG, 10)]
        total = 10
        for species, gain in steps:
            mat = unit(species)
            reply = mix(base, [mat])
            name = mst[species].get('utP1c0CD', str(species))
            check(f'gain: {species} ({name}) gives {gain} SP',
                  'error' not in reply and (sp(base) or (None,))[0] == total + gain and not exists(mat),
                  (reply.get('error'), sp(base)))
            check(f'result: {species} before/after/limit shown', result(reply) == (total, total + gain, 100, 100),
                  result(reply))
            check(f'roster: {species} new SP rides the full unit refresh',
                  roster(reply, base) == (total + gain, 0, 100), roster(reply, base))
            total += gain
        check('sequence: 10 + 1 + 2 + 5 + 20 + 30 + 5 + 10 = 83', total == 83 and sp(base) == (83, 0, 100), sp(base))

        # A batch sums, and the limit counts spent SP: 83 + 31 stops at 100.
        a, b = unit(OMNI_FROG), unit(BURST_FROG)
        reply = mix(base, [a, b])
        check('limit: a batch past the limit stops exactly at it', sp(base) == (100, 0, 100) and result(reply)[:2] == (83, 100),
              (sp(base), result(reply)))
        reply = mix(base, [unit(BURST_FROG)])
        check('limit: a full unit gains nothing more', sp(base) == (100, 0, 100), sp(base))

        # Spent SP counts against the same limit.
        spent = maxed()
        set_sp(spent, 40, 50)
        reply = mix(spent, [unit(OMNI_FROG)])
        check('limit: 40 unspent + 50 spent takes only 10 of an Omni Frog', sp(spent) == (50, 50, 100)
              and result(reply) == (40, 50, 100, 100), (sp(spent), result(reply)))

        # ---- the Sphere Frog: slot first, SP only once the slot exists ------------
        one_slot = maxed(sphere_ext=0, eqip_item_frame_id2=-1)
        reply = mix(one_slot, [unit(SPHERE_FROG)])
        opened = db.execute('SELECT sphere_ext,eqip_item_frame_id2 FROM user_units WHERE user_unit_id=?',
                            (one_slot,)).fetchone()
        check('sphere frog: opens the slot and gives no SP when the slot was closed',
              tuple(opened) == (1, 0) and (sp(one_slot) or (None,))[0] == 10, (tuple(opened), sp(one_slot)))

        # ---- ineligible bases gain nothing ----------------------------------------
        for label, uid in (('level below max', unit(omni_id, unit_lvl=149, total_exp=1000, bb_lvl=10, sbb_lvl=10)),
                           ('SBB below 10', maxed(sbb_lvl=9)),
                           ('not Omni (rarity 7)', maxed(int(seven['pn16CNah'])))):
            before = sp(uid)
            reply = mix(uid, [unit(OMNI_FROG)])
            check(f'ineligible: {label} gains no SP', 'error' not in reply and sp(uid) == before,
                  (sp(uid), reply.get('error')))
        emperor_base = maxed()
        mix(emperor_base, [unit(OMNI_EMPEROR)])
        check('omni emperor: no Omni+3 in this server, so no SP or limit change', sp(emperor_base) == (10, 0, 100),
              sp(emperor_base))
        ordinary_base = maxed()
        mix(ordinary_base, [unit(ordinary)])
        check('ordinary unit: no SP (the wiki\'s random +1 has no documented rate)', sp(ordinary_base) == (10, 0, 100),
              sp(ordinary_base))

        # ---- a refused fusion changes nothing -----------------------------------
        refused = maxed()
        frog = unit(OMNI_FROG)
        reply = mix(refused, [frog, 987654321])
        check('refusal: unowned material fails the whole fusion', 'error' in reply and sp(refused) == (10, 0, 100)
              and exists(frog), (reply.get('error'), sp(refused)))
        db.close()

    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx)
        view = c.call('UserInfo')
        saved = next((u for u in view.get('4ceMWH6k', []) if u.get('edy7fq3L') == str(base)), {})
        check('restart: saved SP is served again', (saved.get('bFQbZh3x'), saved.get('GIO9DTif')) == ('100', '100'),
              saved.get('bFQbZh3x'))
    return check.summary('enhancement SP')


if __name__ == '__main__':
    sys.exit(main())
