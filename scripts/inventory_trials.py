"""#12 "Vortex Trials": inventory what the name covers in this build, and how
much of it is playable, before anything is authored.

python scripts/inventory_trials.py [--exe PATH_TO_DEBUG_EXE --port 19986]

Prints one row per mission for every trial-like zone, with the columns the
issue asks for: where the client files it, its unlock chain, battles, energy,
rewards, whether the archive can serve it, whether the client ships a mission
script for it (F_MISSION_SCRIPT_MST, decrypted read-only from LocalState when
present), whether the wiki boss scrape (tools/wiki_bosses) knows its boss, and
-- with --exe -- whether an isolated server actually permits it.

"Vortex Trials" is a real category here: AreaMst 100003, land 99 (the Vortex),
"sp_quest_banner_legacy4.png".  It is NOT Trial of the Gods (800051, filed
under 100002 "Travellers"), the Research Lab's Trial Test (2000000) or
Strategy Zone (2100000), or "CH3 Trials" (600000, land 999).  Those are
listed separately so none of them stands in for another.
"""
import argparse
import base64
import json
import os
import sys
from pathlib import Path

from bf_testkit import ROOT, Fixture, IsolatedServer, Client
from bf_testkit import qa_dir  # noqa: E402

LOCALSTATE = Path(os.environ.get('LOCALAPPDATA', '')) / 'Packages' / 'gumi.BraveFrontier_99p3jr0gh0z6w' / 'LocalState'
ZONES = [  # (label, root area) -- the root and every descendant area are listed
    ('Vortex Trials (Vortex category)', '100003'),
    ('Trial of the Gods (under Travellers)', '800051'),
    ('Research Lab: Trial Test', '2000000'),
    ('Research Lab: Strategy Zone', '2100000'),
    ('CH3 Trials (land 999)', '600000'),
]


def mst(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


def client_scripts():
    """Mission ids that have a row in the client's own F_MISSION_SCRIPT_MST."""
    files = sorted(LOCALSTATE.glob('Ver*_F2Dz3QHU*.dat')) if LOCALSTATE.is_dir() else []
    if not files:
        return None
    from Crypto.Cipher import AES
    from Crypto.Util.Padding import unpad
    key = b'U91CxXGi'.ljust(16, b'\0')
    ids = set()
    for f in files:
        rows = json.loads(unpad(AES.new(key, AES.MODE_ECB).decrypt(base64.b64decode(f.read_bytes())), 16))
        ids |= {r['j28VNcUW'] for r in rows}
    return ids


def wiki_quests():
    catalog = json.loads((ROOT / 'tools/wiki_bosses/boss_catalog.json').read_text(encoding='utf-8'))
    quests = {}
    for boss, entry in catalog.items():
        for q in entry.get('quests', []):
            quests.setdefault(q.get('quest'), []).append(boss)
    return quests


def permitted(exe, port):
    fx = Fixture.create(qa_dir('trials_inventory'), port=port)
    with IsolatedServer(exe, fx):
        reply = Client(fx).call('UserInfo', body={})
    # One id key per row, and the key IS the kind (see net/permit_place.kdl).
    kinds = {'VjCY7rX4': 'area', 'MHx05sXt': 'dungeon', 'j28VNcUW': 'mission'}
    seen = {}
    for key in ('yXNM8kL3', 'Y73tHKS8', 'Y73mHKS8'):
        for row in reply.get(key, []):
            for field, kind in kinds.items():
                if field in row:
                    seen.setdefault((kind, str(row[field])), set()).add(key)
    return seen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe')
    ap.add_argument('--port', type=int, default=19986)
    args = ap.parse_args()
    areas = {a['VjCY7rX4']: a for a in mst('area')}
    dungeons = mst('dungeon')
    missions = mst('mission')
    archived = {m['id'] for m in json.loads((ROOT / 'deploy/archive/mission.json').read_text(encoding='utf-8'))}
    scripts = client_scripts()
    quests = wiki_quests()
    permits = permitted(args.exe, args.port) if args.exe else None

    def subtree(root):
        out, todo = [], [root]
        while todo:
            aid = todo.pop(0)
            if aid in areas:
                out.append(aid)
            todo += sorted((a for a, row in areas.items() if row['sHMai2ZG'] == aid), key=int)
        return out

    total = built = 0
    summary = []
    print('| zone | area | dungeon | mission | battles | energy | needs | rewards | archived | client script | wiki boss | permitted |')
    print('|---|---|---|---|---|---|---|---|---|---|---|---|')
    for label, root in ZONES:
        tree = subtree(root)
        rows = zone_built = zone_scripts = zone_wiki = 0
        for aid in tree:
            area_dungeons = [d for d in dungeons if d['VjCY7rX4'] == aid]
            if not area_dungeons:
                kids = sum(1 for row in areas.values() if row['sHMai2ZG'] == aid)
                what = f'(category of {kids} areas)' if kids else '(no DungeonMst row in this build)'
                print(f"| {label} | {aid} {areas[aid]['V84mzqoX']} | {what} | | | | | | | | | "
                      f"{('area ' + ','.join(sorted(permits[('area', aid)]))) if permits and ('area', aid) in permits else ('area no' if permits is not None else 'n/a')} |")
                continue
            for d in area_dungeons:
                for m in (m for m in missions if m['MHx05sXt'] == d['MHx05sXt']):
                    mid = int(m['j28VNcUW'])
                    rows += 1
                    total += 1
                    built += mid in archived
                    zone_built += mid in archived
                    zone_scripts += bool(scripts) and str(mid) in scripts
                    zone_wiki += bool(quests.get(m['0iAIR2LP']))
                    wiki = ', '.join(quests.get(m['0iAIR2LP'], [])) or '-'
                    script = 'n/a' if scripts is None else ('yes' if str(mid) in scripts else 'no')
                    permit = 'n/a' if permits is None else '; '.join(
                        f"{kind} {','.join(sorted(permits.get((kind, ident), []))) or 'no'}"
                        for kind, ident in (('area', aid), ('dungeon', d['MHx05sXt']), ('mission', str(mid))))
                    print(f"| {label} | {aid} {areas[aid]['V84mzqoX']} | {d['MHx05sXt']} {d['bWsLFP96']} | "
                          f"{mid} {m['0iAIR2LP']} | {m['69vnphig']} | {m.get('A8DEK5ob')} | {m['HSRhkf70'] or '-'} | "
                          f"{m['SiYs27Cj'] or '-'} | {'yes' if mid in archived else 'NO'} | {script} | {wiki} | {permit} |")
        summary.append((label, len(tree), rows, zone_built, zone_scripts, zone_wiki))
    print('\n| zone | areas | missions | archived | client script | wiki boss |')
    print('|---|---|---|---|---|---|')
    for row in summary:
        print('| ' + ' | '.join(map(str, row)) + ' |')
    print(f'\n{built} of {total} listed missions are archived (servable).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
