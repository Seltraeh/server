"""Bring a run's copied save to "TARGET is next": every prerequisite cleared, TARGET itself not.

    python scripts/qa/prep_chain.py --qa-dir DIR --run NAME --target MISSION [--units 10017,40987,...]

Prerequisites are the server's story rule (PermitPlace.cpp storyMissionUnlocked): a mission needs every id in
its own need list (F_MISSION_MST HSRhkf70), its dungeon's (F_DUNGEON_MST) and that dungeon's area's
(F_AREA_MST), each a comma list where 0 means none.  The closure follows ALL of them, transitively.  It
refuses (QaError, nothing written): a need naming no mission row, a cycle, a target already cleared in the
copy, and a copy whose user_info does not hold exactly one account.  Clears are written the way the debug
CLI's clearmission does (state 2, 100 %, one clear) in one transaction.  Scenario markers are NOT written: the
client shows the presentations these clears imply on its first visit, then stores its own markers.

--units copies those species' strongest rows from the live save (opened read-only) into the copy, owned by
its account with spheres emptied, as the active squad in the given order (the first leads).
"""
from __future__ import annotations

import argparse
import json
import sqlite3
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import qa_run as q  # noqa: E402

MST = q.REPO / 'deploy' / 'mst'


def ids(text) -> set[int]:
    out = set()
    for part in str(text or '').split(','):
        part = part.strip()
        if part:
            try:
                out.add(int(part))
            except ValueError:
                raise q.QaError(f'need list {text!r} is not a comma list of ids') from None
    out.discard(0)
    return out


def rows(name: str) -> list[dict]:
    d = json.loads((MST / f'{name}.json').read_text(encoding='utf-8'))
    return next(v for v in d.values() if isinstance(v, list))


def need_graph(missions: list[dict], dungeons: list[dict], areas: list[dict]) -> dict[int, set[int]]:
    """mission id -> every mission id it needs directly (own, dungeon and area need lists)."""
    area_need = {int(a['VjCY7rX4']): ids(a.get('HSRhkf70')) for a in areas}
    dungeon_need = {int(d['MHx05sXt']): ids(d.get('HSRhkf70')) | area_need.get(int(d['VjCY7rX4']), set())
                    for d in dungeons}
    return {int(m['j28VNcUW']): ids(m.get('HSRhkf70')) | dungeon_need.get(int(m.get('MHx05sXt') or 0), set())
            for m in missions}


def closure(graph: dict[int, set[int]], target: int) -> list[int]:
    """Every mission the target needs, transitively, prerequisites first; QaError on a missing row or cycle."""
    if target not in graph:
        raise q.QaError(f'mission {target} has no row')
    order, state = [], {}             # state: 1 = on the current path, 2 = done
    stack = [(target, iter(sorted(graph[target])))]
    state[target] = 1
    while stack:
        node, it = stack[-1]
        nxt = next(it, None)
        if nxt is None:
            stack.pop()
            state[node] = 2
            if node != target:
                order.append(node)
            continue
        if nxt not in graph:
            raise q.QaError(f'mission {node} needs {nxt}, which has no mission row')
        if state.get(nxt) == 1:
            path = [n for n, _ in stack]
            raise q.QaError(f'prerequisite cycle: {" -> ".join(map(str, path[path.index(nxt):] + [nxt]))}')
        if state.get(nxt) is None:
            state[nxt] = 1
            stack.append((nxt, iter(sorted(graph[nxt]))))
    return order


def apply_clears(db: sqlite3.Connection, chain: list[int], target: int) -> str:
    n = db.execute('SELECT COUNT(*) FROM user_info').fetchone()[0]
    if n != 1:
        raise q.QaError(f'the copy holds {n} accounts; a fixture needs exactly one')
    user = db.execute('SELECT id FROM user_info').fetchone()[0]
    done = db.execute('SELECT state FROM user_campaign_missions WHERE user_id=? AND mission_id=?',
                      (user, str(target))).fetchone()
    if done and done[0] == 2:
        raise q.QaError(f'mission {target} is already cleared in this copy; its first clear cannot be tested here')
    now = int(time.time())
    with db:
        for mid in chain:
            db.execute('INSERT INTO user_campaign_missions (user_id, mission_id, state, attain_percent, clear_count, '
                       'last_cleared_at) VALUES (?, ?, 2, 100, 1, ?) ON CONFLICT(user_id, mission_id) DO UPDATE SET '
                       'state=2, attain_percent=100, clear_count=MAX(1, clear_count), last_cleared_at=?',
                       (user, str(mid), now, now))
    return user


def copy_units(db: sqlite3.Connection, user: str, species: list[int]) -> list[tuple]:
    live = sqlite3.connect(f'file:{q.LIVE_SAVE.as_posix()}?mode=ro', uri=True)
    try:
        cols = [r[1] for r in live.execute('PRAGMA table_info(user_units)')]
        picked = []
        for s in species:
            r = live.execute('SELECT * FROM user_units WHERE unit_id=? ORDER BY base_atk DESC LIMIT 1', (s,)).fetchone()
            if r is None:
                raise q.QaError(f'the live save has no unit {s}')
            picked.append(dict(zip(cols, r)))
    finally:
        live.close()
    deck = db.execute('SELECT active_deck FROM user_info').fetchone()[0]
    nid = db.execute('SELECT COALESCE(MAX(user_unit_id), 0) FROM user_units').fetchone()[0] + 1
    out = []
    with db:
        db.execute('DELETE FROM user_decks WHERE user_id=? AND deck_type=1 AND deck_num=?', (user, deck))
        for pos, u in enumerate(picked):
            u = dict(u, user_unit_id=nid + pos, user_id=user, eqip_item_id=0, eqip_item_frame_id=-1,
                     eqip_item_id2=0, eqip_item_frame_id2=-1, favorite_flg=0, new=0)
            db.execute(f'INSERT INTO user_units ({", ".join(u)}) VALUES ({", ".join("?" * len(u))})', list(u.values()))
            db.execute('INSERT INTO user_decks (user_id, user_unit_id, deck_type, deck_num, member_type, disp_order) '
                       'VALUES (?, ?, 1, ?, ?, ?)', (user, nid + pos, deck, 0 if pos == 0 else 1, pos))
            out.append((nid + pos, u['unit_id'], u['unit_lvl'], u['base_hp'], u['base_atk']))
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--qa-dir', required=True)
    ap.add_argument('--run', required=True)
    ap.add_argument('--target', type=int, required=True)
    ap.add_argument('--units', default='')
    a = ap.parse_args(argv)
    try:
        run = q.run_dir(q.qa_dir(a.qa_dir), a.run, must_exist=True)
        species = [int(s) for s in a.units.split(',') if s.strip()]
        chain = closure(need_graph(rows('mission_mst'), rows('dungeon_mst'), rows('area_mst')), a.target)
        for side in ('gme.sqlite-journal', 'gme.sqlite-wal'):     # SQLite writes these beside the database
            q.run_file(run, side)
        db = sqlite3.connect(q.run_file(run, 'gme.sqlite', must_exist=True))
        try:
            user = apply_clears(db, chain, a.target)
            units = copy_units(db, user, species) if species else []
        finally:
            db.close()
        print(f'{len(chain)} prerequisite(s) of {a.target} cleared: {chain[:4]} ... {chain[-4:]}; {a.target} left uncleared')
        if units:
            print(f'squad from the live save (read only): {units}')
        return 0
    except q.QaError as ex:
        print(f'refused: {ex}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
