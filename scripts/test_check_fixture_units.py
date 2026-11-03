"""Tests for scripts/qa/check_fixture_units.py on a synthetic save (deploy/mst is only read).

    python scripts/test_check_fixture_units.py

Organic rows ('' element, 0 leader skill, extra_skill_id mirroring the SBB) give no finding; a row cloned from
another unit is CLONE (skills, element, stats), an unknown unit id and a squad slot naming a missing row are
ERROR, a short total_exp is EXP; --repair fixes CLONE and EXP in place, leaves the ERRORs, keeps everything else
in the row, and refuses a file outside out/; without --repair the file is not written.
"""
import hashlib
import shutil
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'qa'))
sys.path.insert(0, str(HERE))
import check_fixture_units as cfu  # noqa: E402
from bf_testkit import ROOT, Checker  # noqa: E402

TOOL = HERE / 'qa' / 'check_fixture_units.py'
COLS = ['user_unit_id', 'user_id', 'unit_id', 'unit_type_id', 'unit_lvl', 'base_hp', 'base_atk', 'base_def',
        'base_rec', 'bb_id', 'bb_lvl', 'sbb_id', 'sbb_lvl', 'total_exp', 'extra_skill_id', 'leader_skill_id',
        'element', 'favorite_flg']
KLAUS, DURUMN, UNKNOWN = 20735, 840278, 99999999


def own_row(units, pattern, uuid, uid, level, **over):
    m = units[uid]
    sbb = cfu.num(m['iEFZ6H19'])
    row = dict(user_unit_id=uuid, user_id=1, unit_id=uid, unit_type_id=1, unit_lvl=level,
               **{col: cfu.scaled(m, level, lo, hi) for col, lo, hi in cfu.STATS},
               bb_id=str(cfu.num(m['nj9Lw7mV'])), bb_lvl=10, sbb_id=str(sbb) if sbb else '', sbb_lvl=10 if sbb else 0,
               total_exp=cfu.exp_needed(m, pattern, level), extra_skill_id=sbb,
               leader_skill_id=cfu.num(m['oS3kTZ2W']), element=cfu.ELEMENT[str(m['iNy0ZU5M'])], favorite_flg=1)
    row.update(over)
    return row


def build(path, units, pattern):
    db = sqlite3.connect(path)
    db.execute(f'CREATE TABLE user_units ({", ".join(COLS)})')
    db.execute('CREATE TABLE user_decks (user_id, user_unit_id, deck_type, deck_num, member_type, disp_order)')
    klaus = own_row(units, pattern, 1, KLAUS, 100)
    rows = [
        klaus,
        own_row(units, pattern, 2, KLAUS, 1, element='', leader_skill_id=0, extra_skill_id=0),   # organic shape
        dict(klaus, user_unit_id=3, unit_id=DURUMN),                                            # the clone
        dict(klaus, user_unit_id=4, unit_id=UNKNOWN),                                           # crashes View Units
        own_row(units, pattern, 5, DURUMN, 150, total_exp=5),                                   # short of its level
        own_row(units, pattern, 6, DURUMN, 150, base_hp=1150, base_atk=529, base_def=377, base_rec=474),  # R8 v12
    ]
    for r in rows:
        db.execute(f'INSERT INTO user_units ({", ".join(r)}) VALUES ({", ".join("?" * len(r))})', list(r.values()))
    for slot, uuid in enumerate((1, 3, 77)):
        db.execute('INSERT INTO user_decks VALUES (1, ?, 1, 0, ?, ?)', (uuid, int(slot == 0), slot))
    db.commit()
    db.close()


def run(*args):
    return subprocess.run([sys.executable, '-I', str(TOOL), *map(str, args)], capture_output=True, text=True)


def kinds(stdout):
    return sorted((line.split()[0], int(line.split()[2])) for line in stdout.splitlines()
                  if line.split() and line.split()[0] in ('ERROR', 'CLONE', 'EXP'))


def main():
    check = Checker()
    units, pattern = cfu.load(ROOT / 'deploy' / 'mst')
    work = ROOT / 'out' / 'qa' / 'test_check_fixture_units'
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    try:
        db = work / 'fixture.sqlite'
        build(db, units, pattern)
        digest = hashlib.sha256(db.read_bytes()).hexdigest()
        res = run(db)
        want = [('CLONE', 3), ('CLONE', 6), ('ERROR', 4), ('ERROR', 77), ('EXP', 5)]
        check('findings: the clones, the unknown unit, the dangling slot, the short exp', kinds(res.stdout) == want,
              kinds(res.stdout))
        check('organic rows give no finding', not any(k[1] in (1, 2) for k in kinds(res.stdout)), res.stdout)
        line = {int(x.split()[2]): x for x in res.stdout.splitlines() if x.startswith('CLONE')}
        check('the clone names its borrowed burst and element (its stats are plausible for the unit)',
              'bb_id' in line.get(3, '') and 'element' in line.get(3, '') and 'base_hp' not in line.get(3, ''),
              line.get(3))
        check('stats far outside the unit\'s range are a clone on their own', 'base_hp 1150' in line.get(6, '')
              and 'bb_id' not in line.get(6, ''), line.get(6))
        check('exit 1 with findings', res.returncode == 1, res.returncode)
        check('a check without --repair writes nothing', hashlib.sha256(db.read_bytes()).hexdigest() == digest)

        res = run(db, '--repair')
        check('--repair leaves only the ERRORs', kinds(res.stdout.split('repaired;')[-1]) == [('ERROR', 4), ('ERROR', 77)],
              res.stdout)
        con = sqlite3.connect(db)
        con.row_factory = sqlite3.Row
        fixed = dict(con.execute('SELECT * FROM user_units WHERE user_unit_id=3').fetchone())
        exp5 = con.execute('SELECT total_exp FROM user_units WHERE user_unit_id=5').fetchone()[0]
        organic = dict(con.execute('SELECT * FROM user_units WHERE user_unit_id=2').fetchone())
        con.close()
        m = units[DURUMN]
        check('the clone gets its own burst, SBB, leader skill and element',
              (cfu.num(fixed['bb_id']), cfu.num(fixed['sbb_id']), fixed['leader_skill_id'], fixed['element'],
               fixed['extra_skill_id'])
              == (cfu.num(m['nj9Lw7mV']), cfu.num(m['iEFZ6H19']), cfu.num(m['oS3kTZ2W']),
                  cfu.ELEMENT[str(m['iNy0ZU5M'])], cfu.num(m['iEFZ6H19'])), fixed)
        check('and the server\'s level-scaled stats', fixed['base_hp'] == cfu.scaled(m, 100, 'UZ1Bj7w2', '3WMz78t6'),
              fixed['base_hp'])
        check('the rest of the row is kept', (fixed['unit_lvl'], fixed['bb_lvl'], fixed['favorite_flg']) == (100, 10, 1),
              fixed)
        check('the short exp rises to its level', exp5 == cfu.exp_needed(m, pattern, 150), exp5)
        check('the organic row is untouched', (organic['element'], organic['leader_skill_id']) == ('', 0), organic)

        outside = Path(tempfile.mkdtemp()) / 'fixture.sqlite'
        shutil.copyfile(db, outside)
        res = run(outside, '--repair')
        check('--repair refuses a file outside out/', res.returncode != 0 and 'refusing' in (res.stderr + res.stdout),
              res.stderr[-200:])
        shutil.rmtree(outside.parent, ignore_errors=True)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return check.summary('test_check_fixture_units')


if __name__ == '__main__':
    sys.exit(main())
