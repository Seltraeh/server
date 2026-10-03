"""SP enhancements: purchase smoke plus the ShopUse type-9 reset matrix.

    python scripts/test_fe_skill_qc_wire.py PATH_TO_DEBUG_EXE [--port 19995]

Copied save, isolated server (out/qa/current/fe_smoke); the player's own units
are never touched -- the fixture's first owned unit is re-seeded as an Omni
Vargas 10017.

The reset request is UnitDetailVirtuallyInfoScene::resetConnect @0x1BDEBDC:
ShopUse xe8tiSf4 / qthMXTQSkz3KfH9R, 32ibWjFG [{60IsqxDt 9, 03UGMHxF the price
the client shows (DefineMst reset_fe_skill_dia_count), 5gXxT7LZ 0, rA9jDCP5 0,
edy7fq3L the owned unit, 9i2xhMaJ}] -- every number quoted, as the client
sends it.  The scene redraws from team_info (gems) and the full 4ceMWH6k roster.

Policy under test (gme/handlers/ShopUse.cpp resetFeSkills): spent SP back to
available, skills cleared, used 0, cap and total kept, the server's price
charged once in one committed transaction; an empty unit or a short balance
changes nothing and resynchronises; unit-level refusals go Home.
"""
import argparse
import concurrent.futures
import json
import sys
from bf_testkit import ROOT, Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir  # noqa: E402

