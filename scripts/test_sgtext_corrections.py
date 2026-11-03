"""Tests for scripts/sgtext_corrections.py (deploy/ is only read).

    python scripts/test_sgtext_corrections.py [--work out/qa/current/<round>/sgtext_corrections]
"""
from __future__ import annotations

import argparse
import base64
import json
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import sgtext_corrections as s  # noqa: E402
from bf_testkit import Checker  # noqa: E402

WORK = s.REPO / 'out' / 'qa' / 'current' / 'sgtext_corrections_selftest'


def raises(fn, *a, **kw):
    try:
        fn(*a, **kw)
    except s.Refused as ex:
        return str(ex)
    return None


def main(argv=None) -> int:
    global WORK
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', type=Path, default=WORK)
    WORK = ap.parse_args(argv).work.resolve()
    if (s.REPO / 'out' / 'qa' / 'current').resolve() not in WORK.parents:
        print('refused: --work must lie below out/qa/current', file=sys.stderr)
        return 2
    if WORK.exists():
        shutil.rmtree(WORK)
    WORK.mkdir(parents=True)
    check = Checker()
    c = s.CORRECTIONS[0]
    csv = (b'sgtext_leaderSkillDescription^^\n' + c.key.encode() + b'^' + c.old.encode() + b'^thai <br> kept^x\n'
           + b'MST_LEADERSKILLS_1_DESCRIPTION^' + c.old.encode() + b'^y^z\n')
    out, notes = s.correct_csv(csv, [c])
    rows = s.rows_of_csv(out)
    check('only the corrected key\'s English field changes; other languages and other keys keep "<br>"',
          rows[c.key.encode()] == c.new.encode() + b'^thai <br> kept^x'
          and rows[b'MST_LEADERSKILLS_1_DESCRIPTION'] == c.old.encode() + b'^y^z', notes)
    check('already-corrected input is unchanged (idempotent)', s.correct_csv(out, [c])[0] == out)
    bad = csv.replace(c.old.encode(), b'something else', 1)
    check('an unknown English value is refused', 'refusing' in (raises(s.correct_csv, bad, [c]) or ''))
    check('a duplicated key is refused', raises(s.correct_csv, csv + c.key.encode() + b'^a^b\n', [c]) is not None)

    deploy_before = s.tree_hashes(s.DEPLOY / 'game_content' / 'mst') | s.tree_hashes(s.DEPLOY / 'game_content' / 'content' / 'sgtext')
    # The base is what deploy stages (round 8: F_RESOURCE_MST 3277 listing the EU reissue _499), so the
    # correction is _499 -> _500 under 3278.  Derived from deploy, so the test follows the next restage too.
    base, base_parts, old_rows = s.resource_base(s.DEPLOY / 'game_content')
    listed = [r['ZwstRU92'] for r in old_rows if 'sgtext_leaderSkillDescription_' in (r.get('ZwstRU92') or '')]
    n = int(listed[0].rsplit('_', 1)[1][:-4]) if len(listed) == 1 else -1
    check(f'deploy\'s F_RESOURCE_MST {base} lists one leader-skill dictionary (_{n})', len(listed) == 1, listed)
    _, current, _ = s.decode_zip((s.DEPLOY / 'game_content' / 'content' / 'sgtext' / listed[0].rsplit('/', 1)[1]).read_bytes())
    if s.correct_csv(current, [c])[0] == current:
        # Deploy carries the correction itself (released November 2: _500 under 3278, then jp_units.py's _501 under
        # 3279 kept it).  Staging it again is refused by design: SOURCE_ZIP_SHA names only the dictionaries it was
        # checked against.  The staging checks above ran on the pre-release base until then (14/0).
        check(f'deploy\'s _{n} already holds the corrected line (released)',
              s.rows_of_csv(current)[c.key.encode()].split(b'^')[0] == c.new.encode())
        msg = raises(s.stage, WORK / 'r3', base + 1)
        check('staging over the already-corrected dictionary is refused (not in SOURCE_ZIP_SHA)',
              msg and 'SOURCE_ZIP_SHA' in msg, msg)
        check('a root inside deploy/ is refused', 'inside deploy' in (raises(s.stage, s.DEPLOY / 'game_content', 3290) or ''))
        check('deploy content (mst and sgtext) is unchanged',
              s.tree_hashes(s.DEPLOY / 'game_content' / 'mst') | s.tree_hashes(s.DEPLOY / 'game_content' / 'content' / 'sgtext') == deploy_before)
        return check.summary('sgtext corrections')
    version = base + 1
    root = WORK / 'root'
    report = s.stage(root, version)
    z = root / 'content' / 'sgtext' / f'sgtext_leaderSkillDescription_{n + 1}.zip'
    name, fixed, _ = s.decode_zip(z.read_bytes())
    _, orig, _ = s.decode_zip((s.DEPLOY / 'game_content' / 'content' / 'sgtext' / f'sgtext_leaderSkillDescription_{n}.zip').read_bytes())
    a, b = s.rows_of_csv(orig), s.rows_of_csv(fixed)
    diff = sorted(k for k in a.keys() | b.keys() if a.get(k) != b.get(k))
    check(f'the _{n + 1} dictionary differs from _{n} in exactly one row, and only in its English field',
          diff == [c.key.encode()] and b[c.key.encode()].split(b'^')[0] == c.new.encode()
          and b[c.key.encode()].split(b'^')[1:] == a[c.key.encode()].split(b'^')[1:]
          and fixed.split(b'\n')[0] == orig.split(b'\n')[0] and name == f'sgtext_leaderSkillDescription_{n + 1}.csv', diff)
    raw = __import__('zipfile').ZipFile(z).read(name)
    check('its CSV is base64 in 64-character lines like the shipped one',
          {len(x) for x in raw.split(b'\n')[:-2]} == {64} and base64.b64decode(raw) == fixed)
    folder = root / 'content' / 'F_RESOURCE_MST'
    new_rows = []
    for i in range(1, base_parts + 1):
        new_rows += json.loads(s.unpad(s.AES.new(s.resource_key(), s.AES.MODE_ECB).decrypt(
            base64.b64decode((folder / f'Ver{version}_hdt2N6mr_{i}.dat').read_bytes())), 16))
    changed = [(o, n2) for o, n2 in zip(old_rows, new_rows) if o != n2]
    man = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
    check(f'F_RESOURCE_MST v{version}: same rows and order as v{base}, only the dictionary path changed; manifest matches',
          len(new_rows) == len(old_rows) and changed == [
              ({'ZwstRU92': f'/sgtext/sgtext_leaderSkillDescription_{n}.zip'},
               {'ZwstRU92': f'/sgtext/sgtext_leaderSkillDescription_{n + 1}.zip'})]
          and man == {'table': 'F_RESOURCE_MST', 'file_key': 'hdt2N6mr', 'version': version, 'file_count': base_parts,
                      'record_num': len(old_rows)}, (changed, man, report))
    check('the other reissued dictionaries stay listed (the EU rows survive)',
          sum(1 for r in new_rows if r.get('ZwstRU92', '').startswith('/sgtext/'))
          == sum(1 for r in old_rows if r.get('ZwstRU92', '').startswith('/sgtext/')))
    before = s.tree_hashes(root)
    rep2 = s.stage(root, version)
    check('staging the same version again keeps every file (identical bytes)',
          s.tree_hashes(root) == before and 'written' not in rep2, rep2)
    (folder / f'Ver{version + 1}_hdt2N6mr_1.dat').write_bytes(b'planted')
    msg = raises(s.stage, root, version + 1)
    check('a different file already under a target name is a refused collision, and kept',
          msg and 'collision' in msg and (folder / f'Ver{version + 1}_hdt2N6mr_1.dat').read_bytes() == b'planted', msg)
    check(f'a version not above the staged {base} is refused', 'above' in (raises(s.stage, WORK / 'r2', base) or ''))
    check('a root inside deploy/ is refused', 'inside deploy' in (raises(s.stage, s.DEPLOY / 'game_content', 3290) or ''))
    check('deploy content (mst and sgtext) is unchanged',
          s.tree_hashes(s.DEPLOY / 'game_content' / 'mst') | s.tree_hashes(s.DEPLOY / 'game_content' / 'content' / 'sgtext') == deploy_before)
    return check.summary('sgtext corrections')


if __name__ == '__main__':
    raise SystemExit(main())
