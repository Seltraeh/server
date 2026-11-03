"""Tests for scripts/gen_story_bosses.py (#18): the generator in memory, every boss AI in the client model, the
output on an isolated server.

    python scripts/test_gen_story_bosses.py PATH_TO_DEBUG_EXE [--port 19973]

Offline (deploy/ is only read):
  * the sourced numbers still hold (Juggernaut's mission against Tower of Mistral rev 674494; the Defense System
    and Granvalm HP the wiki gives), and the authored HP follow the EXP line the docstring states;
  * the generator changes only the named monsters' skills, AI id, resists, HP and action counts plus their AI
    records, and is idempotent;
  * every skill is the boss's own named burst (by the wiki's name) in the monster range, with a level-1 row, and
    is battle-temporary in the client (ToZj4zyn 0), so it needs the 8aiBoHg5 server;
  * Juggernaut's 9600 inflicts Weakness at level 1 and its band never does;
  * each AI in scripts/ai_model.py over a whole fight: never stalls, fires every skill, and keeps the turn
    pattern the wiki states (every 3rd/4th/5th turn; Cardes' turns 1, 6, 11 and heals on 5, 10; Defense
    System's single repair below 50 %; Abaddon's stronger AoE only below 50 %);
  * --check reports and nothing is written without --out.
Wire (needs a server that sends 8aiBoHg5): a disposable archive (hard links to deploy/archive, the generated
mission.json and ai.json) loads, and MissionStart for each boss mission sends the boss's skills, resists, HP and
AI id, the AI rows, and every skill's own F_SKILL_MST row under 8aiBoHg5 (without them the Windows client
crashes at battle start).
"""
import argparse
import hashlib
import json
import os
import random
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_story_bosses as g  # noqa: E402
from ai_model import Monster  # noqa: E402
from bf_testkit import (ROOT, PRE_RELEASE, Fixture, IsolatedServer, Client, Checker, mission_start_body,  # noqa: E402
                        mission_end_body, started_serial, qa_dir)

MISSIONS, AIS = g.ARCHIVE / 'mission.json', g.ARCHIVE / 'ai.json'
WIKI_85 = {'hp': 50000, 'element': 4, 'energy_cost': 8, 'battles': 8, 'exp': 180}
SOURCED_HP = {85: 50_000, 776: 180_000, 866: 200_000}
# The wiki's burst names per boss, in kit order (Juggernaut's 9600 is the authored Weakness skill).
NAMES = {85: ['Atom Cannon'], 265: ['Black Force', 'Evil Hole', 'Necro Curse'], 365: ['Genesis', 'Endless'],
         465: ['Blood Cyclone', 'Amdahl Vision'], 565: ['Chaos Rage', 'Gravity Nova'],
         666: ['Hidden Dimension', 'Foreign Light', 'Endless'],
         776: ['Destruct Drive', 'Heavy Barrage', 'System Repair', 'Genocide Order'],
         866: ['Tail Swing', 'Crash Nail', 'Crunch Rack'], 20067: ['Fatal Blow', 'Grand Destroyer']}
# The wiki's susceptibilities per boss: these land, the others are immune (Maxwell: "all status ailments").
SUSCEPTIBLE = {85: {'injury', 'curse', 'poison'}, 265: {'sick'}, 365: {'poison', 'weak', 'sick', 'injury', 'curse',
               'paralysis'}, 465: {'weak', 'paralysis', 'curse'}, 565: {'injury', 'sick', 'weak'}, 666: {'injury'},
               776: {'weak'}, 866: {'paralysis', 'weak', 'poison'}, 20067: {'weak', 'injury'}}
FIELDS = ('skills', 'ai_id', 'ailment_resists', 'hp', 'act_max', 'act_rate')


def boss_rows(missions):
    by_id = {m['id']: m for m in missions}
    return {k['mission']: next(b for s in by_id[k['mission']]['stages'] for b in s['battle_monsters']
                               if b['id'] == k['monster']) for k in g.BOSSES}


def strip(missions):
    for b in boss_rows(missions).values():
        for k in FIELDS:
            b.pop(k, None)
    return missions


