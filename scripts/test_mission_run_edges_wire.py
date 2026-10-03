"""Independent QC: legacy results and stale continues must not mutate a new run.

python scripts/test_mission_run_edges_wire.py PATH_TO_DEBUG_EXE
Uses a disposable save/server. Intentionally fails until the two QC findings
in docs/QC_DIFF_REVIEW_2026-09-30.md are repaired.
"""
import argparse
import sys
from bf_testkit import Fixture, IsolatedServer, Client, Checker, mission_start_body, mission_end_body, started_serial
from bf_testkit import qa_dir  # noqa: E402

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('exe')
    args=ap.parse_args()
    fx=Fixture.create(qa_dir('mission_edges'),port=19989)
    check=Checker()
    with IsolatedServer(args.exe,fx):
        c=Client(fx)
        db=fx.db()
        def info():
            return dict(db.execute('SELECT * FROM user_info WHERE id=?',(c.user,)).fetchone())
        def start():
            db.execute('UPDATE user_info SET energy=20000,energy_full_ts=0,zel=1000,gems=100 WHERE id=?',(c.user,))
            db.commit()
            return started_serial(c.call('MissionStart',body=mission_start_body(10)))
        start()
        current=start()
        before=info()
        c.call('MissionEnd',body=mission_end_body(10,serial=10,zel=123))
        after=info()
        check('legacy result cannot close or reward a serial-issued run',
              after['open_mission_serial']==current and after['zel']==before['zel'],
              {'open_before':current,'open_after':after['open_mission_serial'],'zel_delta':after['zel']-before['zel']})
        stale=start()
        current=start()
        before=info()
        prior_continues=[tuple(r) for r in db.execute('SELECT * FROM user_mission_continues WHERE user_id=?',(c.user,))]
        c.call('MissionContinue',body={
            '6FrKacq7':[{'Kn51uR4Y':'5EdKHavF'}],
            'Kz7qfSs5':[{'k9cxD7Ba':str(stale),'j3g5P4cq':'4'}],
            '5PR2VmH1':[{'k9cxD7Ba':str(stale),'j0Uszek2':'4','K2gIYm0h':'stale-qc-blob'}]})
        after=info()
        continued=[tuple(r) for r in db.execute('SELECT * FROM user_mission_continues WHERE user_id=?',(c.user,))]
        check('stale continue cannot charge gems, replace resume data or mark the current run continued',
              after['gems']==before['gems'] and after['mission_break_serial']==before['mission_break_serial'] and continued==prior_continues,
              {'current':current,'stale':stale,'gem_delta':after['gems']-before['gems'],'break_after':after['mission_break_serial']})
        db.close()
    return check.summary('independent mission edges')

if __name__=='__main__':
    sys.exit(main())
