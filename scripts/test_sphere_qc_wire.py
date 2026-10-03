"""Persistent warehouse migration, copies, favorites, equipment and crafting QC.

python scripts/test_sphere_qc_wire.py PATH_TO_DEBUG_EXE
Only an isolated SQLite backup and server are mutated. Includes known regression
cases from the September 29 stride-alias implementation.
"""
import argparse
import concurrent.futures
import sys
from bf_testkit import Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir, replay_warehouse_migration  # noqa: E402


def main():
    ap=argparse.ArgumentParser();ap.add_argument('exe');args=ap.parse_args()
    fx=Fixture.create(qa_dir('qc_spheres'),port=19974)
    check=Checker()
    with fx.db() as db:
        user=db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
        # The live save is migrated; seed the PRE-migration state this suite tests
        # (bf_testkit.replay_warehouse_migration).
        replay_warehouse_migration(db)
        # Before migration, seed a valid real id above the old alias stride.
        db.execute('INSERT INTO user_items(user_id,item_id,item_num,favorite_flg) VALUES (?,30000,3,0) ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=3,favorite_flg=0',(user,))
        db.execute('UPDATE user_items SET instance_id=2097123 WHERE user_id=? AND item_id=30000',(user,))
        db.execute('INSERT INTO user_items(user_id,item_id,item_num,favorite_flg) VALUES (?,30001,2,1) ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=2,favorite_flg=1',(user,))
        db.execute('UPDATE user_units SET eqip_item_id=0,eqip_item_id2=0,eqip_item_frame_id=0,eqip_item_frame_id2=-1 WHERE user_id=?',(user,))
        unit=db.execute('SELECT user_unit_id FROM user_units WHERE user_id=? LIMIT 1',(user,)).fetchone()[0]
        db.execute('UPDATE user_units SET sphere_ext=0 WHERE user_unit_id=?',(unit,))
        template=dict(db.execute('SELECT * FROM user_units WHERE user_unit_id=?',(unit,)).fetchone());template.pop('user_unit_id')
        other=db.execute(f"INSERT INTO user_units ({','.join(template)}) VALUES ({','.join('?' for _ in template)})",tuple(template.values())).lastrowid
        initial={r['item_id']:r['item_num'] for r in db.execute('SELECT item_id,item_num FROM user_items WHERE user_id=?',(user,))}
        db.commit()
    with IsolatedServer(args.exe,fx):
        c=Client(fx);db=fx.db()
        def stock(item=30000):
            r=db.execute('SELECT item_num FROM user_items WHERE user_id=? AND item_id=?',(user,item)).fetchone()
            return r[0] if r else 0
        def view():return c.call('UserInfo')
        def warehouse(item=30000):return [r for r in view()['9wjrh74P'] if r['kixHbe54']==str(item)]
        def available(item=30000):return [r for r in warehouse(item) if int(r['wgV86x1q'])>0]
        def ids(item=30000):return {r['n6E8iMf3'] for r in available(item)}
        def favorites():return {r['n6E8iMf3'] for r in view().get('VSRPkdId',[]) if r['5JbjC3Pp']=='1'}
        def lock(values):return c.call('I8il6EiI','aRoIftRy',{'VSRPkdId':[{'n6E8iMf3':str(v),'5JbjC3Pp':'1'} for v in values]})
        def sell(values):return c.call('qDQerU74','73aFNjPu',{'M73i1c5U':[{'n6E8iMf3':str(v),'wgV86x1q':str(q)} for v,q in values]})
        def equip(ops):return c.call('ItemSphereEqp',body={'wx1ZLFj9':[{'a2utCvs8':','.join(':'.join(map(str,x)) for x in ops)}]})
        def state():
            return {t:[tuple(r) for r in db.execute(f'SELECT * FROM {t} WHERE '+('id' if t=='user_info' else 'user_id')+'=?',(user,))] for t in ('user_items','user_units','user_warehouse_rows','user_info','user_team_archive')}
        rows=available();original=ids()
        check('migration: aggregate quantities are unchanged',{r['item_id']:r['item_num'] for r in db.execute('SELECT item_id,item_num FROM user_items WHERE user_id=?',(user,))}==initial)
        check('migration: original high real instance ID preserved','2097123' in original,original)
        check('spheres: three independent one-count rows',len(rows)==3 and len(original)==3 and all(r['wgV86x1q']=='1' for r in rows))
        check('migration: all copies of old favorited stack remain protected',ids(30001)<=favorites())
        target=next(x for x in original if x!='2097123')
        lock([target])
        check('spheres: favoriting one copy locks only that copy',favorites() & original=={target},favorites() & original)
        check('spheres: repeated refresh preserves row IDs',ids()==original)
        before=state();sell([(target,1)])
        check('spheres: a locked copy cannot be sold',state()==before)
        before=state();sell([('2097123',1),(target,1)])
        check('spheres: a mixed valid/locked sale rolls back entirely',state()==before)
        before=state();sell([('2097123',2)])
        check('spheres: one row cannot sell two copies',state()==before)
        # Exact old real ID resolves; no modulo mapping.
        sell([('2097123',1)])
        check('spheres: selling the high real ID subtracts exactly one',stock()==2)
        check('spheres: another surviving copy keeps its ID and lock',target in ids() and target in favorites())
        before=state();sell([('2097123',1)])
        check('spheres: stale sold ID cannot sell a surviving copy',state()==before)
        before=state();lock([2147483000])
        check('favorites: invalid full-set update does not erase old locks',state()==before)
        # Foreign-user identity cannot be selected by this account.
        foreign=db.execute("INSERT INTO user_warehouse_rows(user_id,item_id,item_num) VALUES ('qc-other',30000,1) RETURNING instance_id").fetchone()[0];db.commit()
        before=state();sell([(foreign,1)]);lock([foreign])
        check('ownership: foreign inventory row cannot be sold or favorited',state()==before)
        # Equipped copy retains its favorite flag, while storage quantity falls.
        equip([(target,0,unit,30000,0)])
        check('equip: exact copy leaves storage',stock()==1)
        eq=[r for r in warehouse() if r['n6E8iMf3']==target]
        # A worn copy is an ordinary count-1 row on the wire, named by the
        # unit's frame (item_num / equipitem_frame_id in net/user.kdl); it was
        # sent at zero until 2026-10-02, which hid it from the client's picker.
        frame=next(u['0R3qTPK9'] for u in view()['4ceMWH6k'] if u['edy7fq3L']==str(unit))
        check('equip: same row retained, sent as a worn count-1 row named by the unit, favorite kept',
              len(eq)==1 and eq[0]['wgV86x1q']=='1' and frame==target and target in favorites(),(eq,frame))
        check('equip: first slot does not unlock second slot',db.execute('SELECT eqip_item_frame_id2 FROM user_units WHERE user_unit_id=?',(unit,)).fetchone()[0]==-1)
        before=state();sell([(target,1)])
        check('equip: equipped copy cannot be sold',state()==before)
        before=state();equip([(0,target,other,0,30000)])
        check('equip: locked second slot cannot consume or duplicate copy',state()==before)
        before=state();equip([(target,0,other,30000,0)])
        check('equip: already-equipped copy cannot be assigned again',state()==before)
        # Transfer to another unit in reverse order; all returns precede spends.
        equip([(target,0,other,30000,0),(0,0,unit,0,0)])
        check('equip: transfer between units preserves quantity',stock()==1 and db.execute('SELECT eqip_item_id FROM user_units WHERE user_unit_id=?',(other,)).fetchone()[0]==30000)
        equip([(0,0,other,0,0)])
        check('unequip: same copy and favorite return',stock()==2 and target in ids() and target in favorites())
        before=state();equip([(2147483000,0,unit,30000,0)])
        check('equip: unknown copy does not mint a sphere',state()==before)
        before=state();equip([(target,0,unit,20000,0)])
        check('equip: item/species mismatch is rejected',state()==before)
        # Database failure after sidecar/quantity updates must undo both.
        db.execute("CREATE TRIGGER qc_equip_fail BEFORE UPDATE ON user_units BEGIN SELECT RAISE(ABORT,'QC equip'); END");db.commit()
        before=state();equip([(target,0,unit,30000,0)])
        check('equip: write failure rolls back copy and aggregate',state()==before)
        db.execute('DROP TRIGGER qc_equip_fail');db.commit()
        free=next(x for x in ids() if x!=target)
        db.execute("CREATE TRIGGER qc_sell_fail BEFORE UPDATE OF zel ON user_info BEGIN SELECT RAISE(ABORT,'QC sell'); END");db.commit()
        before=state();sell([(free,1)])
        check('sale: payment failure rolls back quantity and row',state()==before)
        db.execute('DROP TRIGGER qc_sell_fail');db.commit()
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            list(pool.map(lambda _:sell([(free,1)]),range(2)))
        check('sale: concurrent requests sell a single copy once',stock()==1 and ids()=={target})
        # Real recipe 2005, sphere 30000, five 10400 per craft, 500 Karma.
        db.execute('INSERT INTO user_town_facilities(user_id,facility_id,lv) VALUES (?,1,1) ON CONFLICT(user_id,facility_id) DO UPDATE SET lv=1',(user,))
        db.execute("INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,'21',2) ON CONFLICT(user_id,mission_id) DO UPDATE SET state=2",(user,))
        db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,10400,15) ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=15',(user,))
        db.execute('UPDATE user_info SET karma=5000 WHERE id=?',(user,));db.commit()
        r=c.call('ItemMix',body={'JTf2jY5o':[{'4HqhTf3a':'2005:3'}]})
        check('craft: succeeds and adds three separate spheres','error' not in r and stock()==4 and len(available())==4)
        check('craft: charges exact material and Karma',stock(10400)==0 and db.execute('SELECT karma FROM user_info WHERE id=?',(user,)).fetchone()[0]==3500)
        check('craft: existing favorite stays on only its old copy',favorites() & ids()=={target})
        check('craft: sold IDs are never recycled',free not in ids() and '2097123' not in ids())
        # Consume all copies of a normal material, then regrant: its old ID must
        # likewise not point at the new grant. Ordinary stacks remain stackable.
        db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,20000,101) ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=101',(user,));db.commit()
        regular=available(20000)
        check('regular items: split only at MST cap 99',sorted(int(r['wgV86x1q']) for r in regular)==[2,99])
        persisted=(ids(),favorites() & ids(),stock())
        equip([(target,0,unit,30000,0)])
        db.close()
    with IsolatedServer(args.exe,fx,log_name='restart.log'):
        c=Client(fx);db=fx.db()
        check('restart: equipped row still has its original favorite',target in favorites() and stock()==persisted[2]-1)
        equip([(0,0,unit,0,0)])
        check('restart: unequip restores exact saved IDs and favorites',(ids(),favorites() & ids(),stock())==persisted)
        # The migration must not seed aliases again on restart.
        duplicates=db.execute('SELECT instance_id,COUNT(*) n FROM user_warehouse_rows GROUP BY instance_id HAVING n>1').fetchall()
        check('restart: persistent IDs remain unique',not duplicates)
        db.close()
    return check.summary('sphere QC')

if __name__=='__main__':sys.exit(main())
