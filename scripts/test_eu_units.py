"""Tests for scripts/eu_units.py: install the EU units into a disposable mirror of deploy and check the result.

python scripts/test_eu_units.py [PATH_TO_DEBUG_EXE] [--port 19973]

Offline (deploy/ is only read, never written): the installer runs into a hard-link mirror, then
  - every EU unit's leader/extra/BB/SBB/UBB id has a row; every evolution's materials exist;
  - every unit has its sprite, part file, three cgs files (as the CGS table names them), thumbnail and full art,
    and no cgs addresses a frame its part file lacks;
  - each staged table version decodes to exactly the server's merged rows, above the versions deploy stages;
  - F_RESOURCE_MST names the reissued dictionaries and they hold every new key;
  - a second install writes nothing, `check` passes, and deploy's files are byte-identical before and after.
Wire (with an exe): an isolated server on the mirror boots, declares the new versions, serves a part, and a
player who owns an EU unit gets it back from UnitList-style replies without an error.
"""
import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import eu_units as eu  # noqa: E402
import unit_mst_corrections as umc  # noqa: E402
import sgtext_corrections as sgt  # noqa: E402
from bf_testkit import ROOT, Checker, qa_dir  # noqa: E402

DEPLOY = ROOT / 'deploy'
WATCH = [DEPLOY / 'mst' / v[0] for v in eu.TABLES.values()] + [DEPLOY / 'archive' / 'unit.json']


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def rows(path: Path):
    return next(iter(json.loads(path.read_text(encoding='utf-8-sig')).values()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe', nargs='?')
    ap.add_argument('--port', type=int, default=19973)
    args = ap.parse_args()
    check = Checker()
    before = {p: sha(p) for p in WATCH}
    root = qa_dir('eu_units_root')
    if root.exists():
        shutil.rmtree(root)
    for sub in ('mst', 'archive', 'game_content'):
        umc.mirror(DEPLOY / sub, root / sub)
    eu.install(root, None)
    second = eu.install(root, None)
    check('a second install writes nothing', second == [], second[:3])
    check('`check` reports the install complete', eu.install(root, None, check_only=True) == [])
    check('deploy files are byte-identical before and after (the mirror is never written through)',
          {p: sha(p) for p in WATCH} == before)

    units = eu.load('unit_mst.json')
    ids = {u['pn16CNah'] for u in units}
    content = root / 'game_content' / 'content'
    m = {name: rows(root / 'mst' / name) for name, *_ in eu.TABLES.values()}
    key = {name: v[3] for v in eu.TABLES.values() for name in [v[0]]}
    have = {name: {r[key[name]] for r in rs} for name, rs in m.items()}
    check(f'all {len(ids)} EU units are in the server unit table', ids <= have['unit_mst.json'])
    refs = {'oS3kTZ2W': 'leader_skill_mst.json', 'cP83zNsv': 'extra_passive_skill_mst.json',
            'nj9Lw7mV': 'skill_mst.json', 'iEFZ6H19': 'skill_mst.json', 'XuJL4pc5': None}
    dangling = []
    for u in units:
        for col, table in (('oS3kTZ2W', 'leader_skill_mst.json'), ('cP83zNsv', 'extra_passive_skill_mst.json'),
                           ('nj9Lw7mV', 'skill_mst.json'), ('iEFZ6H19', 'skill_mst.json'),
                           ('Lkh6gYkT' if False else 'nj9Lw7mV', 'skill_level_mst.json')):
            v = u.get(col, '0')
            if v not in ('0', '') and v not in have[table]:
                dangling.append((u['pn16CNah'], col, v))
    check('every leader/extra/BB/SBB id an EU unit names has a row (and each BB its level row)', not dangling,
          dangling[:5])
    del refs
    evo = [r for r in m['unit_evo_mst.json'] if r['pn16CNah'] in ids or r['74VFwuTd'] in ids]
    keys = list(m['unit_evo_mst.json'][0])
    bad_mat = []
    for r in evo:
        for i in range(10):
            t, mid = r[keys[4 + i]], r[keys[14 + i]]
            if mid == '0':
                continue
            pool = have['unit_mst.json'] if t == '1' else have['item_mst.json']
            if mid not in pool:
                bad_mat.append((r['pn16CNah'], mid))
    check(f'{len(evo)} evolutions: every material is a known unit or item', evo and not bad_mat, bad_mat[:5])

    cgs_rows = {r['pn16CNah']: r['Kn51uR4Y'] for r in m['unit_cgs_mst.json']}
    missing, frames_bad = [], []
    for uid in ids:
        names = [f'unit_anime_{uid}.png', f'unit_ills_thum_{uid}.png', f'unit_ills_full_{uid}.png',
                 f'unit_ills_battle_{uid}.png']
        missing += [n for n in names if not (content / 'unit' / 'img' / n).is_file()]
        cgg = content / 'unit' / 'cgg' / f'unit_cgg_{uid}.csv'
        if not cgg.is_file():
            missing.append(cgg.name)
            continue
        nframes = sum(1 for line in cgg.read_text().splitlines() if line.strip())
        for part in cgs_rows.get(uid, '').split('|'):
            f = content / 'unit' / 'cgs' / part.split(':', 1)[-1]
            if not f.is_file():
                missing.append(f.name)
            elif eu.cgs_frames(f) > nframes:
                frames_bad.append((uid, f.name, eu.cgs_frames(f), nframes))
    check('every EU unit has sprite, thumbnail, full art, part file and the cgs files its CGS row names',
          not missing, missing[:6])
    check('no cgs file addresses a frame its part file lacks', not frames_bad, frames_bad[:4])
    authored = [u for u in ids if int(u) not in eu.IDENTICAL_SHEETS]
    check(f'{len(authored)} authored part files and {len(ids) - len(authored)} borrowed from an identical Global sheet',
          len(authored) + len(eu.IDENTICAL_SHEETS) == len(ids))

    staged_ok, deploy_v = [], {}
    for table, (name, fk, ek, idk, version) in eu.TABLES.items():
        spec = umc.Spec(table, name, '', fk, ek, idk, ())
        man = json.loads((content / table / 'manifest.json').read_text(encoding='utf-8'))
        dman = DEPLOY / 'game_content' / 'content' / table / 'manifest.json'
        deploy_v[table] = json.loads(dman.read_text(encoding='utf-8'))['version'] if dman.is_file() else 0
        parts = sorted((content / table).glob(f'Ver{man["version"]}_*.dat'))
        decoded = [r for p in parts for r in umc.decode_part(p.read_bytes(), spec)]
        # deploy carries these versions itself once the units are installed there (2026-11-01), and later releases
        # restage some tables above them (F_UNIT_MST 1090, #14, November 2): at or above the EU version, same rows
        staged_ok.append((table, man['version'] >= version and man['version'] >= deploy_v[table], decoded == m[name],
                          man['record_num'] == len(m[name])))
    check('every staged table is the server table, at the EU version or a later release''s', all(all(x[1:]) for x in staged_ok),
          [x for x in staged_ok if not all(x[1:])])

    res = json.loads((content / 'F_RESOURCE_MST' / 'manifest.json').read_text(encoding='utf-8'))
    spec = umc.Spec('F_RESOURCE_MST', '', '', sgt.RESOURCE_FILE_KEY, sgt.RESOURCE_ENC_KEY, '', ())
    rrows = [r for p in sorted((content / 'F_RESOURCE_MST').glob(f'Ver{res["version"]}_*.dat'))
             for r in umc.decode_part(p.read_bytes(), spec)]
    paths = {r[sgt.RESOURCE_PATH] for r in rrows}
    text = eu.load('text.json')
    lacking = []
    for family, entries in text.items():
        base = eu.DICTIONARIES[family]
        listed = [p for p in paths if p.lstrip('/').startswith(f'sgtext/{base}_')]
        if len(listed) != 1:
            lacking.append((base, 'listed', len(listed)))
            continue
        _, csv, _ = sgt.decode_zip((content / listed[0].lstrip('/')).read_bytes())
        have_keys = sgt.rows_of_csv(csv)
        lacking += [(base, k) for k in entries if k.encode('utf-8') not in have_keys][:2]
    check(f'F_RESOURCE_MST {res["version"]} lists one dictionary per family and each holds every new key',
          res['version'] >= eu.RESOURCE_VERSION and not lacking and len(rrows) == res['record_num'], lacking[:4])

    if args.exe:
        wire(args, root, check, ids)
    return check.summary('eu units')


def wire(args, root, check, ids):
    from bf_testkit import Fixture, IsolatedServer, Client, KEYS
    KEYS.setdefault('Initialize', ('MfZyu1q9', 'EmcshnQoDr20TZz1'))   # GmeControllerHandlers.cpp REGISTER
    fx = Fixture.create(qa_dir('eu_units_wire'), port=args.port, archive_root=root / 'archive',
                        mst_root=root / 'mst', document_root=root / 'game_content')
    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx)
        db = fx.db()
        uid = int(sorted(ids)[-1])
        template = dict(db.execute('SELECT * FROM user_units WHERE user_id=? LIMIT 1', (c.user,)).fetchone())
        template.pop('user_unit_id')
        template.update(unit_id=uid, unit_lvl=1, total_exp=0)
        db.execute(f"INSERT INTO user_units ({','.join(template)}) VALUES ({','.join('?' for _ in template)})",
                   tuple(template.values()))
        db.commit()
        db.close()
        init = c.call('Initialize')
        check('Initialize answers on the installed tables', 'error' not in init, init.get('error'))
        declared = {v.get('moWQ30GH'): v.get('d2RFtP8T') for v in init.get('KeC10fuL', []) if isinstance(v, dict)}
        want = {t: str(v[4]) for t, v in eu.TABLES.items()} | {'F_RESOURCE_MST': str(eu.RESOURCE_VERSION)}
        staged = {t: json.loads((root / 'game_content' / 'content' / t / 'manifest.json').read_text(encoding='utf-8'))['version']
                  for t in want}
        check('Initialize declares every staged version (the EU one or a later release above it)',
              all(str(declared.get(t)) == str(staged[t]) and staged[t] >= int(v) for t, v in want.items()),
              {t: declared.get(t) for t in want})
        info = c.call('UserInfo')
        units = [u for k, v in info.items() if isinstance(v, list) for u in v if isinstance(u, dict)
                 and str(u.get('pn16CNah', u.get('8hRMaWmC', ''))) == str(uid)]
        check(f'UserInfo answers for a player who owns EU unit {uid}, and lists it', 'error' not in info and units,
              info.get('error'))
        evolve(c, fx, root, check)
        log = server.log_text()
        check('the server log has no unit/skill table error', not any(w in log for w in
                                                                      ('could not be read', 'unit archive error')))


MATERIALS = ['85X6JHQA', 'wh3YRU08', '7MxucW2J', 'j7fTS3ca', 'Hb8yfmv7', 'Voht18AP', '3g8brFoq', 'agp4CKEV', 'eKPWNoLn']
KINDS = ['Xyt6rhx2', '0tna4Idu', '6GwnugW3', 'hdF8ND2H', 'nB7pFdR0', 'IZUvR489', 'bNRUuatB', '2BFgYLjg', '18Oz7z8k']


def evolve(c, fx, root, check):
    """UnitEvo on EU recipes (the client-side flow is not run): the first, middle and last of the 135, each base
    a max-level EU unit carrying its OWN skills (a cloned row would carry another unit's -- round 8's trap)."""
    units = {int(r['pn16CNah']): r for r in rows(root / 'mst' / 'unit_mst.json')}
    recipes = json.loads((HERE / 'data' / 'eu_units' / 'unit_evo_mst.json').read_text(encoding='utf-8'))
    db = fx.db()
    template = dict(db.execute('SELECT * FROM user_units WHERE user_id=? LIMIT 1', (c.user,)).fetchone())
    template.pop('user_unit_id')

    def unit(mid, **kw):
        m = units[mid]
        row = dict(template, unit_id=mid, unit_lvl=int(m['EI1DF8Yt']), favorite_flg=0, bb_id=m['nj9Lw7mV'],
                   sbb_id=m['iEFZ6H19'], leader_skill_id=int(m['oS3kTZ2W'] or 0), bb_lvl=9, sbb_lvl=0,
                   eqip_item_id=0, eqip_item_id2=0, **kw)
        return db.execute(f"INSERT INTO user_units ({','.join(row)}) VALUES ({','.join('?' for _ in row)})",
                          tuple(row.values())).lastrowid

    for recipe in (recipes[0], recipes[len(recipes) // 2], recipes[-1]):
        src, dst = int(recipe['pn16CNah']), int(recipe['74VFwuTd'])
        base, mats, nodes = unit(src), [], []
        for k, kind in zip(MATERIALS, KINDS):
            material = int(recipe[k])
            if not material:
                continue
            if recipe[kind] == '2':
                db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,1) ON CONFLICT(user_id,item_id) '
                           'DO UPDATE SET item_num=item_num+1', (c.user, material))
                nodes.append({'inU8Q4gL': str(material), 'mnZ5K4Ii': '2', '29MgiJIQ': '2'})
            else:
                mats.append(unit(material))
                nodes.append({'inU8Q4gL': str(mats[-1]), 'mnZ5K4Ii': '2', '29MgiJIQ': '1'})
        db.execute('UPDATE user_info SET zel=99999999, karma=99999999 WHERE id=?', (c.user,))
        db.commit()
        reply = c.call('UnitEvo', body={'8Z2NQrx1': [{'inU8Q4gL': str(base), 'mnZ5K4Ii': '1', '29MgiJIQ': '1'}] + nodes,
                                        'I82p0wCL': [{'pn16CNah': str(dst)}],
                                        'mCE3rUu5': [{'Rs7bCE3t': recipe['Rs7bCE3t']}]})
        row = dict(db.execute('SELECT * FROM user_units WHERE user_unit_id=?', (base,)).fetchone())
        t = units[dst]
        check(f'EU evolution {src} -> {dst}: succeeds and the row becomes {dst}',
              'error' not in reply and int(row['unit_id']) == dst, (reply.get('error'), row['unit_id']))
        check(f'EU evolution {src} -> {dst}: the new form\'s BB / SBB / leader skill are stored',
              (int(row['bb_id'] or 0), int(row['sbb_id'] or 0), int(row['leader_skill_id']))
              == (int(t['nj9Lw7mV'] or 0), int(t['iEFZ6H19'] or 0), int(t['oS3kTZ2W'] or 0)), row)
        check(f'EU evolution {src} -> {dst}: level 1, BB 9 halves to 4, materials consumed',
              row['unit_lvl'] == 1 and row['bb_lvl'] == (4 if int(t['nj9Lw7mV'] or 0) else 0)
              and not any(db.execute('SELECT 1 FROM user_units WHERE user_unit_id=?', (m,)).fetchone() for m in mats),
              (row['unit_lvl'], row['bb_lvl']))
    db.close()


if __name__ == '__main__':
    sys.exit(main())
