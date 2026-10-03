"""#27: Brave Slots -- the wiki's prize list, all reel pictures, exact multi-pull grants.

python scripts/test_brave_slots_wire.py PATH_TO_DEBUG_EXE [--port 19982]

A. Table integrity (no server): every row of deploy/archive/brave_slots.json
   draws on reel pictures the machine has AND that sit on its reel strips; unit
   and item ids exist; every matching on the Global wiki "Slots" page (rev
   658840) has rows and no row is outside it.  Weights are authored (no source
   carries the odds), so they are only checked to be positive.
B. Real table, 1..10 pulls: exactly N results; each names a table row whose
   reels it stops on; medals fall by exactly 3N (+1 per medal prize); every unit,
   item and sphere the results name is in storage afterwards, and nothing else.
C. Deterministic categories: the fixture's archive holds ONE row, so ten pulls
   must pay exactly that prize ten times -- unit, battle item (x10 each), sphere,
   Raid Medal.
D. Walk-down and refusal: 5 medals plays one pull, 2 medals plays none.
E. A failure part-way through an action rolls the whole action back.
F. Restart: medals and prizes persist.

The multi-roll LIST's blank tiles are client-side and not tested here:
RandallSlotResultListScene::downloadFiles requests nothing (0x1A72AB4), so a
tile shows only artwork the client already holds (see the session handoff).
"""
import argparse
import json
import shutil
import sys
from collections import Counter
from pathlib import Path
from bf_testkit import ROOT, Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir  # noqa: E402

WIKI = {  # reels -> the prizes the wiki's matching may pay
    (1, 1, 1): {20302}, (3, 3, 3): {10313}, (2, 2, 2): {10312},
    (8, 8, 8): {10344, 20334, 30324, 40324, 50364, 60334},
    (18, 18, 18): {30008, 30108, 30208, 30308, 30007, 30107, 30207, 30307, 30006, 30106, 30206, 30306,
                   30005, 30105, 30205, 30305, 30004, 30104, 30204, 30304, 30003, 30103, 30203, 30303},
    (7, 7, 7): {50612}, (6, 6, 6): {10452, 20442, 30432, 40432},
    (80, 80, 80): {10204, 20204, 30204, 40204, 50204, 60134},
    (13, 13, 13): {70000, 70100, 70200, 70300, 70400, 70500, 70600},
    (81, 81, 81): {50133}, (1, 7, 82): {1}, (9, 9, 9): {50132},
}
COST = 3


