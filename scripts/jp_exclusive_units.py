"""Install the 107 JP-exclusive units (#15 c): the JP-only units with no Global counterpart in the game.

    python scripts/jp_exclusive_units.py check   --root ROOT
    python scripts/jp_exclusive_units.py install --root ROOT [--backup-dir DIR]   # idempotent; deploy/ needs --backup-dir

The 99 other JP-only ids on the wiki ARE in the game already, under their Global counterpart's id
(scripts/data/jp_units/jp_only_roster.json, read from each JP page's "Global counterpart" line), so they are not
duplicated.  Data: scripts/data/jp_units/exclusive/ (tools/jp_datamine/build_jp_units.py from the JP datamine,
English text AUTHORED in text_en.json).  install:

  * tables      unit, skill, skill level, leader/extra skill, evolution, CGS and comment rows merged into
                ROOT/mst and staged as the download versions in TABLES (above everything deploy stages);
                the units into ROOT/archive/unit.json -- eu_units.install_tables / install_archive;
  * text        the seven dictionaries reissued with every name and description the units use (most of their
                skill rows were already in the client's tables, with no text) and F_RESOURCE_MST 3281;
  * art         from the wiki's files for these units (tools/jp_art/raw, or BF_JP_ART): thumbnail as published,
                full art as published or, for the nine small uploads, scaled up (unit_sprite_stand_ins.full_art_png;
                the build computes the screen crops on the same bytes); the battle portrait from the thumbnail (as the EU stand-ins); the battle sprite an
                AUTHORED stand-in (scripts/unit_sprite_stand_ins.py): animated from the wiki's idle/attack/move
                GIFs where the wiki has them (60 units), else a static figure cut from the full art (47).
"""
from __future__ import annotations

import argparse
import io
import json
import os
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import eu_units as eu  # noqa: E402
import jp_units as jp  # noqa: E402
import unit_sprite_stand_ins as sprites  # noqa: E402

REPO = HERE.parent
DATA = HERE / 'data' / 'jp_units' / 'exclusive'
ART = Path(os.environ.get('BF_JP_ART', REPO / 'tools' / 'jp_art' / 'raw'))

# table -> (deploy mst file, file key, encode key, id key, staged version): above deploy's November 2 versions
# (F_UNIT_MST 1090 #14, the EU install's others), skipping the 1091/1538/.../3280 the round-8 client run v22 cached
# from an earlier build of these rows (a cached version is never fetched again)
# F_UNIT_MST 1093 (not 1092, which went live first): 22 Home crops re-authored (home_crops.json) after Evan's report
# F_UNIT_EVO_MST 601 (not 600, live first): the 9 evolutions the wiki names for units already in the game (Crimson
# Scout Reda 10716->10717, High Commander Rahnas 61016->61017, MEIKO / KAITO / Samatha / Rogi / Ramu), 2026-11-03
TABLES = {
    'F_UNIT_MST': ('unit_mst.json', '2r9cNSdt', '7nL1WTUb', 'pn16CNah', 1093),
    'F_SKILL_MST': ('skill_mst.json', 'wkCyV73D', 'JA03wvHG', 'nj9Lw7mV', 1539),
    'F_SKILL_LEVEL_MST': ('skill_level_mst.json', 'zLIvD5o2', 'rF97pVdT', 'nj9Lw7mV', 1490),
    'F_LEADER_SKILL_MST': ('leader_skill_mst.json', '4dE8UKcw', 'HGYZ1Ag5', 'oS3kTZ2W', 754),
    'F_EXTRA_PASSIVE_SKILL_MST': ('extra_passive_skill_mst.json', 'kP4pTJ7n', 'PDGK5sk3', 'cP83zNsv', 774),
    'F_UNIT_EVO_MST': ('unit_evo_mst.json', 'Ef6oG0mz', 'R9IryC5v', 'pn16CNah', 601),
    'F_UNIT_CGS_MST': ('unit_cgs_mst.json', '6rjmY7tV', 'Y7ezNyn2', 'pn16CNah', 814),
    'F_UNIT_COMMENT_MST': ('unit_comment_mst.json', '2npmC9Nw', 'Apc5eIw6', 'pn16CNah', 709),
}
RESOURCE_VERSION = 3281


