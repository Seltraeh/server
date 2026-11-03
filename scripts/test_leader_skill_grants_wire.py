"""Leader skills on archive-granted units: the grant fix, the save repair, the packets.

    python scripts/test_leader_skill_grants_wire.py EXE [--port 19992]

The client reads user_units.leader_skill_id (UserUnitInfo.oS3kTZ2W) in Training
Grounds (PlayerParty::entrySandbag -> setGuestLeaderSkillID, a helper taken
from the player's own units), the Challenge Arena copies (Arena is padlocked
offline) and the campaign / Frontier Gate save data it uploads.  Ordinary,
Trial, Raid and campaign battles read UnitMst instead.  Until Session 9 every
unit granted through gme::fromArchivedUnit was stored with 0.

A  repair (scripts/repair_leader_skills.py) on a copied save seeded with: an
   old zero-value grant, an evolved form at 0 (whose base form's skill
   differs), a nonzero override, a unit UnitMst does not know, a unit whose
   UnitMst skill LeaderSkillMst lacks, a correct row and a material.  Dry run
   changes nothing; --apply repairs exactly the zero rows that have a
   resolvable skill and backs up first; a second --apply changes nothing.
B  packets on the repaired copy: the login roster (4ceMWH6k, which feeds
   Training Grounds and the save-data builders) carries every stored value;
   NEW grants through four archive paths -- PresentReceipt, GachaAction,
   a MissionEnd capture, CLI addunit -- store and send the UnitMst skill of the
   granted form; FrontierGateStart echoes the uploaded units' skills and
   FrontierGateContinue hands the same values back.
C  restart: the values survive a server restart, and a repair after all that
   finds nothing to change.

Against a server without the grant fix, B's new-grant checks fail (stored 0).
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import (ROOT, Checker, Client, Fixture, IsolatedServer, mission_end_body,  # noqa: E402
                        mission_start_body, qa_dir, started_serial)
from repair_leader_skills import load_leader_skill_ids, load_unit_mst  # noqa: E402

REPAIR = ROOT / 'scripts' / 'repair_leader_skills.py'
UNITS = load_unit_mst()
SKILLS = load_leader_skill_ids()
ARCHIVE = {u['id'] for u in json.loads((ROOT / 'deploy/archive/unit.json').read_text(encoding='utf-8'))}
EVO = list(json.loads((ROOT / 'deploy/mst/unit_evo_mst.json').read_text(encoding='utf-8')).values())[0]
PRESENT_UNIT, GACHA_UNIT, CAPTURE_UNIT, CLI_UNIT = 20011, 51247, 10017, 50253   # Selena, Lukroar, Ignis Vargas, Tilith
GACHA_GATE = 99008                     # narrowed to GACHA_UNIT in a copied archive (as test_summon_reveal_wire)
FG_GATE = 1                            # Hall of Challengers
UNKNOWN_UNIT = '99999999'
FG_KEYS = ('h7eY3sAK edy7fq3L 98WfKiyA pn16CNah nBTx56W9 D9wXQI2V d96tuT2E gQInj3H6 e7DK0FQT cuIWp89g TokWs1B3'
           ' 67CApcti RT4CtH5d t4m1RH6Y q08xLEsy GcMD0hy6 e6mY8Z0k PWXu25cg C1HZr3pb X6jf8DUw oS3kTZ2W nj9Lw7mV'
           ' 3NbeC8AB iEFZ6H19 RQ5GnFE2 Ge8Yo32T 0R3qTPK9 mZA7fH2v RXfC31FA cP83zNsv LjY4DfRg T4rewHa9 2pAyFjmZ'
           ' 49sa3sld Fnxab5CN jkldTrhL 9i2xhMaJ iNy0ZU5M btZizNep RVVgyuor U8FCB2Wj NqVAPbLC mgNdrCEe').split()


def ls(unit) -> int:
    return int(UNITS[int(unit)]['oS3kTZ2W'])


def unresolved_unit():
    """A UnitMst unit whose leader skill LeaderSkillMst lacks (50876 in this data), or None."""
    return next((u for u, r in sorted(UNITS.items()) if int(r['oS3kTZ2W']) > 0
                 and int(r['oS3kTZ2W']) not in SKILLS), None)


def evolved_pair():
    """(base, target): an evolution whose two forms have different, resolvable skills."""
    for row in EVO:
        base, target = int(row['pn16CNah']), int(row['74VFwuTd'])
        if base in UNITS and target in UNITS and ls(base) > 0 and ls(target) > 0 and ls(base) != ls(target) \
                and ls(target) in SKILLS:
            return base, target
    return None


def run_repair(fx: Fixture, *extra):
    report = fx.dir / 'repair_report.json'
    if report.exists():
        report.unlink()
    r = subprocess.run([sys.executable, str(REPAIR), '--save', str(fx.db_path), '--json', str(report),
                        '--backup-dir', str(fx.dir / 'repair_backups'), *extra], capture_output=True, text=True)
    data = json.loads(report.read_text(encoding='utf-8')) if report.exists() else {}
    return r.returncode, r.stdout + r.stderr, data


def stored(db, user) -> dict:
    return {r[0]: int(r[1] or 0) for r in db.execute(
        'SELECT user_unit_id, leader_skill_id FROM user_units WHERE user_id=?', (user,))}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19992)
    args = ap.parse_args()
    check = Checker()

    # ---- data the cases depend on --------------------------------------------------
    for unit in (PRESENT_UNIT, GACHA_UNIT, CAPTURE_UNIT, CLI_UNIT):
        check(f'data: {unit} is archived and its UnitMst skill {ls(unit)} is in LeaderSkillMst',
              unit in ARCHIVE and ls(unit) > 0 and ls(unit) in SKILLS)
    pair = evolved_pair()
    check('data: an evolution whose forms carry different skills exists', pair is not None)
    base, target = pair
    loose = unresolved_unit()
    check(f'data: UnitMst names an unresolvable skill on one unit ({loose})', loose is not None)
    check(f'data: {UNKNOWN_UNIT} is not a UnitMst unit', int(UNKNOWN_UNIT) not in UNITS)

    archive = qa_dir('leader_skill_archive')
    if archive.exists():
        shutil.rmtree(archive)
    archive.mkdir(parents=True)
    for f in (ROOT / 'deploy/archive').iterdir():
        if f.is_file() and '.bak' not in f.name:
            shutil.copy2(f, archive / f.name)
    gates = json.loads((archive / 'gacha.json').read_text(encoding='utf-8'))
    gate = next(g for g in gates if g['id'] == GACHA_GATE)
    gate['pool'] = [p for p in gate['pool'] if p['unit_id'] == GACHA_UNIT]
    check(f'data: the copied archive narrows gate {GACHA_GATE} to unit {GACHA_UNIT}', len(gate['pool']) == 1)
    (archive / 'gacha.json').write_text(json.dumps(gates, indent=1, ensure_ascii=False), encoding='utf-8')

    # ---- A. the repair on a seeded copy --------------------------------------------
    fx = Fixture.create(qa_dir('leader_skill'), port=args.port, archive_root=archive)
    db = fx.db()
    user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    template = dict(db.execute('SELECT * FROM user_units WHERE user_id=? ORDER BY user_unit_id LIMIT 1',
                               (user,)).fetchone())
    template.update(eqip_item_id=0, eqip_item_id2=0, eqip_item_frame_id=0, eqip_item_frame_id2=-1,
                    favorite_flg=0, sphere_ext=0)
    template.pop('user_unit_id')

    def seed(unit_id, leader):
        row = dict(template, unit_id=str(unit_id), leader_skill_id=leader)
        return db.execute(f"INSERT INTO user_units ({','.join(row)}) VALUES ({','.join('?' for _ in row)})",
                          tuple(row.values())).lastrowid

    zero_old = db.execute(
        'SELECT user_unit_id, unit_id FROM user_units WHERE user_id=? AND leader_skill_id=0 ORDER BY user_unit_id',
        (user,)).fetchall()
    zero_old = next((r for r in zero_old if int(str(r[1]).split('_')[0]) in UNITS and ls(r[1]) > 0), None)
    check('A: the copy holds an old grant stored at 0 whose UnitMst names a skill', zero_old is not None)
    zero_old, zero_unit = zero_old
    material = next((r[0] for r in db.execute('SELECT user_unit_id, unit_id FROM user_units WHERE user_id=?', (user,))
                     if int(str(r[1]).split('_')[0]) in UNITS and ls(r[1]) == 0), None)
    other = next(s for s in sorted(SKILLS) if s not in (ls(PRESENT_UNIT), 0))
    evolved = seed(target, 0)
    override = seed(PRESENT_UNIT, other)
    unknown = seed(UNKNOWN_UNIT, 0)
    loose_row = seed(loose, 0)
    correct = seed(CAPTURE_UNIT, ls(CAPTURE_UNIT))
    db.execute('UPDATE user_info SET max_unit_count=MAX(COALESCE(max_unit_count,0),200), gems=100, energy=20000,'
               ' energy_full_ts=0, hunter_orbs=5, hunter_orb_rest_ts=0 WHERE id=?', (user,))
    db.commit()
    expected = {zero_old: ls(zero_unit), evolved: ls(target), override: other, unknown: 0, loose_row: 0,
                correct: ls(CAPTURE_UNIT)}
    if material:
        expected[material] = 0
    before = stored(db, user)

    code, out, rep = run_repair(fx)
    cats = {r['user_unit_id']: r['category'] for r in rep.get('rows', [])}
    check('A: dry run exits 0 and writes nothing', code == 0 and stored(db, user) == before
          and rep.get('applied') is False, out[-800:])
    check('A: dry run classifies the seeded rows',
          (cats.get(zero_old), cats.get(evolved), cats.get(override), cats.get(unknown), cats.get(loose_row),
           cats.get(correct)) == ('repair', 'repair', 'preserved', 'unknown', 'unresolved', 'ok'),
          {k: cats.get(k) for k in (zero_old, evolved, override, unknown, loose_row, correct)})
    to_fix = {uu for uu, c in cats.items() if c == 'repair'}
    check('A: only stored-0 rows with a resolvable UnitMst skill are proposed',
          all(before[uu] == 0 for uu in to_fix) and zero_old in to_fix and evolved in to_fix, sorted(to_fix))

    code, out, rep = run_repair(fx, '--apply')
    after = stored(db, user)
    check('A: --apply exits 0 and repairs every proposed row', code == 0 and rep.get('repaired') == len(to_fix),
          out[-800:])
    check('A: the old zero-value grant now carries its UnitMst skill', after[zero_old] == ls(zero_unit),
          (after[zero_old], ls(zero_unit)))
    check(f'A: the evolved form at 0 takes ITS form\'s skill ({ls(target)}), not its base form\'s ({ls(base)})',
          after[evolved] == ls(target), after[evolved])
    check('A: the nonzero override is preserved', after[override] == other, after[override])
    check('A: the unit UnitMst does not know keeps 0 (no guessed skill)', after[unknown] == 0)
    check('A: the unresolvable UnitMst skill is not written', after[loose_row] == 0)
    check('A: rows outside the repair set are byte-for-byte unchanged',
          all(after[uu] == before[uu] for uu in before if uu not in to_fix))
    backup = Path(rep.get('backup') or '')
    if backup.is_file():
        import sqlite3
        bdb = sqlite3.connect(f'file:{backup.as_posix()}?mode=ro', uri=True)
        check('A: the backup holds the pre-repair values', stored(bdb, user) == before)
        bdb.close()
    else:
        check('A: the backup holds the pre-repair values', False, rep.get('backup'))
    code, out, rep = run_repair(fx, '--apply')
    check('A: a second --apply repairs nothing and changes nothing (idempotent)',
          code == 0 and rep.get('repaired') == 0 and stored(db, user) == after, out[-400:])
    expected[zero_old], expected[evolved] = ls(zero_unit), ls(target)

    # ---- B. packets ------------------------------------------------------------------
    roster_before_restart = {}
    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx, user)
        r = c.call('UserInfo')
        roster = {int(u['edy7fq3L']): u for u in r.get('4ceMWH6k', [])}
        check('B: the login roster (4ceMWH6k) lists every unit', set(roster) == set(stored(db, user)),
              len(roster))
        check('B: every roster row carries its stored leader skill', all(
            int(u.get('oS3kTZ2W', -1)) == stored(db, user)[uu] for uu, u in roster.items()))
        for uu, want in expected.items():
            check(f'B: roster uu {uu} unit {roster.get(uu, {}).get("pn16CNah")} sends skill {want}',
                  int(roster.get(uu, {}).get('oS3kTZ2W', -1)) == want, roster.get(uu, {}).get('oS3kTZ2W'))

        def settle():
            # Drogon queues a transaction's COMMIT after the reply; BadgeInfo
            # reads without one on the server's single connection, so its reply
            # arrives only after that COMMIT -- then the save may be read.
            c.call('nJ3A7qFp', 'bGxX67KB', {})

        def new_rows(reply, key='qC2tJs4E'):
            return [u for u in reply.get(key, []) if int(u['edy7fq3L']) not in roster]

        def granted(label, rows, unit):
            settle()
            want = ls(unit)
            ids = [int(u['edy7fq3L']) for u in rows if int(u['pn16CNah']) == unit]
            db_ok = ids and all(stored(db, user).get(i) == want for i in ids)
            check(f'B: {label}: the reply\'s new unit {unit} carries skill {want}', ids and all(
                int(u['oS3kTZ2W']) == want for u in rows if int(u['pn16CNah']) == unit),
                  [(u.get('pn16CNah'), u.get('oS3kTZ2W')) for u in rows])
            check(f'B: {label}: stored {want} in user_units', db_ok, [stored(db, user).get(i) for i in ids])
            roster.update({int(u['edy7fq3L']): u for u in rows})
            return ids

        # PresentReceipt (present type 6 -> fromArchivedUnit)
        pid = db.execute("INSERT INTO user_presents (user_id, present_type, target_id, target_cnt, target_param,"
                         " receipt_type, description, present_date, receipt_date, is_receipt)"
                         " VALUES (?, 6, ?, 1, '', 0, 'leader skill test', strftime('%s','now'), 0, 0)",
                         (user, str(PRESENT_UNIT))).lastrowid
        db.commit()
        reply = c.call('PresentReceipt', body={'o6uWU0Z7': [{'i1WQkh4G': '0', 'S1B82FHK': str(pid)}]})
        check('B: present claimed', 'error' not in reply, reply.get('error'))
        granted('PresentReceipt', new_rows(reply), PRESENT_UNIT)

        # GachaAction on the narrowed gate
        reply = c.call('GachaAction', body={'6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
                                             '1IR86sAv': [{'7Ffmi96v': str(GACHA_GATE), 'a329kbl8': '1',
                                                           '324b023k': '0'}]})
        check('B: summon accepted', 'error' not in reply, reply.get('error'))
        granted('GachaAction', new_rows(reply), GACHA_UNIT)

        # A capture reported at MissionEnd (mission 10, cleared on the copied save)
        start = c.call('MissionStart', body=mission_start_body(10))
        check('B: mission 10 started', 'error' not in start, start.get('error'))
        reply = c.call('MissionEnd', body=mission_end_body(10, status=2, units=f'{CAPTURE_UNIT}:1:1',
                                                           serial=started_serial(start)))
        check('B: mission 10 ended with the capture', 'error' not in reply, reply.get('error'))
        granted('MissionEnd capture', new_rows(reply), CAPTURE_UNIT)

        # CLI addunit through the server's own debug console
        with open(r'\\.\pipe\gimudebug_' + str(server.proc.pid), 'r+b', buffering=0) as pipe:
            def response():
                data = bytearray()
                while True:
                    b = pipe.read(1)
                    if not b:
                        raise RuntimeError('debug pipe disconnected')
                    if b == b'\x01':
                        return data.decode(errors='replace')
                    data.extend(b)
            response()
            pipe.write(f'addunit {CLI_UNIT}\n'.encode())
            said = response()
        check('B: CLI addunit accepted', f'Added unit {CLI_UNIT}' in said, said)
        r = c.call('UserInfo')
        granted('CLI addunit (login roster)', [u for u in r.get('4ceMWH6k', []) if int(u['edy7fq3L']) not in roster],
                CLI_UNIT)

        # Frontier Gate: the client uploads its units (createBodySaveDataUnit reads
        # UserUnitInfo::getLeaderSkillID); the server echoes and persists them.
        r = c.call('UserInfo')
        roster = {int(u['edy7fq3L']): u for u in r.get('4ceMWH6k', [])}
        party = [zero_old, evolved, override]
        units = [{k: v for k, v in roster[uu].items() if k in FG_KEYS} for uu in party]
        reply = c.call('l3lkDBSc', 'vYPCD34q', {'Mg8K8Y1a': [{'hBNPQAU0': str(FG_GATE), 'NungTq5g': '0'}],
                                               'MxmCpDRC': [], '52xDBRGr': units})
        echoed = {int(u['edy7fq3L']): int(u['oS3kTZ2W']) for u in reply.get('52xDBRGr', [])}
        check('B: FrontierGateStart opened a run', 'error' not in reply and 'Mg8K8Y1a' in reply,
              reply.get('error') or list(reply))
        check('B: FrontierGateStart echoes each uploaded unit with its skill',
              echoed == {uu: expected[uu] for uu in party}, echoed)
        num = reply.get('Mg8K8Y1a', {}).get('2FQdEbG3', '0')
        reply = c.call('uiFIMUH6', 'ZiosS4cd', {'Mg8K8Y1a': [{'2FQdEbG3': str(num)}]})
        handed = {int(u['edy7fq3L']): int(u['oS3kTZ2W']) for u in reply.get('52xDBRGr', [])}
        check('B: FrontierGateContinue hands the same units and skills back',
              handed == {uu: expected[uu] for uu in party}, (reply.get('error'), handed))
        settle()
        roster_before_restart = stored(db, user)

    # ---- C. restart and a repeated repair ------------------------------------------
    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx, user)
        r = c.call('UserInfo')
        roster = {int(u['edy7fq3L']): int(u['oS3kTZ2W']) for u in r.get('4ceMWH6k', [])}
        check('C: after a restart the roster sends the same skills', roster == roster_before_restart,
              {k: (roster.get(k), v) for k, v in roster_before_restart.items() if roster.get(k) != v})
    code, out, rep = run_repair(fx, '--apply')
    check('C: a repair after play and restart finds nothing to change',
          code == 0 and rep.get('repaired') == 0 and rep['counts']['repair'] == 0
          and stored(db, user) == roster_before_restart, out[-400:])
    db.close()
    return check.summary('leader skill grants')


if __name__ == '__main__':
    sys.exit(main())
