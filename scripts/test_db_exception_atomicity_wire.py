"""A request that fails after its writes keeps nothing of them.

    python scripts/test_db_exception_atomicity_wire.py EXE [--port 19999]

Drogon commits a transaction from its destructor when nothing committed or
rolled it back.  The handlers commit explicitly before replying
(gme::commitTransaction), so what still reached that destructor commit was a
request that FAILED inside its transaction -- a non-database exception after
some writes, or a refusal after writes with no rollback -- and its writes were
committed although the client was told it failed.  gme::beginTransaction now
hands out a handle that rolls back, when the request lets go of it, whatever
was not explicitly committed.

The debug console's `dbfault` makes the next commitTransaction throw a
non-database exception instead of committing: a handler failing after ALL of
its writes, the worst case.  Six transactional handlers are each sent once with
the fault armed and once without:

  ItemMix (craft)      materials, Karma, product, warehouse rows
  ItemSell (sale)      item stack, Zel, warehouse rows
  PresentReceipt       the present, a new unit, the dictionary
  GachaAction (summon) gems, a new unit, the dictionary
  MissionEnd           rewards, progression, the open battle
  FixGiftInfo          the wanted gift (it catches only database exceptions)
and, converted this session from separate autocommit statements to one
transaction each:
  MissionStart         energy, the run record, the open battle
  DailyLogin (spin)    the prize and the spin it uses
  MysteryBoxClaim      the rewards and the opened chest (if one is provisioned)
  GuildCreate          the Zel fee and the guild
  CampaignReceipt      the Grand Quest claim and its rewards

With the fault: the client gets an error, the server logs the exception, and
the save is unchanged table for table.  Without it: the same request succeeds
and does change the save (so the comparison has teeth).  Tables a read-only
request changes are measured first and left out.  Then a restart: exactly the
successful requests persisted, and the server serves on.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sqlite3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import (ROOT, Checker, Client, Fixture, IsolatedServer, mission_end_body,  # noqa: E402
                        mission_start_body, qa_dir, started_serial)

RECIPE, PRODUCT, MATERIAL = 2005, 30000, 10400          # Famous Blade: 5 x 10400 + 500 Karma
PRESENT_UNIT, GACHA_UNIT = 20011, 51247                 # Selena, Lukroar
GACHA_GATE = 99008                                      # narrowed to GACHA_UNIT in a copied archive
MISSION = 10
GRAND_QUEST = '5000000'                                 # first-clear bonus: gems (grand_mission_reward_mst)


def fingerprint(path: Path) -> dict[str, str]:
    """Every table's rows, hashed (order-independent), read through a read-only connection."""
    with sqlite3.connect(f'file:{path}?mode=ro', uri=True, timeout=10) as conn:
        tables = [r[0] for r in conn.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name")]
        out = {}
        for t in tables:
            rows = sorted(repr(r) for r in conn.execute(f'SELECT * FROM "{t}"'))
            out[t] = hashlib.sha256('\n'.join(rows).encode()).hexdigest()
        return out


def differing(a: dict, b: dict, skip: set) -> list[str]:
    return sorted(t for t in set(a) | set(b) if t not in skip and a.get(t) != b.get(t))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19999)
    ap.add_argument('--triggers-only', action='store_true',
                    help='only the trigger cases, which need no debug-console fault (any build)')
    args = ap.parse_args()
    check = Checker()

    archive = qa_dir('db_exception_atomicity_archive')
    if archive.exists():
        shutil.rmtree(archive)
    archive.mkdir(parents=True)
    for f in (ROOT / 'deploy/archive').iterdir():
        if f.is_file():
            shutil.copy2(f, archive / f.name)
    gates = json.loads((archive / 'gacha.json').read_text(encoding='utf-8'))
    gate = next(g for g in gates if g['id'] == GACHA_GATE)
    gate['pool'] = [p for p in gate['pool'] if p['unit_id'] == GACHA_UNIT]
    (archive / 'gacha.json').write_text(json.dumps(gates, indent=1, ensure_ascii=False), encoding='utf-8')

    fx = Fixture.create(qa_dir('db_exception_atomicity'), port=args.port, archive_root=archive)
    with fx.db() as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,1000)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=1000', (user, MATERIAL))
        db.execute('DELETE FROM user_items WHERE user_id=? AND item_id=?', (user, PRODUCT))
        db.execute('DELETE FROM user_warehouse_rows WHERE user_id=? AND item_id=?', (user, PRODUCT))
        for col in ('eqip_item_id', 'eqip_item_id2'):
            frame = col.replace('item_id', 'item_frame_id')
            db.execute(f'UPDATE user_units SET {col}=0,{frame}=0 WHERE user_id=? AND {col}=?', (user, PRODUCT))
        db.execute('UPDATE user_info SET karma=1000000, zel=1000000, gems=100, energy=20000,'
                   ' max_warehouse_count=3200, max_unit_count=MAX(COALESCE(max_unit_count,0),200) WHERE id=?',
                   (user,))
        db.execute('INSERT INTO user_town_facilities(user_id,facility_id,lv) VALUES (?,1,1)'
                   ' ON CONFLICT(user_id,facility_id) DO UPDATE SET lv=1', (user,))
        db.execute("INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,'21',2)"
                   ' ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2', (user,))
        db.execute('DELETE FROM user_daily_spin WHERE user_id=?', (user,))
        db.execute('DELETE FROM user_guild_members WHERE member_id=?', (user,))
        db.execute('DELETE FROM user_guilds WHERE owner_user_id=?', (user,))
        db.execute("INSERT INTO user_campaign_missions(user_id,mission_id,state,attain_percent,reward_claimed)"
                   " VALUES (?,?,2,100,0) ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2,"
                   " attain_percent=100, reward_claimed=0", (user, GRAND_QUEST))
        present = db.execute("INSERT INTO user_presents (user_id, present_type, target_id, target_cnt, target_param,"
                             " receipt_type, description, present_date, receipt_date, is_receipt)"
                             " VALUES (?, 6, ?, 1, '', 0, 'atomicity test', strftime('%s','now'), 0, 0)",
                             (user, str(PRESENT_UNIT))).lastrowid
        db.commit()

    def owned_units(unit_id: int) -> int:
        with sqlite3.connect(f'file:{fx.db_path}?mode=ro', uri=True, timeout=10) as conn:
            return conn.execute('SELECT COUNT(*) FROM user_units WHERE user_id=? AND unit_id=?',
                                (user, unit_id)).fetchone()[0]

    successes = {}
    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx, user)
        pipe = open(r'\\.\pipe\gimudebug_' + str(server.proc.pid), 'r+b', buffering=0)

        def console(command: str) -> str:
            pipe.write(command.encode() + b'\n')
            data = bytearray()
            while True:
                b = pipe.read(1)
                if not b:
                    raise RuntimeError('debug pipe disconnected')
                if b == b'\x01':
                    return data.decode(errors='replace')
                data.extend(b)

        def settle():
            # BadgeInfo reads without a transaction on the server's one
            # connection: its reply arrives only after anything queued there.
            return c.call('nJ3A7qFp', 'bGxX67KB', {})

        # Drain the console's greeting.
        data = bytearray()
        while (b := pipe.read(1)) and b != b'\x01':
            data.extend(b)

        c.call('UserInfo')                       # the warehouse rows exist from here
        settle()
        base = fingerprint(fx.db_path)
        settle()
        volatile = set(differing(base, fingerprint(fx.db_path), set()))
        check('setup: a read-only request changes no table (or only these, left out)', True, sorted(volatile))

        def trigger_case(label: str, trigger: str, send):
            """A statement in the middle of the request fails (a trigger RAISEs):
            nothing of the request may be kept, and it must not be acknowledged."""
            settle()
            before = fingerprint(fx.db_path)
            with fx.db() as db:
                db.execute(f"CREATE TRIGGER qc_atomicity BEFORE {trigger} BEGIN SELECT RAISE(ABORT, 'QC'); END")
                db.commit()
            failed = send()
            settle()
            with fx.db() as db:
                db.execute('DROP TRIGGER qc_atomicity')
                db.commit()
            changed = differing(before, fingerprint(fx.db_path), volatile)
            check(f'{label}: a statement failing part-way is answered with an error', 'error' in failed, failed)
            check(f'{label}: and nothing of the request was kept', not changed, changed)

        # A failing statement in the middle of the request.  Before this
        # session these handlers ran statement by statement: the energy, run
        # record and helper landed without the battle; the claim and its items
        # landed without the Gems, and the receipt answered as paid.
        trigger_case('MissionStart, the open-battle update fails', 'UPDATE OF open_mission_id ON user_info',
                     lambda: c.call('MissionStart', body=mission_start_body(MISSION)))
        trigger_case('CampaignReceipt, the Gem credit fails', 'UPDATE OF gems ON user_info',
                     lambda: c.call('5Imq3wC0', '4DAgP80B', {'j28VNcUW': GRAND_QUEST}))
        if args.triggers_only:
            pipe.close()
            return check.summary('db exception atomicity (triggers only)')

        def case(label: str, send, effect):
            """Send with the fault armed, then without; returns the successful reply."""
            settle()
            before = fingerprint(fx.db_path)
            said = console('dbfault')
            failed = send()
            settle()
            after = fingerprint(fx.db_path)
            check(f'{label}: the debug console armed the fault', 'next commit throws' in said, said.strip())
            check(f'{label}: the failed request is answered with an error, not acknowledged', 'error' in failed,
                  failed)
            changed = differing(before, after, volatile)
            check(f'{label}: nothing of the failed request was kept (every table unchanged)', not changed, changed)
            ok = send()
            settle()
            final = fingerprint(fx.db_path)
            check(f'{label}: the same request then succeeds', 'error' not in ok, ok.get('error'))
            changed = differing(after, final, volatile)
            check(f'{label}: and does change the save ({", ".join(changed[:4])})', bool(changed) and effect(), changed)
            successes[label] = ok
            return ok

        with fx.db() as db:
            def stock(item):
                row = db.execute('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?', (user, item)).fetchone()
                return row[0] if row else 0
            materials_before = stock(MATERIAL)

        case('ItemMix', lambda: c.call('ItemMix', body={'JTf2jY5o': [{'4HqhTf3a': f'{RECIPE}:1'}]}),
             lambda: True)

        with sqlite3.connect(f'file:{fx.db_path}?mode=ro', uri=True, timeout=10) as conn:
            row = conn.execute('SELECT instance_id FROM user_warehouse_rows WHERE user_id=? AND item_id=?'
                               ' AND favorite_flg=0 AND equip_unit_id=0 AND item_num>=1'
                               ' ORDER BY instance_id LIMIT 1', (user, MATERIAL)).fetchone()
        check('setup: the material has an unlocked, unequipped warehouse row to sell from', row is not None, row)
        if row:
            case('ItemSell', lambda: c.call('qDQerU74', '73aFNjPu',
                                            {'M73i1c5U': [{'n6E8iMf3': str(row[0]), 'wgV86x1q': '1'}]}),
                 lambda: True)

        units_before = owned_units(PRESENT_UNIT)
        case('PresentReceipt',
             lambda: c.call('PresentReceipt', body={'o6uWU0Z7': [{'i1WQkh4G': '0', 'S1B82FHK': str(present)}]}),
             lambda: owned_units(PRESENT_UNIT) == units_before + 1)

        gacha_before = owned_units(GACHA_UNIT)
        case('GachaAction',
             lambda: c.call('GachaAction', body={'6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
                                                 '1IR86sAv': [{'7Ffmi96v': str(GACHA_GATE), 'a329kbl8': '1',
                                                               '324b023k': '0'}]}),
             lambda: owned_units(GACHA_UNIT) == gacha_before + 1)

        def reads(sql, *a):
            with sqlite3.connect(f'file:{fx.db_path}?mode=ro', uri=True, timeout=10) as conn:
                row = conn.execute(sql, a).fetchone()
                return row[0] if row else None

        # Converted from autocommit statements to one transaction each:
        energy_before = reads('SELECT energy FROM user_info WHERE id=?', user)
        start = case('MissionStart', lambda: c.call('MissionStart', body=mission_start_body(MISSION)),
                     lambda: reads('SELECT open_mission_id FROM user_info WHERE id=?', user) == MISSION)
        case('DailyLogin (spin)', lambda: c.call('4aClzokO', 'stI81haQ', {}),
             lambda: (reads('SELECT spins_used FROM user_daily_spin WHERE user_id=?', user) or 0) >= 1)
        boxes = c.call('pAJ2Xesw', 'DaswA3rE', {}).get('d0ajLeRi') or []
        if boxes:
            box = str(boxes[0]['rEFRefr8'])
            case('MysteryBoxClaim', lambda: c.call('2paswUpR', 'kadRadU5', {'d0ajLeRi': [{'rEFRefr8': box}]}),
                 lambda: True)
        else:
            check('MysteryBoxClaim: no chest provisioned on this save (case skipped)', True)
        zel_before = reads('SELECT zel FROM user_info WHERE id=?', user)
        case('GuildCreate', lambda: c.call('g298Da10', 'G23Bd01d', {'IkdSufj5': [
                 {'sD73jd20': '0', 's35idar9': 'QC Atomicity', 'qp37xTDh': 'test', 'dDKCN293': '1'}]}),
             lambda: reads('SELECT COUNT(*) FROM user_guilds WHERE owner_user_id=?', user) == 1
             and reads('SELECT zel FROM user_info WHERE id=?', user) < zel_before)
        case('CampaignReceipt', lambda: c.call('5Imq3wC0', '4DAgP80B', {'j28VNcUW': GRAND_QUEST}),
             lambda: reads('SELECT reward_claimed FROM user_campaign_missions WHERE user_id=? AND mission_id=?',
                           user, GRAND_QUEST) == 1)
        check('MissionStart: the successful start spent energy (the failed one spent none, above)',
              (reads('SELECT energy FROM user_info WHERE id=?', user) or 0) < energy_before,
              (energy_before, reads('SELECT energy FROM user_info WHERE id=?', user)))
        if 'error' not in start:
            serial = started_serial(start)
            case('MissionEnd', lambda: c.call('MissionEnd', body=mission_end_body(MISSION, status=2, zel=100,
                                                                                   karma=50, serial=serial)),
                 lambda: True)

        # FixGiftInfo catches only database exceptions, and its transaction
        # lives inside that try: a non-database exception unwinds it -- the
        # path that reached Drogon's destructor COMMIT with nothing to stop it.
        with sqlite3.connect(f'file:{fx.db_path}?mode=ro', uri=True, timeout=10) as conn:
            current = conn.execute('SELECT want_gift FROM user_info WHERE id=?', (user,)).fetchone()[0]
        wanted = '3' if str(current) == '2' else '2'

        def want_gift():
            with sqlite3.connect(f'file:{fx.db_path}?mode=ro', uri=True, timeout=10) as conn:
                return str(conn.execute('SELECT want_gift FROM user_info WHERE id=?', (user,)).fetchone()[0])
        case('FixGiftInfo', lambda: c.call('gLRIn74v', '15gTE9ft',
                                           {'Gg5c3eM6': [{'s2WnRw9N': wanted, '23Xi0jom': '', 'qD74LPXb': ''}]}),
             lambda: want_gift() == wanted)

        log = server.log_text()
        faults = log.count('injected fault before COMMIT')
        expected = 10 + (1 if boxes else 0)
        check(f'every faulted request reached the server as a handler exception ({faults})', faults >= expected,
              (faults, expected))
        check('no commit ran from a destructor and failed', 'Transaction submission failed' not in log, '')
        pipe.close()

    # Restart: exactly the successful requests persisted.
    with sqlite3.connect(f'file:{fx.db_path}?mode=ro', uri=True, timeout=10) as conn:
        product = conn.execute('SELECT COALESCE(SUM(item_num),0) FROM user_items WHERE user_id=? AND item_id=?',
                               (user, PRODUCT)).fetchone()[0]
        material = conn.execute('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?',
                                (user, MATERIAL)).fetchone()[0]
        claimed = conn.execute('SELECT is_receipt FROM user_presents WHERE rowid=?', (present,)).fetchone()[0]
    check('restart: one craft persisted (one product)', product == 1, product)
    check('restart: materials = before - one craft (5) - one sale (1)', material == materials_before - 5 - 1,
          (materials_before, material))
    check('restart: the present is claimed exactly once', int(claimed) == 1 and owned_units(PRESENT_UNIT) == units_before + 1,
          (claimed, owned_units(PRESENT_UNIT)))
    check('restart: one summoned unit', owned_units(GACHA_UNIT) == gacha_before + 1, owned_units(GACHA_UNIT))
    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx, user)
        info = c.call('UserInfo')
        check('restart: the server answers UserInfo', 'error' not in info, info.get('error'))
        again = c.call('ItemMix', body={'JTf2jY5o': [{'4HqhTf3a': f'{RECIPE}:1'}]})
        check('restart: and crafts again', 'error' not in again, again.get('error'))
    return check.summary('db exception atomicity')


if __name__ == '__main__':
    sys.exit(main())
