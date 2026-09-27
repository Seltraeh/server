"""Research Lab wire regressions.  ONLY run against the isolated server on 19960.

    python scripts/test_research_lab_wire.py out/trial003-tests-2026-09-25/gme.sqlite

The save must be a FRESH COPY under out/ (sqlite3's backup API) in which the
trials have not been cleared; energy is topped up and missions are cleared in
it.  Covers, for Trials No. 001-003 and The Creation God (2100004):
MissionStart's encounter payload, the per-player unlock order in UserInfo's
permit list, and MissionEnd's pay rules (a loss, a first clear, a replay),
plus ordinary mission 10 as a control.
"""
import base64
import gzip
import json
import sqlite3
import sys
import urllib.request
from pathlib import Path

from Crypto.Cipher import AES

ROOT = Path(__file__).resolve().parents[1]
DB = Path(sys.argv[1]).resolve()
assert DB.is_relative_to(ROOT / 'out'), 'Refusing to modify a live save'
HOST = 'http://127.0.0.1:19960/bf/gme/action.php'
TRIALS = (2000000, 2000001, 2000002)
CREATION_GOD = 2100004
LAB = TRIALS + (CREATION_GOD,)
db = sqlite3.connect(DB, timeout=15)
user, login = db.execute('SELECT id, gumi_user_id FROM user_info LIMIT 1').fetchone()
AIS = {a['id']: a for a in json.loads((ROOT / 'deploy/archive/ai.json').read_text(encoding='utf-8'))}
ARCHIVE = {m['id']: m for m in json.loads((ROOT / 'deploy/archive/mission.json').read_text(encoding='utf-8'))}
UNIT_REWARD = {2000000: '20233', 2000001: '60324', 2000002: '50525', 2100004: '51147'}
failures = 0


def check(label, cond, detail=''):
    global failures
    print(('PASS  ' if cond else 'FAIL  ') + label + ('' if cond else f'  {detail}'))
    failures += 0 if cond else 1