FE_GET = ('nSQxNOeL', 'nZ2bVoWu')
SHOP_USE = ('xe8tiSf4', 'qthMXTQSkz3KfH9R')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19995)
    args = ap.parse_args()
    fx = Fixture.create(qa_dir('fe_smoke'), port=args.port)
    check = Checker()
    skills = json.loads((ROOT/'deploy/mst/fe_skill_mst.json').read_text(encoding='utf-8'))['2h9r3yEY']
    cost = {s['ri6D9yBi']: int(s['G5wFDad6']) for s in skills}
    defines = json.loads((ROOT/'deploy/mst/defines_mst.json').read_text(encoding='utf-8'))
    price = int(defines['5csFoG1G'])
    check('the reset price is DefineMst reset_fe_skill_dia_count = 1 gem', price == 1, price)

    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx)
        db = fx.db()
        uid = db.execute('SELECT user_unit_id FROM user_units WHERE user_id=? ORDER BY user_unit_id LIMIT 1',
                         (c.user,)).fetchone()[0]
        roster_size = db.execute('SELECT count(*) FROM user_units WHERE user_id=?', (c.user,)).fetchone()[0]

        def seed(**changes):
            data = dict(unit_id='10017', unit_lvl=150, bb_lvl=10, sbb_lvl=10, fe_sp=100, fe_used_sp=0,
                        fe_max_sp=100, fe_skill_info='', user_id=c.user)
            data.update(changes)
            db.execute('UPDATE user_units SET ' + ','.join(k + '=?' for k in data) + ' WHERE user_unit_id=?',
                       (*data.values(), uid))
            db.commit()

        def gems(value=None):
            if value is not None:
                db.execute('UPDATE user_info SET gems=? WHERE id=?', (value, c.user))
                db.commit()
            return db.execute('SELECT gems FROM user_info WHERE id=?', (c.user,)).fetchone()[0]

        def state():
            return tuple(db.execute('SELECT fe_sp,fe_used_sp,fe_max_sp,fe_skill_info FROM user_units'
                                    ' WHERE user_unit_id=?', (uid,)).fetchone())

        def others():
            return [tuple(r) for r in db.execute(
                'SELECT user_unit_id,fe_sp,fe_used_sp,fe_max_sp,fe_skill_info FROM user_units'
                ' WHERE user_id=? AND user_unit_id!=? ORDER BY user_unit_id', (c.user, uid))]

        def unit(reply):
            return next((r for r in reply.get('4ceMWH6k', []) if str(r.get('edy7fq3L')) == str(uid)), {})

        def header_gems(reply):
            team = (reply.get('fEi17cnx') or [{}])[0]
            return int(team['03UGMHxF']) if '03UGMHxF' in team else None

        def buy(skill):
            return c.call(*FE_GET, {'6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
                                    'bx56032l': [{'pn16CNah': '10017', 'edy7fq3L': str(uid), 'ri6D9yBi': skill}]})

        def reset(owned=None, claimed=None):
            return c.call(*SHOP_USE, {'6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
                                      '32ibWjFG': [{'60IsqxDt': '9', '03UGMHxF': str(price if claimed is None else claimed),
                                                    '5gXxT7LZ': '0', 'rA9jDCP5': '0',
                                                    'edy7fq3L': str(uid if owned is None else owned),
                                                    '9i2xhMaJ': '0'}]})

        def refused(label, reply, before, before_gems):
            check(f'{label}: refused before anything changes',
                  'error' in reply and state() == before and gems() == before_gems,
                  (reply.get('error'), state(), gems()))

        # ---- purchase smoke (unchanged from the first QC) -------------------------------
        seed()
        gems(10)
        r = buy('510000')
        check('purchase: valid owned node spends exact SP and persists skill',
              'error' not in r and state() == (100 - cost['510000'], cost['510000'], 100, '1@510000'),
              {'error': r.get('error'), 'stored': state()})
        check('purchase: complete roster refresh contains acquired skill', unit(r).get('Fnxab5CN') == '1@510000',
              unit(r).get('Fnxab5CN'))
        r = c.call('UserInfo')
        check('schema: UserInfo sends the acquired-skill column', unit(r).get('Fnxab5CN') == '1@510000',
              unit(r).get('Fnxab5CN'))

        # ---- reset: refund, clear, one gem ------------------------------------------------
        buy('610000')
        buy('1020001')
        spent = cost['510000'] + cost['610000'] + cost['1020001']
        check('setup: three skills in two categories bought',
              state() == (100 - spent, spent, 100, '1@510000:610000/2@1020001'), state())
        bystanders = others()
        r = reset()
        check('reset: every spent SP refunded, skills cleared, used 0, cap kept, one gem charged',
              'error' not in r and state() == (100, 0, 100, '') and gems() == 9, {'stored': state(), 'gems': gems(),
                                                                                    'error': r.get('error')})
        check('reset: total SP (available + used) preserved, nothing granted', sum(state()[:2]) == 100, state())
        wire = unit(r)
        check('reset reply: full roster with the cleared unit (skills, SP, used, cap)',
              len(r.get('4ceMWH6k', [])) == roster_size and wire.get('Fnxab5CN') in ('', '0')
              and str(wire.get('bFQbZh3x')) == '100' and str(wire.get('3RgneFpP')) == '0'
              and str(wire.get('GIO9DTif')) == '100', wire)
        check('reset reply: team_info carries the charged balance', header_gems(r) == 9, header_gems(r))
        check('reset: no other unit changed', others() == bystanders)

        # ---- retry / empty reset ---------------------------------------------------------
        r = reset()
        check('retry of the reset: empty unit, nothing charged, ordinary resync reply',
              'error' not in r and state() == (100, 0, 100, '') and gems() == 9 and bool(unit(r))
              and header_gems(r) == 9, (r.get('error'), state(), gems()))
        seed(fe_sp=37, fe_used_sp=0, fe_skill_info='')
        r = reset()
        check('an already-empty unit (37 SP unspent) is never charged and keeps its SP',
              'error' not in r and state() == (37, 0, 100, '') and gems() == 9, (state(), gems()))
        for _ in range(3):
            reset()
        check('repeated empty resets charge nothing', gems() == 9, gems())

        # ---- the server's price, not the claim ---------------------------------------------
        for claimed in ('5', '0', '-3'):
            seed(fe_sp=90, fe_used_sp=10, fe_skill_info='1@510000')
            before = gems()
            r = reset(claimed=claimed)
            check(f'claimed price {claimed}: the server charges its own {price}',
                  'error' not in r and state() == (100, 0, 100, '') and gems() == before - price,
                  (r.get('error'), state(), before, gems()))

        # ---- no gems --------------------------------------------------------------------
        seed(fe_sp=90, fe_used_sp=10, fe_skill_info='1@510000')
        gems(0)
        r = reset()
        check('no gems: nothing changes, the reply resynchronises (0 gems, skills kept)',
              'error' not in r and state() == (90, 10, 100, '1@510000') and gems() == 0
              and header_gems(r) == 0 and unit(r).get('Fnxab5CN') == '1@510000', (r.get('error'), state(), gems()))
        gems(10)

        # ---- wrong, foreign, missing and malformed -------------------------------------------
        seed(fe_sp=90, fe_used_sp=10, fe_skill_info='1@510000')
        before, before_gems = state(), gems()
        refused('missing owned unit', reset(owned=2147483647), before, before_gems)
        for label, owned in (('empty unit id', ''), ('zero unit id', '0'), ('negative unit id', '-1'),
                             ('malformed unit id', '12x'), ('overflowing unit id', '9' * 40)):
            refused(label, reset(owned=owned), before, before_gems)
        refused('malformed price', reset(claimed='1x'), before, before_gems)
        seed(fe_sp=90, fe_used_sp=10, fe_skill_info='1@510000', unit_id='10016')
        before = state()
        refused('a non-Omni unit (rarity 7) has no enhancements to reset', reset(), before, before_gems)
        seed(fe_sp=90, fe_used_sp=10, fe_skill_info='1@510000', user_id='fe-test-other-owner')
        r = reset()
        foreign = db.execute('SELECT fe_sp,fe_used_sp,fe_skill_info FROM user_units WHERE user_unit_id=?',
                             (uid,)).fetchone()
        check("another player's unit: refused, their unit and our gems untouched",
              'error' in r and tuple(foreign) == (90, 10, '1@510000') and gems() == before_gems,
              (r.get('error'), tuple(foreign), gems()))
        seed(fe_sp=-5, fe_used_sp=10, fe_skill_info='1@510000')
        before = state()
        refused('corrupt negative SP', reset(), before, before_gems)

        # ---- rollback -----------------------------------------------------------------------
        seed(fe_sp=90, fe_used_sp=10, fe_skill_info='1@510000')
        db.execute("CREATE TRIGGER fe_reset_fail BEFORE UPDATE OF fe_skill_info ON user_units"
                   " BEGIN SELECT RAISE(ABORT,'qc reset rollback'); END")
        db.commit()
        before, before_gems = state(), gems()
        refused('a database failure rolls back the charge with the refund', reset(), before, before_gems)
        db.execute('DROP TRIGGER fe_reset_fail')
        db.commit()
        before = gems()
        r = reset()
        check('the retry after the failure resets once', 'error' not in r and state() == (100, 0, 100, '')
              and gems() == before - price, (r.get('error'), state(), before, gems()))

        # ---- concurrent copies ----------------------------------------------------------
        seed(fe_sp=90, fe_used_sp=10, fe_skill_info='1@510000')
        before = gems()
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
            replies = list(pool.map(lambda _: reset(), range(3)))
        check('three simultaneous resets: one refund, one charge, all replies ordinary',
              all('error' not in r for r in replies) and state() == (100, 0, 100, '') and gems() == before - price,
              ([r.get('error') for r in replies], state(), before, gems()))

        # ---- repurchase, then a genuine second reset -------------------------------------
        r = buy('510000')
        check('repurchase after a reset works from the refunded SP',
              'error' not in r and state() == (90, 10, 100, '1@510000'), state())
        before = gems()
        r = reset()
        check('a second, genuine reset after repurchasing charges again (indistinguishable from a late copy)',
              'error' not in r and state() == (100, 0, 100, '') and gems() == before - price, (state(), gems()))

        # ---- other ShopUse types keep their shape --------------------------------------------
        box_before = db.execute('SELECT max_unit_count FROM user_info WHERE id=?', (c.user,)).fetchone()[0]
        before = gems()
        r = c.call(*SHOP_USE, {'32ibWjFG': [{'60IsqxDt': '2', '03UGMHxF': '1', '5gXxT7LZ': '5', 'rA9jDCP5': '0'}]})
        box_after = db.execute('SELECT max_unit_count FROM user_info WHERE id=?', (c.user,)).fetchone()[0]
        check('type 2 unit-box expansion still works and sends no roster',
              'error' not in r and '4ceMWH6k' not in r and box_after == box_before + 5 and gems() == before - 1,
              (r.get('error'), box_before, box_after, sorted(r)))
        log = server.log_text()
        check('the server log records the reset and the refusals',
              'ShopUse: reset user unit' in log and 'no enhancements to reset' in log)

        seed(fe_sp=80, fe_used_sp=20, fe_skill_info='1@510000:610000')
        persisted = state()
        persisted_gems = gems()
        db.close()

    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx)
        db = fx.db()
        check('restart: SP and skills survive', state() == persisted, state())
        r = reset()
        check('restart: a reset after the restart refunds and charges once',
              'error' not in r and state() == (100, 0, 100, '') and gems() == persisted_gems - price, (state(), gems()))
        r = c.call('UserInfo')
        check('restart: login refresh shows the cleared unit', unit(r).get('Fnxab5CN') in ('', '0')
              and str(unit(r).get('bFQbZh3x')) == '100', unit(r))
        db.close()
    return check.summary('FE skill independent QC')


if __name__ == '__main__':
    sys.exit(main())
