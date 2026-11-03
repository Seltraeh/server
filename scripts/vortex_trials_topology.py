"""#12: put Winter Paradise where the Vortex can draw it.

    python scripts/vortex_trials_topology.py check   --root ROOT
    python scripts/vortex_trials_topology.py install --root ROOT [--backup-dir DIR]   # idempotent; deploy/ needs it

Its dungeon 80013400 (mission 83013400, scripts/gen_vortex_trials.py) sits directly in area 100003 "Vortex Trials",
which the client draws as a list of CHILD AREAS only, so the fight had no tile (client run v21).  The table already
holds an area for it: 80013400 "Winter Paradise Day 1" (land 99, plate sp_quest_banner_winter2018_paradise.png, no
parent), one of 21 day-areas left from the 2018 run.  So:

  * area 80013400   parent 100003, name "Winter Paradise" (ROOT/mst/area_mst.json; the client takes the area table
                    from UserInfo 3SG2wX0R, a full replace, so no F_AREA_MST download is staged);
  * dungeon 80013400 area 100003 -> 80013400 (ROOT/mst/dungeon_mst.json and F_DUNGEON_MST, which reaches the client
                    only over the version/download channel: staged as DUNGEON_VERSION).

The area is above the Vortex id range; the server reaches it through the Vortex Trials subtree
(ServerCache::m_vortexTrialAreas, 2026-11-03), and PermitPlace advertises a subtree area only when a dungeon under
it has a battle.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import eu_units as eu  # noqa: E402

REPO = HERE.parent
AREA_CHANGES = {'80013400': {'sHMai2ZG': '100003', 'V84mzqoX': 'Winter Paradise'}}
DUNGEON_CHANGES = {'80013400': {'VjCY7rX4': '80013400'}}
DUNGEON_VERSION = 1004
DUNGEON_TABLE = ('F_DUNGEON_MST', 'dungeon_mst.json', 'B57MJYoF', 'w5W8RPSG', 'MHx05sXt')


def changed_rows(root: Path, fname: str, idk: str, changes: dict) -> tuple[list, list]:
    """(the table's rows, the rows these changes produce)."""
    (_, rows), = json.loads((root / 'mst' / fname).read_text(encoding='utf-8')).items()
    by_id = {r[idk]: r for r in rows}
    missing = [i for i in changes if i not in by_id]
    if missing:
        raise SystemExit(f'{fname}: no row {missing}')
    return rows, [dict(by_id[i], **c) for i, c in changes.items()]


def install(root: Path, backup: Path | None, check_only=False) -> tuple[list[str], list[str]]:
    log, problems = [], []
    # the area: server table only
    raw = (root / 'mst' / 'area_mst.json').read_bytes()
    doc, dump = eu.layout(raw)
    (wrapper, rows), = doc.items()
    _, new = changed_rows(root, 'area_mst.json', 'VjCY7rX4', AREA_CHANGES)
    merged, n = eu.merged_rows(rows, new, 'VjCY7rX4')
    if check_only:
        if n:
            problems.append(f'area_mst.json: {n} row(s) not moved')
    else:
        eu.put(root / 'mst' / 'area_mst.json', dump({wrapper: merged}), log, backup, root)
    # the dungeon: server table + the client's download
    table, fname, fkey, ekey, idk = DUNGEON_TABLE
    _, new = changed_rows(root, fname, idk, DUNGEON_CHANGES)
    data = root / '.vortex_trials_topology'
    try:
        data.mkdir(exist_ok=True)
        (data / fname).write_text(json.dumps(new, ensure_ascii=False), encoding='utf-8')
        problems += eu.install_tables(root, data, {table: (fname, fkey, ekey, idk, DUNGEON_VERSION)}, log, backup,
                                      check_only)
    finally:
        (data / fname).unlink(missing_ok=True)
        data.rmdir()
    return log, problems


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('command', choices=['check', 'install'])
    ap.add_argument('--root', type=Path, required=True)
    ap.add_argument('--backup-dir', type=Path)
    a = ap.parse_args(argv)
    root = a.root.resolve()
    if a.command == 'check':
        _, problems = install(root, None, check_only=True)
        print('complete' if not problems else f'{len(problems)} problem(s):\n  ' + '\n  '.join(problems))
        return 1 if problems else 0
    if root == (REPO / 'deploy').resolve() and a.backup_dir is None:
        sys.exit('installing into deploy/ needs --backup-dir (every replaced file is copied there first)')
    log, problems = install(root, a.backup_dir.resolve() if a.backup_dir else None)
    print(f'{len(log)} file(s) written' + (f'; problems: {problems}' if problems else ''))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
