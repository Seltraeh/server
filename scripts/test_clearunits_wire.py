"""clearunits (debug console): the roster it leaves must be one the client can draw.

    python scripts/test_clearunits_wire.py SERVER_EXE

Every case sends the real console command over the server's debug pipe, on a
COPY of the live save (isolated port 19977, the copy seeded before the server
starts), then reads what the client receives -- UserInfo's units (4ceMWH6k),
squads (dX7S2Lc1), team (fEi17cnx) and warehouse (9wjrh74P) -- and the
database.  Session 11's small-roster fixture crashed the client: deck 0 still
named deleted units, and PartyEditTopScene2 dereferences a slot's unit without
a null check (x86 +0xA6ECFC in all three Run C dumps; arm64 setUnitAnime).

  A  the live copy, seeded with every reference a roster can hold: a squad whose
     leader goes and whose member is the kept unit (promoted in place), a
     campaign squad wholly of doomed units, a campaign squad led by the kept
     unit with a doomed member, DBB bonds naming doomed units, and the live
     save's own spheres worn by doomed units.  After clearunits: one unit;
     roster_problems() is empty; no dangling row in any table; each sphere the
     deleted units wore is back in stock under its own row id.
  B  clearunits again: nothing deleted, and no row of any roster table changes.
  C  the damage the old command did (units deleted, squads untouched): the new
     command repairs it -- the active squad is led by the kept unit.
  D  no units at all: nothing changes, and the server keeps answering.
  E  two users: only the console's user (the first user_info row) is cleared;
     the other user's units, squads and bonds are untouched.
"""
from __future__ import annotations

import sqlite3
import sys
from contextlib import closing
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import (Checker, Client, DebugConsole, Fixture, IsolatedServer,  # noqa: E402
                        dangling_unit_references, qa_dir, roster_problems)

PORT = 19977
BADGE_INFO = ('nJ3A7qFp', 'bGxX67KB')   # answered only after earlier commits: a barrier before reading the save
ROSTER_TABLES = ('user_units', 'user_decks', 'user_campaign_decks', 'user_unit_dbb', 'user_warehouse_rows', 'user_items')


def dump(db: sqlite3.Connection, user: str) -> dict:
    """Every row of the roster tables for one user, sorted (a byte-level comparison)."""
    out = {}
    for t in ROSTER_TABLES:
        col = 'user_id'
        out[t] = sorted(tuple(r) for r in db.execute(f'SELECT * FROM {t} WHERE {col} = ?', (user,)))
    return out


def fixture(name: str, seed) -> tuple[Fixture, str]:
    fx = Fixture.create(qa_dir('clearunits') / name, port=PORT)
    with closing(sqlite3.connect(fx.db_path)) as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        seed(db, user)
        db.commit()
    return fx, user


def user_info(fx: Fixture, user: str) -> dict:
    client = Client(fx, user)
    reply = client.call('UserInfo')
    client.call(*BADGE_INFO, body={})
    return reply


