"""Tests for scripts/unit_mst_corrections.py (nothing under deploy/ is written).

    python scripts/test_unit_mst_corrections.py [--work out/qa/current/<round>/unit_mst_corrections]

The work folder (default out/qa/current/unit_mst_corrections_selftest) is recreated each run.  deploy/mst/
unit_mst.json and deploy's staged F_UNIT_MST folder are only read, and are hashed before and after.  The
real-data cases run when deploy's unit_mst.json is the pristine source the round-3 acceptance used;
otherwise they are reported as skipped and the synthetic cases still run.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import unit_mst_corrections as u  # noqa: E402
from bf_testkit import Checker  # noqa: E402

DEPLOY_UNIT = u.REPO / 'deploy' / 'mst' / 'unit_mst.json'
ACCEPTED_MANIFEST = '74f51627400b00bc362e0123b17bdd600c226aa8304538306291ea9fc550be8a'
WORK = u.REPO / 'out' / 'qa' / 'current' / 'unit_mst_corrections_selftest'


def raises(fn, *args, **kw) -> str | None:
    try:
        fn(*args, **kw)
    except u.Refused as ex:
        return str(ex)
    return None


def sha_file(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def doc(rows: list[dict]) -> bytes:
    """A small unit_mst document in deploy's own layout (indent 1, CRLF)."""
    return json.dumps({u.WRAPPER: rows}, indent=1, ensure_ascii=False).replace('\n', '\r\n').encode('utf-8')


def row(uid: str, crop: str, name: str = 'x') -> dict:
    return {u.ID: uid, 'utP1c0CD': name, '1W9CxaFK': crop, '6z54rgb3': '1,2,3,4'}


def test_synthetic(check: Checker) -> None:
    c = u.CORRECTIONS[0]
    src = doc([row('50253', c.old, 'Tilith'), row(c.unit, c.old, 'Goddess Tilith'), row('10233', '545,33,174,475')])
    out, notes = u.apply_corrections(src)
    a, b = u.rows_of(src), u.rows_of(out)
    check('only the corrected unit changes; another row holding the same old value is left alone',
          b[1]['1W9CxaFK'] == c.new and b[0]['1W9CxaFK'] == c.old and a[2] == b[2] and a[0] == b[0], notes)
    check('CRLF line endings and every other byte are kept',
          out.count(b'\r\n') == src.count(b'\r\n') and out.replace(c.new.encode(), c.old.encode()) == src)
    again, notes2 = u.apply_corrections(out)
    check('already-corrected input is returned unchanged (idempotent)',
          again == out and any('already' in n for n in notes2), notes2)
    bad = doc([row(c.unit, '1,2,3,4')])
    check('an unknown current value is refused', 'unknown source' in (raises(u.apply_corrections, bad) or ''))
    check('a missing unit is refused', 'exactly one' in (raises(u.apply_corrections, doc([row('1', c.old)])) or ''))
    dup = doc([row(c.unit, c.old), row(c.unit, c.old)])
    check('a duplicated unit row is refused', 'exactly one' in (raises(u.apply_corrections, dup) or ''))
    check('a document without the row list is refused', raises(u.apply_corrections, b'{"x": 1}') is not None)


