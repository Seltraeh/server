"""Tests for scripts/gen_mecha_gods.py (#17): the six Lv. 6 Sanctums as The Mecha Gods Awaken.

    python scripts/test_gen_mecha_gods.py PATH_TO_DEBUG_EXE [--port 19974]

Offline (deploy/ is only read):
  * the sourced facts of rev 659707 (copied below) still match mission_mst, and the cached revision when the
    round-8 wiki cache is present;
  * only missions 100320-100325 change in mission.json, ai.json gains exactly their six AIs, idempotent;
  * each boss is the wiki's unit, alone, under the client dictionary's "<Element> Mecha God" monster id, with the
    wiki's two named skills (monster-range rows with a level-1 row), the authored HP / ATK / DEF and a capture
    of its own unit that MissionEnd can settle (a unit-archive record);
  * room 2 is the wiki's line-up, each row a copy of the archive's own row for that monster, with the wiki's
    susceptibilities, up to 4 actions a turn (High Elf 3); room 1 is the archive's, now acting up to 4 times;
  * each AI in scripts/ai_model.py: never stalls, casts both skills, never one skill twice in a turn, the
    "every 3 turns" skill on exactly turns 3, 6, 9 ..., and 1-4 actions a turn;
  * --check reports and nothing is written without --out.
Wire: a disposable archive (hard links to deploy/archive plus the generated files) loads; MissionStart for all
six sends the boss row (monster id, unit, HP, AI, skills, action count), its AI rows and both skill rows under
8aiBoHg5, and room 2's monsters; a rolled Mecha God capture settles through MissionEnd into the box.
"""
import argparse
import base64
import hashlib
import json
import os
import random
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_mecha_gods as g  # noqa: E402
from ai_model import Monster  # noqa: E402
from bf_testkit import (ROOT, PRE_RELEASE, Fixture, IsolatedServer, Client, Checker, mission_start_body,  # noqa: E402
                        mission_end_body, started_serial, qa_dir)

MISSIONS, AIS = g.ARCHIVE / 'mission.json', g.ARCHIVE / 'ai.json'
WIKI_CACHE = ROOT / 'out/qa/current/fresh_round8_20261031/wiki/history/The_Mecha_Gods_Awaken.json'
# rev 659707, per quest: boss unit, room 2 (name -> count), skill names in order, the "every 3 turns" one
WIKI = {
    100320: ('Fire', 10354, {'Dragon Graven': 2, 'Dalimaone': 1}, ['Explosion', 'Volcanic Cannon'], None),
    100321: ('Water', 20344, {'Siren': 2, 'Meltia': 1}, ['Diamond Dust', 'Freezing Breeze'], 2),
    100322: ('Earth', 30334, {'High Elf': 2, 'Lemenara': 1}, ['Vortex', 'Insane Climate'], 2),
    100323: ('Thunder', 40334, {'Great Falcon Ziz': 2, 'Zazabis': 1}, ['Lightning Bolt', 'Unruly Thunder'], 2),
    100324: ('Light', 50394, {'Valkyrie': 1, 'Sky Hero Athena': 2, 'Legendary Jona': 1},
             ['Southern Cross', 'Falling Star'], None),
    100325: ('Dark', 60344, {'Lich': 2, 'Vampire': 1, 'Legion': 1}, ['Invasion', 'Ominous Shade'], 2),
}
SUSCEPTIBLE = {'Lich': {'sick', 'poison', 'paralysis', 'curse'}, 'Vampire': {'poison'}, 'Legion': {'injury'},
               'High Elf': {'weak'}, 'Lemenara': {'weak'}, 'Siren': {'paralysis'}, 'Meltia': {'paralysis'}}


def monster_names():
    z = zipfile.ZipFile(next((ROOT / 'deploy/game_content/content/sgtext').glob('sgtext_monsters_*.zip')))
    name = z.namelist()[0]
    text = base64.b64decode(''.join(z.read(name).decode().split())).decode('utf-8', 'replace')
    out = {}
    for line in text.split('\n'):
        p = line.split('^')
        if p[0].startswith('MST_MONSTERS_') and p[0].endswith('_NAME'):
            out[int(p[0].split('_')[2])] = p[1]
    return out