def case_a_and_b(exe: str, check: Checker) -> None:
    state = {}

    def seed(db, user):
        units = [r[0] for r in db.execute('SELECT user_unit_id FROM user_units WHERE user_id=? ORDER BY user_unit_id', (user,))]
        keep, x, y, z = units[0], units[-1], units[-2], units[-3]
        state.update(keep=keep, x=x, y=y, z=z, before=len(units))
        db.execute('DELETE FROM user_decks WHERE user_id=? AND deck_type=1 AND deck_num=3', (user,))
        db.execute('INSERT INTO user_decks VALUES (?,?,1,3,0,2)', (user, x))           # doomed leader in slot 2
        db.execute('INSERT INTO user_decks VALUES (?,?,1,3,1,0)', (user, keep))        # kept unit as a member, slot 0
        db.execute('DELETE FROM user_campaign_decks WHERE user_id=?', (user,))
        db.execute('INSERT INTO user_campaign_decks VALUES (?,0,0,?,2)', (user, y))    # all doomed
        db.execute('INSERT INTO user_campaign_decks VALUES (?,0,1,?,1)', (user, z))
        db.execute('INSERT INTO user_campaign_decks VALUES (?,1,0,?,2)', (user, keep)) # kept leader, doomed member
        db.execute('INSERT INTO user_campaign_decks VALUES (?,1,1,?,0)', (user, y))
        db.execute('DELETE FROM user_unit_dbb WHERE user_id=?', (user,))
        db.execute("INSERT INTO user_unit_dbb VALUES (?,?,?,'190501',1)", (user, y, z))
        db.execute("INSERT INTO user_unit_dbb VALUES (?,?,?,'190502',2)", (user, keep, x))
        state['worn'] = [tuple(r) for r in db.execute(
            'SELECT w.instance_id, w.item_id FROM user_warehouse_rows w WHERE w.user_id=? AND w.equip_unit_id != 0'
            ' AND w.equip_unit_id != ?', (user, keep))]
        state['stock'] = dict(db.execute('SELECT item_id, item_num FROM user_items WHERE user_id=?', (user,)).fetchall())
        state['active'] = db.execute('SELECT active_deck FROM user_info WHERE id=?', (user,)).fetchone()[0]

    fx, user = fixture('a_live', seed)
    check('A: the live copy has spheres worn by units that will be deleted', len(state['worn']) > 0, state['worn'])
    with IsolatedServer(exe, fx) as server, DebugConsole(server) as console:
        said = console.run('clearunits')
        reply = user_info(fx, user)
        print(said.strip())
        check('A: the console reports the clear and what it repaired',
              f'Cleared {state["before"] - 1} unit(s); kept user unit {state["keep"]}' in said
              and 'Returned' in said and 'Now at 1/' in said, said)
        units = [str(u['edy7fq3L']) for u in reply.get('4ceMWH6k', [])]
        check('A: UserInfo carries exactly the kept unit', units == [str(state['keep'])], units)
        problems = roster_problems(reply)
        check('A: every squad slot resolves, one leader each, active squad led by an owned unit', not problems, problems)
        slots = {(str(s['U9ABSYEp']), str(s['zsiAn9P1']), str(s['XuJL4pc5'])): str(s['gr48vsdJ'])
                 for s in reply.get('dX7S2Lc1', [])}
        check('A: deck 3 promoted the kept member in place (slot 0 is now its leader; the doomed leader is gone)',
              slots.get(('1', '3', '0')) == '0' and ('1', '3', '2') not in slots,
              {k: v for k, v in slots.items() if k[1] == '3'})
        active = [s for s in reply.get('dX7S2Lc1', []) if str(s['zsiAn9P1']) == str(state['active'])]
        check('A: the active squad is exactly the kept unit as leader',
              [(str(s['edy7fq3L']), str(s['gr48vsdJ'])) for s in active] == [(str(state['keep']), '0')], active)
        live_rows = {int(w['n6E8iMf3']): int(w['wgV86x1q']) for w in reply.get('9wjrh74P', [])}
        check('A: every sphere the deleted units wore is a live row again (item_num 1, own row id)',
              all(live_rows.get(i) == 1 for i, _item in state['worn']), {i: live_rows.get(i) for i, _ in state['worn']})
        with closing(sqlite3.connect(fx.db_path)) as db:
            dangling = dangling_unit_references(db, user)
            check('A: no table names a deleted unit', not dangling, dangling)
            campaign = sorted(tuple(r) for r in db.execute(
                'SELECT deck_num, member_type, user_unit_id, disporder FROM user_campaign_decks WHERE user_id=?', (user,)))
            check('A: campaign squads: the doomed one is re-led by the kept unit in its old leader slot; the other keeps its leader',
                  campaign == [(0, 0, state['keep'], 2), (1, 0, state['keep'], 2)], campaign)
            bonds = db.execute('SELECT COUNT(*) FROM user_unit_dbb WHERE user_id=?', (user,)).fetchone()[0]
            check('A: both DBB bonds naming deleted units are gone', bonds == 0, bonds)
            stock = dict(db.execute('SELECT item_id, item_num FROM user_items WHERE user_id=?', (user,)).fetchall())
            returned = {}
            for _i, item in state['worn']:
                returned[item] = returned.get(item, 0) + 1
            check('A: each returned sphere is back in stock (user_items +1 per worn copy)',
                  all(stock.get(item, 0) == state['stock'].get(item, 0) + n for item, n in returned.items()),
                  {item: (state['stock'].get(item, 0), stock.get(item, 0)) for item in returned})
            before_b = dump(db, user)
        said_b = console.run('clearunits')    # its commit is durable before the console answers
        with closing(sqlite3.connect(fx.db_path)) as db:
            after_b = dump(db, user)
        reply_b = user_info(fx, user)
        check('B: clearunits again deletes nothing', 'Cleared 0 unit(s)' in said_b, said_b)
        check('B: and changes no row of any roster table', after_b == before_b)
        check('B: the payload is still drawable', not roster_problems(reply_b), roster_problems(reply_b))