def test_propose(check: Checker, pristine: bool) -> None:
    deploy_before = sha_file(DEPLOY_UNIT)
    out = WORK / 'proposal' / 'unit_mst.json'
    if pristine:
        report = u.propose(DEPLOY_UNIT, out)
        check('the pristine deploy source yields exactly the round-3 accepted proposal',
              sha_file(out) == u.CORRECTED_SHA, report)
        a, b = u.rows_of(DEPLOY_UNIT.read_bytes()), u.rows_of(out.read_bytes())
        diff = [(x[u.ID], k) for x, y in zip(a, b) for k in x if x[k] != y[k]]
        check('parsed: 2291 rows, exactly one value differs (50254 1W9CxaFK)',
              len(a) == len(b) == 2291 and diff == [('50254', '1W9CxaFK')], diff)
        report = u.propose(DEPLOY_UNIT, out)
        check('proposing again onto identical bytes reports unchanged', 'unchanged' in report, report)
        again = WORK / 'proposal' / 'again.json'
        u.propose(out, again)
        check('the corrected proposal as a source gives identical bytes', again.read_bytes() == out.read_bytes())
    else:
        print('SKIP  real-data propose: deploy/mst/unit_mst.json is not the pristine round-3 source')
    other = WORK / 'proposal' / 'different.json'
    other.write_bytes(b'{}')
    check('an existing output with different bytes is refused, and kept',
          'refusing to overwrite' in (raises(u.propose, DEPLOY_UNIT, other) or '') and other.read_bytes() == b'{}')
    check('writing onto the source is refused', 'overwrites its input' in (raises(u.propose, DEPLOY_UNIT, DEPLOY_UNIT) or ''))
    target = u.REPO / 'deploy' / 'mst' / 'unit_mst_correction_probe.json'
    check('writing into deploy/ is refused and nothing is created',
          'inside deploy' in (raises(u.propose, DEPLOY_UNIT, target) or '') and not target.exists())
    check('deploy/mst/unit_mst.json is unchanged', sha_file(DEPLOY_UNIT) == deploy_before)


def test_stage(check: Checker, pristine: bool) -> None:
    deploy_stage = u.tree_hashes(u.DEPLOY_STAGE)
    rows_file = WORK / 'proposal' / 'unit_mst.json'
    if not pristine:
        print('SKIP  real-data stage: no pristine proposal')
        return
    # A source content root holding v1086 as COPIES (deploy's own files are never linked by the tests).
    src = WORK / 'src_root'
    (src / 'content' / u.TABLE).mkdir(parents=True)
    for p in u.DEPLOY_STAGE.iterdir():
        (src / 'content' / u.TABLE / p.name).write_bytes(p.read_bytes())
    (src / 'content' / 'other.txt').write_bytes(b'unrelated content')
    src_hashes = u.tree_hashes(src)
    dst = WORK / 'dst_root'
    report = u.stage(rows_file, dst, 1087, mirror_from=src)
    folder = dst / 'content' / u.TABLE
    check('staging v1087 regenerates the client-accepted part and manifest byte for byte',
          sha_file(folder / 'Ver1087_2r9cNSdt.dat') == u.ACCEPTED_PARTS[1087][0]
          and sha_file(folder / 'manifest.json') == ACCEPTED_MANIFEST and 'matches' in report, report)
    check('the mirror source is untouched: its manifest still says v1086 and every hash holds',
          u.tree_hashes(src) == src_hashes
          and json.loads((src / 'content' / u.TABLE / 'manifest.json').read_text())['version'] == 1086)
    check('the staged manifest and part are new files; the mirrored v1086 stays linked to the source',
          os.stat(folder / 'manifest.json').st_nlink == 1 and os.stat(folder / 'Ver1087_2r9cNSdt.dat').st_nlink == 1
          and os.stat(folder / 'Ver1086_2r9cNSdt.dat').st_nlink == 2)
    before = u.tree_hashes(dst)
    report = u.stage(rows_file, dst, 1087)
    check('staging the same version again keeps everything (identical bytes)',
          u.tree_hashes(dst) == before and 'written' not in report, report)
    # A root whose SOURCE already holds v1087 (a mirror of dst): same bytes are kept, nothing written through.
    dst2 = WORK / 'dst2_root'
    report = u.stage(rows_file, dst2, 1087, mirror_from=dst)
    check('a mirror whose source already holds v1087: kept, the source inode is not written through',
          'written' not in report and u.tree_hashes(dst) == before
          and os.stat(dst2 / 'content' / u.TABLE / 'Ver1087_2r9cNSdt.dat').st_nlink == 2, report)   # dst + dst2
    changed = WORK / 'proposal' / 'changed.json'
    rows = u.rows_of(rows_file.read_bytes())
    rows[0]['utP1c0CD'] = 'collision probe'
    changed.write_bytes(json.dumps({u.WRAPPER: rows}).encode('utf-8'))
    msg = raises(u.stage, changed, dst2, 1087)
    check('different rows under an already-staged version are a refused collision; nothing changes',
          msg and 'collision' in msg and u.tree_hashes(dst) == before, msg)
    msg = raises(u.stage, rows_file, dst2, 1086)
    check('a version below the staged manifest is refused', msg and 'below' in msg, msg)
    report = u.stage(changed, dst2, 1088)
    check('an explicit higher version stages beside the linked v1087 without touching it',
          'Ver1088_2r9cNSdt.dat: written' in report and u.tree_hashes(dst) == before, report)
    msg = raises(u.stage, WORK / 'proposal' / 'different.json', WORK / 'never', 1089)
    check('rows that are not a unit_mst document are refused', msg is not None and not (WORK / 'never').exists(), msg)
    pristine_rows = WORK / 'proposal' / 'pristine.json'
    pristine_rows.write_bytes(DEPLOY_UNIT.read_bytes())
    msg = raises(u.stage, pristine_rows, WORK / 'never', 1089)
    check('uncorrected rows are refused (propose first)', msg and 'propose first' in msg, msg)
    msg = raises(u.stage, rows_file, u.REPO / 'deploy' / 'game_content', 1087)
    check('a content root inside deploy/ is refused', msg and 'inside deploy' in msg, msg)
    check('deploy\'s staged F_UNIT_MST folder is unchanged', u.tree_hashes(u.DEPLOY_STAGE) == deploy_stage)


