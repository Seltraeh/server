"""Check a QA save copy's user_units for the traces of a cloned row, and repair them.

    python -I scripts/qa/check_fixture_units.py DB [--repair] [--mst-root deploy/mst]

QA fixtures add units by copying an existing user_units row and changing unit_id (prep_r8.py, prep_eu.py).
Whatever the copy does not overwrite still belongs to the source unit, and the client shows it: round 8's EU
fixture rows cloned from Klaus cast his "Zwei Raid" (bb_id), and three unit ids that UnitMst does not have
crashed View Units (x86 +0xDE25E).  A save is checked against the unit's own UnitMst row:

  ERROR  unit_id missing from UnitMst, or a squad slot naming a user_unit_id that does not exist
         (PartyEditTopScene2 null read; the server's `clearunits` CLI repairs that one).  Reported only:
         which unit was meant is a decision, not a repair.
  CLONE  a non-empty bb_id / sbb_id / leader_skill_id / element that is not this unit's, extra_skill_id
         that is not this unit's SBB (the legacy mirror UnitEvo keeps in step with sbb_id), or a base stat
         outside half the unit's minimum to 1.6x its maximum.  Empty and 0 are what organic rows carry
         ('' element on most of them, 0 leader skill on old archive grants) and are left alone: the 10-24
         save backup, the S19 fixture and round 8's v12 save have no CLONE finding under these rules.
  EXP    total_exp below what the stored unit_lvl needs: the server levels from total_exp, so the unit
         drops back on its next experience gain.

--repair (only on a file under out/) gives every CLONE row its own skills, element and the server's
level-scaled base stats (Common.hpp scaleUnitBaseStats), raises EXP rows to their level's threshold, in one
transaction, then checks again.  Exit status: 0 when nothing is left to report.
"""
import argparse
import json
import sqlite3
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ELEMENT = {'1': 'fire', '2': 'water', '3': 'earth', '4': 'thunder', '5': 'light', '6': 'dark'}
# user_units column -> UnitMst key.  extra_skill_id mirrors the SBB (UnitEvo.cpp), not UnitMst's extra skill.
SKILLS = (('bb_id', 'nj9Lw7mV'), ('sbb_id', 'iEFZ6H19'), ('leader_skill_id', 'oS3kTZ2W'),
          ('extra_skill_id', 'iEFZ6H19'))
STATS = (('base_hp', 'UZ1Bj7w2', '3WMz78t6'), ('base_atk', 'i9Tn7kYr', 'omuyP54D'),
         ('base_def', 'q78KoWsg', '32INDST4'), ('base_rec', '92ij6UGB', 'X9P3AN5d'))


def num(v) -> int:
    s = str(v).strip() if v is not None else ''
    return int(s) if s.lstrip('-').isdigit() else 0


def scaled(mst: dict, level: int, lo: str, hi: str) -> int:
    """Common.hpp scaleUnitBaseStats: min at level 1, max at max_lv, linear and truncated between."""
    max_lv, a, b = num(mst['EI1DF8Yt']), num(mst[lo]), num(mst[hi])
    if max_lv <= 1 or level <= 1:
        return a
    if level >= max_lv:
        return b
    return a + int((b - a) * (level - 1) / (max_lv - 1))


def load(mst_root: Path):
    units = {num(r['pn16CNah']): r for r in
             json.loads((mst_root / 'unit_mst.json').read_text(encoding='utf-8-sig'))['2r9cNSdt']}
    pattern = {}
    for r in json.loads((mst_root / 'unit_exp_pattern_mst.json').read_text(encoding='utf-8-sig'))['JYFGe9y6']:
        pattern.setdefault(num(r['5UvTp7q1']), {})[num(r['D9wXQI2V'])] = num(r['grKTq15A'])
    return units, pattern


def exp_needed(mst: dict, pattern: dict, level: int) -> int:
    """prep_r8.py's total_exp_for: the pattern's per-level experience summed over levels 2..level."""
    levels = pattern.get(num(mst['5UvTp7q1']), {})
    return sum(e for lv, e in levels.items() if 2 <= lv <= level)


