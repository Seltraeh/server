"""MissionStart sends the F_SKILL_MST rows of every monster skill (8aiBoHg5), on an isolated server.

    python scripts/test_mission_monster_skills_wire.py PATH_TO_DEBUG_EXE [--port 19990]

Round 8 (November 1): any monster skill crashed the Windows client at battle start, because the client files
skills flagged ToZj4zyn 0 (most of the monster range) as battle-temporary and empties that list on entering Home.  The original
server's contract, recovered from the client: the battle's skill rows ride on MissionStart under 8aiBoHg5
(UnitSkillMstResponse(true), no master wipe).  Checked here, on a disposable archive (hard links to
deploy/archive; mission 10 wave 1 given two skills, mission 12 an unknown one):
  * the block holds exactly the named skills, once each, in first-use order, each row equal to the server's
    skill table (ToZj4zyn kept, so the client files it as temporary);
  * never 5hafnym7 (wipes the master skill list) or 3ktU0D2e (wipes the permanent level list);
  * an ordinary mission sends no block at all;
  * an unknown skill refuses the start, spending nothing;
  * a Research Lab trial from deploy (2000002) carries the skills of its wave AND scripted monsters.
"""
import argparse
import json
import os
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from bf_testkit import (ROOT, Fixture, IsolatedServer, Client, Checker, mission_start_body,  # noqa: E402
                        mission_end_body, started_serial, qa_dir)
from gen_research_lab import dump  # noqa: E402

ARCHIVE = ROOT / 'deploy' / 'archive'
SKILLS = [1902, 9600]
UNKNOWN = 999999901


def skill_table():
    return {int(r['nj9Lw7mV']): r for r in next(iter(json.loads(
        (ROOT / 'deploy/mst/skill_mst.json').read_text(encoding='utf-8')).values()))}


def monster_skill_ids(mission):
    out = []
    for b in [b for s in mission['stages'] for b in s['battle_monsters']] + list(mission.get('script_monsters') or []):
        for k in b.get('skills') or []:
            if k['skill_id'] not in out:
                out.append(k['skill_id'])
    return out


def build_archive(folder):
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    for p in ARCHIVE.iterdir():
        if p.is_file() and p.name != 'mission.json':
            os.link(p, folder / p.name)
    missions = json.loads((ARCHIVE / 'mission.json').read_text(encoding='utf-8'))
    by_id = {m['id']: m for m in missions}
    by_id[10]['stages'][0]['battle_monsters'][0]['skills'] = [{'skill_id': i, 'show_name': True} for i in SKILLS]
    # A monster with a different id: MonsterMst rows are keyed by monster id, and the first monsters of waves
    # 1 and 2 are the same 10550.
    first = by_id[10]['stages'][0]['battle_monsters'][0]['id']
    other = next(b for b in by_id[10]['stages'][1]['battle_monsters'] if b['id'] != first)
    other['skills'] = [{'skill_id': SKILLS[0], 'show_name': False}]
    by_id[12]['stages'][0]['battle_monsters'][0]['skills'] = [{'skill_id': UNKNOWN, 'show_name': True}]
    (folder / 'mission.json').write_bytes(dump(missions, 1, False))
    return by_id


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19990)
    args = ap.parse_args()
    check = Checker()
    table = skill_table()
    folder = qa_dir('mission_monster_skills_archive')
    by_id = build_archive(folder)
    lab = by_id.get(2000002)
    lab_skills = monster_skill_ids(lab) if lab else []
    check('the Lab trial in deploy names skills on wave and scripted monsters',
          lab and lab_skills and lab.get('script_monsters'), lab_skills)

    fx = Fixture.create(qa_dir('mission_monster_skills'), port=args.port, archive_root=folder)
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        db.execute('UPDATE user_info SET level=50, exp=0, energy=20000, energy_full_ts=0 WHERE id=?', (c.user,))
        db.execute('DELETE FROM user_campaign_missions WHERE user_id=?', (c.user,))
        db.executemany('INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,?,2)',
                       [(c.user, str(m)) for m in sorted(by_id) if m < 100000])
        db.commit()
        db.close()
        c.call('UserInfo')

        reply = c.call('MissionStart', body=mission_start_body(10))
        check('mission 10 starts', 'error' not in reply, reply.get('error'))
        rows = reply.get('8aiBoHg5') or []
        check('8aiBoHg5 holds the named skills once each, in first-use order',
              [int(r.get('nj9Lw7mV', 0)) for r in rows] == SKILLS, [r.get('nj9Lw7mV') for r in rows])
        diffs = [(int(r['nj9Lw7mV']), k) for r in rows for k, v in table[int(r['nj9Lw7mV'])].items()
                 if v is not None and str(r.get(k)) != str(v)]
        check('each row equals the server skill table key for key', rows and not diffs, diffs[:5])
        check('ToZj4zyn stays "0", so the client files the rows as battle-temporary',
              rows and all(r.get('ToZj4zyn') == '0' for r in rows), [r.get('ToZj4zyn') for r in rows])
        check('no wiping block: neither 5hafnym7 nor 3ktU0D2e',
              '5hafnym7' not in reply and '3ktU0D2e' not in reply, [k for k in ('5hafnym7', '3ktU0D2e') if k in reply])
        sent = [r.get('F4bQ7r8C') for r in reply.get('U0v5IeJo', []) if r.get('F4bQ7r8C')]
        check('the monster rows still name their skills', sorted(sent) == ['1902', '1902:1@9600:1'], sent)
        if 'error' not in reply:
            c.call('MissionEnd', body=mission_end_body(10, serial=started_serial(reply)))

        reply = c.call('MissionStart', body=mission_start_body(11))
        check('an ordinary mission sends no 8aiBoHg5 block', 'error' not in reply and '8aiBoHg5' not in reply,
              (reply.get('error'), list(reply)[:12]))
        if 'error' not in reply:
            c.call('MissionEnd', body=mission_end_body(11, serial=started_serial(reply)))

        db = fx.db()
        energy = db.execute('SELECT energy FROM user_info WHERE id=?', (c.user,)).fetchone()[0]
        db.close()
        reply = c.call('MissionStart', body=mission_start_body(12))
        db = fx.db()
        after = db.execute('SELECT energy FROM user_info WHERE id=?', (c.user,)).fetchone()[0]
        db.close()
        check('an unknown monster skill refuses the start and spends nothing',
              'error' in reply and after == energy, (reply.get('error'), energy, after))

        reply = c.call('MissionStart', body=mission_start_body(2000002))
        got = [int(r.get('nj9Lw7mV', 0)) for r in reply.get('8aiBoHg5') or []]
        check(f'Research Lab trial 2000002: 8aiBoHg5 carries all {len(lab_skills)} skills, scripted monsters\' included',
              'error' not in reply and got == lab_skills, (reply.get('error'), got, lab_skills))
        if 'error' not in reply:
            c.call('MissionEnd', body=mission_end_body(2000002, serial=started_serial(reply)))
    return check.summary('test_mission_monster_skills_wire')


if __name__ == '__main__':
    sys.exit(main())
