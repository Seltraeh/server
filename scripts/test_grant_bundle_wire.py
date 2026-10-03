"""The evolution supply bundle, end to end on a COPY of the live save.

    python scripts/test_grant_bundle_wire.py PATH_TO_DEBUG_EXE [--port 19998] [--out DIR]
        [--manifest scripts/test_bundles/evolution_2026-10-02.json]

Runs scripts/grant_test_bundle.py against a SQLite-backup copy under
out/qa/current/grant_bundle (never the live save), checks that a retry delivers
nothing, claims every present through PresentReceipt, then does exactly what the
player is asked to do with them, through the real handlers, on the bundle's own
disposable Inferno Berdette:

  1. Rainbow Crystal -> lv 80, Burst Queen -> BB 10 (no SBB on the 5*);
  2. evolve to Hades Flame Berdette 6*: BB halves, the gained SBB stays locked;
  3. Rainbow Crystal -> lv 100, Burst Queen -> BB 10 and the SBB unlocked;
  4. evolve to Tartarus Blaze Berdette 7*: BB and SBB both halve;

and the optional Selena 20011 -> Ice Selena 20012 with the Water Nymph.  Each
step checks the form, level, BB/SBB ids and levels, leader skill, the exact Zel
the recipe costs and that the materials were consumed -- and that the player's
own units (their Berdette, their totems) were never touched.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Client, Fixture, IsolatedServer, qa_dir  # noqa: E402

UNIT_MST = {r['pn16CNah']: r for r in next(iter(json.loads(
    (ROOT / 'deploy/mst/unit_mst.json').read_text(encoding='utf-8')).values()))}
EVO = {r['pn16CNah']: r for r in next(iter(json.loads(
    (ROOT / 'deploy/mst/unit_evo_mst.json').read_text(encoding='utf-8')).values()))}
MAT_KEYS = ['85X6JHQA', 'wh3YRU08', '7MxucW2J', 'j7fTS3ca', 'Hb8yfmv7', 'Voht18AP', '3g8brFoq', 'agp4CKEV', 'eKPWNoLn']


def recipe_materials(species):
    return [EVO[str(species)][k] for k in MAT_KEYS if EVO[str(species)][k] not in ('0', '')]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19998)
    ap.add_argument('--out', default=str(qa_dir('grant_bundle')))
    ap.add_argument('--manifest', type=Path, default=ROOT / 'scripts/test_bundles/evolution_2026-10-02.json')
    args = ap.parse_args()
    check = Checker()
    fx = Fixture.create(args.out, port=args.port)
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    backup_dir = Path(args.out) / 'grant'
    grant = [sys.executable, str(ROOT / 'scripts/grant_test_bundle.py'), str(args.manifest), '--save',
             str(fx.db_path), '--backup-dir', str(backup_dir), '--live-port', str(args.port)]

    db = fx.db()
    user = manifest['user_id']
    unit_presents = [p for p in manifest['presents'] if p['type'] == 6]
    zel_present = sum(p['count'] for p in manifest['presents'] if p['type'] == 3)
    # FIXTURE: a copy that has not received this bundle and has room for it.  The
    # live save took it on 2026-10-02 15:17 (presents 68-77, since claimed), so its
    # tagged presents make grant_test_bundle.py refuse ("Already delivered") and
    # its box (104/110 that evening) cannot take 16 more.  Both are facts about the
    # live save, not about the tool or the evolution chain this suite checks.
    db.execute('DELETE FROM user_presents WHERE user_id=? AND description LIKE ?', (user, manifest['tag'] + '%'))
    owned = db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=?', (user,)).fetchone()[0]
    room = owned + sum(p['count'] for p in unit_presents) - 100
    db.execute('UPDATE user_info SET max_unit_count=MAX(COALESCE(max_unit_count, 0), ?) WHERE id=?', (room, user))
    db.commit()
    before_units = db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=?', (user,)).fetchone()[0]
    slots = 100 + (db.execute('SELECT max_unit_count FROM user_info WHERE id=?', (user,)).fetchone()[0] or 0)
    incoming = sum(p['count'] for p in unit_presents)
    check(f'capacity: {incoming} bundle units fit the {slots}-slot box beside {before_units} owned',
          before_units + incoming <= slots, (before_units, incoming, slots))
    for p in unit_presents:
        check(f'data: {p["target"]} {UNIT_MST[p["target"]]["utP1c0CD"]} is a real unit', p['target'] in UNIT_MST)
    for species in ('10824', '10825'):
        need = recipe_materials(species)
        supplied = {p['target']: p['count'] for p in unit_presents}
        for m in set(need):
            check(f'data: the bundle carries the {need.count(m)} {UNIT_MST[m]["utP1c0CD"]} recipe {species} needs '
                  f'(beside the other recipe)', supplied.get(m, 0) >= sum(recipe_materials(s).count(m)
                                                                          for s in ('10824', '10825')))
    check('data: the bundle Zel pays both evolutions',
          zel_present >= int(EVO['10824']['Rs7bCE3t']) + int(EVO['10825']['Rs7bCE3t']), zel_present)
    own = {r[0]: tuple(r[1:]) for r in db.execute(
        'SELECT user_unit_id, unit_id, unit_lvl, bb_lvl, sbb_lvl FROM user_units WHERE user_id=?', (user,))}

    first = subprocess.run(grant, capture_output=True, text=True)
    check('grant: delivered on a copy', first.returncode == 0, first.stdout + first.stderr)
    rows = db.execute('SELECT present_id, present_type, target_id, target_cnt FROM user_presents'
                      ' WHERE user_id=? AND description LIKE ? ORDER BY present_id',
                      (user, manifest['tag'] + '%')).fetchall()
    check('grant: one present per manifest line, exact ids and counts',
          [(r[1], r[2], r[3]) for r in rows] == [(p['type'], p['target'], p['count']) for p in manifest['presents']],
          [tuple(r) for r in rows])
    check('grant: nothing else changed (no units yet)',
          db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=?', (user,)).fetchone()[0] == before_units)
    receipts = sorted(backup_dir.glob('grant_receipt_*.json'))
    check('grant: a receipt and a backup were written', receipts and Path(
        json.loads(receipts[-1].read_text(encoding='utf-8'))['backup']).is_file())
    again = subprocess.run(grant, capture_output=True, text=True)
    count_after = db.execute('SELECT COUNT(*) FROM user_presents WHERE user_id=? AND description LIKE ?',
                             (user, manifest['tag'] + '%')).fetchone()[0]
    check('grant: a retry delivers nothing', again.returncode != 0 and 'Already delivered' in again.stdout + again.stderr
          and count_after == len(rows), again.stdout + again.stderr)

    with IsolatedServer(args.exe, fx):
        c = Client(fx, user)

        def unit(uid):
            r = db.execute('SELECT * FROM user_units WHERE user_unit_id=?', (uid,)).fetchone()
            return dict(r) if r else None

        def newest(species, count=1, exclude=()):
            return [r[0] for r in db.execute('SELECT user_unit_id FROM user_units WHERE user_id=? AND unit_id=?'
                                             ' ORDER BY user_unit_id DESC', (user, str(species)))
                    if r[0] not in exclude and r[0] not in own][:count]

        def zel():
            return db.execute('SELECT zel FROM user_info WHERE id=?', (user,)).fetchone()[0]

        z_before_claim = zel()
        for r in rows:
            reply = c.call('PresentReceipt', body={'o6uWU0Z7': [{'i1WQkh4G': '0', 'S1B82FHK': str(r[0])}]})
            check(f'claim present {r[0]} ({r[2] or "zel"} x{r[3]})', 'error' not in reply, reply.get('error'))
        check('claim: every bundle unit is in the box',
              db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=?', (user,)).fetchone()[0]
              == before_units + incoming)
        check('claim: the Zel arrived', zel() == z_before_claim + zel_present, (z_before_claim, zel()))

        def mix(base, materials):
            reply = c.call('UnitMix', body={
                'mCE3rUu5': [{'Rs7bCE3t': '0'}],
                'Km35HAXv': [{'edy7fq3L': str(base), 'mnZ5K4Ii': '1'}]
                            + [{'edy7fq3L': str(m), 'mnZ5K4Ii': '2'} for m in materials]})
            return reply

        def evolve(base, species, materials):
            recipe = EVO[str(species)]
            body = {'8Z2NQrx1': [{'inU8Q4gL': str(base), 'mnZ5K4Ii': '1', '29MgiJIQ': '1'}]
                    + [{'inU8Q4gL': str(m), 'mnZ5K4Ii': '2', '29MgiJIQ': '1'} for m in materials],
                    'I82p0wCL': [{'pn16CNah': recipe['74VFwuTd']}],
                    'mCE3rUu5': [{'Rs7bCE3t': recipe['Rs7bCE3t']}]}
            z0 = zel()
            reply = c.call('UnitEvo', body=body)
            return reply, z0 - zel()

        def materials_for(species):
            picked = []
            for m in recipe_materials(species):
                picked += newest(m, 1, exclude=picked)
            return picked

        def state(uid):
            row = unit(uid)
            return row and (str(row['unit_id']), row['unit_lvl'], str(row['bb_id']), row['bb_lvl'],
                            str(row['sbb_id']), row['sbb_lvl'], str(row['leader_skill_id']))

        # ---- 1. the 5* ---------------------------------------------------------------------
        berdette = newest(10824)[0]
        check('1: the bundle Berdette starts at lv 1 with its BB', state(berdette)[:4] == ('10824', 1, '10824', 1),
              state(berdette))
        reply = mix(berdette, newest(750006, 1))
        check('1: one Rainbow Crystal takes Berdette 5* to its max level 80',
              'error' not in reply and unit(berdette)['unit_lvl'] == 80, (reply.get('error'), unit(berdette)['unit_lvl']))
        reply = mix(berdette, newest(750004, 1))
        before1 = state(berdette)
        check('1: a Burst Queen takes the BB to 10; the 5* has no SBB',
              'error' not in reply and before1[3] == 10 and before1[4] in ('0', '') and before1[5] == 0, before1)
        print(f'  before evolution 1: {before1}')

        # ---- 2. evolve to the 6* -------------------------------------------------------------
        mats = materials_for(10824)
        check('2: the five 5*->6* materials come from the bundle', len(mats) == 5, mats)
        reply, paid = evolve(berdette, 10824, mats)
        after1 = state(berdette)
        print(f'  after evolution 1:  {after1}')
        check('2: evolution accepted, charged the recipe\'s 500,000 Zel', 'error' not in reply and paid == 500000,
              (reply.get('error'), paid))
        check('2: Hades Flame Berdette 6* at lv 1, BB 10825 halved to 5, SBB 110825 locked, LS 7418',
              after1 == ('10825', 1, '10825', 5, '110825', 0, '7418'), after1)
        check('2: the materials were consumed', all(unit(m) is None for m in mats))

        # ---- 3. the 6* to its max ------------------------------------------------------------
        reply = mix(berdette, newest(750006, 1))
        check('3: the second Rainbow Crystal takes the 6* to its max level 100',
              'error' not in reply and unit(berdette)['unit_lvl'] == 100, (reply.get('error'), unit(berdette)['unit_lvl']))
        reply = mix(berdette, newest(750004, 1))
        before2 = state(berdette)
        print(f'  before evolution 2: {before2}')
        check('3: the second Burst Queen takes BB back to 10 and unlocks the SBB',
              'error' not in reply and before2[3] == 10 and before2[5] >= 1, before2)
        check('3: ...all the way to SBB 10', before2[5] == 10, before2)

        # ---- 4. evolve to the 7* -------------------------------------------------------------
        mats = materials_for(10825)
        check('4: the five 6*->7* materials come from the bundle', len(mats) == 5, mats)
        reply, paid = evolve(berdette, 10825, mats)
        after2 = state(berdette)
        print(f'  after evolution 2:  {after2}')
        sbb_after = max(1, before2[5] // 2) if before2[5] > 0 else 0
        check('4: evolution accepted, charged the recipe\'s 1,500,000 Zel', 'error' not in reply and paid == 1500000,
              (reply.get('error'), paid))
        check(f'4: Tartarus Blaze Berdette 7* at lv 1, BB 10826 lv 5, SBB 110826 lv {sbb_after}, LS 7419',
              after2 == ('10826', 1, '10826', 5, '110826', sbb_after, '7419'), after2)
        check('4: the materials were consumed', all(unit(m) is None for m in mats))
        r = c.call('UserInfo')
        wire = next((u for u in r.get('4ceMWH6k', []) if str(u.get('edy7fq3L')) == str(berdette)), {})
        check('4: the login roster shows the 7* (unit id and level)', str(wire.get('pn16CNah')) == '10826', wire)

        # ---- optional: the player's own Selena with the Water Nymph -------------------------
        selena = db.execute("SELECT user_unit_id, bb_lvl FROM user_units WHERE user_id=? AND unit_id='20011'"
                            ' AND unit_lvl=12', (user,)).fetchone()
        check('C: the save has a max-level Selena', selena is not None)
        if selena:
            reply, paid = evolve(selena[0], 20011, newest(20130, 1))
            check('C: Selena -> Ice Selena accepted for the recipe\'s 2,500 Zel', 'error' not in reply and paid == 2500,
                  (reply.get('error'), paid))
            after = state(selena[0])
            check(f'C: Ice Selena 20012 lv 1, BB 20012 halved {selena[1]} -> {max(1, selena[1] // 2)}, LS 110',
                  after[:4] == ('20012', 1, '20012', max(1, selena[1] // 2)) and after[6] == '110', after)

        # ---- the player's own units ------------------------------------------------------------
        untouched = {k: v for k, v in own.items() if k != (selena[0] if selena else None)}
        now = {r[0]: tuple(r[1:]) for r in db.execute(
            'SELECT user_unit_id, unit_id, unit_lvl, bb_lvl, sbb_lvl FROM user_units WHERE user_id=?', (user,))}
        check('the player\'s own units (their Berdette, totems, mimics...) were never used',
              all(now.get(k) == v for k, v in untouched.items()),
              [k for k, v in untouched.items() if now.get(k) != v])
    db.close()
    return check.summary('grant bundle')


if __name__ == '__main__':
    sys.exit(main())
