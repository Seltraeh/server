"""#26: leader skills that boost quest EXP (Roglizer) and battle drops (Fuu).

python scripts/test_leader_exp_boost_wire.py PATH_TO_DEBUG_EXE [--port 19980]

Evidence (see gimuserver/gme/handlers/Mission.cpp, leaderExpBoostPercent):
* LS process 97 = Player EXP Boost; Global wiki "Player EXP Boost" rev 653737:
  quest EXP x (1 + leader 1 + leader 2 ...), the two leaders ADDING.
* Perpetual Flaw Roglizer 61287 (LS 10168) carries 97 with 20 -- wiki rev
  642770 notes "20% EXP"; Holy End Roglizer 61286 (LS 10167) carries 15.
* Fuu 830838 (LS 830838) carries process 19, Drop Rate Boost (wiki rev 645616:
  "when dealing damage to enemies"): the battle is the client's, which reports
  the Zel/Karma/items it collected -- the server must credit them as reported.

Fixed rewards: mission 10 pays 20 EXP / 300 Zel, mission 12 pays 29 EXP, so every
expected number is exact arithmetic (rounded down).  Requests echo the battle
serial and use the client's quoted numbers; the stored level/EXP/Zel are read
back, not just the reply.
"""
import argparse
import sys
from bf_testkit import (Fixture, IsolatedServer, Client, Checker, mission_start_body,
                        mission_end_body, started_serial)
from bf_testkit import qa_dir  # noqa: E402

ROGLIZER_OMNI, ROGLIZER_7, FUU, PLAIN = 61287, 61286, 830838, 20011
HELPER = 'QCHELPER01'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19980)
    args = ap.parse_args()
    fx = Fixture.create(qa_dir('leader_exp_boost'), port=args.port)
    check = Checker()
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()

        def execute(sql, params=()):
            r = db.execute(sql, params)
            db.commit()
            return r

        template = dict(db.execute('SELECT * FROM user_units WHERE user_id=? LIMIT 1', (c.user,)).fetchone())
        template.pop('user_unit_id')

        def unit(species):
            row = dict(template, user_id=c.user, unit_id=str(species), unit_lvl=1, favorite_flg=0)
            uid = db.execute(f"INSERT INTO user_units ({','.join(row)}) VALUES ({','.join('?' for _ in row)})",
                             tuple(row.values())).lastrowid
            db.commit()
            return uid

        roglizer, holy_end, fuu, plain = unit(ROGLIZER_OMNI), unit(ROGLIZER_7), unit(FUU), unit(PLAIN)
        # Mission 12 needs 11 (mission_mst), and MissionStart enforces the map's
        # prerequisites; the copied save has only cleared 1, 2 and 10.
        execute("INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,'11',2)"
                ' ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2', (c.user,))
        # A roster friend on the Roglizer chain: at the player's Omni peak the
        # picker (and gme::helperLeaderUnit) field 61287.
        execute('DELETE FROM user_friends WHERE user_id=? AND friend_id=?', (c.user, HELPER))
        execute("INSERT INTO user_friends(user_id,friend_id,handle_name,base_unit_id,is_dev,favorite)"
                " VALUES (?,?,?,?,0,0)", (c.user, HELPER, 'QC Helper', ROGLIZER_7))

        def play(mission, leader, helper='0', status=2, zel=0, level=50):
            execute('UPDATE user_info SET level=?,exp=0,energy=20000,energy_full_ts=0 WHERE id=?', (level, c.user))
            body = mission_start_body(mission)
            body['9Q1Lq5FS'][0]['h7eY3sAK'] = helper
            start = c.call('MissionStart', body=body)
            assert 'error' not in start, start
            before = dict(db.execute('SELECT level,exp,zel FROM user_info WHERE id=?', (c.user,)).fetchone())
            reply = c.call('MissionEnd', body=mission_end_body(
                mission, status=status, zel=zel, serial=started_serial(start), deck_units=[leader, plain]))
            after = dict(db.execute('SELECT level,exp,zel FROM user_info WHERE id=?', (c.user,)).fetchone())
            inc = int(reply.get('F5Vs19mb', [{}])[0].get('d96tuT2E', -1))
            return reply, inc, before, after

        cases = [
            ('no qualifying leader', 10, plain, '0', 20),
            ('Perpetual Flaw Roglizer leads: +20%', 10, roglizer, '0', 24),
            ('Holy End Roglizer leads: +15%', 10, holy_end, '0', 23),
            ('Roglizer as the HELPER\'s leader: +20%', 10, plain, HELPER, 24),
            ('Roglizer leads AND the helper\'s leader: 20% + 20% add', 10, roglizer, HELPER, 28),
            ('rounded down: 29 x 1.20 = 34.8', 12, roglizer, '0', 34),
            ('rounded down: 29 x 1.15 = 33.35', 12, holy_end, '0', 33),
            ('rounded down: 29 x 1.40 = 40.6', 12, roglizer, HELPER, 40),
            ('Fuu leads: no EXP effect (hers are drop rates)', 10, fuu, '0', 20),
        ]
        for label, mission, leader, helper, expected in cases:
            reply, inc, before, after = play(mission, leader, helper)
            check(f'{label}: result shows {expected}', 'error' not in reply and inc == expected,
                  (reply.get('error'), inc))
            check(f'{label}: stored EXP matches what was shown', after['level'] == before['level']
                  and after['exp'] - before['exp'] == inc, (before, after))

        reply, inc, before, after = play(10, fuu, zel=12345)
        check('Fuu: the battle Zel the client reports is credited unclipped (+ 300 clear Zel)',
              after['zel'] - before['zel'] == 12345 + 300, after['zel'] - before['zel'])
        reply, inc, before, after = play(10, roglizer, status=0)
        check('a lost battle earns no EXP, boost or not', inc == 0 and after['exp'] == before['exp'], (inc, before, after))
        reply, inc, before, after = play(10, roglizer, level=999)
        check('level 999: nothing earned, boost or not', inc == 0 and after['exp'] == 0 and after['level'] == 999,
              (inc, after))
        db.close()
    return check.summary('leader EXP boost')


if __name__ == '__main__':
    sys.exit(main())
