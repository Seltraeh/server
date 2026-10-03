"""Independent QC of September 29 gameplay changes on a disposable save.

python scripts/test_bugfix_qc_wire.py PATH_TO_DEBUG_EXE
Starts its own server; never changes the live save or live server.
"""
import argparse
import json
import sys
from pathlib import Path
from bf_testkit import ROOT, Fixture, IsolatedServer, Client, Checker, mission_start_body, mission_end_body, started_serial
from bf_testkit import qa_dir  # noqa: E402


def mst(name):
    data = json.loads((ROOT / 'deploy/mst' / (name + '_mst.json')).read_text(encoding='utf-8'))
    return next(iter(data.values()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19972)
    args = ap.parse_args()
    fx = Fixture.create(qa_dir('qc'), port=args.port)
    check = Checker()
    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx)
        db = fx.db()
        def execute(sql, params=()):
            result = db.execute(sql, params)
            db.commit()
            return result
        def info():
            return dict(db.execute('SELECT * FROM user_info WHERE id=?', (c.user,)).fetchone())
        def stock(item=20000):
            row = db.execute('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?', (c.user,item)).fetchone()
            return row[0] if row else 0
        def bar():
            return [tuple(r) for r in db.execute('SELECT disp_order,item_id,item_num FROM user_equip_items WHERE user_id=? ORDER BY disp_order',(c.user,))]
        def edit(slots):
            return c.call('ItemEdit', body={'71U5wzhI':[{'XuJL4pc5':str(n),'kixHbe54':str(i),'wgV86x1q':str(q)} for n,i,q in slots], 'nAligJSQ':[]})
        # The client echoes the battle serial MissionStart issued (MissionEndRequest
        # reads MissionInfo::getMissionSerialID); sending the mission id instead only
        # ever settled a serial-issued run through the legacy loophole fixed after the
        # 2026-09-30 QC, so end() defaults to the last serial start() was given.
        issued = {}
        def start(mid=10):
            execute('UPDATE user_info SET energy=20000,energy_full_ts=0 WHERE id=?',(c.user,))
            reply = c.call('MissionStart', body=mission_start_body(mid))
            assert 'error' not in reply, reply
            issued[mid] = started_serial(reply)
        def end(mid=10, **kw):
            kw.setdefault('serial', issued.get(mid))
            return c.call('MissionEnd', body=mission_end_body(mid, **kw))
        execute('DELETE FROM user_equip_items WHERE user_id=?',(c.user,))
        execute('DELETE FROM user_equip_bonus_items WHERE user_id=?',(c.user,))
        execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,20000,20) ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=20',(c.user,))
        edit([(0,20000,5)])
        check('items: equip transfers stock into bar',stock()==15 and bar()==[(0,20000,5)], (stock(),bar()))
        edit([(0,20000,5)])
        check('items: repeated identical edit does not move stock twice',stock()==15)
        edit([(0,20000,3)])
        check('items: reducing bar returns exactly two',stock()==17 and bar()==[(0,20000,3)])
        before=(stock(),bar())
        edit([(0,20000,999)])
        check('items: above-cap edit leaves all state unchanged',(stock(),bar())==before)
        edit([(0,20000,2),(0,20000,2)])
        check('items: repeated slots rejected atomically',(stock(),bar())==before)
        edit([(0,99999999,1)])
        check('items: unknown item rejected atomically',(stock(),bar())==before)
        execute('UPDATE user_items SET item_num=0 WHERE user_id=? AND item_id=20000',(c.user,))
        edit([(0,20000,5)])
        check('items: insufficient stock does not alter bar',stock()==0 and bar()==before[1])
        execute('UPDATE user_items SET item_num=17 WHERE user_id=? AND item_id=20000',(c.user,))
        start()
        reply=end(used='20000:2', items='20000:1')
        check('items: mission settlement succeeds','error' not in reply,reply.get('error'))
        check('items: use only spends bar; actual loot adds one to storage',stock()==18 and bar()==[(0,20000,1)],(stock(),bar()))
        before=(stock(),bar(),info()['zel'],info()['exp'])
        repeat=end(used='20000:2',items='20000:1')
        check('items: duplicate result neither pays nor spends',(stock(),bar(),info()['zel'],info()['exp'])==before)
        start()
        before=(stock(),bar(),info()['zel'],info()['open_mission_id'])
        end(used='20000:1,')
        check('items: malformed log rolls back entire result',(stock(),bar(),info()['zel'],info()['open_mission_id'])==before)
        end(status=0,used='20000:1')
        check('items: loss consumes owned item without returning unused stock',stock()==18 and bar()==[(0,20000,0)])
        edit([(0,20000,3)])
        start(6000000)
        before=(stock(),bar())
        end(6000000, used='20000:50')
        check('items: simulator supplied items cannot consume owned stock',(stock(),bar())==before)
        edit([])
        check('items: clearing bar returns only unspent owned items',stock()==18 and bar()==[])

        # Cutscene markers are authoritative only when uploaded by the client.
        marker='11,1@80@1@1&|0,0'
        payload={'IKqx1Cn9':[{'N4XVE1uA':marker,'9yVsu21R':'randall,challenge,'}]}
        c.call('BjAt1D6b','k5EiNe9x',payload)
        check('scenarios: event upload stores both markers',info()['scenario_info']==marker and info()['special_scenario_info']=='randall,challenge,')
        start()
        reply=end()
        login=reply.get('IKqx1Cn9',[{}])[0]
        check('scenarios: MissionEnd omits both stale marker fields','N4XVE1uA' not in login and '9yVsu21R' not in login)
        login=c.call('UserInfo').get('IKqx1Cn9',[{}])[0]
        check('scenarios: login restores persisted marker',login.get('N4XVE1uA')==marker,login.get('N4XVE1uA'))
        c.call('BjAt1D6b','k5EiNe9x',{})
        check('scenarios: absent upload does not erase state',info()['scenario_info']==marker)
        marker='85,1@200@1@1&|0,0'
        reply=c.call('m2Ve9PkJ','d7UuQsq8',{'IKqx1Cn9':[{'N4XVE1uA':marker,'9yVsu21R':'randall,challenge,'}],
                                            'Ti62XfZK':[{'Z0Y4RoD7':'0'}], 'dX7S2Lc1':[]})
        check('scenarios: deck edit persists uploaded progress','error' not in reply and info()['scenario_info']==marker,reply)

        levels={int(r['D9wXQI2V']):r for r in mst('user_level')}
        check('level cap: positive level-1000 animation sentinel exists',int(levels.get(1000,{}).get('d96tuT2E',0))>0)
        for lv,xp in [(999,0),(999,4294967290),(998,int(levels[999]['d96tuT2E'])-1)]:
            execute('UPDATE user_info SET level=?,exp=? WHERE id=?',(lv,xp,c.user))
            start()
            reply=end(zel=7)
            check(f'level cap: {lv}/{xp} completes with level 999 and zero residual XP','error' not in reply and info()['level']==999 and info()['exp']==0,(reply.get('error'),info()['level'],info()['exp']))
        # Actual debug-console clock controls the C++ helper (not a Python model).
        with open(r'\\.\pipe\gimudebug_'+str(server.proc.pid),'r+b',buffering=0) as pipe:
            def response():
                data=bytearray()
                while True:
                    b=pipe.read(1)
                    if not b: raise RuntimeError('debug pipe disconnected')
                    if b==b'\x01': return data.decode(errors='replace')
                    data.extend(b)
            response()
            def clock(t):
                pipe.write(f'clock {t}\n'.encode()); response()
            now=1800000000
            for lv in (998,999):
                cap=int(levels[lv]['0P9X1YHs'])
                rate=2 if lv==999 else 1
                for elapsed in (0,179,180,181,360):
                    clock(now+elapsed)
                    execute('UPDATE user_info SET level=?,energy=?,energy_full_ts=? WHERE id=?',(lv,cap-2*rate,now+360,c.user))
                    r=c.call('UserInfo')
                    # Packet field obtained from the schema, independent expected tick math.
                    team=r['fEi17cnx'][0]
                    expected=cap-2*rate+(elapsed//180)*rate
                    check(f'energy: level {lv}, elapsed {elapsed}s',int(team['0P9X1YHs'])==expected,(team.get('0P9X1YHs'),expected))
            clock(now)
            execute('UPDATE user_info SET level=999,energy=431,energy_full_ts=0 WHERE id=?',(c.user,))
            r=c.call('MissionStart',body=mission_start_body(10)) # cost 3
            check('energy: odd cost leaves 428 immediately',int(c.call('UserInfo')['fEi17cnx'][0]['0P9X1YHs'])==428)
            clock(now+180)
            check('energy: odd cost recovers exactly two at first tick',int(c.call('UserInfo')['fEi17cnx'][0]['0P9X1YHs'])==430)
            clock(now+179)
            # Spend at an existing timer boundary; do not reset its phase.
            execute('UPDATE user_info SET level=999,energy=428,energy_full_ts=? WHERE id=?',(now+360,c.user))
            c.call('MissionStart',body=mission_start_body(10))
            check('energy: spending while refilling retains current quantity',int(c.call('UserInfo')['fEi17cnx'][0]['0P9X1YHs'])==425)
            clock(now+180)
            check('energy: spending preserves next tick phase',int(c.call('UserInfo')['fEi17cnx'][0]['0P9X1YHs'])==427)
            execute('UPDATE user_info SET level=999,energy=0,energy_full_ts=? WHERE id=?',(now+1000000,c.user))
            check('energy: inconsistent long timer cannot unsigned-wrap',int(c.call('UserInfo')['fEi17cnx'][0]['0P9X1YHs'])==0)
            clock('off')
        start()
        end()
        persisted=(stock(),bar(),info()['scenario_info'],info()['level'],info()['exp'])
        db.close()
    with IsolatedServer(args.exe,fx,log_name='restart.log'):
        c=Client(fx);db=fx.db()
        check('migration/reload: migrated save starts a second time and retains state',
              (stock(),bar(),info()['scenario_info'],info()['level'],info()['exp'])==persisted)
        before=(stock(),bar(),info()['zel'],info()['exp'])
        end(items='20000:1',used='20000:1')
        check('mission retry: closed battle remains closed across server restart',
              (stock(),bar(),info()['zel'],info()['exp'])==before)
        check('scenarios: persisted progress survives server restart',c.call('UserInfo')['IKqx1Cn9'][0]['N4XVE1uA']==marker)
        db.close()
    return check.summary('bugfix QC')


if __name__=='__main__':
    sys.exit(main())