def test_temp_files(check: Checker, pristine: bool) -> None:
    """Round-4 QC P2: a writer must never open an existing name.  Every case below plants files at the
    old predictable temporary names; their bytes and link targets must survive, and no temporary remains."""
    d = WORK / 'temp_cases'
    d.mkdir()
    sentinel = d / 'sentinel.txt'
    sentinel.write_bytes(b'QA sentinel unchanged expected')
    stale = d / 'stale.bin'
    stale.write_bytes(b'an earlier attempt')

    def planted(out: Path) -> dict:
        tmp = out.with_name(out.name + '.tmp')
        os.link(sentinel, tmp)                                  # hard link to the sentinel
        made = {'hard': tmp}
        dangling = out.with_name(out.name + '.dangling.tmp')
        try:
            os.symlink(d / 'nowhere.bin', dangling)
            made['dangling'] = dangling
        except OSError as ex:
            print(f'SKIP  dangling-link case: {ex}')
        return made

    out = d / 'proposal.json'
    links = planted(out)
    if pristine:
        report = u.propose(DEPLOY_UNIT, out)
        check('propose with a hard-linked <out>.tmp in place: the sentinel keeps its bytes, output is correct',
              sentinel.read_bytes() == b'QA sentinel unchanged expected' and sha_file(out) == u.CORRECTED_SHA, report)
    check('the planted names are untouched (still a link to the sentinel; dangling link still dangling)',
          os.stat(links['hard']).st_nlink == 2 and (('dangling' not in links)
          or (os.path.islink(links['dangling']) and not os.path.exists(d / 'nowhere.bin'))))
    target = d / 'elsewhere.json'
    target.write_bytes(b'elsewhere')
    link_out = d / 'linked_out.json'
    try:
        os.symlink(target, link_out)
    except OSError as ex:
        print(f'SKIP  symlinked output: {ex}')
    else:
        msg = raises(u.propose, DEPLOY_UNIT, link_out)
        check('an output that is a symlink is refused and its target keeps its bytes',
              msg and 'link' in msg and target.read_bytes() == b'elsewhere', msg)
    hl = d / 'hl.bin'
    os.link(stale, hl)
    u.replace_into(hl, b'new bytes')
    check('replace_into over a hard-linked name swaps the entry; the other name keeps its bytes',
          hl.read_bytes() == b'new bytes' and stale.read_bytes() == b'an earlier attempt'
          and os.stat(stale).st_nlink == 1)
    before = sorted(p.name for p in d.iterdir())
    real = os.replace

    def failing(*a, **kw):
        raise OSError('injected rename failure')
    os.replace = failing
    try:
        failed = False
        try:
            u.replace_into(d / 'fails.bin', b'x')
        except OSError:
            failed = True
    finally:
        os.replace = real
    check('a failed rename leaves nothing behind: only the temporary this call created is removed',
          failed and sorted(p.name for p in d.iterdir()) == before and not (d / 'fails.bin').exists())
    check('no temporary file is left anywhere in the cases',
          not [p.name for p in d.iterdir() if p.name.startswith('.') and p.name.endswith('.tmp')])
    if pristine:
        root = WORK / 'temp_stage'
        folder = root / 'content' / u.TABLE
        folder.mkdir(parents=True)
        os.link(sentinel, folder / 'manifest.json.tmp')
        os.link(sentinel, folder / 'Ver1087_2r9cNSdt.dat.tmp')
        report = u.stage(WORK / 'proposal' / 'unit_mst.json', root, 1087)
        check('staging with hard-linked old-style temporaries in place: sentinel intact, accepted bytes produced',
              sentinel.read_bytes() == b'QA sentinel unchanged expected'
              and sha_file(folder / 'Ver1087_2r9cNSdt.dat') == u.ACCEPTED_PARTS[1087][0]
              and sha_file(folder / 'manifest.json') == ACCEPTED_MANIFEST, report)


