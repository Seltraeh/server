"""Tests for scripts/gen_material_drops.py: the generator in memory, then its output on an isolated server.

python scripts/test_gen_material_drops.py PATH_TO_DEBUG_EXE [--port 19971]

Offline (deploy/ is only read): mission 10 is served the live capture verbatim (and the sources file still
carries it as the log has it); the generator changes nothing but `material_drops` and is idempotent; every list
is valid for MissionArchiver, names only ItemMst materials and uses only the capture's rates; Mimics, script
monsters, the Battle Simulator and missions the sources say nothing about get no list; the policy's pool
rotation, boss list, dungeon rare and named-enemy placement hold; --check reports and nothing is written
without --out.
Wire: a disposable archive (hard links to deploy/archive, the generated mission.json) loads with every list
accepted, and MissionStart sends each sampled monster exactly its generated list, in the client's format.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_material_drops as g  # noqa: E402
from bf_testkit import (ROOT, Fixture, IsolatedServer, Client, Checker, mission_start_body,  # noqa: E402
                        mission_end_body, started_serial, qa_dir)


def strip(missions):
    for m in missions:
        for s in m['stages']:
            for b in s['battle_monsters']:
                b.pop('material_drops', None)
    return missions


def wire(drops):
    """The server's encoding (MissionArchiver encodeMaterialDrops): per with no trailing .0, ':' and ','."""
    fmt = lambda p: str(int(p)) if float(p).is_integer() else repr(float(p))   # noqa: E731
    return ','.join(f"{fmt(d['per'])}:{d['item_id']}:{d['max_count']}" for d in drops) if drops else None


