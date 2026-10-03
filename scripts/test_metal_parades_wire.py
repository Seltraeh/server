"""#39: the three Metal Parades serve the enemy tiers their wiki pages describe.

python scripts/test_metal_parades_wire.py PATH_TO_DEBUG_EXE [--port 19984]

Evidence (Global wiki; see scripts/gen_metal_parades.py): Metal Parade! (rev
630717) Ghost 10 / King 35 HP; Super Metal Parade! (rev 630719) "No Metal Ghosts
(or similar)" -- King 35 / God 100 / Crystal 300; Mega Metal Parade! (rev 630716)
"No Metal Ghosts and Kings (or similar)" -- God 100 / Crystal 300.  Wave layouts
are authored (the pages give none), so only what the pages state is asserted:
which tiers appear, their HP, five battles, the 1-damage defense, and that each
enemy is capturable as itself (the pages list the units as obtainable).  The
capture ROLL is random server-side and not asserted.
"""
import argparse
import json
import sys
from collections import Counter
from pathlib import Path
from bf_testkit import (ROOT, Fixture, IsolatedServer, Client, Checker, mission_start_body, mission_end_body,
                        started_serial)
from bf_testkit import qa_dir  # noqa: E402

TIER_BY_HP = {10: 'Ghost', 35: 'King', 100: 'God', 300: 'Crystal'}
EXPECT = {100600: {'Ghost', 'King'}, 100601: {'King', 'God'}, 100614: {'God', 'Crystal'}}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19984)
    ap.add_argument('--archive', type=Path, default=ROOT / 'deploy/archive',
                    help='archive_root to serve (a copy of an older archive, for before/after evidence)')
    args = ap.parse_args()
    fx = Fixture.create(qa_dir('metal_parades'), port=args.port, archive_root=args.archive)
    check = Checker()
    archive = {r['id']: r for r in json.loads((args.archive / 'mission.json').read_text(encoding='utf-8'))
               if r['id'] in EXPECT}
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        db.execute('UPDATE user_info SET level=50,exp=0,energy=20000,energy_full_ts=0 WHERE id=?', (c.user,))
        db.commit()
        for mission, tiers in EXPECT.items():
            reply = c.call('MissionStart', body=mission_start_body(mission))
            check(f'{mission}: starts', 'error' not in reply, reply.get('error'))
            monsters = {m['o49dYfpH']: m for m in reply.get('U0v5IeJo', [])}
            slots = reply.get('75t0sx9z', [])
            battles = {g['ZSf8e1MG'] for g in reply.get('pj41dy9g', [])}
            seen = Counter(TIER_BY_HP.get(int(monsters[s['o49dYfpH']]['e7DK0FQT']), 'unknown')
                           for s in slots if s['o49dYfpH'] in monsters)
            check(f'{mission}: five battles', len(battles) == 5, len(battles))
            check(f'{mission}: only {sorted(tiers)} appear', set(seen) == tiers, dict(seen))
            check(f'{mission}: every enemy takes at most 1 damage (defense >= 999999)',
                  all(int(m['q08xLEsy']) >= 999999 for m in monsters.values()))
            record = archive[mission]
            check(f'{mission}: every enemy is capturable as itself (archive unit_drop_id == unit_id)',
                  all(b['unit_drop_id'] == b['unit_id'] and b['unit_drop_chance'] > 0
                      for s in record['stages'] for b in s['battle_monsters']))
            end = c.call('MissionEnd', body=mission_end_body(mission, serial=started_serial(reply)))
            check(f'{mission}: the battle settles', 'error' not in end, end.get('error'))
        db.close()
    return check.summary('metal parades')


if __name__ == '__main__':
    sys.exit(main())
