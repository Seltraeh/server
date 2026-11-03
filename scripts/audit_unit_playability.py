"""#15: is every unit playable?  The wiki's whole roster (scripts/data/wiki_units.json) against what this server and
its client downloads actually hold.

    python scripts/audit_unit_playability.py [--root deploy] [--md docs/UNIT_PLAYABILITY_AUDIT.md]
                                             [--csv docs/unit_playability.csv] [--assets FILE]

Read-only.  A unit is PLAYABLE when every piece the game needs to own, show and field it is present:
  * data   its unit_mst row (the client lists it and the server serves it), an archive record (the server can
           grant it: summons, presents, captures), its own name line in the newest sgtext_units dictionary, a
           unit-comment row, and every skill id its row names (BB / SBB / UBB in F_SKILL_MST, leader skill,
           extra skill) plus each evolution target it names;
  * art    unit_ills_thum / unit_ills_battle / unit_ills_full / unit_anime (img), the part layout its row names
           (cgg) and every animation script its F_UNIT_CGS_MST row names (idle, move, attack, skill; shared
           files count, e.g. the metal units play 60132's).  A file counts as present in game_content/content
           or bundled in the client package (Assets/Resources, 25 starter units).
A SuperAnim unit (its row names unit_anime_<id>.sam for the part layout and every cgs slot) is drawn from
unit_sam/unit_<id>/unit_anime_<id>.sam and that .sam's page images: the client queues every resource whose path holds
"/unit_sam/unit_<id>" (x86 0x8265B0) and caches by file name, so either copy of the .sam counts (Korzan and Haile
also ship one under unit/cgs).  Client run v21 (November 2) drew Megurine Luka 60584, Red Hood Ciara 810104 and
Snow Queen Eva 850116 this way, with their Brave Burst cut-ins.
"Stand-in sprite" marks the EU units whose battle sheet is the authored static one (deploy/game_content/eu_parts
holds the real sheet, waiting for a part layout).  Where the wiki has the unit too, the audit also reports
what the wiki has and our row lacks (an SBB, UBB, extra skill or evolution) and rarity disagreements.

The wiki side: every main-namespace page transcluding Template:Unit (2,490 ids; `unreleased = true` marks units
that never reached Global).  Source revisions are in the JSON (revid per unit).  A JP-only id whose page names a
Global counterpart that is in the game (scripts/data/jp_units/jp_only_roster.json, 99 ids) is COUNTERPART: the
same unit is playable under the Global id, so the JP id is not built twice.
"""
from __future__ import annotations

import argparse
import base64
import csv
import io
import json
import re
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WIKI = ROOT / 'scripts' / 'data' / 'wiki_units.json'
ROSTER = ROOT / 'scripts' / 'data' / 'jp_units' / 'jp_only_roster.json'
ART = ('unit_ills_thum', 'unit_ills_battle', 'unit_ills_full', 'unit_anime')
CGS_SLOTS = {'1': 'idle cgs', '2': 'move cgs', '3': 'atk cgs', '4': 'skill cgs'}
BUNDLE = ROOT.parent / 'DecompFrontier-Installer' / 'build' / 'BraveFrontierAppxClient' / 'Assets' / 'Resources'
RARITY = {'8': 'Omni'}


def table(root, name):
    return next(iter(json.loads((root / 'mst' / name).read_text(encoding='utf-8')).values()))


def newest(folder: Path, stem: str) -> Path | None:
    zips = sorted(folder.glob(f'{stem}_*.zip'), key=lambda p: int(re.search(r'_(\d+)\.zip$', p.name).group(1)))
    return zips[-1] if zips else None


def dictionary_keys(path: Path | None) -> set[str]:
    if path is None:
        return set()
    z = zipfile.ZipFile(path)
    text = base64.b64decode(''.join(z.read(z.namelist()[0]).decode().split())).decode('utf-8', 'replace')
    return {line.split('^', 1)[0] for line in text.split('\n')}