def lists_of(mission):
    return [b.get('material_drops') for s in mission['stages'] for b in s['battle_monsters']]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19971)
    args = ap.parse_args()
    check = Checker()
    raw = g.MISSIONS.read_bytes()
    src = json.loads(g.SOURCES.read_text(encoding='utf-8'))
    mats = g.materials()
    dungeon, area = g.dungeon_of()
    out, report = g.generate(raw)
    before = {m['id']: m for m in json.loads(raw)}
    after = {m['id']: m for m in json.loads(out)}
    entry = lambda per, item, n: {'per': per, 'item_id': item, 'max_count': n}   # noqa: E731

    # ---- the live capture -----------------------------------------------------------------------------
    cap = src['capture']
    if g.CAPTURE_LOG.is_file():
        check('the sources file carries the surviving live MissionStart verbatim', g.capture() == cap, cap)
    m10 = after[10]
    boss = g.boss_of(m10)
    regular = [entry(*e) for e in cap['regular']]
    others = [b for s in m10['stages'] for b in s['battle_monsters'] if b is not boss and not g.is_mimic(b)]
    check('mission 10: every regular monster carries the capture\'s "7:10000:3,7:10300:3"',
          others and all(wire(b.get('material_drops')) == '7:10000:3,7:10300:3' for b in others),
          [wire(b.get('material_drops')) for b in others])
    check('mission 10: the boss King Sparky carries the capture\'s "7:10301:5,3:10603:1"',
          boss['name'] == 'King Sparky' and wire(boss.get('material_drops')) == '7:10301:5,3:10603:1',
          (boss['name'], boss.get('material_drops')))
    check('... and its Mimics carry nothing', all('material_drops' not in b for s in m10['stages']
                                                    for b in s['battle_monsters'] if g.is_mimic(b)))
    check('... which is the capture\'s regular list, as authored', regular == others[0].get('material_drops'))

    # ---- only material_drops, idempotent ------------------------------------------------------------
    plain = lambda ms: json.dumps(strip(ms), indent=1, ensure_ascii=False)   # noqa: E731
    check('only material_drops changes: every other field of every mission is the source\'s',
          plain(json.loads(out)) == plain(json.loads(raw)) and set(after) == set(before))
    again, _ = g.generate(out)
    check('regenerating the output is a no-op', again == out)
    check('the output keeps the archive layout (CRLF, 1-space indent)', out.startswith(b'[\r\n {\r\n  "id"'))

    # ---- every list valid, materials only, the capture's rates ---------------------------------------
    every = [(mid, d) for mid, m in after.items() for lst in lists_of(m) for d in lst or []]
    bad = [(mid, d) for mid, d in every if not (0 < d['per'] <= 100 and 1 <= d['max_count'] <= 99)]
    check(f'all {len(every)} entries pass MissionArchiver\'s bounds (0 < per <= 100, 1 <= max_count <= 99)',
          every and not bad, bad[:3])
    check('every item is an ItemMst material (kind 2)', all(d['item_id'] in mats for _, d in every),
          sorted({d['item_id'] for _, d in every if d['item_id'] not in mats})[:5])
    dupes = [(mid, lst) for mid, m in after.items() for lst in lists_of(m)
             if lst and len({d['item_id'] for d in lst}) != len(lst)]
    check('no list names an item twice', not dupes, dupes[:2])
    rates = {(d['per'], d['max_count']) for _, d in every}
    check('rates are the capture\'s only: 7 % (up to 3, or 5 on a boss) and 3 % (up to 1, or the page\'s N)',
          {p for p, _ in rates} == {7, 3} and {n for p, n in rates if p == 7} <= {3, 5}, sorted(rates))
    check(f'{report["missions"]} missions and {report["monsters"]} monsters get a list (the counts reported)',
          report['missions'] == sum(1 for m in after.values() if any(lists_of(m)))
          and report['monsters'] == sum(1 for m in after.values() for lst in lists_of(m) if lst))

    # ---- what gets nothing --------------------------------------------------------------------------
    check('the Battle Simulator (6000000) gets nothing', not any(lists_of(after[g.SIMULATOR])))
    check('no Mimic anywhere gets a list', not any('material_drops' in b for m in after.values() for s in m['stages']
                                                    for b in s['battle_monsters'] if g.is_mimic(b)))
    check('script monsters get nothing', all('material_drops' not in b for m in after.values()
                                              for b in m.get('script_monsters') or []))
    silent = [mid for mid in after if g.SIMULATOR != mid and str(mid) not in src['missions']
              and str(dungeon.get(mid)) not in src['dungeons'] and str(area.get(mid)) not in src['areas']]
    check(f'the {len(silent)} missions no source covers (Parade Garden, Menon, ...) get nothing',
          silent and not any(any(lists_of(after[mid])) for mid in silent)
          and {100600, 10900} <= {area[mid] for mid in silent}, silent[:5])

    # ---- the policy ---------------------------------------------------------------------------------
    pool = src['dungeons'][str(dungeon[11])]['common']
    m11 = after[11]
    b11 = g.boss_of(m11)
    regs = [b.get('material_drops') for s in m11['stages'] for b in s['battle_monsters']
            if b is not b11 and not g.is_mimic(b)]
    check('mission 11 (2nd of its dungeon): all regulars share the pool\'s next pair at 7 %, up to 3',
          regs and all(r == [entry(7, pool[2], 3), entry(7, pool[3], 3)] for r in regs), regs[:1])
    special = src['missions']['11']['special']
    check('mission 11\'s boss: its material-page item at 7 % up to 5, then the quest\'s special at 3 % up to 1',
          b11['material_drops'] == [entry(7, i, 5) for i in src['missions']['11']['page_items']]
          + [entry(3, i, 1) for i in special] and special, b11['material_drops'])
    rank = {}
    for mid in sorted(after):
        rank.setdefault(dungeon.get(mid), []).append(mid)
    rare_hit = None
    for did, d in src['dungeons'].items():
        for k, mid in enumerate(rank.get(int(did), [])):
            if d['rare'] and mid != 10 and g.boss_of(after[mid]) is not None:
                rare_hit = (mid, entry(3, d['rare'][k % len(d['rare'])], 1))
                break
        if rare_hit:
            break
    check('a dungeon rare rotates onto the boss at 3 %, up to 1',
          rare_hit and rare_hit[1] in g.boss_of(after[rare_hit[0]])['material_drops'], rare_hit)
    named = None
    for mid, rec in src['missions'].items():
        m = after.get(int(mid))
        if not m or int(mid) == 10:
            continue
        monsters = [b for s in m['stages'] for b in s['battle_monsters'] if not g.is_mimic(b)]
        for item, enemy, up_to in rec.get('enemy_items', []):
            target = g.named(monsters, enemy) if enemy else None
            if item in mats and target is not None and target is not g.boss_of(m):
                named = (int(mid), target['name'], entry(3, item, up_to or 1), target.get('material_drops'))
                break
        if named:
            break
    check('a boss item whose entry names a non-boss enemy goes on that enemy at 3 %',
          named and named[2] in (named[3] or []), named)

    # ---- the command line ---------------------------------------------------------------------------
    folder = qa_dir('gen_material_drops_archive')
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    for p in g.MISSIONS.parent.iterdir():
        if p.is_file() and p.name != 'mission.json':
            os.link(p, folder / p.name)
    (folder / 'mission.json').write_bytes(out)
    deploy_sha = hashlib.sha256(raw).hexdigest()
    run = lambda *a: subprocess.run([sys.executable, str(HERE / 'gen_material_drops.py'), *a],   # noqa: E731
                                    capture_output=True, text=True)
    r = run('--check', '--archive', str(folder))
    check('--check on the generated archive: exit 0, "up to date"', r.returncode == 0 and 'up to date' in r.stdout,
          r.stdout[-200:])
    r = run()
    check('no --out: nothing is written and the exit is non-zero', r.returncode != 0
          and hashlib.sha256(g.MISSIONS.read_bytes()).hexdigest() == deploy_sha, (r.returncode, r.stderr[-200:]))

    # ---- the wire -----------------------------------------------------------------------------------
    sample = [10, 11, 12]
    if rare_hit:
        sample.append(rare_hit[0])
    if named:
        sample.append(named[0])
    by_area = next((mid for mid in sorted(after) if str(dungeon.get(mid)) not in src['dungeons']
                    and str(area.get(mid)) in src['areas'] and any(lists_of(after[mid]))), None)
    if by_area:
        sample.append(by_area)
    fx = Fixture.create(qa_dir('gen_material_drops'), port=args.port, archive_root=folder)
    with IsolatedServer(args.exe, fx) as server:
        c = Client(fx)
        db = fx.db()
        db.execute('UPDATE user_info SET level=50, exp=0, energy=20000, energy_full_ts=0 WHERE id=?', (c.user,))
        # Story progress: every sampled mission must be past its PermitPlace gate on this fresh save.
        db.execute('DELETE FROM user_campaign_missions WHERE user_id=?', (c.user,))
        db.executemany('INSERT INTO user_campaign_missions(user_id,mission_id,state) VALUES (?,?,2)',
                       [(c.user, str(mid)) for mid in sorted(after) if mid < 100000])
        db.commit()
        db.close()
        c.call('UserInfo')
        log = server.log_text()
        check(f'load: the generated archive keeps all {report["monsters"]} lists, none refused',
              f' {report["monsters"]} monster(s) with per-hit material drops; 0 invalid list(s) refused' in log,
              [ln for ln in log.splitlines() if 'material drops' in ln][-1:])
        for mid in sample:
            reply = c.call('MissionStart', body=mission_start_body(mid))
            if 'error' in reply:
                check(f'mission {mid} starts', False, reply.get('error'))
                continue
            sent = {(int(r['Qzhp8B40']), int(r['hZtF1s8B'])): r.get('i30R8TAs') for r in reply.get('75t0sx9z', [])}
            want = {(w + 1, k): wire(b.get('material_drops')) for w, s in enumerate(after[mid]['stages'])
                    for k, b in enumerate(s['battle_monsters'])}
            # Rows past the waves are Mimic chest side groups (MonsterParty::entryMimic), which carry none.
            extra = {k: v for k, v in sent.items() if k not in want}
            check(f'mission {mid}: MissionStart sends every wave monster exactly its generated list '
                  f'({sum(1 for v in want.values() if v)} lists; {len(extra)} chest row(s) with none)',
                  {k: sent.get(k, 'absent') for k in want} == want and not any(extra.values()),
                  ([(k, sent.get(k, 'absent'), v) for k, v in want.items() if sent.get(k, 'absent') != v][:3],
                   {k: v for k, v in extra.items() if v}))
            c.call('MissionEnd', body=mission_end_body(mid, serial=started_serial(reply), items=''))
    return check.summary('gen material drops')


if __name__ == '__main__':
    sys.exit(main())
