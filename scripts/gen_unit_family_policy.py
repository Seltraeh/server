"""Generate the content provider's unit families (Phases C1, C2): owned only where verified.

    python scripts/gen_unit_family_policy.py --out POLICY.json [--report REPORT.json]
                                             [--phase c1|c2] [--root DIR]
                                             [--base deploy/system/content_provider.json]
                                             [--content deploy/game_content/content]

Phase C1 -- for every unit in UnitMst (deploy/mst/unit_mst.json) the client
requests, through GameScene::requestUnitFiles (docs/OFFLINE_CONTENT_PHASE_C1_PLAN.md):

  unit/img  unit_ills_full_<id>.png, unit_ills_battle_<id>.png,
            unit_ills_thum_<id>.png, unit_anime_<id>.png   (GameUtils::getUnitIllsImage,
                                                            getUnitAnmImage: from the id)
  unit/cgg  the file UnitMst names in anim_cgg (QqfI9mM4) -- not always <id>'s own
  unit/cgs  the files unit_cgs_mst (Kn51uR4Y) lists for the unit's actions

A unit is OWNED only if every one of those names is a regular file directly in
its folder and its basename occurs nowhere else under the content root (the
broker serves flat basenames; a second copy would be a collision).  Owned units
become exact scope paths plus one all-or-nothing bundle per unit, so the
provider never serves a frame layout without its texture.  Everything else
stays with the client's own (legacy) download path and is REPORTED -- missing
names, collisions, and the variant files a phase does not cover -- never
silently claimed.

Phase C2 (the default) appends the rest of the unit art the traced first
visits still downloaded (docs/OFFLINE_CONTENT_PHASE_C2.md), each producer
read in libgame.so (arm64):

  alt     unit_ills_{full,battle,thum}_<id>_2.png -- alternate art.
          GameUtils::getUnitCutinImage / getUnitIllsImage / getUnitListThum
          append "_<n>" when the image type is >= 2; the type is 2 only while
          the dictionary's alt-art bit (2pAyFjmZ) is set.  Owned for the
          UnitMst ids whose three files exist; one bundle per unit.
  dbb     unit_ills_thum_<id>_100.png and unit_ills_thum_<id>_2_100.png --
          getUnitListThum (0x117CBC4) appends "_100" when its DBB flag is set
          and DbbMstList::getDbbMstListByUnitId(id) (first OR second unit of a
          DbbMst pair) is non-empty.  One single-name bundle each.
  dbbart  the DbbMst row columns anime_img / cgg_file / atk_cgs_file (the
          combined-unit animation a DBB pair shows in battle; content/xbb).
          One bundle per row: texture and frame layout only together.
  ills    unit_ills_{full,battle,thum}_<id>.png of the UnitMst units C1 left
          incomplete: they animate through SuperAnim (content/unit_sam/
          unit_<id>/), which stays legacy, but their illustrations are plain
          images read like every other unit's.  One bundle per unit.

The base policy's own scope, aliases and bundles (the Town and Zeal fixture)
are kept; its comments are dropped (the output is generated data).  --phase c1
reproduces the Session 10 policy exactly; c2 only appends after it.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FOLDERS = ('unit/img', 'unit/cgg', 'unit/cgs')


def load_json_with_comments(path: Path) -> dict:
    text = path.read_text(encoding='utf-8-sig')
    text = re.sub(r'^\s*//.*$', '', text, flags=re.M)
    return json.loads(text)


def mst_rows(path: Path) -> list[dict]:
    data = json.loads(path.read_text(encoding='utf-8'))
    return data if isinstance(data, list) else next(iter(data.values()))


def unique_rel(where: dict, name: str, folder: str | None = None) -> tuple[str | None, str | None]:
    """(path, None) for the only copy of a flat name under the content root, else (None, why)."""
    paths = where.get(name.lower(), [])
    if not paths:
        return None, 'missing'
    if len(paths) > 1:
        return None, 'collision: ' + ' | '.join(paths)
    if folder is not None and paths[0] != f'{folder}/{name}':
        return None, f'not directly in {folder}: {paths[0]}'
    return paths[0], None


def c2_families(where: dict, units: list[dict], incomplete: dict) -> tuple[list, dict]:
    """[(bundle, [path, ...], kind)] for the Phase C2 unit art, and what stays legacy (see the docstring)."""
    ids = sorted({str(u['pn16CNah']) for u in units}, key=int)
    known = set(ids)
    dbb_rows = mst_rows(ROOT / 'deploy/mst/dbb_mst.json')
    families = []
    legacy = defaultdict(dict)    # kind -> {id: {name: why}}

    def take(kind: str, bundle: str, key: str, names: list[tuple[str, str | None]]) -> bool:
        found = [(n,) + unique_rel(where, n, folder) for n, folder in names]
        if all(rel for _n, rel, _why in found):
            families.append((bundle, [rel for _n, rel, _why in found], kind))
            return True
        legacy[kind][key] = {n: why for n, rel, why in found if not rel}
        return False

    alt = set()
    for uid in ids:
        names = [f'unit_ills_full_{uid}_2.png', f'unit_ills_battle_{uid}_2.png', f'unit_ills_thum_{uid}_2.png']
        if not any(n.lower() in where for n in names):
            continue                                # no alternate art for this unit: nothing to request
        if take('alt', f'unit_{uid}_alt', uid, [(n, 'unit/img') for n in names]):
            alt.add(uid)

    pairs = sorted({str(r.get(k, '')).strip() for r in dbb_rows for k in ('3gZAK2vU', 'zY9XKiSV')} - {''}, key=int)
    for uid in pairs:
        if uid not in known:
            legacy['dbb'][uid] = {'*': 'DbbMst unit id is not in UnitMst'}
            continue
        take('dbb', f'unit_{uid}_dbb_thum', uid, [(f'unit_ills_thum_{uid}_100.png', 'unit/img')])
        if uid in alt:
            take('dbb', f'unit_{uid}_alt_dbb_thum', uid + '_2', [(f'unit_ills_thum_{uid}_2_100.png', 'unit/img')])

    for row in dbb_rows:
        names = [str(row.get(k, '')).strip() for k in ('2EF0d6ue', 'QqfI9mM4', '0QxL2is7')]
        if any(names):
            take('dbbart', f'dbb_{row["XIvaD6Jp"]}', str(row['XIvaD6Jp']), [(n, None) for n in names if n])

    for uid in sorted(incomplete, key=int):
        take('ills', f'unit_{uid}_ills', uid,
             [(f'unit_ills_{kind}_{uid}.png', 'unit/img') for kind in ('full', 'battle', 'thum')])
    return families, legacy


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--report')
    ap.add_argument('--phase', choices=('c1', 'c2'), default='c2',
                    help='c1 reproduces the Session 10 policy; c2 appends the unit art variants')
    ap.add_argument('--base', default=str(ROOT / 'deploy/system/content_provider.json'))
    ap.add_argument('--content', default=str(ROOT / 'deploy/game_content/content'))
    ap.add_argument('--root', help='policy root to write (default: keep the base policy\'s)')
    args = ap.parse_args()
    content = Path(args.content)

    # Every basename under the content root, with where it occurs.
    where = defaultdict(list)
    for dirpath, _dirs, files in os.walk(content):
        rel_dir = Path(dirpath).relative_to(content).as_posix()
        for f in files:
            where[f.lower()].append(f'{rel_dir}/{f}' if rel_dir != '.' else f)

    units = mst_rows(ROOT / 'deploy/mst/unit_mst.json')
    cgs_rows = {r['pn16CNah']: r['Kn51uR4Y'] for r in mst_rows(ROOT / 'deploy/mst/unit_cgs_mst.json')}

    owned, incomplete, collided = {}, {}, {}
    missing_by_kind = Counter()
    for u in units:
        uid = str(u['pn16CNah'])
        names = [('unit/img', f'unit_ills_full_{uid}.png'), ('unit/img', f'unit_ills_battle_{uid}.png'),
                 ('unit/img', f'unit_ills_thum_{uid}.png'), ('unit/img', f'unit_anime_{uid}.png')]
        cgg = (u.get('QqfI9mM4') or '').strip()
        if cgg:
            names.append(('unit/cgg', cgg))
        for part in (cgs_rows.get(uid) or '').split('|'):
            if ':' in part:
                names.append(('unit/cgs', part.split(':', 1)[1].strip()))
        seen, unique = set(), []
        for folder, name in names:
            if name and name.lower() not in seen:
                seen.add(name.lower())
                unique.append((folder, name))
        missing, clash = [], []
        for folder, name in unique:
            paths = where.get(name.lower(), [])
            if f'{folder}/{name}' not in paths:
                missing.append(f'{folder}/{name}')
                missing_by_kind[re.sub(r'\d+', '<n>', name)] += 1
            elif len(paths) > 1:
                clash.append({'name': name, 'paths': paths})
        if missing:
            incomplete[uid] = missing
        elif clash:
            collided[uid] = clash
        else:
            owned[uid] = [f'{folder}/{name}' for folder, name in unique]

    c2, c2_legacy = c2_families(where, units, incomplete) if args.phase == 'c2' else ([], {})

    # Files in the unit folders that the phase leaves to the legacy path.
    family = set()
    for paths in owned.values():
        family.update(p.lower() for p in paths)
    for _bundle, paths, _kind in c2:
        family.update(p.lower() for p in paths)
    variants = Counter()
    for folder in FOLDERS:
        for f in os.listdir(content / folder):
            p = f'{folder}/{f}'
            if (content / p).is_file() and p.lower() not in family:
                variants[re.sub(r'\d+', '<n>', f)] += 1

    policy = load_json_with_comments(Path(args.base))
    if args.root:
        policy['root'] = args.root
    scope = list(policy.get('scope', []))
    bundles = dict(policy.get('bundles', {}))
    present = set(p.lower() for p in scope)
    shared = 0
    for uid, paths in sorted(owned.items(), key=lambda kv: int(kv[0])):
        for p in paths:
            if p.lower() not in present:
                scope.append(p)
                present.add(p.lower())
            else:
                shared += 1
        bundles.setdefault(f'unit_{uid}', [p.rsplit('/', 1)[1] for p in paths])
    # C2 only appends: the C1 scope and bundles stay an exact prefix.
    for bundle, paths, _kind in c2:
        assert bundle not in bundles, f'bundle name {bundle} already used'
        for p in paths:
            assert p.lower() not in present, f'{p} is already in scope'
            scope.append(p)
            present.add(p.lower())
        bundles[bundle] = [p.rsplit('/', 1)[1] for p in paths]
    policy['scope'] = scope
    policy['bundles'] = bundles
    Path(args.out).write_text(json.dumps(policy, indent=1, ensure_ascii=False), encoding='utf-8')

    report = {
        'units_in_unit_mst': len(units),
        'owned_units': len(owned),
        'owned_files': sum(1 for p in scope if p.lower().startswith(('unit/img/', 'unit/cgg/', 'unit/cgs/'))),
        'files_shared_between_units': shared,
        'incomplete_units': len(incomplete),
        'missing_names_by_pattern': dict(missing_by_kind.most_common()),
        'collided_units': len(collided),
        'legacy_variant_files_by_pattern': dict(variants.most_common()),
        'incomplete': incomplete,
        'collided': collided,
    }
    if args.phase == 'c2':
        kinds = Counter(kind for _b, _p, kind in c2)
        files = Counter()
        for _b, paths, kind in c2:
            files[kind] += len(paths)
        report['c2'] = {
            'bundles_by_kind': dict(kinds),
            'files_by_kind': dict(files),
            'files': sum(files.values()),
            'policy_names': len(scope),
            'legacy_by_kind': {k: v for k, v in c2_legacy.items()},
        }
    if args.report:
        Path(args.report).write_text(json.dumps(report, indent=1), encoding='utf-8')
    print(f'{len(units)} units in UnitMst: {len(owned)} owned ({report["owned_files"]} files, '
          f'{shared} shared between units), {len(incomplete)} incomplete, {len(collided)} with a basename '
          f'collision; legacy variant files: {sum(variants.values())}')
    print('missing names by pattern:', dict(missing_by_kind.most_common(8)))
    print('legacy variants by pattern:', dict(variants.most_common(8)))
    if args.phase == 'c2':
        c = report['c2']
        print(f'C2: {c["files"]} more files in {len(c2)} bundles {c["bundles_by_kind"]} '
              f'(files {c["files_by_kind"]}); policy scope now {c["policy_names"]} entries')
        print('C2 left legacy:', {k: len(v) for k, v in c2_legacy.items()})
    return 0


if __name__ == '__main__':
    sys.exit(main())
