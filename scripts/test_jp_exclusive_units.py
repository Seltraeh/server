"""Tests for scripts/jp_exclusive_units.py (#15 c): the 107 JP-exclusive units, installed into a mirror of deploy.

    python scripts/test_jp_exclusive_units.py [PATH_TO_DEBUG_EXE] [--port 19972]

Offline (deploy/ is only read): the installer runs into a hard-link mirror (game_content, mst, archive), then
  * every unit has a unit_mst row, an archive record, a CGS and a comment row, and every BB/SBB/UBB, leader and
    extra skill id it names resolves; every evolution target and unit material is in the game (four end in the
    Global counterpart, e.g. 11175 -> Fallen Morgana 710168);
  * each staged table is above deploy's version and decodes to the server table; F_RESOURCE_MST names one
    dictionary per family, each holding every name and description line, none of them Japanese;
  * the art: thumbnail, full art, battle portrait, sprite sheet, part file and three cgs files per unit, no cgs step
    past the part file; 60 animated stand-ins (wiki GIFs) and 47 static; the roster's 99 counterparts stay out;
  * a second install writes nothing, check passes, deploy's files are byte-identical, and the playability audit on
    the mirror counts all 107 PLAYABLE.
Wire (with an exe): the mirror's server boots, and a present of Harley Quinn 11105 arrives with its own skills.
"""
import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import audit_unit_playability as audit  # noqa: E402
import eu_units as eu  # noqa: E402
import jp_exclusive_units as jx  # noqa: E402
import sgtext_corrections as sgt  # noqa: E402
import unit_mst_corrections as umc  # noqa: E402
from bf_testkit import ROOT, PRE_JP_EXCLUSIVE, Fixture, IsolatedServer, Client, Checker, qa_dir  # noqa: E402