def audit(root: Path, wiki_path: Path = WIKI, bundle: Path = BUNDLE):
    wiki = {u['id']: u for u in json.loads(wiki_path.read_text(encoding='utf-8'))['units']}
    mst = {int(r['pn16CNah']): r for r in table(root, 'unit_mst.json')}
    archive = {u['id'] for u in json.loads((root / 'archive' / 'unit.json').read_text(encoding='utf-8'))}
    skills = {r['nj9Lw7mV'] for r in table(root, 'skill_mst.json')}
    leaders = {r['oS3kTZ2W'] for r in table(root, 'leader_skill_mst.json')}
    extras = {r['cP83zNsv'] for r in table(root, 'extra_passive_skill_mst.json')}
    comments = {int(r['pn16CNah']) for r in table(root, 'unit_comment_mst.json')}
    evo = defaultdict(set)
    for r in table(root, 'unit_evo_mst.json'):
        evo[int(r['pn16CNah'])].add(int(r['74VFwuTd']))
    content = root / 'game_content' / 'content'
    names = dictionary_keys(newest(content / 'sgtext', 'sgtext_units'))
    files = {p.name for sub in ('img', 'cgg', 'cgs') for p in (content / 'unit' / sub).glob('*') if p.is_file()}
    files |= {p.name for p in (content / 'unit_sam').rglob('*.png')}      # SuperAnim parts, unit_sam/unit_<id>/
    counterparts = ({int(k): v for k, v in json.loads(ROSTER.read_text(encoding='utf-8'))['counterparts'].items()}
                    if ROSTER.is_file() else {})
    if bundle.is_dir():
        files |= {p.name for p in bundle.glob('unit_*')}
    cgs = {int(r['pn16CNah']): dict(part.split(':', 1) for part in r['Kn51uR4Y'].split('|'))
           for r in table(root, 'unit_cgs_mst.json')}
    eu_parts = {int(m.group(1)) for p in (root / 'game_content' / 'eu_parts').glob('*.png')
                if (m := re.match(r'unit_anime_(\d+)\.eu_parts\.png$', p.name))}

    rows = []
    for uid in sorted(set(wiki) | set(mst)):
        if uid in (1,):                       # unit 1 is a system row, not a unit
            continue
        w, m = wiki.get(uid), mst.get(uid)
        missing, notes = [], []
        if m is None:
            missing.append('unit_mst row')
        if uid not in archive:
            missing.append('archive record')
        if f'MST_UNIT_{uid}_NAME' not in names:
            missing.append('name text')
        if uid not in comments:
            missing.append('unit comment')
        if m is not None:
            for col, label, pool in (('nj9Lw7mV', 'BB', skills), ('iEFZ6H19', 'SBB', skills),
                                     ('cb0P4mp1', 'UBB', skills), ('oS3kTZ2W', 'leader skill', leaders),
                                     ('cP83zNsv', 'extra skill', extras)):
                sid = m.get(col, '0') or '0'
                if sid != '0' and sid not in pool:
                    missing.append(f'{label} row {sid}')
            bad_evo = [t for t in evo.get(uid, ()) if t not in mst]
            if bad_evo:
                missing.append(f'evolution target {bad_evo}')
        art = {a: f'{a}_{uid}.png' in files for a in ART}
        sam = next((p for p in (content / 'unit_sam' / f'unit_{uid}' / f'unit_anime_{uid}.sam',
                                content / 'unit' / 'cgs' / f'unit_anime_{uid}.sam') if p.is_file()), None)
        if not art['unit_anime'] and sam is not None:
            # a SuperAnim unit: the .sam's image table is its dependency list, and the .sam is its part layout and
            # every animation script (the row names unit_anime_<id>.sam for the cgg and all cgs slots)
            deps = set(re.findall(rb'unit_anime_[0-9A-Za-z_]+\.png', sam.read_bytes()))
            art['unit_anime'] = bool(deps) and all(d.decode() in files for d in deps)
            if art['unit_anime']:
                files.add(sam.name)
                notes.append('SuperAnim sprite')
        cgg_name = (m or {}).get('QqfI9mM4') or f'unit_cgg_{uid}.csv'
        art['unit_cgg'] = cgg_name in files
        mapped = cgs.get(uid) or {k: f'unit_{n.split()[0]}_cgs_{uid}.csv' for k, n in CGS_SLOTS.items() if k != '4'}
        for slot, label in CGS_SLOTS.items():
            if slot in mapped:
                art[label] = mapped[slot] in files
        missing += [k for k, ok in art.items() if not ok]
        if uid in eu_parts:
            notes.append('stand-in sprite')
        if w and m:
            for key, col, label in (('sbb', 'iEFZ6H19', 'SBB'), ('ubb', 'cb0P4mp1', 'UBB'), ('es', 'cP83zNsv', 'extra skill')):
                if w.get(key, '--').strip('- ') and (m.get(col, '0') or '0') == '0':
                    notes.append(f'wiki has {label} {w[key]!r}')
            if w.get('evointo', '').isdigit() and int(w['evointo']) not in evo.get(uid, set()):
                notes.append(f'wiki evolves into {w["evointo"]}')
            ours = RARITY.get(m.get('7ofj5xa1'), m.get('7ofj5xa1'))
            if w.get('rarity') and w['rarity'] != ours:
                notes.append(f'rarity wiki {w["rarity"]} vs ours {ours}')
        if m is None and uid in counterparts:
            notes.append(f"in the game as {counterparts[uid]['global_id']} {counterparts[uid]['global_title']}")
        region = ('EU' if 710000 <= uid < 900000 and not w else
                  'JP-only' if w and w.get('unreleased') == 'true' else
                  'Global' if w else 'not on wiki')
        rows.append({
            'id': uid, 'name': (m or {}).get('utP1c0CD') or (w or {}).get('title', ''),
            'wiki_title': (w or {}).get('title', ''), 'wiki_revid': (w or {}).get('revid', ''),
            'region': region, 'rarity': (w or {}).get('rarity') or RARITY.get((m or {}).get('7ofj5xa1'), (m or {}).get('7ofj5xa1', '')),
            'element': (w or {}).get('element', ''),
            'verdict': ('PLAYABLE' if not missing else 'COUNTERPART' if m is None and uid in counterparts
                        else 'NOT IN CLIENT' if m is None else 'INCOMPLETE'),
            'missing': '; '.join(missing), 'notes': '; '.join(notes),
            **{k: ('yes' if art[k] else 'NO') if k in art else '' for k in (*ART, 'unit_cgg', *CGS_SLOTS.values())},
            '_cgs': mapped, '_cgg': cgg_name,
        })
    return rows


