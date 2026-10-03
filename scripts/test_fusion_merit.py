"""Integration regressions. ONLY run against the isolated server on port 19960.

Usage: python scripts/test_fusion_merit.py out/bugfix-tests-2026-09-23/gme.sqlite
Requires pycryptodome and a copied save with an existing account.
"""
import base64
import gzip
import json
from pathlib import Path
import sqlite3
import sys
import urllib.request
from Crypto.Cipher import AES

ROOT = Path(__file__).resolve().parents[1]
DB = Path(sys.argv[1]).resolve()
assert DB.is_relative_to(ROOT / 'out'), 'Refusing to modify a live save'
db = sqlite3.connect(DB, timeout=15)
db.row_factory = sqlite3.Row
user, login = db.execute('SELECT id,gumi_user_id FROM user_info LIMIT 1').fetchone()
mst = {int(r['pn16CNah']): r for r in json.loads((ROOT / 'deploy/mst/unit_mst.json').read_text(encoding='utf-8'))['2r9cNSdt']}
species = next(r for r in mst.values() if r['7ofj5xa1'] == '8' and r.get('iEFZ6H19', '0') not in ('', '0'))
template = dict(db.execute('SELECT * FROM user_units LIMIT 1').fetchone())
template.pop('user_unit_id')


def call(group, key, body, error=False):
    body['IKqx1Cn9'] = [{'iN7buP2h': login, 'h7eY3sAK': user}]
    key = key.encode().ljust(16, b'\0')[:16]
    raw = json.dumps(body).encode()
    pad = 16 - len(raw) % 16
    cipher = AES.new(key, AES.MODE_ECB)
    envelope = {'F4q6i9xe': {'Hhgi79M1': group, 'aV6cLn3v': 'regression'},
                'a3vSYuq2': {'Kn51uR4Y': base64.b64encode(cipher.encrypt(raw + bytes([pad]) * pad)).decode()}}
    req = urllib.request.Request('http://127.0.0.1:19960/bf/gme/action.php',
                                 json.dumps(envelope).encode(), {'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=40) as response:
        data = response.read()
        if response.headers.get('Content-Encoding') == 'gzip':
            data = gzip.decompress(data)
    outer = json.loads(data)
    if error:
        assert 'b5PH6mZa' in outer, outer
        return outer
    assert 'b5PH6mZa' not in outer, outer
    raw = cipher.decrypt(base64.b64decode(outer['a3vSYuq2']['Kn51uR4Y']))
    return json.loads(raw[:-raw[-1]])


def unit(unit_id, **updates):
    row = dict(template, user_id=user, unit_id=unit_id, unit_type_id=1,
               unit_lvl=1, total_exp=0, exp=0, bb_lvl=1, sbb_lvl=0,
               ext_hp=50, ext_atk=20, ext_def=20, ext_rec=20,
               sphere_ext=1, eqip_item_id=0, eqip_item_id2=0,
               eqip_item_frame_id=0, eqip_item_frame_id2=0)
    data = mst[unit_id]
    row.update(bb_id=data['nj9Lw7mV'], sbb_id=data['iEFZ6H19'],
               base_hp=int(data['UZ1Bj7w2']), base_atk=int(data['i9Tn7kYr']),
               base_def=int(data['q78KoWsg']), base_rec=int(data['92ij6UGB']))
    row.update(updates)
    result = db.execute(f"INSERT INTO user_units ({','.join(row)}) VALUES ({','.join('?' for _ in row)})", tuple(row.values()))
    db.commit()
    return result.lastrowid


def read(uid):
    return dict(db.execute('SELECT * FROM user_units WHERE user_unit_id=?', (uid,)).fetchone())


def mix(base, materials, error=False):
    return call('Mw08CIg2', 'JnegC7RrN3FoW8dQ', {
        'mCE3rUu5': [{'Rs7bCE3t': '0'}],
        'Km35HAXv': [{'edy7fq3L': str(base), 'mnZ5K4Ii': '1'}] +
                     [{'edy7fq3L': str(m), 'mnZ5K4Ii': '2'} for m in materials]}, error)


for bb, sbb, material, expected in [(1, 0, 750004, (10, 10)),
        (8, 0, 10313, (10, 4)), (9, 0, 10312, (10, 1)),
        (10, 4, 10313, (10, 9)), (10, 10, 750004, (10, 10))]:
    base = unit(int(species['pn16CNah']), bb_lvl=bb, sbb_lvl=sbb)
    donor = unit(material)
    result = mix(base, [donor])
    after = read(base)
    assert (after['bb_lvl'], after['sbb_lvl']) == expected, after
    # The result screen reads UnitOpeResult (1ZbHB6Im).  xZH6EIQ7 is the helper
    # picker list: an id-less row there empties it, so a fusion must not send it.
    assert 'xZH6EIQ7' not in result, result['xZH6EIQ7']
    assert (int(result['1ZbHB6Im'][0]['8hVR4Fjr']), int(result['1ZbHB6Im'][0]['0xgWq5bD'])) == expected
    assert db.execute('SELECT 1 FROM user_units WHERE user_unit_id=?', (donor,)).fetchone() is None
    mix(base, [donor], error=True)
    assert read(base) == after
print('PASS: five burst progression cases, response levels, consumed-material replay')

for old_type, frog, target in [(1, 730312, 1), (1, 730322, 2), (1, 730332, 3),
        (1, 730342, 4), (1, 730352, 5), (1, 730362, 6)] + [(t, 730302, None) for t in range(1, 7)]:
    base = unit(int(species['pn16CNah']), unit_type_id=old_type, unit_lvl=80,
                total_exp=1000000, bb_lvl=10, sbb_lvl=10, fe_bp=40,
                fe_max_usable_bp=100, limit_over_hp=123, dbb_unlocked=1)
    before = read(base)
    presents = db.execute('SELECT COALESCE(SUM(target_cnt),0) FROM user_presents WHERE user_id=? AND target_id=?', (user, '750006')).fetchone()[0]
    result = mix(base, [unit(frog)])
    after = read(base)
    assert after['unit_type_id'] == target if target else after['unit_type_id'] != old_type
    assert (after['unit_lvl'], after['exp'], after['total_exp']) == (1, 0, 0)
    assert after['base_hp'] == int(species['UZ1Bj7w2'])
    for field in ['bb_lvl', 'sbb_lvl', 'ext_hp', 'ext_atk', 'ext_def', 'ext_rec',
                  'sphere_ext', 'fe_bp', 'fe_max_usable_bp', 'limit_over_hp', 'dbb_unlocked']:
        assert before[field] == after[field], field
    assert db.execute('SELECT COALESCE(SUM(target_cnt),0) FROM user_presents WHERE user_id=? AND target_id=?', (user, '750006')).fetchone()[0] == presents + 3
    assert result['1ZbHB6Im'][0]  # reset response must include a result record
    assert 'xZH6EIQ7' not in result, result['xZH6EIQ7']  # the helper picker list
print('PASS: all six fixed Mystery Frogs, random exclusion for every prior type, preserved upgrades, compensation')

base = unit(int(species['pn16CNah']))
frog, other = unit(730302), unit(10312)
mix(base, [frog, other], error=True)
mix(base, [frog, frog], error=True)
mix(base, [base], error=True)
assert read(frog) and read(other)
print('PASS: invalid mixed, duplicated and self-fusion materials rejected without consumption')

catalog = json.loads((ROOT / 'deploy/mst/achievement_trade_mst.json').read_text(encoding='utf-8'))['82CcMZhp']
offer = next(r for r in catalog if r['hQ1SJZU9'] == 'Legend Stone')
db.execute('UPDATE user_info SET achieve_point=1000000 WHERE id=?', (user,))
db.execute('DELETE FROM user_achievement_trades WHERE user_id=? AND trade_id=?', (user, offer['Mdgsh04u']))
db.commit()
before = db.execute('SELECT COALESCE(SUM(item_num),0) FROM user_items WHERE user_id=? AND item_id=110100', (user,)).fetchone()[0]
def purchase(count, error=False):
    return call('m9LiF6P2', '0IWC9LVq', {'rX74GNsm': [{'Mdgsh04u': offer['Mdgsh04u'], 'H6k1LIxC': str(count)}]}, error)
purchase(int(offer['S8rdp9zk']) + 1, error=True)
purchase(1)
assert db.execute('SELECT COALESCE(SUM(item_num),0) FROM user_items WHERE user_id=? AND item_id=110100', (user,)).fetchone()[0] == before + 1
assert db.execute('SELECT achieve_point FROM user_info WHERE id=?', (user,)).fetchone()[0] == 1000000 - int(offer['3EWLm0sA'])
db.execute('UPDATE user_info SET achieve_point=0 WHERE id=?', (user,)); db.commit()
purchase(1, error=True)
assert db.execute('SELECT count FROM user_achievement_trades WHERE user_id=? AND trade_id=?', (user, offer['Mdgsh04u'])).fetchone()[0] == 1
print('PASS: Legend Stone delivered and charged, first-purchase over-limit rejected, insufficient funds rolled back')

# Exchange handlers: initial catalogue, payment, limits and atomic batches.
db.execute("DELETE FROM user_exchange_purchases WHERE user_id=?", (user,))
for token in (8, 13, 61):
    db.execute('INSERT INTO user_event_tokens(user_id,token_id,count) VALUES(?,?,100000) ON CONFLICT(user_id,token_id) DO UPDATE SET count=100000', (user, str(token)))
db.commit()
for token in (8, 13, 61):
    result = call('49zxdfl3', 'v2DfDSFl', {'l234vdKs': [{'Slkc395l': str(token)}]})
    assert result['c2Sls4DD'], token
def event_buy(nodes, error=False):
    return call('SLf48fs0', 'Odiel30s', {'c2Sls4DD': nodes}, error)
def event_node(offer_id, count, token=13):
    return {'Kd3DL39d': offer_id, 'H6k1LIxC': str(count), 'Slkc395l': str(token)}
def wallet(token=13):
    return db.execute('SELECT count FROM user_event_tokens WHERE user_id=? AND token_id=?', (user, str(token))).fetchone()[0]
before = wallet()
event_buy([event_node('13-4-110100', 41)], error=True)
event_buy([event_node('13-4-110100', -1)], error=True)
event_buy([event_node('13-4-110100', 1, 61)], error=True)
event_buy([event_node('13-4-110100', 1), event_node('missing', 1)], error=True)
assert wallet() == before
assert db.execute("SELECT count(*) FROM user_exchange_purchases WHERE user_id=? AND shop='event'", (user,)).fetchone()[0] == 0
result = event_buy([event_node('13-4-110100', 1), event_node('13-6-730322', 2)])
assert wallet() == before - 3 - 80
assert len(result['qC2tJs4E']) == 2
assert all(int(r['Sc3slc04']) == 1 for r in result['Sdvs2lds'])
assert next(int(r['S8rdp9zk']) for r in result['c2Sls4DD'] if r['Kd3DL39d'] == '13-4-110100') == 39
event_buy([event_node('13-6-730322', 9)], error=True)
assert wallet() == before - 83
print('PASS: three Bazaar catalogues, units and items paid, remaining stock, invalid currency/count and batch rollback')

guilds = db.execute('SELECT guild_id FROM user_guild_members WHERE member_id=?', (user,)).fetchone()
if not guilds:
    db.execute('UPDATE user_info SET zel=100000000 WHERE id=?', (user,)); db.commit()
    call('g298Da10', 'G23Bd01d', {'IkdSufj5': [{'sD73jd20': '', 's35idar9': 'Regression Guild', 'qp37xTDh': '', 'dDKCN293': '1'}]})
db.execute('UPDATE user_info SET guild_tokens=1000000 WHERE id=?', (user,)); db.commit()
result = call('138ba8d4', '23gD81ia', {})
assert len(result['baD81eqw']) == 121
assert len(result['nMe3ai17']) == 121
guild_offer = next(r for r in result['baD81eqw'] if r['qBAb07rh'].startswith('6:') and int(r['S8rdp9zk']) > 1)
gid, price, limit = int(guild_offer['Yxo3bEic']), int(guild_offer['3EWLm0sA']), int(guild_offer['S8rdp9zk'])
def guild_buy(count, error=False):
    return call('38adiJeb', 'ja3biAqb', {'ah82D1iq': [{'Yxo3bEic': str(gid), 'H6k1LIxC': str(count)}]}, error)
guild_buy(limit+1, error=True)
result = guild_buy(1)
assert len(result['qC2tJs4E']) == 1
assert db.execute('SELECT guild_tokens FROM user_info WHERE id=?', (user,)).fetchone()[0] == 1000000-price
assert next(int(r['H6k1LIxC']) for r in result['nMe3ai17'] if int(r['Yxo3bEic']) == gid) == 1
assert next(int(r['23b6d2ia']) for r in result['csIuech30'] if r['h7eY3sAK'] == user) == 1000000-price
db.execute('UPDATE user_info SET guild_tokens=0 WHERE id=?', (user,)); db.commit()
guild_buy(1, error=True)
assert db.execute("SELECT count FROM user_exchange_purchases WHERE user_id=? AND shop='guild' AND offer_id=?", (user, str(gid))).fetchone()[0] == 1
print('PASS: Guild catalogue, tokens charged, unit delivered, client balance refreshed and failed payment rolled back')

present = db.execute("INSERT INTO user_presents(user_id,present_type,target_cnt) VALUES (?,8002,1234)", (user,)).lastrowid
db.commit()
result = call('bV5xa0ZW', 'X2QFqAKfomPIg3rG', {'o6uWU0Z7': [{'i1WQkh4G': '0', 'S1B82FHK': str(present)}]})
assert db.execute('SELECT guild_tokens FROM user_info WHERE id=?', (user,)).fetchone()[0] == 1234
assert next(int(r['23b6d2ia']) for r in result['csIuech30'] if r['h7eY3sAK'] == user) == 1234
call('bV5xa0ZW', 'X2QFqAKfomPIg3rG', {'o6uWU0Z7': [{'i1WQkh4G': '0', 'S1B82FHK': str(present)}]})
assert db.execute('SELECT guild_tokens FROM user_info WHERE id=?', (user,)).fetchone()[0] == 1234
print('PASS: Guild Token present credited once and client wallet refreshed')

# A type change must affect subsequent levelling, not only the displayed label.
base = unit(int(species['pn16CNah']))
mix(base, [unit(730322)])  # Anima
mix(base, [unit(750004)])
after = read(base)
levels = after['unit_lvl'] - 1
assert levels > 0
def standard(low, high):
    return int(species[low]) + int((int(species[high])-int(species[low])) * levels / (int(species['EI1DF8Yt'])-1))
assert 5*levels <= after['base_hp']-standard('UZ1Bj7w2', '3WMz78t6') <= 10*levels
assert -3*levels <= after['base_rec']-standard('92ij6UGB', 'X9P3AN5d') <= -levels
assert after['base_atk'] == standard('i9Tn7kYr', 'omuyP54D')
assert after['base_def'] == standard('q78KoWsg', '32INDST4')
print('PASS: Anima stat growth after a Mystery Frog reset follows the MST ranges')