def table(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / name).read_text(encoding='utf-8')).values()))


def fight(ai, boss, turns=60, seed=11):
    m = Monster(ai, act_min=boss['act_min'], act_max=boss['act_max'], act_rate=boss['act_rate'],
                rng=random.Random(seed))
    out = []
    for t in range(1, turns + 1):
        m.set_hp_pct(max(1.0, 100.0 - (t - 1) * 99.0 / (turns - 1)))
        out.append((t, m.take_turn(t)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19974)
    args = ap.parse_args()
    check = Checker()
    m_raw, a_raw = MISSIONS.read_bytes(), AIS.read_bytes()
    deploy_digest = hashlib.sha256(m_raw + a_raw).hexdigest()
    if g.generate(m_raw, a_raw)[:2] == (m_raw, a_raw) and (PRE_RELEASE / 'archive' / 'mission.json').is_file():
        # deploy carries this generator's output (released November 2): diff against the archive before the release
        print(f'deploy already carries it: diffing against {PRE_RELEASE / "archive"}')
        m_raw, a_raw = (PRE_RELEASE / 'archive' / 'mission.json').read_bytes(), (PRE_RELEASE / 'archive' / 'ai.json').read_bytes()
    new_m, new_a, report = g.generate(m_raw, a_raw)
    again = g.generate(new_m, new_a)
    before, after = json.loads(m_raw), json.loads(new_m)
    by_before, by_after = {m['id']: m for m in before}, {m['id']: m for m in after}
    ais_before = {a['id']: a for a in json.loads(a_raw)}
    ais_after = {a['id']: a for a in json.loads(new_a)}

    # ---- the source ----------------------------------------------------------------------------------
    mst = {r['j28VNcUW']: r for r in table('mission_mst.json')}
    bad = [(mid, mst[str(mid)]['0iAIR2LP'], mst[str(mid)]['A8DEK5ob'], mst[str(mid)]['69vnphig'],
            mst[str(mid)]['d96tuT2E']) for mid, (el, *_r) in WIKI.items()
           if (mst[str(mid)]['0iAIR2LP'], mst[str(mid)]['A8DEK5ob'], mst[str(mid)]['69vnphig'],
               mst[str(mid)]['d96tuT2E']) != (f'Sanctum of {el} Lv. 6', '70', '3', '7000')]
    check('mission_mst agrees with rev 659707: Sanctum of <element> Lv. 6, energy 70, 3 battles, EXP 7,000',
          not bad, bad)
    if WIKI_CACHE.exists():
        revs = json.loads(WIKI_CACHE.read_text(encoding='utf-8'))['revisions']
        text = revs[0]['text']
        miss = [f'{el} {unit} {name}' for el, unit, room, names, _ in WIKI.values()
                for name in [str(unit), *room, *names] if name not in text]
        check('the cached latest revision is 659707 and names every boss, room-2 monster and skill used here',
              revs[0]['revid'] == g.REVID and not miss and 'act up to 4 times per turn' in text, miss[:4])

    # ---- the generator -------------------------------------------------------------------------------
    changed = sorted(i for i in by_after if by_after[i] != by_before.get(i))
    check('only missions 100320-100325 change in mission.json', changed == sorted(WIKI), changed)
    want_ais = {30_000_000 + mid for mid in WIKI}
    check('ai.json gains exactly the six Mecha God AIs, the rest unchanged',
          set(ais_after) - set(ais_before) == want_ais and all(ais_after[k] == v for k, v in ais_before.items()))
    check('idempotent', again[:2] == (new_m, new_a))

    names = monster_names()
    skills = {r['nj9Lw7mV']: r for r in table('skill_mst.json')}
    levels = {}
    for r in table('skill_level_mst.json'):
        levels.setdefault(r['nj9Lw7mV'], []).append(r['Kn51uR4Y'])
    units = {u['id'] for u in json.loads((g.ARCHIVE / 'unit.json').read_text(encoding='utf-8'))}
    boss_bad, room_bad = [], []
    pool = g.templates(before)
    for mid, (el, unit, room, skill_names, _every) in WIKI.items():
        stages = by_after[mid]['stages']
        bosses = stages[2]['battle_monsters']
        b = bosses[0]
        got = (len(bosses), stages[2]['is_boss'], b['unit_id'], names.get(b['id']), b['hp'], b['atk'], b['def'],
               b['unit_drop_id'], b['unit_drop_chance'], b['unit_drop_level'], b['act_max'], b['ai_id'],
               [skills[str(s['skill_id'])]['0nxpBDz2'] for s in b['skills']],
               all(levels.get(str(s['skill_id']), [''])[0].startswith('1:') for s in b['skills']), unit in units)
        want = (1, True, unit, f'{el} Mecha God', 700_000, 580, 100, unit, 40, 1, 4, 30_000_000 + mid,
                skill_names, True, True)
        if got != want:
            boss_bad.append((mid, got))
        line = stages[1]['battle_monsters']
        counts = {}
        for r in line:
            counts[r['name'].strip()] = counts.get(r['name'].strip(), 0) + 1
            t = pool[r['name'].strip()]
            same = all(r[k] == t[k] for k in ('id', 'unit_id', 'atk', 'def', 'unit_drop_id', 'wait'))
            land = {a for a, v in (r.get('ailment_resists') or {a: 0 for a in g.AILMENTS}).items() if v == 0}
            want_land = SUSCEPTIBLE.get(r['name'].strip(), set(g.AILMENTS))
            acts = 3 if r['name'].strip() == 'High Elf' else 4
            if not same or land != want_land or r['act_max'] != acts or r['hp'] != 100_000 or 'skills' in r:
                room_bad.append((mid, r['name'], same, sorted(land), r['act_max'], r['hp']))
        if counts != room:
            room_bad.append((mid, counts))
        if stages[0]['battle_monsters'] != [dict(x, act_min=1, act_max=4, act_rate=50.0)
                                            for x in by_before[mid]['stages'][0]['battle_monsters']]:
            room_bad.append((mid, 'room 1 changed beyond its action count'))
    check('each boss: the wiki\'s unit alone, the dictionary\'s "<Element> Mecha God" id, the wiki\'s two skills '
          '(level-1 rows), HP 700,000 / ATK 580 / DEF 100, capture of its own unit at 40 %, settleable, 4 actions',
          not boss_bad, boss_bad[:3])
    check('room 2: the wiki\'s line-up, copied rows, the wiki\'s susceptibilities, 100,000 HP, 4 actions '
          '(High Elf 3); room 1 kept, acting up to 4 times', not room_bad, room_bad[:4])

    # ---- the AIs in the client model ------------------------------------------------------------------
    ai_bad = []
    for mid, (el, unit, room, skill_names, every) in WIKI.items():
        b = by_after[mid]['stages'][2]['battle_monsters'][0]
        try:
            log = fight(ais_after[b['ai_id']], b)
        except Exception as ex:  # noqa: BLE001
            ai_bad.append((mid, 'stall', str(ex)))
            continue
        cast = {n: [t for t, acts in log if ('skill', n) in acts] for n in (1, 2)}
        if not cast[1] or not cast[2]:
            ai_bad.append((mid, 'unused', {n: len(v) for n, v in cast.items()}))
        if any(acts.count(('skill', n)) > 1 for _, acts in log for n in (1, 2)):
            ai_bad.append((mid, 'a skill twice in one turn'))
        if every and cast[every] != list(range(3, 61, 3)):
            ai_bad.append((mid, 'every 3', cast[every][:6]))
        if {len(a) for _, a in log} != {1, 2, 3, 4}:
            ai_bad.append((mid, 'actions', sorted({len(a) for _, a in log})))
    check('every AI over a whole fight: no stall, both skills cast, none twice a turn, the "every 3 turns" '
          'skill on exactly turns 3, 6, 9 ..., and 1-4 actions a turn', not ai_bad, ai_bad[:4])

    # ---- the command line ----------------------------------------------------------------------------
    folder = qa_dir('gen_mecha_gods_archive')
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    for p in MISSIONS.parent.iterdir():
        if p.is_file() and p.name not in ('mission.json', 'ai.json'):
            os.link(p, folder / p.name)
    (folder / 'mission.json').write_bytes(new_m)
    (folder / 'ai.json').write_bytes(new_a)
    run = lambda *a: subprocess.run([sys.executable, str(HERE / 'gen_mecha_gods.py'), *a],   # noqa: E731
                                    capture_output=True, text=True)
    r = run('--check', '--archive', str(folder))
    check('--check on the generated archive: up to date', r.returncode == 0 and 'up to date' in r.stdout,
          r.stdout[-200:] + r.stderr[-200:])
    r = run()
    check('no --out: nothing is written', r.returncode != 0
          and hashlib.sha256(MISSIONS.read_bytes() + AIS.read_bytes()).hexdigest() == deploy_digest, r.stderr[-200:])

    # ---- the wire ------------------------------------------------------------------------------------
    fx = Fixture.create(qa_dir('gen_mecha_gods'), port=args.port, archive_root=folder)
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        db.execute('UPDATE user_info SET level=200, exp=0, energy=20000, energy_full_ts=0 WHERE id=?', (c.user,))
        db.commit()
        db.close()
        c.call('UserInfo')
        wire_bad = []
        for mid, (el, unit, room, skill_names, _every) in WIKI.items():
            b = by_after[mid]['stages'][2]['battle_monsters'][0]
            reply = c.call('MissionStart', body=mission_start_body(mid))
            if 'error' in reply:
                wire_bad.append((mid, 'error', reply.get('error')))
                continue
            rows = reply.get('U0v5IeJo', [])
            row = next((r for r in rows if r.get('o49dYfpH') == str(b['id'])), {})
            want = {'pn16CNah': str(unit), 'e7DK0FQT': str(b['hp']), 'i74vGUFa': str(b['ai_id']),
                    'F4bQ7r8C': '@'.join(f'{s["skill_id"]}:1' for s in b['skills']), 'oMGC3hW0': '4'}
            got = {k: row.get(k) for k in want}
            if got != want:
                wire_bad.append((mid, 'boss row', got))
            room_ids = {str(r['id']) for r in by_after[mid]['stages'][1]['battle_monsters']}
            if not room_ids <= {r.get('o49dYfpH') for r in rows}:
                wire_bad.append((mid, 'room 2 rows missing'))
            ai_rows = [r for r in reply.get('89ausgc4', []) if r.get('4eEVw5hL') == str(b['ai_id'])]
            if len(ai_rows) != len(ais_after[b['ai_id']]['actions']):
                wire_bad.append((mid, 'ai rows', len(ai_rows)))
            sent = [int(r.get('nj9Lw7mV', 0)) for r in reply.get('8aiBoHg5') or []]
            if not all(s['skill_id'] in sent for s in b['skills']):
                wire_bad.append((mid, '8aiBoHg5', sent))
            c.call('MissionEnd', body=mission_end_body(mid, serial=started_serial(reply)))
        check('MissionStart for all six: the boss row (monster id, unit, HP, AI, both skills, 4 actions), its AI '
              'rows, both skill rows under 8aiBoHg5, and room 2\'s monsters', not wire_bad, wire_bad[:3])

        # a rolled capture of the Fire Mecha God settles into the box
        caught, tries = None, 0
        for tries in range(1, 31):
            reply = c.call('MissionStart', body=mission_start_body(100320))
            records = reply['Kz7qfSs5'][0]['LE6JkUp7'].split('@')
            boss_field = next((r.split('|')[1] for r in records if r.split('|')[0] == '3' and '|' in r), '')
            if ':10354:' in boss_field:
                t = boss_field.split(':')
                claim = f'10354:{t[3]}:{t[4]}'
                n = lambda: db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=? AND unit_id=10354',  # noqa
                                       (c.user,)).fetchone()[0]
                db = fx.db()
                had = n()
                end = c.call('MissionEnd', body=mission_end_body(100320, serial=started_serial(reply), units=claim))
                caught = ('error' not in end, n() - had, claim)
                db.close()
                break
            c.call('MissionEnd', body=mission_end_body(100320, serial=started_serial(reply)))
        check('a rolled Fire Mecha God capture (boss wave) settles through MissionEnd: one 10354 in the box',
              caught is not None and caught[:2] == (True, 1), (tries, caught))
    return check.summary('test_gen_mecha_gods')


if __name__ == '__main__':
    sys.exit(main())