def missing_assets(rows):
    out = []
    for r in rows:
        if r['verdict'] == 'COUNTERPART':          # playable under the Global id: nothing to source
            continue
        for a in ART:
            if r[a] == 'NO':
                out.append(f'unit/img/{a}_{r["id"]}.png')
        if r['unit_cgg'] == 'NO':
            out.append(f'unit/cgg/{r["_cgg"]}')
        for slot, label in CGS_SLOTS.items():
            if r[label] == 'NO':
                out.append(f'unit/cgs/{r["_cgs"][slot]}')
    return sorted(set(out))


def markdown(rows):
    by = Counter((r['region'], r['verdict']) for r in rows)
    regions = ['Global', 'JP-only', 'EU', 'not on wiki']
    verdicts = ['PLAYABLE', 'COUNTERPART', 'INCOMPLETE', 'NOT IN CLIENT']
    lines = ['# Unit playability audit', '',
             'Generated by `scripts/audit_unit_playability.py` (read-only) from `scripts/data/wiki_units.json` (the BF '
             'Global wiki: every page using the unit infobox, revid per unit) and `deploy/`. Full table: '
             '[unit_playability.csv](unit_playability.csv). Do not edit by hand; rerun the script.', '',
             '| Region | ' + ' | '.join(verdicts) + ' | Total |', '|---|' + '---:|' * (len(verdicts) + 1)]
    for reg in regions:
        n = [by[(reg, v)] for v in verdicts]
        lines.append(f'| {reg} | ' + ' | '.join(map(str, n)) + f' | {sum(n)} |')
    tot = [sum(by[(reg, v)] for reg in regions) for v in verdicts]
    lines += [f'| **All** | ' + ' | '.join(f'**{t}**' for t in tot) + f' | **{sum(tot)}** |', '',
              '- **PLAYABLE**: data and art complete. **INCOMPLETE**: in the client\'s unit table, but something is '
              'missing (listed per unit). **NOT IN CLIENT**: no `unit_mst` row at all, so the client cannot show it.',
              '- **COUNTERPART**: a JP-only id whose wiki page names its Global counterpart, and that counterpart is in '
              'the game: the same unit is playable under the Global id (listed below).',
              '- "JP-only" is the wiki\'s own `unreleased = true` flag. "EU" are the 224 EU units installed on '
              'November 1 (the Global wiki has no pages for them).', '']
    gaps = Counter()
    for r in rows:
        if r['verdict'] == 'COUNTERPART':
            continue
        for part in filter(None, r['missing'].split('; ')):
            gaps[re.sub(r' (row )?\d+$| \[.*\]$', '', part)] += 1
    lines += ['## What is missing, by piece (COUNTERPART ids excluded)', '', '| Piece | Units |', '|---|---:|']
    lines += [f'| {k} | {v} |' for k, v in gaps.most_common()]
    lines.append('')
    notes = Counter()
    for r in rows:
        for part in filter(None, r['notes'].split('; ')):
            notes[re.sub(r" '.*'$| \d+$| wiki \S+ vs ours \S+$", '', part)] += 1
    lines += ['## Where the wiki and our tables disagree (units both have)', '', '| Note | Units |', '|---|---:|']
    lines += [f'| {k} | {v} |' for k, v in notes.most_common()]
    lines.append('')
    sel = [r for r in rows if r['verdict'] == 'COUNTERPART']
    lines += [f'## COUNTERPART ({len(sel)})', '', '| JP id | JP name | rarity | in the game as |', '|---:|---|---|---|']
    lines += [f"| {r['id']} | {r['wiki_title']} | {r['rarity']} | {r['notes'].split('in the game as ', 1)[-1]} |"
              for r in sel]
    lines.append('')
    for verdict in ('INCOMPLETE', 'NOT IN CLIENT'):
        sel = [r for r in rows if r['verdict'] == verdict]
        lines += [f'## {verdict} ({len(sel)})', '', '| id | name | region | rarity | missing |', '|---:|---|---|---|---|']
        for r in sel:
            miss = r['missing'] if verdict == 'INCOMPLETE' else 'everything (no unit_mst row' + \
                (', no art' if all(r[a] == 'NO' for a in ART) else ', some art present') + ')'
            lines.append(f"| {r['id']} | {r['name']} | {r['region']} | {r['rarity']} | {miss} |")
        lines.append('')
    return '\n'.join(lines) + '\n'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--root', type=Path, default=ROOT / 'deploy')
    ap.add_argument('--md', type=Path)
    ap.add_argument('--csv', type=Path)
    ap.add_argument('--assets', type=Path, help='write the missing client files, one path per line')
    a = ap.parse_args()
    rows = audit(a.root)
    c = Counter((r['region'], r['verdict']) for r in rows)
    for k in sorted(c):
        print(f'  {k[0]:12} {k[1]:14} {c[k]}')
    if a.md:
        a.md.write_text(markdown(rows), encoding='utf-8')
    if a.csv:
        buf = io.StringIO()
        w = csv.DictWriter(buf, fieldnames=[k for k in rows[0] if not k.startswith('_')], lineterminator='\n',
                           extrasaction='ignore')
        w.writeheader()
        w.writerows(rows)
        a.csv.write_text(buf.getvalue(), encoding='utf-8')
    if a.assets:
        a.assets.parent.mkdir(parents=True, exist_ok=True)
        a.assets.write_text('\n'.join(missing_assets(rows)) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