def load(name):
    return json.loads((DATA / name).read_text(encoding='utf-8'))


def battle_portrait(thumb: Path) -> bytes:
    """unit_ills_battle_<id>.png (108x108): the thumbnail's picture inside its element frame, scaled up."""
    im = Image.open(thumb).convert('RGBA')
    w, h = im.size
    inner = im.crop((round(w * 0.07), round(h * 0.07), round(w * 0.93), round(h * 0.93)))
    buf = io.BytesIO()
    inner.resize((108, 108), Image.LANCZOS).save(buf, format='PNG', optimize=False)
    return buf.getvalue()


def sprite(uid: int, totals: dict) -> tuple[bytes, bytes, dict, str]:
    gif = {k: ART / f'Unit_ills_{n}_{uid}.gif' for k, n in (('idle', 'anime'), ('atk', 'atk'), ('move', 'move'))}
    if gif['idle'].is_file() and gif['atk'].is_file():
        png, cgg, cgs, _ = sprites.animated({k: p if p.is_file() else None for k, p in gif.items()}, totals)
        return png, cgg, cgs, 'animated'
    png, cgg, cgs, _ = sprites.static(io.BytesIO(sprites.full_art_png(ART / f'Unit_ills_full_{uid}.png')), totals)
    return png, cgg, cgs, 'static'


def install(root: Path, backup: Path | None, check_only=False) -> tuple[list[str], list[str]]:
    log, problems = [], []
    problems += eu.install_tables(root, DATA, TABLES, log, backup, check_only)
    problems += eu.install_archive(root, load('archive_unit.json'), log, backup, check_only, 'JP-exclusive')
    problems += eu.reissue_dictionaries(root, load('text.json'), RESOURCE_VERSION, log, backup, check_only,
                                        reserved=jp.RESERVED, force=bool(log))
    content = root / 'game_content' / 'content' / 'unit'
    img, cgg_dir, cgs_dir = content / 'img', content / 'cgg', content / 'cgs'
    for a in load('animations.json'):
        uid = a['id']
        files = {img / f'unit_ills_thum_{uid}.png': None, img / f'unit_ills_full_{uid}.png': None,
                 img / f'unit_ills_battle_{uid}.png': None, img / f'unit_anime_{uid}.png': None,
                 cgg_dir / f'unit_cgg_{uid}.csv': None}
        files.update({cgs_dir / f'unit_{k}_cgs_{uid}.csv': None for k in ('idle', 'move', 'atk')})
        if check_only:
            problems += [f'{p.name} missing' for p in files if not p.is_file()]
            continue
        eu.put(img / f'unit_ills_thum_{uid}.png', (ART / f'Unit_ills_thum_{uid}.png').read_bytes(), log, backup, root)
        eu.put(img / f'unit_ills_full_{uid}.png', sprites.full_art_png(ART / f'Unit_ills_full_{uid}.png'), log, backup, root)
        eu.put(img / f'unit_ills_battle_{uid}.png', battle_portrait(ART / f'Unit_ills_thum_{uid}.png'), log, backup, root)
        png, cgg, cgs, _ = sprite(uid, {'idle': a['idle'], 'atk': a['atk'], 'move': a['move']})
        eu.put(img / f'unit_anime_{uid}.png', png, log, backup, root)
        eu.put(cgg_dir / f'unit_cgg_{uid}.csv', cgg, log, backup, root)
        for kind, data in cgs.items():
            eu.put(cgs_dir / f'unit_{kind}_cgs_{uid}.csv', data, log, backup, root)
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
        print('complete' if not problems else f'{len(problems)} problem(s):\n  ' + '\n  '.join(problems[:30]))
        return 1 if problems else 0
    if root == (REPO / 'deploy').resolve() and a.backup_dir is None:
        sys.exit('installing into deploy/ needs --backup-dir (every replaced file is copied there first)')
    log, problems = install(root, a.backup_dir.resolve() if a.backup_dir else None)
    print(f'{len(log)} file(s) written' + (f'; backups in {a.backup_dir}' if a.backup_dir else '')
          + (f'; {len(problems)} problem(s): {problems[:5]}' if problems else ''))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
