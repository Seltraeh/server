"""Build offline exchange stock from decoded Guild MST and documented Bazaar offers.

Guild: newest supported offer per reward; prices and limits are retained.
Bazaar: recovered prices from the Global wiki, explicitly curated (not a recovered
complete server catalogue). Availability is permanent; limits do not auto-reset.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--guild-source', type=Path, required=True,
                    help='Original decoded F_GUILD_POINT_EXCHANGE_MST_Ver233.json')
args = parser.parse_args()
units = {u['name']: u['id'] for u in json.loads((ROOT/'deploy/archive/unit.json').read_text(encoding='utf-8'))}
item_rows = next(v for v in json.loads((ROOT/'deploy/mst/item_mst.json').read_text(encoding='utf-8')).values() if isinstance(v, list))
items = {r['c7Z6xDB2']: int(r['kixHbe54']) for r in item_rows}
unit_ids, item_ids = set(map(str, units.values())), set(map(str, items.values()))
rows = json.loads(args.guild_source.read_text(encoding='utf-8'))
if isinstance(rows, dict):
    rows = rows['baD81eqw']
latest = {}
for row in rows:
    if not row['Yxo3bEic'].isdigit() or row['Yxo3bEic'] == '0':
        continue
    reward = row['qBAb07rh'].split(':')
    if reward[0] not in ('4', '6', '7') or reward[1] not in (unit_ids if reward[0] == '6' else item_ids):
        continue
    key = tuple(reward[:3])
    if key not in latest or (row['qA7M9EjP'], int(row['Yxo3bEic'])) > (latest[key]['qA7M9EjP'], int(latest[key]['Yxo3bEic'])):
        latest[key] = row
guild = [dict(r, qA7M9EjP='2020-01-01 00:00:00', SzV0Nps7='2037-12-31 23:59:59') for r in latest.values()]
guild.sort(key=lambda r: int(r['Yxo3bEic']))
(ROOT/'deploy/mst/guild_point_exchange_mst.json').write_text(json.dumps({'baD81eqw': guild}, ensure_ascii=False, indent=1)+'\n', encoding='utf-8')

offers = []
def add(token, name, price, limit, quantity=1, source=''):
    kind = 6 if name in units else 4
    target = units[name] if kind == 6 else items[name]
    offers.append(dict(id=f'{token}-{kind}-{target}', token_id=token, name=name,
                       price=price, limit=limit, reward_type=kind, target_id=target,
                       quantity=quantity, source=source))

insignia = 'https://bravefrontierglobal.fandom.com/wiki/Event_Bazaar/Brave_Insignia'
burst = 'https://bravefrontierglobal.fandom.com/wiki/Event_Bazaar/Brave_Bazaar'
for name in ['Lord', 'Anima', 'Breaker', 'Guardian', 'Oracle']:
    add(13, name+' Mystery Frog', 40, 10, source=insignia)
add(13, 'Rex Mystery Frog', 100, 5, source=insignia)
add(13, 'Random Mystery Frog', 10, 60, source=insignia)
for name, price, limit in [('Geminus Tome', 1, 480), ('Elementum Tome', 1, 480),
                           ('Amber Butterfly', 10, 60), ('Legend Stone', 3, 40)]:
    add(13, name, price, limit, source=insignia)
for element in ['Fire', 'Water', 'Earth', 'Thunder', 'Light', 'Dark']:
    add(13, element+' Golem', 9, 27, source=insignia)
    add(61, element+' Golem', 100, 2, source=burst)
    add(61, element+' Mecha God', 3, 5, source=burst)
for name, price, limit in [('Omni Frog', 6, 5), ('Burst Queen', 6, 5),
        ('Miracle Totem', 3, 4), ('Metal Mimic', 3, 2), ('Random Mystery Frog', 60, 10),
        ('Rex Mystery Frog', 600, 1), ('Legend Stone', 20, 2), ('Amber Butterfly', 50, 3),
        ('Crescent Dew', 25, 4), ('Distilled Ether', 100, 2), ('Fujin Potion', 1, 20),
        ('Fujin Tonic', 2, 15), ('Hero Crystal', 4, 5), ('Revive Light', 2, 10)]:
    add(61, name, price, limit, source=burst)
for name in ['Geminus Tome', 'Elementum Tome']:
    add(61, name, 20, 5, 10, burst)
# Original Rift prices are documented on the individual unit/sphere pages.
# One-per-offer is the conservative offline limit for the two unit offers.
for name in ['Black Knight Kielazar', 'Time-Weaver Elaina', 'Orphira', "Uln'gha", 'Kronax', 'Obrim']:
    add(8, name, 5000, 1, source='https://bravefrontierglobal.fandom.com/wiki/'+name.replace(' ', '_'))
    if name in items:
        offers[-1]['reward_type'] = 7
(ROOT/'deploy/archive/event_exchange.json').write_text(json.dumps(offers, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(f'{len(guild)} Guild offers; {len(offers)} Bazaar offers')
