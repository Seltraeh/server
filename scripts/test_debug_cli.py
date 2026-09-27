"""Test CLI grants on a copied save; never edits the source. Windows only.
Usage: python scripts/test_debug_cli.py SOURCE_SAVE [DEBUG_EXE]
Requires a tutorial-complete account with six free slots, and unused port 19960.
"""
import sys, tempfile
import json, sqlite3, subprocess, time, urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
(ROOT/'out').mkdir(exist_ok=True)
work=Path(tempfile.mkdtemp(prefix='cli-grants-',dir=ROOT/'out'))
dbpath=work/'gme.sqlite'
source=sqlite3.connect(Path(sys.argv[1]).resolve().as_uri()+'?mode=ro',uri=True)
db=sqlite3.connect(dbpath,timeout=20)
source.backup(db);source.close()
db.row_factory=sqlite3.Row
config=json.loads((ROOT/'packaging/config.json').read_text(encoding='utf-8'))
config['db_clients'][0]['filename']=dbpath.as_posix()
config['listeners'][0]['port']=19960
for folder in ('mst','system','archive'):
    config['plugins'][0]['config']['server'][folder+'_root']=(ROOT/'deploy'/folder).as_posix()
(work/'config.json').write_text(json.dumps(config))
exe=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else ROOT/'out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe'
log=(work/'server.log').open('w')
p=subprocess.Popen([str(exe),str(work/'config.json')],stdout=log,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
try:
    for attempt in range(120):
        if p.poll() is not None: raise RuntimeError('server exited; see log')
        try:
            urllib.request.urlopen('http://127.0.0.1:19960/offline_mod/fps_cap',timeout=1).close()
            break
        except Exception: time.sleep(.5)
    else: raise RuntimeError('server not ready')
    pipe=open(r'\\.\pipe\gimudebug_'+str(p.pid),'r+b',buffering=0)
    def response():
        data=bytearray()
        while True:
            b=pipe.read(1)
            if not b: raise RuntimeError('pipe disconnected')
            if b==b'\x01': return data.decode('utf-8',errors='replace')
            data.extend(b)
    response()
    def command(s):
        pipe.write((s+'\n').encode()); result=response();print(s+': '+result.strip(),flush=True);return result
    user=db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
    def count(): return db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=?',(user,)).fetchone()[0]
    def counter(): return db.execute('SELECT unit_sum_cnt FROM user_team_archive WHERE user_id=?',(user,)).fetchone()[0]
    before=count(); total=counter()
    for unit in (750004,730302,730322,50253,10017,10017):
        assert 'Added unit' in command('addunit '+str(unit))
        row=db.execute('SELECT * FROM user_units WHERE user_id=? AND unit_id=? ORDER BY user_unit_id DESC LIMIT 1',(user,unit)).fetchone()
        assert row['unit_lvl']==1,dict(row)
        assert row['eqip_item_frame_id2']==-1,dict(row)
        if unit==10017: assert (row['bb_lvl'],row['sbb_lvl'])==(1,0),dict(row)
        assert db.execute('SELECT COUNT(*) FROM user_unit_dictionary WHERE user_id=? AND unit_id=?',(user,unit)).fetchone()[0]==1
    assert count()==before+6
    assert counter()==total+6
    print('PASS: all five IDs, duplicate grant, archive defaults, sphere lock, dictionary and acquisition counter',flush=True)
    command('addunit 999999999')
    assert count()==before+6
    db.execute("CREATE TRIGGER cli_grant_failure BEFORE INSERT ON user_unit_dictionary BEGIN SELECT RAISE(ABORT, 'cli rollback probe'); END")
    db.commit()
    assert 'cli rollback probe' in command('addunit 10017')
    assert count()==before+6 and counter()==total+6
    db.execute('DROP TRIGGER cli_grant_failure');db.commit()
    print('PASS: invalid ID leaves inventory unchanged; dictionary failure rolls back grant',flush=True)
    pipe.close()
finally:
    p.terminate();p.wait(timeout=15);log.close();db.close()