def skill_rows():
    sk = {r['nj9Lw7mV']: r for r in next(iter(json.loads(
        (ROOT / 'deploy/mst/skill_mst.json').read_text(encoding='utf-8')).values()))}
    lv = {}
    for r in next(iter(json.loads((ROOT / 'deploy/mst/skill_level_mst.json').read_text(encoding='utf-8')).values())):
        lv.setdefault(r['nj9Lw7mV'], []).append(r['Kn51uR4Y'])
    return sk, lv


def ailments(sk, lv, sid):
    procs = sk[sid]['hjAy9St3'].split('@')
    if '11' not in procs:
        return {}
    level1 = lv[sid][0].split('|')[0].split(':', 2)[2].split('@')
    p = (level1[procs.index('11')].split(',') + ['0'] * 8)[:8]
    return {p[k]: int(p[k + 1]) for k in range(0, 8, 2) if p[k] not in ('0', '')}


def fight(kit, boss, turns=60, seed=7):
    """One whole fight in the client model, HP falling evenly from 100 % to near 0: [(turn, hp%, actions)]."""
    m = Monster(kit, act_min=boss.get('act_min', 1), act_max=boss.get('act_max', 1),
                act_rate=boss.get('act_rate', 100.0), rng=random.Random(seed))
    out = []
    for t in range(1, turns + 1):
        hp = max(1.0, 100.0 - (t - 1) * 99.0 / (turns - 1))
        m.set_hp_pct(hp)
        out.append((t, hp, m.take_turn(t)))
    return out