def findings(db, units, pattern):
    out = []
    ids = {r['user_unit_id'] for r in db.execute('SELECT user_unit_id FROM user_units')}
    for r in db.execute('SELECT * FROM user_units ORDER BY user_unit_id'):
        mst = units.get(r['unit_id'])
        if mst is None:
            out.append(('ERROR', r['user_unit_id'], r['unit_id'], 'unit_id is not in UnitMst'))
            continue
        bad = [f"{col} {r[col]!r} (own {num(mst[key])})" for col, key in SKILLS
               if num(r[col]) and num(r[col]) != num(mst[key])]
        own = ELEMENT.get(str(mst['iNy0ZU5M']))
        if r['element'] and r['element'] != own:
            bad.append(f"element {r['element']!r} (own {own!r})")
        bad += [f"{col} {r[col]} (own {num(mst[lo])}..{num(mst[hi])})" for col, lo, hi in STATS
                if not num(mst[lo]) * 0.5 <= num(r[col]) <= num(mst[hi]) * 1.6]
        if bad:
            out.append(('CLONE', r['user_unit_id'], r['unit_id'], '; '.join(bad)))
        need = exp_needed(mst, pattern, r['unit_lvl'])
        if num(r['total_exp']) < need:
            out.append(('EXP', r['user_unit_id'], r['unit_id'],
                        f"total_exp {r['total_exp']} < {need} for level {r['unit_lvl']}"))
    for d in db.execute('SELECT user_unit_id, deck_num, disp_order FROM user_decks'):
        if d['user_unit_id'] not in ids:
            out.append(('ERROR', d['user_unit_id'], None,
                        f"squad {d['deck_num']} slot {d['disp_order']} names a missing user_unit_id"))
    return out


def repair(db, units, pattern, found):
    with db:
        for kind, uuid, uid, _ in found:
            mst = units.get(uid)
            if kind == 'CLONE':
                lv = db.execute('SELECT unit_lvl FROM user_units WHERE user_unit_id=?', (uuid,)).fetchone()[0]
                sbb = num(mst['iEFZ6H19'])
                db.execute('UPDATE user_units SET bb_id=?, sbb_id=?, leader_skill_id=?, extra_skill_id=?, element=?, '
                           'base_hp=?, base_atk=?, base_def=?, base_rec=?, sbb_lvl=CASE WHEN ?=0 THEN 0 ELSE sbb_lvl END '
                           'WHERE user_unit_id=?',
                           (num(mst['nj9Lw7mV']), sbb, num(mst['oS3kTZ2W']), sbb, ELEMENT.get(str(mst['iNy0ZU5M']), ''),
                            *(scaled(mst, lv, lo, hi) for _, lo, hi in STATS), sbb, uuid))
            elif kind == 'EXP':
                lv = db.execute('SELECT unit_lvl FROM user_units WHERE user_unit_id=?', (uuid,)).fetchone()[0]
                db.execute('UPDATE user_units SET total_exp=? WHERE user_unit_id=?',
                           (exp_needed(mst, pattern, lv), uuid))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('db', type=Path)
    ap.add_argument('--repair', action='store_true')
    ap.add_argument('--mst-root', type=Path, default=ROOT / 'deploy' / 'mst')
    a = ap.parse_args()
    path = a.db.resolve()
    if a.repair and (ROOT / 'out') not in path.parents:
        sys.exit(f'refusing to repair {path}: only copies under {ROOT / "out"}')
    units, pattern = load(a.mst_root)
    db = sqlite3.connect(path) if a.repair else sqlite3.connect(f'file:{path.as_posix()}?mode=ro', uri=True)
    db.row_factory = sqlite3.Row
    found = findings(db, units, pattern)
    for kind, uuid, uid, what in found:
        print(f'{kind:5}  user_unit {uuid}  unit {uid}: {what}')
    if a.repair and any(k != 'ERROR' for k, *_ in found):
        repair(db, units, pattern, found)
        found = findings(db, units, pattern)
        print(f'repaired; {len(found)} finding(s) left')
        for kind, uuid, uid, what in found:
            print(f'{kind:5}  user_unit {uuid}  unit {uid}: {what}')
    rows = db.execute('SELECT COUNT(*) FROM user_units').fetchone()[0]
    print(f'{path.name}: {rows} unit row(s), {len(found)} finding(s)')
    db.close()
    return 1 if found else 0


if __name__ == '__main__':
    sys.exit(main())
