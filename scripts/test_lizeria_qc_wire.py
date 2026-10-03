"""Lizeria compound prerequisite: map truth table and direct-entry backstop."""
import argparse
import sys
from bf_testkit import Fixture, IsolatedServer, Client, Checker, mission_start_body
from bf_testkit import qa_dir  # noqa: E402

def main():
    ap=argparse.ArgumentParser();ap.add_argument('exe');args=ap.parse_args()
    fx=Fixture.create(qa_dir('qc_lizeria'),port=19975)
    check=Checker()
    with IsolatedServer(args.exe,fx):
        c=Client(fx);db=fx.db()
        for cleared in ((),(666,),(20067,),(666,20067)):
            db.execute('DELETE FROM user_campaign_missions WHERE user_id=?',(c.user,))
            for mid in cleared:
                db.execute('INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,?,2)',(c.user,str(mid)))
            db.execute('UPDATE user_info SET energy=10000,energy_full_ts=0,open_mission_id=10 WHERE id=?',(c.user,));db.commit()
            r=c.call('UserInfo'); permits=r.get('yXNM8kL3',[])
            allowed=len(cleared)==2
            for key,value in (('9C64Qwe0','4'),('VjCY7rX4','700'),('MHx05sXt','700'),('j28VNcUW','700')):
                check(f'Lizeria {cleared}: {key} visibility',any(x.get(key)==value for x in permits)==allowed)
            before=tuple(db.execute('SELECT energy,open_mission_id FROM user_info WHERE id=?',(c.user,)).fetchone())
            reply=c.call('MissionStart',body=mission_start_body(700))
            check(f'Lizeria {cleared}: direct entry gated',('error' not in reply)==allowed)
            if not allowed:
                check(f'Lizeria {cleared}: denial preserves energy and open battle',tuple(db.execute('SELECT energy,open_mission_id FROM user_info WHERE id=?',(c.user,)).fetchone())==before)
        db.close()
    return check.summary('Lizeria QC')

if __name__=='__main__':sys.exit(main())
