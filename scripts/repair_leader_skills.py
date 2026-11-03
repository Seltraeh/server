"""Repair user_units.leader_skill_id on saves written before archive grants stored it.

    python scripts/repair_leader_skills.py [--save PATH] [--apply] [--backup-dir DIR] [--json FILE]

Until Session 9, every unit granted through gme::fromArchivedUnit -- summons,
presents, exchanges, achievement trades, captures, tutorial starters, selector
picks, CLI addunit -- was stored with leader_skill_id 0.  What a grant stores
now, and what addUserUnit(UnitMst) and UnitEvo always stored, is the UnitMst
row's leader skill (oS3kTZ2W) of the unit's CURRENT form.  The client reads the
stored value in Training Grounds (a helper taken from the player's own units),
the Challenge Arena copies and the campaign / Frontier Gate save data; ordinary
battles and the unit screens read UnitMst and were never affected.

Every row is classified; only `repair` rows are written:

  repair      stored 0, UnitMst names a skill LeaderSkillMst has -> set to it
  ok          stored == UnitMst (both 0 for materials and the like)
  preserved   stored nonzero and different from UnitMst: NEVER touched (an
              override, an upgrade or a form this tool cannot judge); listed
  unknown     no UnitMst row for the unit: left as is -- no guessed skill
  unresolved  stored 0, UnitMst names a skill LeaderSkillMst lacks: left as
              is (the client could not resolve it); listed

Idempotent: a repaired row is `ok` on the next run, and each UPDATE is guarded
by leader_skill_id = 0, so a value written after the dry run is never
overwritten.  UnitMst is deploy/mst/unit_mst.json plus the homebrew overlay,
merged as the server merges it (an overlay id that collides is ignored).

Dry run unless --apply.  --apply takes a backup-API copy of the save first and
writes in one transaction.  Either mode refuses deploy/gme.sqlite while a
gimuserverw process runs: stop the server (bf_ctl.ps1 -Action stop), repair,
start it.  A
running client sees the values at its next return to Home (UserInfo replaces
the whole unit list, 4ceMWH6k) or after a relaunch.
"""
from __future__ import annotations

import argparse
import datetime
import json
import sqlite3
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import LIVE_SAVE, OUT, ROOT, backup_copy  # noqa: E402

MST = ROOT / 'deploy' / 'mst'
UNIT_ID, UNIT_NAME, UNIT_LS = 'pn16CNah', 'utP1c0CD', 'oS3kTZ2W'
CATEGORIES = ('repair', 'ok', 'preserved', 'unknown', 'unresolved')


def mst_rows(path: Path) -> list:
    """The row list of a client MST file (one wrapper key around a list)."""
    data = json.loads(path.read_text(encoding='utf-8'))
    lists = [v for v in data.values() if isinstance(v, list)]
    if len(lists) != 1:
        raise ValueError(f'{path}: expected one row list, found {len(lists)}')
    return lists[0]


def load_unit_mst(mst_root: Path = MST) -> dict:
    """{unit id: row} as ServerCache builds it: shipped rows, then homebrew rows
    whose ids do not collide with a shipped unit."""
    units = {int(r[UNIT_ID]): r for r in mst_rows(mst_root / 'unit_mst.json')}
    overlay = mst_root / 'unit_mst_homebrew.json'
    if overlay.is_file():
        for row in mst_rows(overlay):
            units.setdefault(int(row[UNIT_ID]), row)
    return units


def load_leader_skill_ids(mst_root: Path = MST) -> set:
    return {int(r[UNIT_LS]) for r in mst_rows(mst_root / 'leader_skill_mst.json')}


def unit_species(raw) -> int | None:
    """user_units.unit_id is TEXT and may carry a '_<suffix>' (see CampaignBattleStart)."""
    text = str(raw).split('_', 1)[0]
    return int(text) if text.isdigit() else None


def classify(rows, units: dict, leader_skills: set) -> list:
    """[(category, row dict, wanted leader skill or None)] for every unit row."""
    out = []
    for r in rows:
        species = unit_species(r['unit_id'])
        mst = units.get(species) if species is not None else None
        have = int(r['leader_skill_id'] or 0)
        if mst is None:
            out.append(('unknown', r, None))
            continue
        want = max(int(mst[UNIT_LS] or 0), 0)
        if have == want:
            out.append(('ok', r, want))
        elif have != 0:
            out.append(('preserved', r, want))
        elif want not in leader_skills:
            out.append(('unresolved', r, want))
        else:
            out.append(('repair', r, want))
    return out


