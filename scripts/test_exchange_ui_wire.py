"""Exchange expiry and guild artwork regressions on isolated port 19960.
Usage: python scripts/test_exchange_ui_wire.py out/exchange-ui-tests-2026-09-26/gme.sqlite
Requires a copied save containing a guild and unit 20011; never a live save.
"""
import base64,gzip,json,sqlite3,sys,urllib.request
from pathlib import Path
from Crypto.Cipher import AES
ROOT=Path(__file__).resolve().parents[1]
DB=Path(sys.argv[1]).resolve()
assert DB.is_relative_to(ROOT/'out')
db=sqlite3.connect(DB,timeout=20)
user,login=db.execute('SELECT id,gumi_user_id FROM user_info LIMIT 1').fetchone()
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


# The top-level Bazaar list must remain visible even with a zero token balance.
db.execute('UPDATE user_event_tokens SET count=0 WHERE user_id=?',(user,));db.commit()
r=call('f49als4D','94lDsgh4',{})
assert {8,13,61} <= {int(x['Slkc395l']) for x in r['l234vdKs']}
assert all(int(x['fE2d6ivS'])==-1 for x in r['l234vdKs'])
print('PASS: zero-balance Bazaar tiles use the client no-expiry sentinel')
r=call('YPBU7MD8','AKjzyZ81',{})
assert len(r['9j3ALx8I'])==241
assert all(int(x['VDKB0Y5h'])==-1 for x in r['9j3ALx8I'])
print('PASS: every Merit offer suppresses the expired-time label')
assert db.execute('SELECT 1 FROM user_guild_members WHERE member_id=?',(user,)).fetchone()
assert db.execute('SELECT 1 FROM user_units WHERE user_id=? AND unit_id=20011',(user,)).fetchone()
db.execute('UPDATE user_units SET unit_lvl=1 WHERE user_id=?',(user,))
db.execute('UPDATE user_units SET unit_lvl=80 WHERE user_id=? AND unit_id=20011',(user,));db.commit()
for growth_type in range(1,7):
 db.execute('UPDATE user_units SET unit_type_id=? WHERE user_id=? AND unit_id=20011',(growth_type,user));db.commit()
 r=call('138ba8d4','23gD81ia',{})
 assert len(r['nMe3ai17'])==121
 assert all(int(x['VDKB0Y5h'])==-1 for x in r['nMe3ai17'])
 owner=next(x for x in r['csIuech30'] if x['h7eY3sAK']==user)
 assert int(owner['pn16CNah'])==20011
 assert int(owner['2pAyFjmZ'])==1,owner
 assert (ROOT/'deploy/game_content/content/unit/img/unit_ills_thum_20011.png').is_file()
 assert not (ROOT/'deploy/game_content/content/unit/img/unit_ills_thum_20011_2.png').exists()
print('PASS: guild expiry and founder thumbnails for all six growth types; stock base artwork exists')
r=call('ja5Enusw','8upheqaC',{})
assert r['fRaBu6et'], 'Copied save needs at least one invitation candidate'
assert all(int(x['JmLLHcDv'])==1 for x in r['fRaBu6et'])
print('PASS: invitation candidates use base artwork independently of growth type')