def case_c(exe: str, check: Checker) -> None:
    def seed(db, user):
        # What the old clearunits did: delete every unit but the oldest, touch nothing else.
        db.execute('DELETE FROM user_units WHERE user_id=? AND user_unit_id <>'
                   ' (SELECT MIN(user_unit_id) FROM user_units WHERE user_id=?)', (user, user))

    fx, user = fixture('c_old_damage', seed)
    with closing(sqlite3.connect(fx.db_path)) as db:
        broken = dangling_unit_references(db, user)
    check('C: the old command left squads naming deleted units', 'user_decks' in broken, broken)
    with IsolatedServer(exe, fx) as server, DebugConsole(server) as console:
        said = console.run('clearunits')
        reply = user_info(fx, user)
    check('C: the new command repairs that roster without deleting anything', 'Cleared 0 unit(s)' in said, said)
    check('C: the payload is drawable again', not roster_problems(reply), roster_problems(reply))
    with closing(sqlite3.connect(fx.db_path)) as db:
        check('C: no table names a deleted unit', not dangling_unit_references(db, user), dangling_unit_references(db, user))


def case_d(exe: str, check: Checker) -> None:
    def seed(db, user):
        db.execute('DELETE FROM user_units WHERE user_id=?', (user,))

    fx, user = fixture('d_empty', seed)
    with closing(sqlite3.connect(fx.db_path)) as db:
        before = dump(db, user)
    with IsolatedServer(exe, fx) as server, DebugConsole(server) as console:
        said = console.run('clearunits')
        badge = Client(fx, user).call(*BADGE_INFO, body={})
    check('D: with no units the command changes nothing and says so', 'nothing was changed' in said, said)
    check('D: the server keeps answering', 'error' not in badge, badge)
    with closing(sqlite3.connect(fx.db_path)) as db:
        check('D: no roster row changed', dump(db, user) == before)


def case_e(exe: str, check: Checker) -> None:
    state = {}

    def seed(db, user):
        other = 'ZZCLR002'
        cols = [r[1] for r in db.execute('PRAGMA table_info(user_info)')]
        select = ', '.join('?' if c == 'id' else ("'ZZGUMI02'" if c == 'gumi_user_id' else c) for c in cols)
        db.execute(f'INSERT INTO user_info ({", ".join(cols)}) SELECT {select} FROM user_info WHERE id = ?', (other, user))
        top = db.execute('SELECT MAX(user_unit_id) FROM user_units').fetchone()[0]
        ucols = [r[1] for r in db.execute('PRAGMA table_info(user_units)')]
        for n, src in enumerate([r[0] for r in db.execute(
                'SELECT user_unit_id FROM user_units WHERE user_id=? ORDER BY user_unit_id LIMIT 3', (user,))], 1):
            sel = ', '.join('?' if c == 'user_id' else ('?' if c == 'user_unit_id' else c) for c in ucols)
            args = [other if c == 'user_id' else top + n for c in ucols if c in ('user_id', 'user_unit_id')]
            db.execute(f'INSERT INTO user_units ({", ".join(ucols)}) SELECT {sel} FROM user_units WHERE user_unit_id = ?',
                       (*args, src))
        db.execute('INSERT INTO user_decks VALUES (?,?,1,0,0,2)', (other, top + 1))
        db.execute('INSERT INTO user_decks VALUES (?,?,1,0,1,0)', (other, top + 2))
        db.execute("INSERT INTO user_unit_dbb VALUES (?,?,?,'190501',1)", (other, top + 2, top + 3))
        state['other'] = other

    fx, user = fixture('e_two_users', seed)
    with closing(sqlite3.connect(fx.db_path)) as db:
        other_before = dump(db, state['other'])
        first = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    check('E: the console user is the first user_info row', first == user, (first, user))
    with IsolatedServer(exe, fx) as server, DebugConsole(server) as console:
        said = console.run('clearunits')
        reply = user_info(fx, user)
    check('E: the console user is cleared', 'Now at 1/' in said and len(reply.get('4ceMWH6k', [])) == 1, said)
    with closing(sqlite3.connect(fx.db_path)) as db:
        check('E: the other user\'s units, squads and bonds are untouched', dump(db, state['other']) == other_before)


def main() -> int:
    exe = sys.argv[1]
    check = Checker()
    case_a_and_b(exe, check)
    case_c(exe, check)
    case_d(exe, check)
    case_e(exe, check)
    return check.summary('clearunits')


if __name__ == '__main__':
    raise SystemExit(main())