def server_running() -> list:
    script = ("Get-CimInstance Win32_Process -Filter \"Name='gimuserverw.exe'\""
              " | ForEach-Object { '{0} {1}' -f $_.ProcessId, $_.ExecutablePath }")
    out = subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command', script],
                         capture_output=True, text=True).stdout
    return [line for line in out.splitlines() if line.strip()]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--save', type=Path, default=LIVE_SAVE)
    ap.add_argument('--apply', action='store_true', help='write the repairs (default: dry run)')
    ap.add_argument('--backup-dir', type=Path, default=OUT / 'backups' / 'leader_skill_repair')
    ap.add_argument('--mst-root', type=Path, default=MST)
    ap.add_argument('--json', type=Path, help='also write the classification here')
    args = ap.parse_args()

    save = args.save.resolve()
    if not save.is_file():
        print(f'no save at {save}')
        return 2
    if save == LIVE_SAVE.resolve():
        # Even a dry run: a read of the live save that overlaps a request makes
        # a server without the Session 9 busy timeout lose that request's COMMIT
        # and fail every later one (scripts/test_db_commit_contention_wire.py).
        running = server_running()
        if running:
            print('refusing: a gimuserverw process is running and may be serving the live save; stop it first'
                  ' (bf_ctl.ps1 -Target server -Action stop), then rerun:')
            for line in running:
                print('  ', line)
            return 2

    units = load_unit_mst(args.mst_root)
    leader_skills = load_leader_skill_ids(args.mst_root)
    db = sqlite3.connect(f'file:{save.as_posix()}?mode=ro', uri=True)
    db.row_factory = sqlite3.Row
    rows = [dict(r) for r in db.execute(
        'SELECT user_unit_id, user_id, unit_id, leader_skill_id FROM user_units ORDER BY user_unit_id')]
    db.close()
    result = classify(rows, units, leader_skills)
    counts = {c: sum(1 for k, _, _ in result if k == c) for c in CATEGORIES}

    def name(r):
        mst = units.get(unit_species(r['unit_id']))
        return mst[UNIT_NAME] if mst else '?'
    print(f'{save}: {len(rows)} unit row(s); UnitMst {len(units)} units, LeaderSkillMst {len(leader_skills)} skills')
    for kind, r, want in result:
        if kind == 'repair':
            print(f'  repair     uu {r["user_unit_id"]:>6} unit {r["unit_id"]:>8} {name(r)!a}: 0 -> {want}')
        elif kind == 'preserved':
            print(f'  preserved  uu {r["user_unit_id"]:>6} unit {r["unit_id"]:>8} {name(r)!a}: stored'
                  f' {r["leader_skill_id"]}, UnitMst {want} (left as is)')
        elif kind == 'unknown':
            print(f'  unknown    uu {r["user_unit_id"]:>6} unit {r["unit_id"]:>8}: no UnitMst row (left as is)')
        elif kind == 'unresolved':
            print(f'  unresolved uu {r["user_unit_id"]:>6} unit {r["unit_id"]:>8} {name(r)!a}: UnitMst {want}'
                  ' is not in LeaderSkillMst (left as is)')
    print('summary: ' + ', '.join(f'{c} {counts[c]}' for c in CATEGORIES))

    report = {'save': str(save), 'counts': counts, 'applied': False, 'repaired': 0, 'backup': None,
              'rows': [{'category': k, 'user_unit_id': r['user_unit_id'], 'unit_id': str(r['unit_id']),
                        'stored': int(r['leader_skill_id'] or 0), 'unit_mst': w} for k, r, w in result]}
    repairs = [(want, r['user_unit_id']) for kind, r, want in result if kind == 'repair']
    if args.apply and repairs:
        stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        backup = args.backup_dir.resolve() / f'{save.stem}_before_leader_skill_repair_{stamp}.sqlite'
        backup_copy(save, backup)
        db = sqlite3.connect(save, timeout=30)
        with db:                              # one transaction: all or nothing
            changed = sum(db.execute('UPDATE user_units SET leader_skill_id=? WHERE user_unit_id=?'
                                     ' AND leader_skill_id=0', pair).rowcount for pair in repairs)
        db.close()
        report.update(applied=True, repaired=changed, backup=str(backup))
        print(f'applied: {changed} row(s) repaired; backup {backup}')
    elif args.apply:
        report['applied'] = True
        print('applied: nothing to repair')
    else:
        print(f'dry run: {len(repairs)} row(s) would be repaired; rerun with --apply to write them')
    if args.json:
        args.json.write_text(json.dumps(report, indent=1), encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main())
