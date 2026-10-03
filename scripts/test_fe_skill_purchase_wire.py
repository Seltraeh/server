"""Encrypted purchase regression; copied SQLite save, never the live server."""
import argparse
import concurrent.futures
import json
import time
from bf_testkit import ROOT, Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    args = ap.parse_args()
    fx = Fixture.create(qa_dir('fe_skill_purchase'), port=19995)
    ck = Checker()
    trees = json.loads((ROOT/'deploy/mst/unit_fe_skill_mst.json').read_text(encoding='utf-8'))['kXes8fSi']
    costs = {int(x['ri6D9yBi']): int(x['G5wFDad6']) for x in json.loads(
        (ROOT/'deploy/mst/fe_skill_mst.json').read_text(encoding='utf-8'))['2h9r3yEY']}
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        uid = db.execute('SELECT user_unit_id FROM user_units WHERE user_id=? LIMIT 1', (c.user,)).fetchone()[0]
        def seed(**changes):
            data = dict(unit_id=10017, unit_lvl=150, bb_lvl=10, sbb_lvl=10,
                        fe_sp=100, fe_used_sp=0, fe_max_sp=100, fe_skill_info='', user_id=c.user)
            data.update(changes)
            db.execute('UPDATE user_units SET '+','.join(k+'=?' for k in data)+' WHERE user_unit_id=?',
                       (*data.values(), uid))
            db.commit()
        def state():
            return tuple(db.execute('SELECT fe_sp,fe_used_sp,fe_max_sp,fe_skill_info FROM user_units WHERE user_unit_id=?', (uid,)).fetchone())
        def buy(skill='510000', species='10017', owned=None):
            return c.call('nSQxNOeL', 'nZ2bVoWu', {
                '6FrKacq7':[{'Kn51uR4Y':'5EdKHavF'}],
                'bx56032l':[{'pn16CNah':str(species), 'edy7fq3L':str(uid if owned is None else owned), 'ri6D9yBi':str(skill)}]})
        def wire_unit(r):
            return next((u for u in r.get('4ceMWH6k', []) if str(u['edy7fq3L']) == str(uid)), {})
        def refused(label, **request):
            before = state()
            r = buy(**request)
            ck(label, 'error' in r and state() == before, r.get('error'))

        seed()
        roster = db.execute('SELECT count(*) FROM user_units WHERE user_id=?', (c.user,)).fetchone()[0]
        r = buy()
        ck('purchase exact cost and category', state() == (90,10,100,'1@510000'), state())
        ck('full roster returned', len(r.get('4ceMWH6k', [])) == roster)
        ck('wire refresh acquired skill and SP', wire_unit(r).get('Fnxab5CN') == '1@510000'
           and str(wire_unit(r).get('bFQbZh3x')) == '90', wire_unit(r))
        before = state()
        r = buy()
        ck('retry refreshes without duplicate spending', 'error' not in r and state() == before and bool(wire_unit(r)))
        r = buy('610000')
        ck('second skill preserves first', state() == (80,20,100,'1@510000:610000'), state())
        r = buy('1020001')
        ck('second category serialized', state() == (60,40,100,'1@510000:610000/2@1020001'), state())
        seed(fe_sp=10, fe_used_sp=90)
        buy()
        ck('exact balance and used-SP cap accepted', state() == (0,100,100,'1@510000'), state())
        for label, changes in [('low SP',dict(fe_sp=9)), ('spent cap',dict(fe_used_sp=95)),
                               ('low level',dict(unit_lvl=149)), ('low SBB',dict(sbb_lvl=9)),
                               ('foreign unit',dict(user_id='fe-test-other-owner'))]:
            seed(**changes)
            refused(label)
        seed()
        before = state()
        r = c.call('nSQxNOeL', 'nZ2bVoWu', {})
        ck('missing request fields do not mutate unit', 'error' in r and state() == before)
        for label, request in [('wrong species',dict(species=20017)), ('unknown node',dict(skill=2147483647)),
                               ('missing owned unit',dict(owned=2147483647)), ('negative id',dict(owned=-1)),
                               ('malformed id',dict(skill='510000junk')), ('overflow id',dict(skill='9'*40))]:
            refused(label, **request)
        node = next(x for x in trees if x['FHThxDv4'].startswith('1@') and '/' not in x['FHThxDv4'])
        species = node['pn16CNah']
        # This shipped Omni uses the same level-150 eligibility as Vargas.
        seed(unit_id=int(species))
        refused('missing prerequisite', skill=node['ri6D9yBi'], species=species)
        prerequisite = node['FHThxDv4'].split('@')[1]
        prereqnode = next(x for x in trees if x['pn16CNah']==species and x['ri6D9yBi']==prerequisite)
        spent = costs[int(prerequisite)]
        seed(unit_id=int(species), fe_sp=100-spent, fe_used_sp=spent,
             fe_skill_info=f"{prereqnode['VgU78CYj']}@{prerequisite}")
        r = buy(node['ri6D9yBi'], species)
        ck('prerequisite permits purchase', 'error' not in r and state()[1] == spent+costs[int(node['ri6D9yBi'])], state())
        seed()
        db.execute("CREATE TRIGGER fe_test_fail BEFORE UPDATE OF fe_skill_info ON user_units BEGIN SELECT RAISE(ABORT,'test rollback'); END")
        db.commit()
        refused('database failure rolls back all SP')
        db.execute('DROP TRIGGER fe_test_fail'); db.commit()
        seed()
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            replies = list(pool.map(lambda _: buy(), range(2)))
        ck('simultaneous duplicate spends once', state() == (90,10,100,'1@510000'), state())
        ck('both duplicate replies refresh roster', all(bool(wire_unit(r)) for r in replies))
        # Delayed A after B is also idempotent.
        buy('610000'); before = state(); buy()
        ck('A/B/A delayed retry does not charge again', state() == before)
        persisted = state()
        db.close()
    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx); db = fx.db()
        ck('restart preserves SP and skills', state() == persisted)
        r = c.call('UserInfo')
        ck('login refresh includes acquired skills', wire_unit(r).get('Fnxab5CN') == persisted[3])
        db.close()
    return ck.summary('FE skill purchase')


if __name__ == '__main__':
    raise SystemExit(main())
