"""#18 large bosses: one row per special-art boss the archive serves, with the
graphics audited separately from the behaviour.

python scripts/inventory_bosses.py

A "large boss" here is a monster drawn from its own monster/img atlas (a
`visuals` block, unit_id 0 -- see the special boss monster contract in
packet-generator/assets/archive/mission.kdl), not a unit sprite.  The battle
simulator's six training dummies (unit_anime_10000..60000) are #11's and are
left out.

Columns:
  graphics  -- every file the visuals block names is on disk, and the sheet size
  behaviour -- the AI ids it runs: ai.json id 1 is "Attack random", the generic
               placeholder; anything else was authored (scripts/gen_research_lab.py)
  hp        -- the archive's HP next to the wiki scrape's recorded HP, if any
  evidence  -- tools/wiki_bosses/boss_catalog.json notes (skills/phases) and
               whether the client ships a mission script for the mission
  status    -- faithful-to-evidence / placeholder / blocked (no behaviour source)
"""
import base64
import json
import os
import sys
from collections import defaultdict
from pathlib import Path

from PIL import Image

from bf_testkit import ROOT

CONTENT = ROOT / 'deploy/game_content/content'
LOCALSTATE = Path(os.environ.get('LOCALAPPDATA', '')) / 'Packages' / 'gumi.BraveFrontier_99p3jr0gh0z6w' / 'LocalState'
DUMMIES = {f'unit_anime_{e}0000.png' for e in range(1, 7)}
PLACEHOLDER_AI = 1


def client_script_missions():
    files = sorted(LOCALSTATE.glob('Ver*_F2Dz3QHU*.dat')) if LOCALSTATE.is_dir() else []
    if not files:
        return None
    from Crypto.Cipher import AES
    from Crypto.Util.Padding import unpad
    key = b'U91CxXGi'.ljust(16, b'\0')
    ids = set()
    for f in files:
        ids |= {int(r['j28VNcUW']) for r in json.loads(
            unpad(AES.new(key, AES.MODE_ECB).decrypt(base64.b64decode(f.read_bytes())), 16))}
    return ids


def main():
    missions = json.loads((ROOT / 'deploy/archive/mission.json').read_text(encoding='utf-8'))
    ais = {a['id']: a['name'] for a in json.loads((ROOT / 'deploy/archive/ai.json').read_text(encoding='utf-8'))}
    catalog = json.loads((ROOT / 'tools/wiki_bosses/boss_catalog.json').read_text(encoding='utf-8'))
    scripted = client_script_missions()
    bosses = defaultdict(lambda: {'names': set(), 'ids': set(), 'missions': {}, 'ai': set(), 'hp': set(), 'visuals': None})
    for m in missions:
        for stage in m['stages']:
            for mon in stage['battle_monsters'] + stage.get('script_monsters', []):
                vis = mon.get('visuals')
                if not vis or vis.get('img_a') in DUMMIES:
                    continue
                e = bosses[vis.get('img_a') or f"(no img_a: {mon['name']})"]
                e['names'].add(mon['name'])
                e['ids'].add(mon['id'])
                e['missions'][m['id']] = m.get('name', '')
                e['ai'].add(mon.get('ai_id'))
                e['hp'].add(mon.get('hp'))
                e['visuals'] = e['visuals'] or vis

    print('| boss | monster ids | missions | graphics | behaviour (AI) | hp archive / wiki | evidence | status |')
    print('|---|---|---|---|---|---|---|---|')
    tally = defaultdict(int)
    for img, e in sorted(bosses.items(), key=lambda kv: (-len(kv[1]['missions']), kv[0])):
        vis = e['visuals']
        files = [('img', 'monster/img', vis.get('img_a')), ('cgg', 'monster/cgg', vis.get('anm_cgg')),
                 ('idle', 'monster/cgs', vis.get('cgs_idle')), ('atk', 'monster/cgs', vis.get('cgs_atk'))]
        missing = [k for k, sub, name in files if not name or not (CONTENT / sub / name).is_file()]
        size = ''
        if vis.get('img_a') and (CONTENT / 'monster/img' / vis['img_a']).is_file():
            with Image.open(CONTENT / 'monster/img' / vis['img_a']) as im:
                size = f'{im.size[0]}x{im.size[1]}'
        graphics = ('complete ' + size) if not missing else f"MISSING {','.join(missing)}"
        placeholder = e['ai'] == {PLACEHOLDER_AI}
        authored = sorted(a for a in e['ai'] if a != PLACEHOLDER_AI)
        behaviour = '; '.join(f'{a} {ais.get(a, "UNRESOLVED")}' for a in sorted(e['ai'], key=lambda x: x or 0))
        wiki = next((catalog[n] for n in e['names'] if n in catalog), None)
        wiki_hp = sorted({h for h in (wiki or {}).get('hp_recorded', []) if h}) if wiki else []
        notes = len((wiki or {}).get('notes', [])) if wiki else 0
        script = 'n/a' if scripted is None else ', '.join(str(mid) for mid in sorted(e['missions']) if mid in scripted) or 'none'
        evidence = f"wiki notes {notes}" if wiki else 'no wiki entry'
        evidence += f"; client script: {script}"
        if placeholder:
            status = 'placeholder (Attack random)' + ('' if wiki else ', blocked: no behaviour source')
        elif authored and PLACEHOLDER_AI in e['ai']:
            status = 'mixed: authored in ' + ','.join(map(str, authored)) + ', placeholder elsewhere'
        else:
            status = 'authored from evidence'
        tally[status.split(' ')[0]] += 1
        mission_list = ', '.join(f'{k} {v}' for k, v in sorted(e['missions'].items()))
        print(f"| {' / '.join(sorted(e['names']))} ({img}) | {','.join(map(str, sorted(e['ids'])))} | {mission_list} | "
              f"{graphics} | {behaviour} | {','.join(map(str, sorted(e['hp'])))} / {','.join(map(str, wiki_hp)) or '-'} | "
              f"{evidence} | {status} |")
    print(f'\n{len(bosses)} special-art bosses: ' + ', '.join(f'{k} {v}' for k, v in sorted(tally.items())))
    return 0


if __name__ == '__main__':
    sys.exit(main())
