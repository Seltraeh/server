"""Feature visit regressions; isolated server on 19962 and copied save under out/.

python scripts/test_feature_visits_wire.py out/feature-visits-tests/gme.sqlite
Restart the isolated server, then repeat with --check-persisted.
Requires pycryptodome. Fixtures deliberately alter only the copied save.
"""
import base64
import concurrent.futures
import gzip
import json
from pathlib import Path
import sqlite3
import sys
import urllib.request
from Crypto.Cipher import AES

ROOT = Path(__file__).resolve().parents[1]
DB = Path(sys.argv[1]).resolve()
assert DB.is_relative_to(ROOT / 'out'), 'Use a copied save under out/'
db = sqlite3.connect(DB, timeout=20)
user, login = db.execute('SELECT id,gumi_user_id FROM user_info LIMIT 1').fetchone()
KEY = '2386Diw1'


def call(group='983D5Dii', key='Dr6pwV3i', rows=None, player=None):
    body = {'IKqx1Cn9': [{'iN7buP2h': login, 'h7eY3sAK': player or user}]}
    if rows is not None:
        body[KEY] = rows
    raw = json.dumps(body).encode()
    pad = 16-len(raw)%16
    cipher = AES.new(key.encode().ljust(16,b'\0'), AES.MODE_ECB)
    payload = {'F4q6i9xe': {'Hhgi79M1': group, 'aV6cLn3v': 'visit-test'},
               'a3vSYuq2': {'Kn51uR4Y': base64.b64encode(cipher.encrypt(raw+bytes([pad])*pad)).decode()}}
    request = urllib.request.Request('http://127.0.0.1:19962/bf/gme/action.php',
        json.dumps(payload).encode(), {'Content-Type': 'application/json'})
    with urllib.request.urlopen(request, timeout=60) as response:
        raw = response.read()
        if response.headers.get('Content-Encoding') == 'gzip': raw = gzip.decompress(raw)
    outer = json.loads(raw)
    if 'b5PH6mZa' in outer: return {'error': outer['b5PH6mZa']}
    plain = cipher.decrypt(base64.b64decode(outer['a3vSYuq2']['Kn51uR4Y']))
    return json.loads(plain[:-plain[-1]])


def row(feature, dungeon=0, flag=0):
    return {'e63D1BV0': str(feature), 'MHx05sXt': str(dungeon), 'Diwl3b56': str(flag)}


def visits(response):
    assert 'error' not in response, response
    return sorted((int(r['e63D1BV0']),int(r['MHx05sXt']),int(r['Diwl3b56'])) for r in response[KEY])


def stored():
    return db.execute('SELECT user_id,feature_id,dungeon_id FROM user_entered_features ORDER BY 1,2,3').fetchall()


expected = [(2,100,0),(2,101,0),(3,0,0),(7,0,0),(11,0,0),(13,0,0)]
if '--check-persisted' in sys.argv:
    assert visits(call('cTZ3W2JG','ScJx6ywWEb0A3njT')) == expected
    assert db.execute("SELECT COUNT(*) FROM migration_status WHERE hash='25092026_CreateUserEnteredFeatures'").fetchone()[0] == 1
    print('PASS: login restores all visits after process restart; migration remains idempotent')
    sys.exit(0)

db.execute('DELETE FROM user_entered_features WHERE user_id IN (?,?)',(user,'visit-other-player'))
db.execute('INSERT INTO user_entered_features VALUES (?,?,?)',('visit-other-player',17,0))
db.commit()
assert visits(call('ynB7X5P9','7kH9NXwC')) == []
assert visits(call(rows=[row(11)])) == [(11,0,0)]
assert visits(call(rows=[row(11)])) == [(11,0,0)]
assert visits(call(rows=[row(3)])) == [(3,0,0),(11,0,0)]
assert visits(call(rows=[row(2,100)])) == [(2,100,0),(3,0,0),(11,0,0)]
assert visits(call(rows=[row(2,101)])) == [(2,100,0),(2,101,0),(3,0,0),(11,0,0)]
print('PASS: first visit, duplicate retry, full snapshots and separate dungeon visits')

for invalid in [None, [], [row(0)], [row(-1)], [row(11,-1)], [row(11,0,1)],
                [row('bad')], [row(2147483648)], [row(7),row(13)]]:
    before = stored()
    assert 'error' in call(rows=invalid), invalid
    assert stored() == before
assert 'error' in call(rows=[row(7)],player='visit-other-player')
print('PASS: malformed visits and mismatched player identity rejected without mutation')

before = stored()
quoted = "'"+user.replace("'", "''")+"'"
db.execute(f"CREATE TRIGGER feature_visit_fault BEFORE INSERT ON user_entered_features WHEN NEW.user_id={quoted} BEGIN SELECT RAISE(ABORT,'injected visit failure'); END")
db.commit()
try:
    assert 'error' in call(rows=[row(7)])
    assert stored() == before
finally:
    db.execute('DROP TRIGGER feature_visit_fault')
    db.commit()
print('PASS: failed persistence returns error without recording a visit')

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    responses = list(pool.map(lambda f: call(rows=[row(f)]), [7,13]))
assert all('error' not in response for response in responses)
for group,key in [('ynB7X5P9','7kH9NXwC'),('RUV94Dqz','hy0P9xjsGJ6MAgb2'),('cTZ3W2JG','ScJx6ywWEb0A3njT')]:
    assert visits(call(group,key)) == expected
assert db.execute('SELECT feature_id FROM user_entered_features WHERE user_id=?',('visit-other-player',)).fetchall() == [(17,)]
print('PASS: concurrent visits retained, login/both refreshes agree, other player history excluded')