def call(group, key, body):
    body['IKqx1Cn9'] = [{'iN7buP2h': login, 'h7eY3sAK': user}]
    k = key.encode().ljust(16, b'\0')[:16]
    raw = json.dumps(body).encode()
    pad = 16 - len(raw) % 16
    cipher = AES.new(k, AES.MODE_ECB)
    envelope = {'F4q6i9xe': {'Hhgi79M1': group, 'aV6cLn3v': 'researchlab'},
                'a3vSYuq2': {'Kn51uR4Y': base64.b64encode(cipher.encrypt(raw + bytes([pad]) * pad)).decode()}}
    req = urllib.request.Request(HOST, json.dumps(envelope).encode(), {'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=60) as r:
        data = r.read()
        if r.headers.get('Content-Encoding') == 'gzip':
            data = gzip.decompress(data)
    outer = json.loads(data)
    if 'b5PH6mZa' in outer:
        return {'error': outer['b5PH6mZa']}
    plain = cipher.decrypt(base64.b64decode(outer['a3vSYuq2']['Kn51uR4Y']))
    return json.loads(plain[:-plain[-1]])


def top_up():
    db.execute('UPDATE user_info SET energy = 500, energy_full_ts = 0 WHERE id = ?', (user,))
    db.commit()


def start(mission):
    top_up()
    return call('jE6Sp0q4', 'csiVLDKkxEwBfR70', {
        '9Q1Lq5FS': [{'h7eY3sAK': '0', 'J3stQ7jd': '0', 'j28VNcUW': str(mission), 'jkldTrhL': '0',
                      'Z0Y4RoD7': '0', 'nA95Bdj6': '0', '5Z1LNoyH': '0', 'u1iPEVUq': '-1'}],
        'JzS3uxsZ': [{'0b4efi1W': '1'}]})


def end(mission, status):
    return call('9TvyNR5H', 'oINq0rfUFPx5MgmT', {
        'Kz7qfSs5': [{'k9cxD7Ba': str(mission), 'j3g5P4cq': str(status)}],
        'rXvA1E5y': [{'Najhr8m6': '0', 'HTVh8a65': '0', '3MAT6quo': '', '4T0Q2Bh5': ''}]})


def advertised(ids=TRIALS):
    resp = call('cTZ3W2JG', 'ScJx6ywWEb0A3njT', {})
    return sorted(int(r['j28VNcUW']) for r in resp.get('Y73tHKS8', [])
                  if 'j28VNcUW' in r and int(r['j28VNcUW']) in ids)


def advertised_trials():
    return advertised(TRIALS)


def snapshot(mission):
    zel, exp, level = db.execute('SELECT zel, exp, level FROM user_info WHERE id = ?', (user,)).fetchone()
    presents = db.execute('SELECT present_type, target_id, target_cnt FROM user_presents'
                          ' WHERE user_id = ? AND description = ? ORDER BY present_id',
                          (user, 'First clear reward')).fetchall()
    clear = db.execute('SELECT state, clear_count FROM user_campaign_missions'
                       ' WHERE user_id = ? AND mission_id = ?', (user, str(mission))).fetchone()
    return {'zel': zel, 'exp': exp, 'level': level, 'presents': presents, 'clear': clear}


def enc_conditions(action):
    party = [f"{c['target_id']}:{c['target_parameter'] or 'non'}:{c['type'] or 'non'}:{c['parameters'] or 'non'}@"
             for c in action['party_conditions']][:4]
    party += ['0:non:non:non@'] * (4 - len(party))
    selfc = [f"{c['type'] or 'non'}:{c['parameter']}@" for c in action['self_conditions']][:5]
    selfc += ['non:0@'] * (5 - len(selfc))
    return ''.join(party) + '#' + ''.join(selfc)


def enc_action(a):
    flags = [str(f) for f in a['flag_changes']][:12]
    flags += ['-1'] * (12 - len(flags))
    return f"{a['type']}@{','.join(flags)},@{1 if a['unknown_bool'] else 0}@{a['unknown_int_1']}@{a['unknown_int_2']}"


def enc_resists(m):
    r = m.get('ailment_resists')
    return ':'.join(str(r[k]) for k in ('poison', 'weak', 'sick', 'injury', 'curse', 'paralysis')) if r else '0:0:0:0:0:0'


# --- the unlock order -------------------------------------------------------------
cleared_before = {r[0] for r in db.execute('SELECT mission_id FROM user_campaign_missions'
                                            ' WHERE user_id = ? AND state = 2', (user,))}
check('fresh save: no trial cleared yet', not cleared_before & {str(t) for t in TRIALS}, str(cleared_before))
had_365 = '365' in cleared_before
if had_365:
    db.execute("DELETE FROM user_campaign_missions WHERE user_id = ? AND mission_id = '365'", (user,))
    db.commit()
check('nothing cleared: only Trial No. 001 is offered', advertised_trials() == [2000000], str(advertised_trials()))
check('nothing cleared: The Creation God is offered (its prerequisites are unbuilt)',
      advertised((CREATION_GOD,)) == [CREATION_GOD], str(advertised((CREATION_GOD,))))

# --- each trial: MissionStart payload, a loss, the first clear, a replay -----------
for mid in LAB:
    rec = ARCHIVE[mid]
    placed_ids = [str(m['id']) for st in rec['stages'] for m in st['battle_monsters']]
    described = [m for st in rec['stages'] for m in st['battle_monsters']] + rec.get('script_monsters', [])
    bosses = ['1' if st['is_boss'] else '0' for st in rec['stages']]
    resp = start(mid)
    check(f'{mid}: MissionStart succeeds', 'error' not in resp, str(resp.get('error')))
    check(f'{mid}: {len(bosses)} battle(s), boss last, only the opening monsters placed',
          [b['etM5TCb9'] for b in resp.get('pj41dy9g', [])] == bosses
          and [g['o49dYfpH'] for g in resp.get('75t0sx9z', [])] == placed_ids,
          f"{[b.get('etM5TCb9') for b in resp.get('pj41dy9g', [])]} {[g.get('o49dYfpH') for g in resp.get('75t0sx9z', [])]}")
    mons = {m['o49dYfpH']: m for m in resp.get('U0v5IeJo', [])}
    ok = set(mons) == {str(m['id']) for m in described}
    for m in described:
        w = mons.get(str(m['id']), {})
        skills = '@'.join(f"{s['skill_id']}:1" for s in m['skills'])
        ok &= (w.get('e7DK0FQT') == str(m['hp']) and w.get('i74vGUFa') == str(m['ai_id'])
               and w.get('oMGC3hW0') == str(m['act_max']) and w.get('F4bQ7r8C') == skills
               and abs(float(w.get('6fwL59FT', -1)) - m.get('act_rate', 100.0)) < 1e-6
               and w.get('CEeqs63b') == enc_resists(m) and w.get('pn16CNah') == str(m['unit_id']))
    check(f'{mid}: MonsterMst rows match the archive (hp, ai, budget, skills, resists, art)', ok,
          json.dumps({k: {f: v.get(f) for f in ('e7DK0FQT', 'i74vGUFa', 'CEeqs63b', 'F4bQ7r8C')} for k, v in mons.items()})[:500])
    ai_ids = []
    for m in described:
        if m['ai_id'] not in ai_ids:
            ai_ids.append(m['ai_id'])
    want = [(i, a) for i in sorted(ai_ids) for a in AIS[i]['actions']]
    got = [(int(r['4eEVw5hL']), r) for r in resp.get('89ausgc4', [])]
    bad = [i for i, (w, g) in enumerate(zip(want, got))
           if g[1]['q7Nit8JW'] != enc_conditions(w[1]) or g[1]['Hhgi79M1'] != enc_action(w[1]['action'])
           or int(g[1]['yu18xScw']) != w[1]['priority'] or int(g[1]['4xctV8gF']) != w[1]['act_target']]
    check(f'{mid}: every AI row, in order, matches an independent encoding',
          [g[0] for g in got] == [w[0] for w in want] and not bad, f'{len(got)} rows, bad {bad[:3]}')
    specials = [m for m in described if m.get('visuals')]
    expected_cgs = sum(1 for m in specials for slot in ('cgs_idle', 'cgs_move', 'cgs_atk', 'cgs_skill')
                       if m['visuals'][slot])
    check(f'{mid}: sprite-animation rows only for special monsters',
          len(resp.get('8hoyIF9Q') or []) == expected_cgs, f"{len(resp.get('8hoyIF9Q') or [])} vs {expected_cgs}")

    s0 = snapshot(mid)
    end(mid, 3)
    s1 = snapshot(mid)
    check(f'{mid}: a loss pays nothing and records no clear',
          (s1['zel'], s1['exp'], s1['presents'], s1['clear']) == (s0['zel'], s0['exp'], s0['presents'], None))

    start(mid)
    s0 = snapshot(mid)
    end(mid, 2)
    s1 = snapshot(mid)
    new = s1['presents'][len(s0['presents']):]
    check(f"{mid}: first clear pays {rec['zel']:,} Zel, EXP, a Gem and unit {UNIT_REWARD[mid]}",
          s1['zel'] - s0['zel'] == rec['zel'] and (s1['level'], s1['exp']) != (s0['level'], s0['exp'])
          and sorted(new) == sorted([(8, '', 1), (6, UNIT_REWARD[mid], 1)]) and s1['clear'] == (2, 1),
          f'{s0} -> {s1}')

    start(mid)
    s0 = snapshot(mid)
    end(mid, 2)
    s1 = snapshot(mid)
    check(f'{mid}: a replay pays nothing', (s1['zel'], s1['exp'], s1['presents']) == (s0['zel'], s0['exp'], s0['presents'])
          and s1['clear'] == (2, 2), f'{s0} -> {s1}')

    offered = advertised_trials()
    if mid == 2000000:
        check('after Trial 001: Trial 002 is offered', offered == [2000000, 2000001], str(offered))
    elif mid == 2000001:
        check('after Trial 002 but not St. Lamia 365: Trial 003 is still hidden', offered == [2000000, 2000001], str(offered))
        db.execute("INSERT OR REPLACE INTO user_campaign_missions (user_id, mission_id, state, attain_percent,"
                   " clear_count, last_cleared_at) VALUES (?, '365', 2, 100, 1, 0)", (user,))
        db.commit()
        offered = advertised_trials()
        check('... and offered once 365 is cleared too', offered == list(TRIALS), str(offered))

# --- control: an ordinary mission keeps paying on replay ------------------------------
paid = []
for _ in range(2):
    start(10)
    z0 = db.execute('SELECT zel FROM user_info WHERE id = ?', (user,)).fetchone()[0]
    end(10, 2)
    paid.append(db.execute('SELECT zel FROM user_info WHERE id = ?', (user,)).fetchone()[0] - z0)
check('control: mission 10 pays its Zel on every clear', paid == [ARCHIVE[10]['zel']] * 2, str(paid))

print(f'\n{failures} failure(s)')
sys.exit(1 if failures else 0)