DEPLOY = ROOT / 'deploy'
PRESENT_UNIT = 11105
CJK = re.compile(r'[　-ヿ㐀-鿿＀-￯]')
WATCH = [DEPLOY / 'mst' / v[0] for v in jx.TABLES.values()] + [
    DEPLOY / 'archive' / 'unit.json', DEPLOY / 'game_content' / 'content' / 'F_RESOURCE_MST' / 'manifest.json']


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def rows(path: Path):
    return next(iter(json.loads(path.read_text(encoding='utf-8-sig')).values()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe', nargs='?')
    ap.add_argument('--port', type=int, default=19972)
    args = ap.parse_args()
    check = Checker()
    if not (jx.ART / 'Unit_ills_full_11105.png').is_file():
        print(f'SKIP: the wiki art is not at {jx.ART} (tools/jp_art/raw is git-ignored; see tools/jp_art/wiki_files.json)')
        return 0
    before = {p: sha(p) for p in WATCH}
    # Installed into deploy on November 2 (Evan: "Install them"): the mirror then already carries it, the install
    # into it writes nothing, and the text's "before" is deploy's previous F_RESOURCE_MST (its parts stay beside).
    applied = not jx.install(DEPLOY, None, check_only=True)[1]
    if applied:
        print('deploy already carries the install')
    root = qa_dir('jp_exclusive_units_root')
    if root.exists():
        shutil.rmtree(root)
    for sub in ('mst', 'archive', 'game_content'):
        umc.mirror(DEPLOY / sub, root / sub)
    log, problems = jx.install(root, None)
    check('install reports no problem' + (' and writes nothing (deploy carries it)' if applied else ''),
          not problems and (not log if applied else bool(log)), (problems[:3], len(log)))
    again, _ = jx.install(root, None)
    check('a second install writes nothing', again == [], again[:3])
    check('`check` reports the install complete', not jx.install(root, None, check_only=True)[1])
    check('deploy files are byte-identical before and after (the mirror is never written through)',
          {p: sha(p) for p in WATCH} == before)

    units = jx.load('unit_mst.json')
    ids = {u['pn16CNah'] for u in units}
    roster = json.loads((HERE / 'data' / 'jp_units' / 'jp_only_roster.json').read_text(encoding='utf-8'))
    check(f'{len(ids)} units = the roster\'s exclusive list; none of its {len(roster["counterparts"])} counterparts',
          ids == set(roster['exclusive']) and not ids & set(roster['counterparts']), len(ids))
    m = {name: rows(root / 'mst' / name) for name, *_ in jx.TABLES.values()}
    have = {name: {r[v[3]] for r in m[name]} for v in jx.TABLES.values() for name in [v[0]]}
    archive = {u['id'] for u in json.loads((root / 'archive' / 'unit.json').read_text(encoding='utf-8'))}
    check('every unit has a unit_mst row, an archive record, a CGS row and a comment row',
          ids <= have['unit_mst.json'] and {int(i) for i in ids} <= archive and ids <= have['unit_cgs_mst.json']
          and ids <= have['unit_comment_mst.json'])
    dangling = [(u['pn16CNah'], col, u[col]) for u in units
                for col, t in (('oS3kTZ2W', 'leader_skill_mst.json'), ('cP83zNsv', 'extra_passive_skill_mst.json'),
                               ('nj9Lw7mV', 'skill_mst.json'), ('iEFZ6H19', 'skill_mst.json'),
                               ('cb0P4mp1', 'skill_mst.json'), ('nj9Lw7mV', 'skill_level_mst.json'))
                if u.get(col, '0') not in ('0', '') and u[col] not in have[t]]
    check('every leader/extra/BB/SBB/UBB id a unit names has a row (and each BB its level row)', not dangling,
          dangling[:5])
    evo = [r for r in m['unit_evo_mst.json'] if r['pn16CNah'] in ids]
    keys = list(m['unit_evo_mst.json'][0])
    bad = [(r['pn16CNah'], r[keys[1]]) for r in evo if r[keys[1]] not in have['unit_mst.json']]
    bad += [(r['pn16CNah'], r[keys[14 + i]]) for r in evo for i in range(10)
            if r[keys[4 + i]] == '1' and r[keys[14 + i]] != '0' and r[keys[14 + i]] not in have['unit_mst.json']]
    routed = {r['pn16CNah']: r[keys[1]] for r in evo if r[keys[1]] in {'710168', '840607', '750166', '860607'}}
    check(f'{len(evo)} evolutions: every target and unit material is in the game; four end in the Global counterpart',
          not bad and routed == {'11175': '710168', '41055': '840607', '51355': '750166', '61294': '860607'},
          (bad[:4], routed))

    content = root / 'game_content' / 'content'
    staged = []
    for table, (name, fk, ek, idk, version) in jx.TABLES.items():
        spec = umc.Spec(table, name, '', fk, ek, idk, ())
        man = json.loads((content / table / 'manifest.json').read_text(encoding='utf-8'))
        dman = json.loads((DEPLOY / 'game_content' / 'content' / table / 'manifest.json').read_text(encoding='utf-8'))
        decoded = [r for p in sorted((content / table).glob(f'Ver{man["version"]}_*.dat'))
                   for r in umc.decode_part(p.read_bytes(), spec)]
        # at its version, never below deploy's: equal once installed, above while staged (F_UNIT_MST 1093 re-stages the
        # Home crops over the 1092 that went live while the other tables stay at deploy's)
        staged.append((table, man['version'] == version >= dman['version'], decoded == m[name]))
    check('every staged table is at its version (not below deploy\'s) and decodes to the server table',
          all(a and b for _, a, b in staged), [x for x in staged if not all(x[1:])])
    res = json.loads((content / 'F_RESOURCE_MST' / 'manifest.json').read_text(encoding='utf-8'))
    spec = umc.Spec('F_RESOURCE_MST', '', '', sgt.RESOURCE_FILE_KEY, sgt.RESOURCE_ENC_KEY, '', ())
    paths = [r[sgt.RESOURCE_PATH] for p in sorted((content / 'F_RESOURCE_MST').glob(f'Ver{res["version"]}_*.dat'))
             for r in umc.decode_part(p.read_bytes(), spec)]
    text, lacking = jx.load('text.json'), []
    res_manifest = (PRE_JP_EXCLUSIVE if applied else DEPLOY) / 'game_content' / 'content' / 'F_RESOURCE_MST' / 'manifest.json'
    deploy_res = json.loads(res_manifest.read_text(encoding='utf-8'))
    deploy_paths = [r[sgt.RESOURCE_PATH] for p in sorted((DEPLOY / 'game_content' / 'content' / 'F_RESOURCE_MST').glob(
        f'Ver{deploy_res["version"]}_*.dat')) for r in umc.decode_part(p.read_bytes(), spec)]
    for family, entries in text.items():
        before_zip = [p for p in deploy_paths if p.lstrip('/').startswith(f'sgtext/{eu.DICTIONARIES[family]}_')][0]
        _, before_csv, _ = sgt.decode_zip((DEPLOY / 'game_content' / 'content' / before_zip.lstrip('/')).read_bytes())
        had = sgt.rows_of_csv(before_csv)       # a key Global already names keeps Global's English (13 leader skills)
        listed = [p for p in paths if p.lstrip('/').startswith(f'sgtext/{eu.DICTIONARIES[family]}_')]
        if len(listed) != 1:
            lacking.append((family, 'listed', len(listed)))
            continue
        _, csv, _ = sgt.decode_zip((content / listed[0].lstrip('/')).read_bytes())
        have_keys = sgt.rows_of_csv(csv)
        lacking += [(family, k) for k, v in entries.items() if k.encode() not in have_keys
                    or (k.encode() not in had and have_keys[k.encode()].split(b'^')[0].decode() != v)][:2]
        lacking += [(family, k, 'changed') for k in had if have_keys.get(k) != had[k]][:2]
    japanese = [(f, k) for f, d in text.items() for k, v in d.items() if CJK.search(v)]
    check(f'F_RESOURCE_MST {res["version"]} names one dictionary per family: every new line as given, no old line changed',
          res['version'] == jx.RESOURCE_VERSION and not lacking, lacking[:4])
    check(f'{sum(len(d) for d in text.values())} text lines, none of them Japanese', not japanese, japanese[:3])

    img, cgg_dir, cgs_dir = content / 'unit' / 'img', content / 'unit' / 'cgg', content / 'unit' / 'cgs'
    missing, past, animated = [], [], 0
    for uid in sorted(ids, key=int):
        for p in (img / f'unit_ills_thum_{uid}.png', img / f'unit_ills_full_{uid}.png', img / f'unit_ills_battle_{uid}.png',
                  img / f'unit_anime_{uid}.png', cgg_dir / f'unit_cgg_{uid}.csv'):
            if not p.is_file():
                missing.append(p.name)
        cgg = cgg_dir / f'unit_cgg_{uid}.csv'
        n = len(cgg.read_text().splitlines()) if cgg.is_file() else 0
        animated += n > 1
        for kind in ('idle', 'move', 'atk'):
            f = cgs_dir / f'unit_{kind}_cgs_{uid}.csv'
            if not f.is_file():
                missing.append(f.name)
            elif eu.cgs_frames(f) > n:
                past.append((uid, kind))
    check('every unit has thumbnail, full art, battle portrait, sprite, part file and three cgs files', not missing,
          missing[:5])
    check('no cgs step names a frame its part file lacks', not past, past[:4])
    check(f'{animated} animated stand-ins from the wiki GIFs and {len(ids) - animated} static ones',
          (animated, len(ids) - animated) == (60, 47), (animated, len(ids) - animated))

    r = {x['id']: x for x in audit.audit(root)}
    bad = [(i, r[int(i)]['missing']) for i in ids if r[int(i)]['verdict'] != 'PLAYABLE']
    check(f'the audit on the mirror: all {len(ids)} PLAYABLE', not bad, bad[:4])

    if args.exe:
        fx = Fixture.create(qa_dir('jp_exclusive_units'), port=args.port, archive_root=root / 'archive',
                            mst_root=root / 'mst', document_root=root / 'game_content')
        with IsolatedServer(args.exe, fx):
            c = Client(fx)
            db = fx.db()
            pid = db.execute("INSERT INTO user_presents (user_id, present_type, target_id, target_cnt, target_param,"
                             " receipt_type, description, present_date, receipt_date, is_receipt)"
                             " VALUES (?, 6, ?, 1, '', 0, 'jp exclusive test', strftime('%s','now'), 0, 0)",
                             (c.user, str(PRESENT_UNIT))).lastrowid
            db.commit()
            held = {x[0] for x in db.execute('SELECT user_unit_id FROM user_units WHERE user_id=?', (c.user,))}
            reply = c.call('PresentReceipt', body={'o6uWU0Z7': [{'i1WQkh4G': '0', 'S1B82FHK': str(pid)}]})
            got = [u for u in reply.get('qC2tJs4E', []) if int(u['edy7fq3L']) not in held
                   and int(u['pn16CNah']) == PRESENT_UNIT]
            row = next(u for u in units if u['pn16CNah'] == str(PRESENT_UNIT))
            want = (row['nj9Lw7mV'], row['iEFZ6H19'], row['oS3kTZ2W'])
            check(f'a present of Harley Quinn {PRESENT_UNIT} arrives in the box with its own BB, SBB and leader skill',
                  'error' not in reply and len(got) == 1
                  and (got[0].get('nj9Lw7mV'), got[0].get('iEFZ6H19'), got[0].get('oS3kTZ2W')) == want,
                  (reply.get('error'), [(u.get('nj9Lw7mV'), u.get('iEFZ6H19'), u.get('oS3kTZ2W')) for u in got], want))
            db.close()
    return check.summary('jp exclusive units')


if __name__ == '__main__':
    sys.exit(main())
