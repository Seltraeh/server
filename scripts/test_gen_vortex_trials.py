"""Tests for scripts/gen_vortex_trials.py (#12) and its sources, scripts/data/vortex_trials_wiki.json and
scripts/data/vortex_trials_skills.json.

    python scripts/test_gen_vortex_trials.py PATH_TO_DEBUG_EXE [--port 19976]

Offline (deploy/ is only read):
  * the dataset covers the Vortex Trials missions with a wiki revid each; every boss unit it names exists in the
    unit table, every skill id it lists carries that exact name in F_SKILL_MST;
  * the generator writes exactly its missions (TRIALS, SKELETON_PATHS, MISSIONS) and their AI records,
    idempotently, with mission_mst's energy / battles / EXP / Zel / Karma; the copied waves (Winter Paradise
    rev 665774, the Skeleton openers) are archived rows of their kind and capturable;
  * the boss trials: every phase HP the wiki gives, every skill a F_SKILL_MST row, every monster drawn by a unit in
    the archive, each script chain's opening monster under the script's id and its later phases in script_monsters,
    the AI-flag chains raising flag 3, no AI row that defeats the party outside a stated turn;
  * every new AI over a 60-turn fight in scripts/ai_model.py never stalls;
  * --check reports and nothing is written without --out.
Wire: a disposable archive loads, MissionStart for each trial sends every wave's monsters (and a chain's later
phases), its skills under 8aiBoHg5, and MissionEnd settles.
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
import gen_vortex_trials as g  # noqa: E402
from ai_model import AuthoringError, Monster  # noqa: E402
from bf_testkit import (ROOT, PRE_RELEASE, Fixture, IsolatedServer, Client, Checker, mission_start_body,  # noqa: E402
                        mission_end_body, started_serial, qa_dir)

DATA = HERE / 'data' / 'vortex_trials_wiki.json'
MISSIONS = g.ARCHIVE / 'mission.json'
AIS = g.ARCHIVE / 'ai.json'
WINTER = [['Vigor Imp Molin', 'Power Imp Pakpak', 'Guard Imp Ganju', 'Healing Imp Fwahl'], ['Burst Frog'] * 3,
          ['Void Tablet Emgif'], ['Imbued Tablet Elgif']]


def table(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / name).read_text(encoding='utf-8')).values()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19976)
    args = ap.parse_args()
    check = Checker()

    # ---- the dataset ---------------------------------------------------------------------------------
    data = json.loads(DATA.read_text(encoding='utf-8'))['missions']
    units = {int(r['pn16CNah']) for r in table('unit_mst.json')}
    skills = {int(r['nj9Lw7mV']): r['0nxpBDz2'].strip() for r in table('skill_mst.json')}
    mst = {r['j28VNcUW']: r for r in table('mission_mst.json')}
    check('the dataset covers 39 trial missions, each with a wiki revid and a mission_mst row',
          len(data) == 39 and all(r['revid'] and str(r['mission']) in mst for r in data), len(data))
    bad_units = [(r['mission'], b['unit']) for r in data for b in r.get('bosses', []) if b['unit'] and b['unit'] not in units]
    check('every boss unit the dataset names is in the unit table', not bad_units, bad_units[:4])
    bad_sk = [(r['mission'], s['name'], i) for r in data for s in r.get('skills', []) for i in s['skill_ids']
              if skills.get(i, '').lower() != s['name'].lower()]
    check('every skill id the dataset lists carries that exact name in F_SKILL_MST', not bad_sk, bad_sk[:4])
    wiki = {r['mission']: r for r in data}

    # ---- the generator -------------------------------------------------------------------------------
    raw, ai_raw = MISSIONS.read_bytes(), AIS.read_bytes()
    deploy_digest = hashlib.sha256(raw + ai_raw).hexdigest()
    if g.generate(raw, ai_raw)[:2] == (raw, ai_raw) and (PRE_RELEASE / 'archive' / 'mission.json').is_file():
        print(f'deploy already carries it: diffing against {PRE_RELEASE / "archive"}')
        raw, ai_raw = (PRE_RELEASE / 'archive' / 'mission.json').read_bytes(), (PRE_RELEASE / 'archive' / 'ai.json').read_bytes()
    new, new_ai, report = g.generate(raw, ai_raw)
    again = g.generate(new, new_ai)[:2]
    before = {m['id']: m for m in json.loads(raw)}
    after = {m['id']: m for m in json.loads(new)}
    ais_before = {a['id']: a for a in json.loads(ai_raw)}
    ais = {a['id']: a for a in json.loads(new_ai)}
    ours = set(g.TRIALS) | set(g.SKELETON_PATHS) | {k for k, v in g.MISSIONS.items() if v}
    changed = sorted(i for i in set(before) | set(after) if before.get(i) != after.get(i))
    check(f'only the generator\'s {len(ours)} missions change', set(changed) <= ours and changed, changed[:6])
    ai_changed = {i for i in set(ais) | set(ais_before) if ais.get(i) != ais_before.get(i)}
    check('only new AI records, none removed', set(ais_before) <= set(ais) and all(i >= g.AI_BASE for i in ai_changed),
          sorted(ai_changed)[:4])
    check('idempotent', again == (new, new_ai))
    bad = []
    for mid in ours:
        rec, row = after[mid], mst[str(mid)]
        if (rec['energy_cost'], len(rec['stages']), rec['exp'], rec['zel'], rec['karma']) != \
                (int(row['A8DEK5ob']), int(row['69vnphig']), int(row['d96tuT2E']), int(row['Rs7bCE3t']), int(row['HTVh8a65'])):
            bad.append((mid, 'mission_mst'))
    check('every mission keeps mission_mst\'s energy, battles, EXP, Zel and Karma', not bad, bad[:4])
    pool = g.templates(json.loads(raw), set(g.TRIALS) | set(g.MISSIONS))
    bad = []
    for mid in set(g.TRIALS) | set(g.SKELETON_PATHS):
        for s in after[mid]['stages']:
            for b in s['battle_monsters']:
                t = pool['Imbued Tablet Elgif'] if b['name'] == 'Void Tablet Emgif' else pool[b['name'].strip()]
                if any(b[k] != t[k] for k in ('atk', 'def', 'ai_id', 'wait')) or b['unit_drop_id'] != b['unit_id'] \
                        or b['unit_drop_chance'] < 40 or b['unit_id'] not in units:
                    bad.append((mid, b['name']))
    check('copied waves: archived rows of their kind, their own unit, capturable at >= 40 %', not bad, bad[:4])
    waves = [[b['name'].strip() for b in s['battle_monsters']] for s in after[83013400]['stages']]
    check('Winter Paradise: the wiki\'s line-up (one group per wave), boss last', waves == WINTER
          and [s['is_boss'] for s in after[83013400]['stages']] == [False, False, False, True], waves)
    emgif = after[83013400]['stages'][2]['battle_monsters'][0]
    check('Void Tablet Emgif fights as unit 60812 under the client dictionary\'s monster 68500',
          (emgif['id'], emgif['unit_id']) == (68500, 60812))

    bosses = {mid: [b for s in after[mid]['stages'] for b in s['battle_monsters']] + after[mid].get('script_monsters', [])
              for mid, v in g.MISSIONS.items() if v}
    bad_hp = []
    for mid, mons in bosses.items():
        hp = wiki[mid]['phase_hp']
        chain = g.SCRIPT_CHAINS.get(mid)
        got = [b['hp'] for b in ([after[mid]['stages'][0]['battle_monsters'][0]] + after[mid].get('script_monsters', []))] \
            if chain else [b['hp'] for b in mons]
        if hp and mid not in g.MERGED_HP and got[:len(hp)] != hp[:len(got)]:
            bad_hp.append((mid, hp, got))
    check('every phase HP the wiki gives is the boss\'s HP (chains in phase order)', not bad_hp, bad_hp[:3])
    bad_sk = [(mid, b['name'], s['skill_id']) for mid, mons in bosses.items() for b in mons for s in b['skills']
              if s['skill_id'] not in skills]
    check('every boss skill is a F_SKILL_MST row', not bad_sk, bad_sk[:4])
    arch_units = {u['id'] for u in json.loads((g.ARCHIVE / 'unit.json').read_text(encoding='utf-8'))}
    bad_u = [(mid, b['name'], b['unit_id']) for mid, mons in bosses.items() for b in mons if b['unit_id'] not in arch_units]
    check('every boss is drawn by a unit in the archive (its own art, or a marked stand-in)', not bad_u, bad_u[:4])
    bad_chain = []
    for mid, (kind, ids) in g.SCRIPT_CHAINS.items():
        rec = after[mid]
        if rec['stages'][0]['battle_monsters'][0]['id'] != ids[0] or [b['id'] for b in rec.get('script_monsters', [])] != ids[1:]:
            bad_chain.append((mid, 'ids'))
        for b in [rec['stages'][0]['battle_monsters'][0]] + rec.get('script_monsters', [])[:-1]:
            raises = any(r['action']['flag_changes'][:2] == [3, 1] for r in ais[b['ai_id']]['actions'])
            if raises != (kind == 'flag'):
                bad_chain.append((mid, b['id'], 'flag 3'))
    check('script chains: the opening monster under the script\'s id, later phases in script_monsters; '
          'the AI-flag chains raise flag 3 before their last phase', not bad_chain, bad_chain[:4])
    lethal = []
    for mid, mons in bosses.items():
        for b in mons:
            for r in ais[b['ai_id']]['actions']:
                if r['action']['type'] != 'skill':
                    continue
                slot = next(c['parameter'] for c in r['self_conditions'] if c['type'] == 'skill')
                sid = b['skills'][slot - 1]['skill_id']
                if 'instantly defeat' in skills.get(sid, '').lower() and not any(c['type'] == 'act' for c in r['self_conditions']):
                    lethal.append((mid, sid))
    check('no row uses a party-defeating skill outside a stated turn', not lethal, lethal[:4])
    stalls = []
    for mid, mons in bosses.items():
        for b in mons:
            m = Monster(ais[b['ai_id']], act_min=b['act_min'], act_max=b['act_max'], act_rate=b['act_rate'],
                        rng=random.Random(5))
            try:
                for t in range(1, 61):
                    m.set_hp_pct(max(1.0, 100.0 - (t - 1) * 99.0 / 59))
                    m.take_turn(t)
            except AuthoringError as e:
                stalls.append((mid, str(e)[:60]))
    check('every new AI over a 60-turn fight in the client model: no turn that never ends', not stalls, stalls[:3])

    folder = qa_dir('gen_vortex_trials_archive')
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    for p in MISSIONS.parent.iterdir():
        if p.is_file() and p.name not in ('mission.json', 'ai.json'):
            os.link(p, folder / p.name)
    (folder / 'mission.json').write_bytes(new)
    (folder / 'ai.json').write_bytes(new_ai)
    run = lambda *a: subprocess.run([sys.executable, str(HERE / 'gen_vortex_trials.py'), *a],   # noqa: E731
                                    capture_output=True, text=True)
    r = run('--check', '--archive', str(folder))
    check('--check on the generated archive: up to date', r.returncode == 0 and 'up to date' in r.stdout, r.stdout[-200:])
    r = run()
    check('no --out: nothing is written', r.returncode != 0
          and hashlib.sha256(MISSIONS.read_bytes() + AIS.read_bytes()).hexdigest() == deploy_digest)

    # ---- the wire ------------------------------------------------------------------------------------
    fx = Fixture.create(qa_dir('gen_vortex_trials'), port=args.port, archive_root=folder)
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        db.execute('UPDATE user_info SET level=200, exp=0, energy=20000, energy_full_ts=0 WHERE id=?', (c.user,))
        db.commit()
        db.close()
        c.call('UserInfo')
        wire_bad = []
        for mid in sorted(ours):
            reply = c.call('MissionStart', body=mission_start_body(mid))
            if 'error' in reply:
                wire_bad.append((mid, reply.get('error')))
                continue
            sent = {r.get('o49dYfpH') for r in reply.get('U0v5IeJo', [])}
            want = {str(b['id']) for s in after[mid]['stages'] for b in s['battle_monsters']} \
                | {str(b['id']) for b in after[mid].get('script_monsters', [])}
            if not want <= sent:
                wire_bad.append((mid, 'monsters', sorted(want - sent)[:3]))
            waves_sent = [x.split('|')[0] for x in reply['Kz7qfSs5'][0]['LE6JkUp7'].split('@')]
            if len(waves_sent) != len(after[mid]['stages']):
                wire_bad.append((mid, 'waves', waves_sent))
            skill_ids = {str(s['skill_id']) for b in bosses.get(mid, []) for s in b['skills']}
            sent_skills = {r.get('nj9Lw7mV') for r in reply.get('8aiBoHg5', [])}
            if not skill_ids <= sent_skills:
                wire_bad.append((mid, 'skills', sorted(skill_ids - sent_skills)[:3]))
            end = c.call('MissionEnd', body=mission_end_body(mid, serial=started_serial(reply)))
            if 'error' in end:
                wire_bad.append((mid, 'end', end.get('error')))
        check(f'MissionStart sends every wave, monster, later phase and skill of the {len(ours)} trials; MissionEnd '
              'settles', not wire_bad, wire_bad[:3])
    return check.summary('test_gen_vortex_trials')


if __name__ == '__main__':
    sys.exit(main())
