"""#34: every reward reply that replaces the warehouse brings the locks with it.

python scripts/test_reward_lock_snapshots_wire.py PATH_TO_DEBUG_EXE [--port 19993]
       [--out out/qa/current/reward_locks]

GameResponseParser::parseBodyTag replaces the client's list on 9wjrh74P and then
calls UserWarehouseInfoList::setFavorite, which sets every row's lock from the
ItemFavoriteInfoList (VSRPkdId).  A reply that sends the list without the lock
set leaves that list stale, so real ids it does not name show unlocked (and are
offered for sale) until some later reply fixes it.  Every packet type that
carries 9wjrh74P declares VSRPkdId too (13 of 13 in packet-generator/assets/net);
this checks the handlers actually fill it, on the reward screens the checklist
names, including ten presents in a row:

  * PresentReceipt: a sphere present (type 7) and an item present (type 4)
  * SlotAction: Brave Slots pulls, whenever a reply carries the list
  * MissionEnd (the battle result)
For each such reply: the rows it lists carry exactly the server's locks, every
live locked copy is listed, and a newly granted copy arrives unlocked.
"""
import argparse
import sys
from bf_testkit import (Fixture, IsolatedServer, Client, Checker, mission_start_body, mission_end_body,
                        started_serial)
from bf_testkit import qa_dir  # noqa: E402

SPHERE, REGULAR = 30000, 20000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19993)
    ap.add_argument('--out', default=str(qa_dir('reward_locks')))
    args = ap.parse_args()
    fx = Fixture.create(args.out, port=args.port)
    check = Checker()
    with fx.db() as db:
        user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        db.execute('INSERT INTO user_items(user_id,item_id,item_num,favorite_flg) VALUES (?,?,3,0)'
                   ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=3,favorite_flg=0', (user, SPHERE))
        db.execute('UPDATE user_items SET favorite_flg=0 WHERE user_id=?', (user,))
        db.execute('UPDATE user_info SET energy=20000, energy_full_ts=0 WHERE id=?', (user,))
        db.commit()

    with IsolatedServer(args.exe, fx):
        c = Client(fx, user_id=user)
        db = fx.db()

        def live_locks():
            return {r[0] for r in db.execute(
                'SELECT instance_id FROM user_warehouse_rows WHERE user_id=? AND favorite_flg!=0'
                ' AND (item_num>0 OR equip_unit_id!=0)', (user,))}

        def consistent(label, reply):
            listed = {int(r['n6E8iMf3']) for r in reply.get('9wjrh74P', [])}
            locked = {int(e['n6E8iMf3']) for e in reply.get('VSRPkdId', []) if e.get('5JbjC3Pp') == '1'}
            server = live_locks()
            check(f'{label}: the list comes with its lock set', 'VSRPkdId' in reply, sorted(reply)[:12])
            check(f'{label}: listed rows carry exactly the server locks, every locked copy listed',
                  locked & listed == server & listed and server <= listed and locked <= listed,
                  {'reply locks': sorted(locked), 'server locks': sorted(server)})

        view = c.call('UserInfo')
        copies = [int(r['n6E8iMf3']) for r in view['9wjrh74P'] if r['kixHbe54'] == str(SPHERE)]
        check('setup: three copies listed', len(copies) == 3, copies)
        lock = c.call('I8il6EiI', 'aRoIftRy', {'VSRPkdId': [{'n6E8iMf3': str(copies[1]), '5JbjC3Pp': '1'}]})
        check('setup: the middle copy is locked', 'error' not in lock and live_locks() == {copies[1]}, live_locks())

        def present(ptype, target, count):
            pid = db.execute('INSERT INTO user_presents(user_id,present_type,target_id,target_cnt) VALUES (?,?,?,?)',
                             (user, ptype, str(target), count)).lastrowid
            db.commit()
            return c.call('PresentReceipt', body={'o6uWU0Z7': [{'i1WQkh4G': '0', 'S1B82FHK': str(pid)}]})

        before = set(copies)
        reply = present(7, SPHERE, 1)
        check('present (sphere): received with the list', 'error' not in reply and '9wjrh74P' in reply,
              reply.get('error'))
        consistent('present (sphere)', reply)
        new = {int(r['n6E8iMf3']) for r in reply.get('9wjrh74P', []) if r['kixHbe54'] == str(SPHERE)} - before
        check('present (sphere): the new copy arrives unlocked', len(new) == 1 and not (new & live_locks()), new)

        reply = present(4, REGULAR, 5)
        check('present (item): received with the list', 'error' not in reply and '9wjrh74P' in reply,
              reply.get('error'))
        consistent('present (item)', reply)

        for n in range(10):
            reply = present(4, REGULAR, 1)
            if n in (0, 9):
                consistent(f'present #{n + 1} of ten in a row', reply)
        check('ten presents in a row: the lock set never moved', live_locks() == {copies[1]}, live_locks())

        db.execute("INSERT INTO user_brave_medals(user_id,medal_id,possession) VALUES (?, '1', 30)"
                   ' ON CONFLICT(user_id,medal_id) DO UPDATE SET possession=30', (user,))
        db.commit()
        carried = 0
        for _ in range(3):
            reply = c.call('SlotAction', body={'kLz5ujP2': [{'zS45RFGb': '1', 'd04gRmkE': '3'}]})
            check('slots: pull accepted', 'error' not in reply, reply.get('error'))
            if '9wjrh74P' in reply:
                carried += 1
                consistent(f'slots (pull carrying the list #{carried})', reply)
        print(f'slots: {carried} of 3 pulls carried the warehouse list (prizes are random)')

        start = c.call('MissionStart', body=mission_start_body(10))
        end = c.call('MissionEnd', body=mission_end_body(10, serial=started_serial(start)))
        check('mission end: settled with the list', 'error' not in end and '9wjrh74P' in end, end.get('error'))
        consistent('mission end', end)
        db.close()
    return check.summary('reward lock snapshots')


if __name__ == '__main__':
    sys.exit(main())
