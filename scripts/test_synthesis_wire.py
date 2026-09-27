"""Synthesis regressions against an isolated server on port 19962.

python scripts/test_synthesis_wire.py out/synthesis-tests/gme.sqlite [--baseline]
Uses a copied account. --baseline reproduces the old non-atomic failure only.
Never point the isolated server at a live save. Requires pycryptodome.
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
path = Path(sys.argv[1]).resolve()
assert path.is_relative_to(ROOT / 'out'), 'Refusing to mutate a live save'
db = sqlite3.connect(path, timeout=20)
user, login = db.execute('SELECT id,gumi_user_id FROM user_info LIMIT 1').fetchone()


def call(csv):
    body = {'IKqx1Cn9': [{'iN7buP2h': login, 'h7eY3sAK': user}],
            'JTf2jY5o': [{'4HqhTf3a': csv}]}
    raw = json.dumps(body).encode()
    pad = 16-len(raw) % 16
    cipher = AES.new(b'AFqKIJ8Z4mHPB9xg', AES.MODE_ECB)
    envelope = {'F4q6i9xe': {'Hhgi79M1': '4P5GELTF', 'aV6cLn3v': 'synthesis-test'},
                'a3vSYuq2': {'Kn51uR4Y': base64.b64encode(cipher.encrypt(raw+bytes([pad])*pad)).decode()}}
    request = urllib.request.Request('http://127.0.0.1:19962/bf/gme/action.php',
                                    json.dumps(envelope).encode(), {'Content-Type': 'application/json'})
    with urllib.request.urlopen(request, timeout=30) as response:
        data = response.read()
        if response.headers.get('Content-Encoding') == 'gzip':
            data = gzip.decompress(data)
    return json.loads(data)


def fixture(materials=15, karma=5000):
    db.execute('INSERT INTO user_town_facilities(user_id,facility_id,lv) VALUES (?,1,1) '
               'ON CONFLICT(user_id,facility_id) DO UPDATE SET lv=1', (user,))
    db.execute("INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,'21',2) "
               'ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2', (user,))
    db.execute('DELETE FROM user_items WHERE user_id=? AND item_id IN (10400,30000)', (user,))
    db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,10400,?)', (user, materials))
    db.execute('UPDATE user_info SET karma=? WHERE id=?', (karma,user))
    db.execute('DELETE FROM user_recipe_crafts WHERE user_id=? AND recipe_id=2005', (user,))
    db.execute('INSERT OR IGNORE INTO user_team_archive(user_id) VALUES (?)', (user,))
    db.execute('UPDATE user_team_archive SET item_mix_cnt=0,item_mix_elem_cnt=0,sphere_mix_cnt=0 WHERE user_id=?', (user,))
    db.commit()


def snapshot():
    return (
        db.execute('SELECT item_id,item_num FROM user_items WHERE user_id=? ORDER BY item_id', (user,)).fetchall(),
        db.execute('SELECT karma FROM user_info WHERE id=?', (user,)).fetchone(),
        db.execute('SELECT * FROM user_recipe_crafts WHERE user_id=? ORDER BY recipe_id', (user,)).fetchall(),
        db.execute('SELECT * FROM user_team_archive WHERE user_id=?', (user,)).fetchall(),
        db.execute('SELECT * FROM user_daily_tasks WHERE user_id=?', (user,)).fetchall(),
    )


def output():
    return db.execute('SELECT COALESCE(SUM(item_num),0) FROM user_items WHERE user_id=? AND item_id=30000', (user,)).fetchone()[0]


def fault_case(table, column):
    fixture()
    before = snapshot()
    # Restrict faults to this isolated fixture account. SQLite trigger bodies
    # cannot bind parameters; quote this DB-derived string as a SQL literal.
    quoted = "'"+user.replace("'", "''")+"'"
    id_column = 'id' if table == 'user_info' else 'user_id'
    db.execute(f'CREATE TRIGGER synthesis_test_fault BEFORE UPDATE OF {column} ON {table} '
               f"WHEN NEW.{id_column}={quoted} BEGIN SELECT RAISE(ABORT,'injected synthesis failure'); END")
    db.commit()
    try:
        response = call('2005:2')
        assert 'b5PH6mZa' in response, response
        return before, snapshot()
    finally:
        db.execute('DROP TRIGGER synthesis_test_fault')
        db.commit()


before, after = fault_case('user_info', 'karma')
if '--baseline' in sys.argv:
    assert before != after and output() == 2, 'Old failure was not reproduced'
    print('REPRODUCED: failed Karma write left two crafted spheres and spent ingredients')
    sys.exit(0)
assert before == after, 'Crafting effects survived a failed Karma write'
before, after = fault_case('user_team_archive', 'item_mix_cnt')
assert before == after, 'Late failure did not roll back inventory, Karma and progress'
print('PASS: failures at payment and final counters roll back all effects')

for malformed in ['2005', '2005:nope', '2005:1x', '2005:0', '2005:-1',
                  '2005:2147483648', '2005:1:2', '2005:1,', '2005:1,2005:bad']:
    fixture()
    before = snapshot()
    assert 'b5PH6mZa' in call(malformed), malformed
    assert before == snapshot(), malformed
print('PASS: malformed counts and mixed valid/invalid batches rejected without mutation')

for csv in ['2005:3', '2005:1,2005:2', '2005:2147483647']:
    fixture()
    assert 'b5PH6mZa' not in call(csv)
    assert output() == 3
    assert db.execute('SELECT karma FROM user_info WHERE id=?', (user,)).fetchone()[0] == 3500
    assert db.execute('SELECT item_mix_cnt,item_mix_elem_cnt,sphere_mix_cnt FROM user_team_archive WHERE user_id=?', (user,)).fetchone() == (3,15,3)
    assert db.execute('SELECT craft_count FROM user_recipe_crafts WHERE user_id=? AND recipe_id=2005', (user,)).fetchone()[0] == 3
    before = snapshot()
    assert 'b5PH6mZa' not in call('2005:3')
    assert before == snapshot(), 'Repeated request granted products without materials'
print('PASS: ordinary/duplicate/huge batches, affordable prefix, counters and spent-stock retry')

fixture(karma=500)
assert 'b5PH6mZa' not in call('2005:3')
assert output() == 1
assert db.execute('SELECT karma FROM user_info WHERE id=?', (user,)).fetchone()[0] == 0
print('PASS: Karma shortage limits crafts without overdrawing')

fixture()
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    responses = list(pool.map(call, ['2005:3', '2005:3']))
assert all('b5PH6mZa' not in response for response in responses)
assert output() == 3, 'Concurrent requests spent the same ingredients twice'
assert db.execute('SELECT karma FROM user_info WHERE id=?', (user,)).fetchone()[0] == 3500
print('PASS: concurrent requests cannot double-spend the same stock')
