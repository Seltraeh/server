"""#11 regression: Randall's Battle Simulator (mission 6000000) -- entry and MissionStart.

    python scripts/test_battle_simulator_wire.py PATH_TO_DEBUG_EXE [--port 19960] [--out DIR]

Starts its own isolated server on a fresh SQLite-backup copy of the live save
under out/qa/current/battle_simulator and stops only that process.

ENTRY (the 2026-10-02 crash).  The squad screen finds the battle itself:
SandbagCheckScene::initialize @0x159A6D0 scans the client's DungeonMstList for
dungeon_type 6 and keeps MissionMstList::getMissionListWithDungeonID(..)[0] of
the LAST such dungeon -- a list holding only missions
PermitPlaceInfoList::isPermitMission accepts (@0x1332350 -> @0x126C824), i.e. a
mission row (j28VNcUW) on any permit channel.  With none it stores null, and
Begin (setPreparation @0x159CE40) calls getMissionID on it unchecked.  The
server never permitted 6000000, so Begin crashed the Windows client before
MissionStart was sent.  This models that lookup against the login (UserInfo)
and MissionEnd permit snapshots, and checks nothing ELSE about the simulator is
permitted (an area row would add a Training Ground tile to the Vortex list).

BATTLE (see scripts/gen_battle_simulator.py): MonsterParty::entryMonstersSandbag
builds "1000000" + (element - 1) for every slot the player switched on and hands
it to MonsterUnit::initialize, which dereferences that MonsterMst unchecked.  So
the response must describe all six dummies 10000000-10000005, one per element,
with art the client can download; the dummies must never act, drop or be
captured; and the simulator is free.
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import (ROOT, Checker, Client, Fixture, IsolatedServer, mission_end_body,  # noqa: E402
                        mission_start_body, permit_rows, permitted, qa_dir, started_serial)

CONTENT = ROOT / 'deploy/game_content/content/monster'
SIM = 6000000
DUMMIES = {10000000 + e - 1: e for e in range(1, 7)}


def mst_rows(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


# The client's own tables hold the same rows for these ids (decrypted out of its
# cache, F_DUNGEON_MST v1003 / F_MISSION_MST v1119, on 2026-10-02): exactly one
# type-6 dungeon, 6000000 in area 6000000 / land 99, holding one mission.
DUNGEONS = mst_rows('dungeon')
MISSIONS = mst_rows('mission')
AREAS = {r['VjCY7rX4']: r for r in mst_rows('area')}


def sandbag_scene_mission(reply):
    """SandbagCheckScene::initialize against a permit snapshot: the first permitted
    mission of the last type-6 dungeon, or None (the crash)."""
    allowed = permitted(reply, 'j28VNcUW')
    found = None
    for dungeon in DUNGEONS:
        if dungeon.get('3hPeI1RV') != '6':
            continue
        listed = [int(m['j28VNcUW']) for m in MISSIONS
                  if m.get('MHx05sXt') == dungeon['MHx05sXt'] and int(m['j28VNcUW']) in allowed]
        found = listed[0] if listed else None
    return found


def check_entry(check, reply, label):
    sandbag = [d for d in DUNGEONS if d.get('3hPeI1RV') == '6']
    check(f'{label}: the client data has exactly one type-6 dungeon (6000000)',
          [d['MHx05sXt'] for d in sandbag] == [str(SIM)], [d['MHx05sXt'] for d in sandbag])
    check(f'{label}: SandbagCheckScene finds mission 6000000 (not null)',
          sandbag_scene_mission(reply) == SIM, sandbag_scene_mission(reply))
    rows = permit_rows(reply)
    special = [r for r in rows['Y73tHKS8'] if r.get('j28VNcUW') == str(SIM)]
    elsewhere = [k for k in ('yXNM8kL3', 'Y73mHKS8') for r in rows[k] if r.get('j28VNcUW') == str(SIM)]
    area_type = AREAS.get(str(SIM), {}).get('3v1qg7Uj')
    check(f'{label}: the row rides the special channel its area (type {area_type}) is cleared by',
          len(special) == 1 and not elsewhere and area_type == '1', (special, elsewhere, area_type))
    check(f'{label}: no area or dungeon row for 6000000 (no Training Ground tile in the Vortex)',
          SIM not in permitted(reply, 'VjCY7rX4') and SIM not in permitted(reply, 'MHx05sXt'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19960)
    ap.add_argument('--out', default=str(qa_dir('battle_simulator')))
    args = ap.parse_args()
    check = Checker()
    fx = Fixture.create(args.out, port=args.port)
    with fx.db() as db:
        db.execute('UPDATE user_info SET energy = 50')
        db.commit()

    with IsolatedServer(args.exe, fx):
        client = Client(fx)

        # ---- entry: the login snapshot the Survey Office is reached with -------
        login = client.call('UserInfo')
        check('login: UserInfo answers', 'error' not in login, login.get('error'))
        check_entry(check, login, 'login')

        def snapshot():
            with fx.db() as db:
                info = dict(db.execute('SELECT energy, energy_full_ts, zel, karma, gems, exp, level FROM user_info WHERE id=?',
                                       (client.user,)).fetchone())
                info['units'] = db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=?', (client.user,)).fetchone()[0]
                info['items'] = db.execute('SELECT COALESCE(SUM(item_num),0) FROM user_items WHERE user_id=?', (client.user,)).fetchone()[0]
                info['clears'] = db.execute('SELECT COUNT(*) FROM user_campaign_missions WHERE user_id=?', (client.user,)).fetchone()[0]
                return info

        replies = []
        before = snapshot()
        for attempt in (1, 2, 3):
            reply = client.call('MissionStart', body=mission_start_body(SIM))
            replies.append(reply)
            check(f'session {attempt}: MissionStart {SIM} answers without an error', 'error' not in reply, reply.get('error'))
            if 'error' in reply:
                break
        after = snapshot()

        reply = replies[0]
        if 'error' not in reply:
            monsters = {int(m['o49dYfpH']): m for m in reply.get('U0v5IeJo', [])}
            groups = reply.get('75t0sx9z', [])
            battles = reply.get('pj41dy9g', [])
            cgs = reply.get('8hoyIF9Q') or []
            ais = {int(a['4eEVw5hL']) for a in reply.get('89ausgc4', [])}
            num = reply['Kz7qfSs5'][0]

            check('start_info echoes mission 6000000', reply['9Q1Lq5FS'][0]['j28VNcUW'] == str(SIM))
            # Not the mission-10 template: every battle group names 6000000, and
            # the battle serial (its own per start since battle serials, see
            # gme/common/MissionRuns.hpp) is recorded against 6000000.
            check('battle groups name mission 6000000 (not the mission-10 template)',
                  battles and all(b['j28VNcUW'] == str(SIM) for b in battles), [b.get('j28VNcUW') for b in battles])
            with fx.db() as db:
                run = db.execute('SELECT mission_id FROM user_mission_runs WHERE serial=? AND user_id=?',
                                 (int(num['k9cxD7Ba']), client.user)).fetchone()
            check('battle serial is recorded against mission 6000000', run is not None and run[0] == SIM,
                  (num, run))
            missing = sorted(set(DUMMIES) - set(monsters))
            check('MonsterMst describes all six element dummies 10000000-10000005', not missing, f'missing {missing}; got {sorted(monsters)}')
            check('no MonsterMst besides the six dummies', set(monsters) <= set(DUMMIES), sorted(set(monsters) - set(DUMMIES)))
            for mid, element in DUMMIES.items():
                m = monsters.get(mid)
                if not m:
                    continue
                img, cgg = m['2EF0d6ue'], m['QqfI9mM4']
                check(f'{mid}: element {element}', m['iNy0ZU5M'] == str(element), m['iNy0ZU5M'])
                check(f'{mid}: unit id does not resolve (monster art root)', m['pn16CNah'] == '0', m['pn16CNah'])
                check(f'{mid}: sprite {img} ships in monster/img', (CONTENT / 'img' / img).is_file(), img)
                check(f'{mid}: sprite is the element atlas', img == f'unit_anime_{element}0000.png', img)
                check(f'{mid}: frame table {cgg} ships in monster/cgg', (CONTENT / 'cgg' / cgg).is_file(), cgg)
                check(f'{mid}: drops no zel or karma', (m['Najhr8m6'], m['HTVh8a65']) == ('0', '0'))
                # Reticle offset calibrated in-client (see gen_battle_simulator.py):
                # "0,-40" drew it ~110 px above the dummy.
                check(f'{mid}: target reticle offset centres on the sprite', m['Lkh6gYkT'] == '0,17', m['Lkh6gYkT'])
                check(f'{mid}: AI {m["i74vGUFa"]} is described', int(m['i74vGUFa']) in ais)
                # NEVER ACTS, by AI: every row of its AI ends the turn
                # (setAiTargetList: action type 4, self-targeted -- no move, no
                # attack).  An action count of 0 does not stop the first action
                # of a turn; with AI 1 the dummies attacked with no attack art.
                rows = [a for a in reply.get('89ausgc4', []) if a['4eEVw5hL'] == m['i74vGUFa']]
                check(f'{mid}: never moves or attacks (its AI only ends the turn)',
                      rows and all(a['Hhgi79M1'].split('@')[0] == 'turn_end' for a in rows),
                      [a['Hhgi79M1'] for a in rows])
                idle = [r for r in cgs if int(r['o49dYfpH']) == mid]
                check(f'{mid}: idle animation row, file ships', len(idle) == 1 and idle[0]['fpc3rbs7'] == '1'
                      and (CONTENT / 'cgs' / idle[0]['0QxL2is7']).is_file(), idle)
            slots = [(int(g['hZtF1s8B']), int(g['o49dYfpH'])) for g in groups]
            check('one battle group of six slots in order 0-5', sorted(slots) == [(i, 10000000 + i) for i in range(6)], slots)
            check('six distinct positions', len({g['3g8PW6x0'] for g in groups}) == 6)
            check('no slot advertises a capture', all(g['hw3L0uVj'] == '0:0:0:0' for g in groups), [g['hw3L0uVj'] for g in groups])
            check('exactly one battle (MST battle count 1), not a boss battle',
                  len(battles) == 1 and battles[0]['etM5TCb9'] in ('0', 'false'), battles)
            check('drop info carries no capture or chest', num['LE6JkUp7'] == '1| | ', num['LE6JkUp7'])
            check('repeat sessions describe the same dummies',
                  all(sorted(int(m['o49dYfpH']) for m in r.get('U0v5IeJo', [])) == sorted(monsters) for r in replies))

        check('no energy spent across three sessions', after['energy'] == before['energy'] and after['energy_full_ts'] == before['energy_full_ts'],
              (before, after))
        check('no currency, unit, item or clear changes', {k: after[k] for k in after if k not in ('energy', 'energy_full_ts')}
              == {k: before[k] for k in before if k not in ('energy', 'energy_full_ts')}, (before, after))

        control = client.call('MissionStart', body=mission_start_body(10))
        check('control: ordinary mission 10 still starts', 'error' not in control, control.get('error'))
        check('control: mission 10 still charges its 3 energy', snapshot()['energy'] == after['energy'] - 3)

        # ---- entry again: the snapshot a MissionEnd leaves the client with ------
        if 'error' not in control:
            ended = client.call('MissionEnd', body=mission_end_body(10, serial=started_serial(control)))
            check('control: mission 10 settles', 'error' not in ended, ended.get('error'))
            if 'error' not in ended:
                check_entry(check, ended, 'after MissionEnd')

        # ---- exit and re-enter after a restart: the permit is not session state --
    with IsolatedServer(args.exe, fx, log_name='server_restart.log'):
        client = Client(fx)
        relogin = client.call('UserInfo')
        check_entry(check, relogin, 'after restart')
        again = client.call('MissionStart', body=mission_start_body(SIM))
        check('after restart: the simulator starts again', 'error' not in again, again.get('error'))

    return check.summary('battle simulator')


if __name__ == '__main__':
    sys.exit(main())
