"""#15 (b): make the 29 JP-only units already in the client's tables grantable and readable, plus the 8 units
whose English name is missing from the units dictionary.

    python scripts/jp_units.py check   --root ROOT                       # exit 1 unless ROOT has it all
    python scripts/jp_units.py install --root ROOT [--backup-dir DIR]    # idempotent; deploy/ needs --backup-dir

ROOT is deploy-shaped (mst/, archive/, game_content/), e.g. a QA mirror or deploy itself.  The playability audit
(scripts/audit_unit_playability.py, docs/UNIT_PLAYABILITY_AUDIT.md) found these INCOMPLETE: their unit_mst, skill,
evolution and art rows all exist (they came with the Japanese MSTs), but
  * no archive record, so the server cannot grant them (fromArchivedUnit has nothing to build from);
  * no MST_UNIT_<id>_NAME line in the units dictionary, which is what the screens draw (6 rows carry only the
    placeholder key, the rest a Japanese literal), and Japanese or placeholder skill / leader / extra text.
install
  * archive  one record per unit from its own unit_mst row, by the column map tools/gen_unit_archive.py proved
             byte-identical on the original 207 rows (bb_id / sbb_id only when they resolve in skill_mst);
  * text     scripts/data/jp_units/units.json (names) and text.json (skills), appended to the seven dictionaries
             with F_RESOURCE_MST staged as RESOURCE_VERSION (eu_units.reissue_dictionaries: a key already present
             is never touched, so text shared with a Global unit stays Global's).  The 8 name-less units (seven
             Mystery Frogs, Wonder Mirror Idol) take their own unit_mst English name.
Provenance: 22 names are the wiki's `altname`, 2 its title, 6 AUTHORED (the MEIKO and KAITO stages, whose Global
namesakes are different units: "Not to be confused with Global counterpart").  Every skill text is an AUTHORED
translation of the unit's Japanese row (for the six placeholder units, of its wiki page), in Global's wording;
four collab burst names the wiki gives as "NA (...)" are AUTHORED too.
Not covered: 50876 (its leader skill row is missing from F_LEADER_SKILL_MST) and 9850167 (the guest Juno-Seto,
not an owned unit).
Versions: F_RESOURCE_MST 3279, above #22-3's staged 3278.  leaderSkillDescription 500 is #22-3's reissue, so it is
skipped on a root without it; install #22-3 FIRST (the release order), or #22-3 must be rebuilt above 3279.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import eu_units as eu  # noqa: E402

DATA = HERE / 'data' / 'jp_units'
RESOURCE_VERSION = 3279
RESERVED = {'sgtext_leaderSkillDescription': {498, 500}}
NAME_ONLY = (730302, 730312, 730322, 730332, 730342, 730352, 730362, 750122)
# unit_mst column per archive field (tools/gen_unit_archive.py COL, self-checked against the original rows)
COL = {'rarity': '7ofj5xa1', 'element': 'iNy0ZU5M', 'effect_frame': 'eyUo6a8c', 'damage_frame': '6Aou5M9r',
       'drop_check_count': 'n9h7p02P', 'move_speed': 'J2hPXGo5', 'attack_move_type': 'mv4o39Uz',
       'skill_move_type': 'XE7Yi5c3', 'hp': 'UZ1Bj7w2', 'atk': 'i9Tn7kYr', 'def': 'q78KoWsg', 'rec': '92ij6UGB',
       'bb': 'nj9Lw7mV', 'sbb': 'iEFZ6H19'}


def as_int(v):
    try:
        return int(str(v).strip())
    except ValueError:
        return 0


def frames(v):
    return re.sub(r'[\r\n]+', '', str(v or ''))


def archive_record(uid: int, name: str, row: dict, skills: set[str]) -> dict:
    """The same fields, order and rules as every record tools/gen_unit_archive.py wrote."""
    real = lambda s: s if s in skills else ''   # noqa: E731
    return {
        'id': uid, 'name': name,
        'rarity': as_int(row.get(COL['rarity'])), 'element': as_int(row.get(COL['element'])),
        'effect_frame': frames(row.get(COL['effect_frame'])), 'damage_frame': frames(row.get(COL['damage_frame'])),
        'drop_check_count': as_int(row.get(COL['drop_check_count'])), 'move_speed': as_int(row.get(COL['move_speed'])),
        'attack_move_type': as_int(row.get(COL['attack_move_type'])), 'back_move_type': 1, 'after_image': 1,
        'skill_move_type': as_int(row.get(COL['skill_move_type'])), 'cursor_disp_pos': '0,0',
        'stats': [{k: as_int(row.get(COL[k])) for k in ('hp', 'atk', 'def', 'rec')}],
        'bb_id': real(str(row.get(COL['bb'], '')).strip()), 'sbb_id': real(str(row.get(COL['sbb'], '')).strip()),
    }


def table(root: Path, name: str) -> list[dict]:
    return next(iter(json.loads((root / 'mst' / name).read_text(encoding='utf-8')).values()))


def text_for(root: Path) -> dict:
    units = json.loads((DATA / 'units.json').read_text(encoding='utf-8'))
    mst = {int(r['pn16CNah']): r for r in table(root, 'unit_mst.json')}
    names = {f'MST_UNIT_{u["id"]}_NAME': u['name'] for u in units}
    names.update({f'MST_UNIT_{uid}_NAME': mst[uid]['utP1c0CD'] for uid in NAME_ONLY})
    return {'units': names, **json.loads((DATA / 'text.json').read_text(encoding='utf-8'))}


def install(root: Path, backup: Path | None, check_only=False) -> tuple[list[str], list[str]]:
    log, problems = [], []
    units = json.loads((DATA / 'units.json').read_text(encoding='utf-8'))
    mst = {int(r['pn16CNah']): r for r in table(root, 'unit_mst.json')}
    skills = {r['nj9Lw7mV'] for r in table(root, 'skill_mst.json')}
    missing = [u['id'] for u in units if u['id'] not in mst]
    if missing:
        raise SystemExit(f'not in {root}/mst/unit_mst.json: {missing}')
    archive = root / 'archive' / 'unit.json'
    araw = archive.read_bytes()
    adoc, adump = eu.layout(araw)
    mine = {u['id']: archive_record(u['id'], u['name'], mst[u['id']], skills) for u in units}
    merged = [mine.get(u['id'], u) for u in adoc] + [u for u in mine.values() if u['id'] not in {x['id'] for x in adoc}]
    if merged != adoc:
        if check_only:
            problems.append('archive/unit.json: JP records missing or different')
        else:
            eu.put(archive, adump(merged), log, backup, root)
    problems += eu.reissue_dictionaries(root, text_for(root), RESOURCE_VERSION, log, backup, check_only,
                                        reserved=RESERVED)
    return log, problems


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('action', choices=('check', 'install'))
    ap.add_argument('--root', type=Path, required=True)
    ap.add_argument('--backup-dir', type=Path)
    a = ap.parse_args(argv)
    root = a.root.resolve()
    if a.action == 'install' and root == (HERE.parent / 'deploy').resolve() and a.backup_dir is None:
        raise SystemExit('installing into deploy/ needs --backup-dir')
    log, problems = install(root, a.backup_dir, check_only=a.action == 'check')
    for line in log:
        print('wrote', line)
    for p in problems:
        print('MISSING', p)
    print('ok' if not problems else f'{len(problems)} problem(s)')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