# deploy/mst/unit_mst.json after the round-8 EU install (scripts/eu_units.py; 2,291 Global + 224 EU rows), whose
# table deploy stages as F_UNIT_MST 1089.  #14 is rebased onto it (round 8, November 1).
EU_SOURCE_SHA = '59ed8ec3526c4ce314ab07f6daf7ba338a32b908fa149f75f5fecd48d9d2f2e7'


def test_rebased(check: Checker) -> None:
    """The correction on the EU-installed table: one value changes, every EU row survives, staged above 1089."""
    if sha_file(DEPLOY_UNIT) != EU_SOURCE_SHA:
        print('SKIP  rebased propose/stage: deploy/mst/unit_mst.json is not the round-8 EU-installed table')
        return
    out = WORK / 'rebased' / 'unit_mst.json'
    report = u.propose(DEPLOY_UNIT, out)
    a, b = u.rows_of(DEPLOY_UNIT.read_bytes()), u.rows_of(out.read_bytes())
    diff = [(x[u.ID], k) for x, y in zip(a, b) for k in x if x[k] != y[k]]
    check('the EU-installed source: 2515 rows, exactly one value differs (50254 1W9CxaFK)',
          len(a) == len(b) == 2515 and diff == [('50254', '1W9CxaFK')], (diff, report))
    staged = json.loads((u.DEPLOY_STAGE / 'manifest.json').read_text(encoding='utf-8'))['version']
    src = WORK / 'rebased_src'
    (src / 'content' / u.TABLE).mkdir(parents=True)
    for p in u.DEPLOY_STAGE.iterdir():
        (src / 'content' / u.TABLE / p.name).write_bytes(p.read_bytes())
    dst = WORK / 'rebased_dst'
    version = staged + 1
    report = u.stage(out, dst, version, mirror_from=src)
    folder = dst / 'content' / u.TABLE
    man = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
    back = [r for name in sorted(folder.glob(f'Ver{version}_*.dat')) for r in u.decode_part(name.read_bytes())]
    check(f'staged as v{version}, above deploy\'s v{staged}: the parts decode to the corrected rows, EU ones included',
          man['version'] == version and man['record_num'] == 2515 and back == b, (man, report))
    check(f'a version not above the v{version} now staged is refused',
          'below the staged' in (raises(u.stage, out, dst, staged) or ''))


def main(argv=None) -> int:
    global WORK
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--work', type=Path, default=WORK)
    a = ap.parse_args(argv)
    WORK = a.work.resolve()
    qa = (u.REPO / 'out' / 'qa' / 'current').resolve()
    if qa not in WORK.parents:
        print(f'refused: --work must lie below {qa}', file=sys.stderr)
        return 2
    if WORK.exists():
        shutil.rmtree(WORK)
    (WORK / 'proposal').mkdir(parents=True)
    check = Checker()
    pristine = sha_file(DEPLOY_UNIT) == u.PRISTINE_SHA
    test_synthetic(check)
    test_propose(check, pristine)
    test_stage(check, pristine)
    test_temp_files(check, pristine)
    test_rebased(check)
    return check.summary('unit_mst corrections')


if __name__ == '__main__':
    raise SystemExit(main())
