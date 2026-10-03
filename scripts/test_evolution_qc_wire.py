"""Recipe, transaction and burst-identity QC; self-contained isolated server."""
import argparse
import json
import sys
from bf_testkit import ROOT, Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir  # noqa: E402

def rows(name):
    return next(iter(json.loads((ROOT/'deploy/mst'/f'{name}_mst.json').read_text(encoding='utf-8')).values()))

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('exe'); args=ap.parse_args()
    fx=Fixture.create(qa_dir('qc_evolution'),port=19973)
    check=Checker()
    units={int(r['pn16CNah']):r for r in rows('unit')}
    recipes={int(r['pn16CNah']):r for r in rows('unit_evo')}
    keys=['85X6JHQA','wh3YRU08','7MxucW2J','j7fTS3ca','Hb8yfmv7','Voht18AP','3g8brFoq','agp4CKEV','eKPWNoLn']
    kinds=['Xyt6rhx2','0tna4Idu','6GwnugW3','hdF8ND2H','nB7pFdR0','IZUvR489','bNRUuatB','2BFgYLjg','18Oz7z8k']
    with IsolatedServer(args.exe,fx):
        c=Client(fx); db=fx.db()
        template=dict(db.execute('SELECT * FROM user_units LIMIT 1').fetchone()); template.pop('user_unit_id')
        def unit(mid,**kw):
            data=units[mid]
            row=dict(template,user_id=c.user,unit_id=str(mid),unit_lvl=int(data['EI1DF8Yt']),favorite_flg=0,bb_id=data['nj9Lw7mV'],sbb_id=data['iEFZ6H19'],bb_lvl=9,sbb_lvl=0,eqip_item_id=0,eqip_item_id2=0)
            row.update(kw)
            uid=db.execute(f"INSERT INTO user_units ({','.join(row)}) VALUES ({','.join('?' for _ in row)})",tuple(row.values())).lastrowid
            db.commit();return uid
        def setup(mid=10011,**kw):
            recipe=recipes[mid]; base=unit(mid,**kw); mats=[]; nodes=[]
            # An unrelated unit's ID is deliberately sent as an item index.
            sentinel=unit(10011)
            for k,kind in zip(keys,kinds):
                material=int(recipe[k])
                if not material: continue
                if recipe[kind]=='2':
                    db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,1) ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=item_num+1',(c.user,material))
                    nodes.append({'inU8Q4gL':str(sentinel),'mnZ5K4Ii':'2','29MgiJIQ':'2'})
                else:
                    uid=unit(material); mats.append(uid)
                    nodes.append({'inU8Q4gL':str(uid),'mnZ5K4Ii':'2','29MgiJIQ':'1'})
            db.execute('UPDATE user_info SET zel=99999999,karma=99999999 WHERE id=?',(c.user,));db.commit()
            body={'8Z2NQrx1':[{'inU8Q4gL':str(base),'mnZ5K4Ii':'1','29MgiJIQ':'1'}]+nodes,'I82p0wCL':[{'pn16CNah':recipe['74VFwuTd']}],'mCE3rUu5':[{'Rs7bCE3t':recipe['Rs7bCE3t']}]}
            return base,mats,sentinel,body
        def state():
            return {table:[tuple(r) for r in db.execute(f'SELECT * FROM {table} WHERE '+('id' if table=='user_info' else 'user_id')+'=?',(c.user,))] for table in ('user_info','user_units','user_items','user_team_archive')}
        for mid in (10011,10015,850637):
            base,mats,sentinel,body=setup(mid)
            before=state(); reply=c.call('UnitEvo',body=body)
            row=dict(db.execute('SELECT * FROM user_units WHERE user_unit_id=?',(base,)).fetchone()); target=units[int(recipes[mid]['74VFwuTd'])]
            check(f'evolution {mid}: succeeds','error' not in reply,reply.get('error'))
            check(f'evolution {mid}: destination BB/SBB/leader IDs stored',(int(row['bb_id']),int(row['sbb_id']),int(row['leader_skill_id']))==(int(target['nj9Lw7mV']),int(target['iEFZ6H19']),int(target['oS3kTZ2W'])))
            check(f'evolution {mid}: reset level and BB9 halves to BB4',row['unit_lvl']==1 and row['bb_lvl']==4)
            check(f'evolution {mid}: unrelated unit survives item index',db.execute('SELECT 1 FROM user_units WHERE user_unit_id=?',(sentinel,)).fetchone() is not None)
            check(f'evolution {mid}: helper picker list untouched (no xZH6EIQ7)','xZH6EIQ7' not in reply,reply.get('xZH6EIQ7'))
            check(f'evolution {mid}: material units consumed',all(db.execute('SELECT 1 FROM user_units WHERE user_unit_id=?',(m,)).fetchone() is None for m in mats))
            after=state(); c.call('UnitEvo',body=body)
            check(f'evolution {mid}: retry is mutation-free',state()==after)
        for case in ('underlevel','favorite material','wrong recipe','insufficient zel','transaction fault'):
            base,mats,sentinel,body=setup(unit_lvl=1) if case=='underlevel' else setup()
            if case=='favorite material': db.execute('UPDATE user_units SET favorite_flg=1 WHERE user_unit_id=?',(mats[0],))
            if case=='wrong recipe': body['I82p0wCL'][0]['pn16CNah']='10017'
            if case=='insufficient zel': db.execute('UPDATE user_info SET zel=0 WHERE id=?',(c.user,))
            if case=='transaction fault': db.execute("CREATE TRIGGER qc_evo_failure BEFORE DELETE ON user_units BEGIN SELECT RAISE(ABORT,'QC failure'); END")
            db.commit(); before=state(); c.call('UnitEvo',body=body)
            check(f'evolution: {case} makes no partial changes',state()==before)
            if case=='transaction fault': db.execute('DROP TRIGGER qc_evo_failure');db.commit()
        db.close()
    return check.summary('evolution QC')

if __name__=='__main__': sys.exit(main())