def mst(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19982)
    args = ap.parse_args()
    check = Checker()
    table = json.loads((ROOT / 'deploy/archive/brave_slots.json').read_text(encoding='utf-8'))
    machine = json.loads((ROOT / 'deploy/system/brave_slots.json').read_text(encoding='utf-8'))
    pictures = {int(p['sE6tyI9i']) for p in machine['rY6j0Jvs']}
    strips = [{int(x) for x in r['Z8eJi4pq'].split(',')} for r in machine['iW62Scdg']]
    units = {int(r['pn16CNah']) for r in mst('unit')}
    items = {int(r['kixHbe54']) for r in mst('item')}

    # ---- A. table integrity ------------------------------------------------------
    for r in table:
        reels = r['reels']
        check(f"table {r['key']}: reels drawable and on every strip",
              all(p in pictures for p in reels) and all(set(reels) <= s for s in strips), reels)
        known = r['target_id'] in (units if r['present_type'] == 6 else items if r['present_type'] in (4, 5, 7)
                                   else {1} if r['present_type'] == 12 else set())
        check(f"table {r['key']}: prize exists", known and r['target_cnt'] >= 1 and r['weight'] > 0,
              (r['present_type'], r['target_id']))
        check(f"table {r['key']}: on the wiki's list", r['target_id'] in WIKI.get(tuple(reels), set()), reels)
    for reels, prizes in WIKI.items():
        paid = {r['target_id'] for r in table if tuple(r['reels']) == reels}
        check(f'wiki matching {reels}: every listed prize is reachable', paid == prizes, sorted(prizes - paid))

    by_payout = {(r['target_id'], r['target_cnt'], ','.join(map(str, r['reels']))): r for r in table}

    def run(fx, label, body):
        with IsolatedServer(args.exe, fx, log_name=label + '.log'):
            c = Client(fx)
            db = fx.db()
            try:
                return body(c, db)
            finally:
                db.close()

    def helpers(c, db):
        def execute(sql, params=()):
            r = db.execute(sql, params)
            db.commit()
            return r

        def medals(value=None):
            if value is not None:
                execute('INSERT INTO user_brave_medals(user_id,medal_id,possession) VALUES (?,?,?)'
                        ' ON CONFLICT(user_id,medal_id) DO UPDATE SET possession=excluded.possession',
                        (c.user, '1', value))
            row = db.execute("SELECT possession FROM user_brave_medals WHERE user_id=? AND medal_id='1'",
                             (c.user,)).fetchone()
            return row[0] if row else None

        def owned():
            u = Counter(int(str(r[0]).split('_')[0]) for r in db.execute(
                'SELECT unit_id FROM user_units WHERE user_id=?', (c.user,)))
            i = Counter({r[0]: r[1] for r in db.execute('SELECT item_id,item_num FROM user_items WHERE user_id=?',
                                                      (c.user,))})
            return u, i

        def pull(n):
            return c.call('vChFp73J', 'hm9X6BQj', {'kLz5ujP2': [{'zS45RFGb': '1', 'd04gRmkE': str(n)}]})

        return execute, medals, owned, pull

    # ---- B. the real table, 1..10 pulls ----------------------------------------------
    fx = Fixture.create(qa_dir('slots'), port=args.port)

    def real(c, db):
        execute, medals, owned, pull = helpers(c, db)
        for n in range(1, 11):
            medals(COST * n)
            before_u, before_i = owned()
            reply = pull(n)
            results = reply.get('s8r5M6wI', [])
            after_u, after_i = owned()
            check(f'{n} pull(s): exactly {n} results', 'error' not in reply and len(results) == n,
                  (reply.get('error'), len(results)))
            rows = []
            for res in results:
                target, count = (int(x) for x in res['CW1bko4F'].split('@'))
                rows.append(by_payout.get((target, count, res['h6smq0WE'])))
            check(f'{n} pull(s): every result is a table row, stopping on its own reels', all(rows) and
                  all(res['h6smq0WE'] == res['D20kuSLy'] for res in results), [r['CW1bko4F'] for r in results])
            won = sum(1 for r in rows if r and r['present_type'] == 12)
            check(f'{n} pull(s): medals charged exactly {COST}x{n}, medal prizes paid', medals() == won,
                  (medals(), won))
            want_u = Counter(r['target_id'] for r in rows if r and r['present_type'] == 6
                             for _ in range(r['target_cnt']))
            want_i = Counter()
            for r in rows:
                if r and r['present_type'] in (4, 5, 7):
                    want_i[r['target_id']] += r['target_cnt']
            check(f'{n} pull(s): exactly the prized units arrived', after_u - before_u == want_u and
                  not (before_u - after_u), (after_u - before_u, want_u))
            check(f'{n} pull(s): exactly the prized items arrived', after_i - before_i == want_i and
                  not (before_i - after_i), (after_i - before_i, want_i))
            if want_u:
                check(f'{n} pull(s): new units ride the reply', len(reply.get('qC2tJs4E', [])) == sum(want_u.values()))
            if want_i:
                check(f'{n} pull(s): the warehouse rides the reply', bool(reply.get('9wjrh74P')))
    run(fx, 'real_table', real)

    # ---- C. one-row tables: every category, ten times ---------------------------------
    archive = qa_dir('slot_archive')
    archive.mkdir(parents=True, exist_ok=True)
    for f in (ROOT / 'deploy/archive').iterdir():
        if f.is_file() and '.bak' not in f.name and f.name != 'brave_slots.json':
            if not (archive / f.name).exists():
                shutil.copy2(f, archive / f.name)
    rows_by_key = {r['key']: r for r in table}
    fx = Fixture.create(qa_dir('slots_single'), port=args.port, archive_root=archive)
    for key in ('sphere_frog', 'item_70000', 'sphere_30008', 'raid_medal'):
        prize = rows_by_key[key]
        (archive / 'brave_slots.json').write_text(json.dumps([prize], indent=4), encoding='utf-8')

        def single(c, db, prize=prize, key=key):
            execute, medals, owned, pull = helpers(c, db)
            medals(COST * 10)
            before_u, before_i = owned()
            results = pull(10).get('s8r5M6wI', [])
            after_u, after_i = owned()
            check(f'{key}: ten results, all this prize',
                  len(results) == 10 and all(r['CW1bko4F'] == f"{prize['target_id']}@{prize['target_cnt']}"
                                             for r in results), [r['CW1bko4F'] for r in results])
            if prize['present_type'] == 6:
                check(f'{key}: ten units granted', (after_u - before_u) == Counter({prize['target_id']: 10}))
            elif prize['present_type'] == 12:
                check(f'{key}: medals 30 - 30 + 10', medals() == 10, medals())
            else:
                check(f"{key}: {10 * prize['target_cnt']} granted",
                      (after_i - before_i) == Counter({prize['target_id']: 10 * prize['target_cnt']}),
                      after_i - before_i)
            if key == 'sphere_frog':
                # D. the client's walk-down, and a refusal that changes nothing
                medals(5)
                results = pull(10).get('s8r5M6wI', [])
                check('walk-down: 5 medals plays exactly one pull', len(results) == 1 and medals() == 2, medals())
                before = owned()
                results = pull(1).get('s8r5M6wI', [])
                check('refusal: 2 medals plays nothing and grants nothing', not results and medals() == 2
                      and owned() == before)
                # E. a failure part-way rolls the whole action back
                medals(30)
                execute("CREATE TRIGGER qc_slot_fail AFTER INSERT ON user_units"
                        " WHEN (SELECT COUNT(*) FROM user_units WHERE user_id=NEW.user_id AND unit_id=NEW.unit_id) > "
                        f"{after_u[prize['target_id']] + 3}"
                        " BEGIN SELECT RAISE(ABORT,'QC slot'); END")
                before = owned()
                reply = pull(10)
                check('rollback: a failed pull keeps every medal and grants nothing',
                      'error' in reply and medals() == 30 and owned() == before, (reply.get('error'), medals()))
                execute('DROP TRIGGER qc_slot_fail')
            return owned(), medals()
        state = run(fx, key, single)

    # ---- F. restart ---------------------------------------------------------------
    def restarted(c, db):
        execute, medals, owned, pull = helpers(c, db)
        check('restart: medals and the last prizes persisted', (owned(), medals()) == state, medals())
    run(fx, 'restart', restarted)
    return check.summary('brave slots')


if __name__ == '__main__':
    sys.exit(main())
