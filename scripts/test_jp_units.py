"""Tests for scripts/jp_units.py (#15 b): the 29 JP-only units already in the tables, plus 8 missing names.

    python scripts/test_jp_units.py [PATH_TO_DEBUG_EXE] [--port 19975]

On a hard-link mirror of deploy (mst, archive, the text and resource tables and the unit art), never deploy itself:
  * the archive gains exactly the 29 records the proven column map builds, every other record byte-identical, and the
    builder reproduces every comparable existing Global record (the generator's own self-check);
  * each of the seven dictionaries is reissued once: the old CSV plus exactly the new keys (no key changed), the
    leader-skill descriptions skip #22-3's reserved 500; F_RESOURCE_MST 3279 lists the same rows with exactly those
    seven paths moved; every name and text line is present; check passes; a second install writes nothing;
  * the playability audit on the mirror counts the 29 and the 8 PLAYABLE, and deploy's files are unchanged.
Wire (with an exe): a server on the mirror's archive loads the 29 records, and a present of a JP unit (60936 Demon
Lord Dino) is received into the box with its own BB, SBB and leader skill.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import audit_unit_playability as audit  # noqa: E402
import eu_units as eu  # noqa: E402
import jp_units as jp  # noqa: E402
import sgtext_corrections as sgt  # noqa: E402
from bf_testkit import ROOT, PRE_RELEASE, Fixture, IsolatedServer, Client, Checker, qa_dir  # noqa: E402

DEPLOY = ROOT / 'deploy'
PRESENT_UNIT = 60936
WATCH = ('archive/unit.json', 'game_content/content/F_RESOURCE_MST/manifest.json', 'mst/unit_mst.json')


def mirror(dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    for sub in ('mst', 'archive', 'game_content/mst', 'game_content/content/sgtext',
                'game_content/content/F_RESOURCE_MST', 'game_content/content/unit', 'game_content/content/unit_sam',
                'game_content/eu_parts'):
        src = DEPLOY / sub
        for dirpath, _dirs, files in os.walk(src):
            out = dst / sub / Path(dirpath).relative_to(src)
            out.mkdir(parents=True, exist_ok=True)
            for f in files:
                os.link(Path(dirpath) / f, out / f)


def unapply(root: Path) -> bool:
    """When deploy already carries the install (released November 2), turn the mirror back into the root it was
    installed onto: the files jp_units.py replaced come back from the release backup (the mirror's links are
    replaced, never written through), and the dictionaries and F_RESOURCE_MST parts it created are removed."""
    backup = PRE_RELEASE / 'jp_units'
    archive = json.loads((root / 'archive/unit.json').read_text(encoding='utf-8'))
    if not backup.is_dir() or not any(r['id'] == PRESENT_UNIT for r in archive):
        return False
    v_now, paths_now = resource(root)
    for src in backup.rglob('*'):
        if src.is_file():
            dst = root / src.relative_to(backup)
            dst.unlink()
            shutil.copyfile(src, dst)
    v_then, paths_then = resource(root)
    # Every dictionary numbered above the one the restored list names did not exist then: this install's AND any
    # later one's (jp_exclusive_units.py moved _501 on to _502, so _501 is no longer listed but still on disk).
    sg = root / 'game_content/content/sgtext'
    for p in paths_then:
        m = re.fullmatch(r'/?sgtext/(.+)_(\d+)\.zip', p)
        if m:
            for z in sg.glob(f'{m.group(1)}_*.zip'):
                n = re.fullmatch(rf'{re.escape(m.group(1))}_(\d+)\.zip', z.name)
                if n and int(n.group(1)) > int(m.group(2)):
                    z.unlink()
    for part in (root / 'game_content/content/F_RESOURCE_MST').glob('Ver*_*.dat'):
        if int(re.match(r'Ver(\d+)_', part.name).group(1)) > v_then:
            part.unlink()
    del v_now, paths_now
    return True


def resource(root: Path) -> tuple[int, list[str]]:
    folder = root / 'game_content/content/F_RESOURCE_MST'
    v = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))['version']
    spec = jp.eu.umc.Spec('F_RESOURCE_MST', '', '', sgt.RESOURCE_FILE_KEY, sgt.RESOURCE_ENC_KEY, '', ())
    rows = [r for p in sorted(folder.glob(f'Ver{v}_*.dat')) for r in jp.eu.umc.decode_part(p.read_bytes(), spec)]
    return v, [r[sgt.RESOURCE_PATH] for r in rows]


def digest(root: Path) -> dict:
    return {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in WATCH}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe', nargs='?')
    ap.add_argument('--port', type=int, default=19975)
    args = ap.parse_args()
    check = Checker()
    deploy_before = digest(DEPLOY)
    root = qa_dir('jp_units_root')
    mirror(root)
    if unapply(root):
        print(f'deploy already carries the install: the mirror starts from {PRE_RELEASE / "jp_units"}')
    v0, paths0 = resource(root)
    old_archive = json.loads((root / 'archive/unit.json').read_text(encoding='utf-8'))
    log, problems = jp.install(root, None)
    v1, paths1 = resource(root)
    new_archive = json.loads((root / 'archive/unit.json').read_text(encoding='utf-8'))
    units = json.loads((jp.DATA / 'units.json').read_text(encoding='utf-8'))
    mst = {int(r['pn16CNah']): r for r in jp.table(root, 'unit_mst.json')}
    skills = {r['nj9Lw7mV'] for r in jp.table(root, 'skill_mst.json')}

    # ---- archive -------------------------------------------------------------------------------------
    added = new_archive[len(old_archive):]
    check('archive: the 29 records appended exactly as the column map builds them, the rest byte-identical',
          not problems and new_archive[:len(old_archive)] == old_archive
          and added == [jp.archive_record(u['id'], u['name'], mst[u['id']], skills) for u in units], problems)
    bad = []
    for rec in old_archive:
        if rec['id'] in mst and rec['id'] < 700000:
            b = jp.archive_record(rec['id'], rec['name'], mst[rec['id']], skills)
            if {**b, 'sbb_id': None} != {**rec, 'stats': rec['stats'][:1], 'sbb_id': None}:
                bad.append(rec['id'])
    check('the builder reproduces every comparable existing Global record (sbb_id aside, as the generator)', not bad,
          bad[:5])
    check('every new record has a resolving BB', all(r['bb_id'] in skills for r in added),
          [r['id'] for r in added if r['bb_id'] not in skills])

    # ---- text ----------------------------------------------------------------------------------------
    text = jp.text_for(root)
    moved = [(a, b) for a, b in zip(paths0, paths1) if a != b]
    check(f'F_RESOURCE_MST {v0} -> {v1}: same rows, exactly the seven dictionaries moved',
          v0 in (3277, 3278) and v1 == jp.RESOURCE_VERSION and len(paths0) == len(paths1) and len(moved) == 7, moved)
    wrong = []
    for old, new in moved:
        _, old_csv, _ = sgt.decode_zip((root / 'game_content/content' / old.lstrip('/')).read_bytes())
        _, new_csv, _ = sgt.decode_zip((root / 'game_content/content' / new.lstrip('/')).read_bytes())
        o, n = sgt.rows_of_csv(old_csv), sgt.rows_of_csv(new_csv)
        family = next(f for f, base in eu.DICTIONARIES.items() if new.split('/')[-1].startswith(base + '_'))
        want = {k: v for k, v in text[family].items() if k.encode() not in o}
        extra = {k.decode() for k in n} - {k.decode() for k in o}
        if extra != set(want) or any(n[k] != v for k, v in o.items()) \
                or any(n[k.encode()].split(b'^')[0].decode() != v for k, v in want.items()):
            wrong.append(new)
    check('each reissued dictionary = the old one plus exactly its new keys, no old line changed, English as given',
          not wrong, wrong)
    lsd = [p for _, p in moved if 'leaderSkillDescription' in p]
    check('the leader-skill descriptions skip #22-3\'s reserved 500 (499 -> 501)',
          lsd == ['/sgtext/sgtext_leaderSkillDescription_501.zip'] or lsd == ['sgtext/sgtext_leaderSkillDescription_501.zip'],
          lsd)
    check('every name and text line is present (check passes)', not jp.install(root, None, check_only=True)[1])
    again, _ = jp.install(root, None)
    check('a second install writes nothing', not again, again)

    # ---- playability -------------------------------------------------------------------------------
    rows = {r['id']: r for r in audit.audit(root)}
    fixed = [u['id'] for u in units] + list(jp.NAME_ONLY)
    check(f'the audit on the mirror: all {len(fixed)} PLAYABLE', all(rows[i]['verdict'] == 'PLAYABLE' for i in fixed),
          [(i, rows[i]['missing']) for i in fixed if rows[i]['verdict'] != 'PLAYABLE'][:4])
    check('deploy\'s files are unchanged', digest(DEPLOY) == deploy_before)

    # ---- wire ----------------------------------------------------------------------------------------
    if args.exe:
        fx = Fixture.create(qa_dir('jp_units'), port=args.port, archive_root=root / 'archive')
        with IsolatedServer(args.exe, fx) as server:
            c = Client(fx)
            db = fx.db()
            pid = db.execute("INSERT INTO user_presents (user_id, present_type, target_id, target_cnt, target_param,"
                             " receipt_type, description, present_date, receipt_date, is_receipt)"
                             " VALUES (?, 6, ?, 1, '', 0, 'jp unit test', strftime('%s','now'), 0, 0)",
                             (c.user, str(PRESENT_UNIT))).lastrowid
            db.commit()
            before = {r[0] for r in db.execute('SELECT user_unit_id FROM user_units WHERE user_id=?', (c.user,))}
            reply = c.call('PresentReceipt', body={'o6uWU0Z7': [{'i1WQkh4G': '0', 'S1B82FHK': str(pid)}]})
            got = [u for u in reply.get('qC2tJs4E', []) if int(u['edy7fq3L']) not in before
                   and int(u['pn16CNah']) == PRESENT_UNIT]
            row = mst[PRESENT_UNIT]
            want = (row['nj9Lw7mV'], row['iEFZ6H19'], row['oS3kTZ2W'])
            check(f'a present of JP unit {PRESENT_UNIT} is received into the box with its own BB, SBB and leader skill',
                  'error' not in reply and len(got) == 1
                  and (got[0].get('nj9Lw7mV'), got[0].get('iEFZ6H19'), got[0].get('oS3kTZ2W')) == want,
                  (reply.get('error'), [(u.get('nj9Lw7mV'), u.get('iEFZ6H19'), u.get('oS3kTZ2W')) for u in got], want))
            db.close()
            log_text = server.log_text() if hasattr(server, 'log_text') else ''
            if log_text:
                check('the server loaded the 29 extra archive records',
                      f'Loaded {len(new_archive)} unit archive records' in log_text)
    return check.summary('test_jp_units')


if __name__ == '__main__':
    sys.exit(main())