def casts(log, n):
    return [t for t, _, acts in log if ('skill', n) in acts]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19973)
    args = ap.parse_args()
    check = Checker()
    m_raw, a_raw = MISSIONS.read_bytes(), AIS.read_bytes()
    deploy_digest = hashlib.sha256(m_raw + a_raw).hexdigest()
    if g.generate(m_raw, a_raw)[:2] == (m_raw, a_raw) and (PRE_RELEASE / 'archive' / 'mission.json').is_file():
        # deploy carries this generator's output (released November 2): diff against the archive before the release
        print(f'deploy already carries it: diffing against {PRE_RELEASE / "archive"}')
        m_raw, a_raw = (PRE_RELEASE / 'archive' / 'mission.json').read_bytes(), (PRE_RELEASE / 'archive' / 'ai.json').read_bytes()
    new_m, new_a, report = g.generate(m_raw, a_raw)
    again_m, again_a, _ = g.generate(new_m, new_a)
    before, after = json.loads(m_raw), json.loads(new_m)
    ais_before = {a['id']: a for a in json.loads(a_raw)}
    ais_after = {a['id']: a for a in json.loads(new_a)}
    bosses = boss_rows(after)
    by_id = {m['id']: m for m in after}
    kits = {k['mission']: k for k in g.BOSSES}

    # ---- the source ----------------------------------------------------------------------------------
    m85, b85 = by_id[85], bosses[85]
    got = {'hp': b85['hp'], 'element': b85['visuals']['element'], 'energy_cost': m85['energy_cost'],
           'battles': len(m85['stages']), 'exp': m85['exp']}
    check('mission 85 still matches rev 674494 on every number it gives', got == WIKI_85, got)
    check('the sourced HP hold (Juggernaut 50,000, Defense System 180,000, Granvalm 200,000)',
          {mid: bosses[mid]['hp'] for mid in SOURCED_HP} == SOURCED_HP)
    line = lambda exp: 50_000 + (exp - 180) * (180_000 - 50_000) / (5_900 - 180)   # noqa: E731
    authored = {mid: (bosses[mid]['hp'], round(line(by_id[mid]['exp']) / 5000) * 5000)
                for mid in kits if mid not in SOURCED_HP}
    check('each authored HP is the sourced EXP line at its mission, rounded to 5,000',
          all(a == b for a, b in authored.values()), authored)

    # ---- the generator -------------------------------------------------------------------------------
    check('only the named bosses\' skills, AI id, resists, HP and action counts change in mission.json',
          strip(before) == strip(json.loads(new_m)))
    want_ais = {k['ai']['id'] for k in g.BOSSES}
    check(f'ai.json gains exactly the {len(want_ais)} story AIs', set(ais_after) - set(ais_before) == want_ais
          and all(ais_after[k] == v for k, v in ais_before.items()), sorted(set(ais_after) ^ set(ais_before)))
    check('idempotent', (again_m, again_a) == (new_m, new_a))
    check('report names every boss', [r[0] for r in report] == [k['mission'] for k in g.BOSSES], report)

    sk, lv = skill_rows()
    bad_name, bad_row = [], []
    for mid, k in kits.items():
        names = [sk[str(i)]['0nxpBDz2'] for i in k['skills']]
        if names != NAMES[mid]:
            bad_name.append((mid, names))
        for i in k['skills']:
            r = sk.get(str(i))
            if not r or not lv.get(str(i)) or not lv[str(i)][0].startswith('1:') or i >= 10000:
                bad_row.append((mid, i))
    check('every skill is the boss\'s own named burst (the wiki\'s names, in kit order)', not bad_name, bad_name)
    # ToZj4zyn 0 (battle-temporary in the client, so it crashes without 8aiBoHg5) on most of them; 1 on Emperor's
    # and Granvalm's, which the client keeps for good.  The server re-sends both kinds: harmless for a 1.
    temporary = sum(1 for k in kits.values() for i in k['skills'] if sk[str(i)]['ToZj4zyn'] == '0')
    check(f'every skill is a monster-range row with a level-1 row ({temporary} of them battle-temporary in the '
          f'client, so they need 8aiBoHg5)', not bad_row and temporary >= 15, bad_row)
    check('"Causes Weakness": 9600 inflicts Weak (ailment 2) at level 1, and nothing else',
          ailments(sk, lv, '9600') == {'2': 45}, ailments(sk, lv, '9600'))
    check('Juggernaut\'s own band never inflicts Weakness (why a skill was borrowed)',
          not any('2' in ailments(sk, lv, str(i)) for i in range(1900, 1905)))
    check('no other archived monster uses 9600', not any(
        k['skill_id'] == 9600 for x in before for s in x['stages'] for b in s['battle_monsters']
        for k in (b.get('skills') or [])))
    res_bad = [(mid, bosses[mid]['ailment_resists']) for mid in kits
               if {a for a, v in bosses[mid]['ailment_resists'].items() if v == 0} != SUSCEPTIBLE[mid]
               or any(v not in (0, 100) for v in bosses[mid]['ailment_resists'].values())]
    check('resists: exactly the wiki\'s susceptibilities land, the rest are immune (Maxwell: all land)',
          not res_bad, res_bad)

    # ---- the AIs in the client model ------------------------------------------------------------------
    stalls, unused = [], []
    for mid, k in kits.items():
        try:
            log = fight(ais_after[k['ai']['id']], bosses[mid])
        except Exception as ex:  # noqa: BLE001
            stalls.append((mid, str(ex)))
            continue
        for n in range(1, len(k['skills']) + 1):
            if not casts(log, n):
                unused.append((mid, n))
    check('no AI stalls a turn over a whole fight', not stalls, stalls)
    check('every skill of every boss is cast at least once over a whole fight', not unused, unused)
    every = {365: (2, 5), 465: (2, 4), 565: (2, 5), 776: (4, 5), 866: (3, 3), 20067: (2, 4)}
    wrong = []
    for mid, (n, period) in every.items():
        log = fight(ais_after[kits[mid]['ai']['id']], bosses[mid])
        if casts(log, n) != [t for t in range(1, 61) if t % period == 0]:
            wrong.append((mid, casts(log, n)[:6]))
    check('the wiki\'s fixed turns hold: every 3rd (Granvalm), 4th (Emperor, Grantos), 5th (Maxwell, Zebra, '
          'Defense System) turn exactly', not wrong, wrong)
    log = fight(ais_after[30000666], bosses[666])
    check('Cardes: Endless on turns 1, 6, 11 ... and the heal on 5, 10 ...',
          casts(log, 3) == [1] + list(range(6, 61, 5)) and casts(log, 2) == list(range(5, 61, 5)),
          (casts(log, 3)[:6], casts(log, 2)[:6]))
    log = fight(ais_after[30000776], bosses[776])
    repair = [(t, hp) for t, hp, acts in log if ('skill', 3) in acts]
    check('Defense System repairs once, and only below 50 % HP', len(repair) == 1 and repair[0][1] < 50, repair)
    log = fight(ais_after[30000265], bosses[265])
    hole = [hp for t, hp, acts in log if ('skill', 2) in acts]
    check('Abaddon\'s stronger AoE only below 50 % HP, and an AoE on most turns',
          hole and max(hole) < 50 and sum(1 for _, _, acts in log if acts[0][0] == 'skill') > 40,
          (max(hole) if hole else None))
    log = fight(ais_after[30000866], bosses[866])
    check('Granvalm acts once or twice a turn, twice on some', {len(a) for _, _, a in log} == {1, 2})

    # ---- the command line ----------------------------------------------------------------------------
    folder = qa_dir('gen_story_bosses_archive')
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    for p in MISSIONS.parent.iterdir():
        if p.is_file() and p.name not in ('mission.json', 'ai.json'):
            os.link(p, folder / p.name)
    (folder / 'mission.json').write_bytes(new_m)
    (folder / 'ai.json').write_bytes(new_a)
    run = lambda *a: subprocess.run([sys.executable, '-I', str(HERE / 'gen_story_bosses.py'), *a],   # noqa: E731
                                    capture_output=True, text=True)
    r = run('--check', '--archive', str(folder))
    check('--check on the generated archive: up to date', r.returncode == 0 and 'up to date' in r.stdout, r.stdout[-200:])
    r = run()
    check('no --out: nothing is written', r.returncode != 0
          and hashlib.sha256(MISSIONS.read_bytes() + AIS.read_bytes()).hexdigest() == deploy_digest, r.stderr[-200:])

    # ---- the wire ------------------------------------------------------------------------------------
    fx = Fixture.create(qa_dir('gen_story_bosses'), port=args.port, archive_root=folder)
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        db.execute('UPDATE user_info SET level=200, exp=0, energy=20000, energy_full_ts=0 WHERE id=?', (c.user,))
        db.execute('DELETE FROM user_campaign_missions WHERE user_id=?', (c.user,))
        db.executemany('INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,?,2)',
                       [(c.user, str(x['id'])) for x in after if x['id'] < 100000])
        db.commit()
        db.close()
        c.call('UserInfo')
        wire_bad = []
        for mid, k in kits.items():
            reply = c.call('MissionStart', body=mission_start_body(mid))
            if 'error' in reply:
                wire_bad.append((mid, 'error', reply.get('error')))
                continue
            row = next((r for r in reply.get('U0v5IeJo', []) if r.get('o49dYfpH') == str(k['monster'])), {})
            want = {'F4bQ7r8C': '@'.join(f'{i}:1' for i in k['skills']), 'i74vGUFa': str(k['ai']['id']),
                    'e7DK0FQT': str(bosses[mid]['hp']),
                    'CEeqs63b': ':'.join(str(bosses[mid]['ailment_resists'][a]) for a in g.AILMENTS)}
            got = {key: row.get(key) for key in want}
            if got != want:
                wire_bad.append((mid, 'row', got))
            ai_rows = [r for r in reply.get('89ausgc4', []) if r.get('4eEVw5hL') == str(k['ai']['id'])]
            if len(ai_rows) != len(k['ai']['actions']):
                wire_bad.append((mid, 'ai rows', len(ai_rows)))
            sent = [int(r.get('nj9Lw7mV', 0)) for r in reply.get('8aiBoHg5') or []]
            if sent != k['skills']:
                wire_bad.append((mid, '8aiBoHg5', sent))
            c.call('MissionEnd', body=mission_end_body(mid, serial=started_serial(reply)))
        check(f'MissionStart for all {len(kits)} boss missions: skills, AI, HP and resists on the boss row, the AI '
              f'rows, and each skill\'s own row under 8aiBoHg5', not wire_bad, wire_bad[:4])
    return check.summary('test_gen_story_bosses')


if __name__ == '__main__':
    sys.exit(main())
