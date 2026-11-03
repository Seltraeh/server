"""Tests for scripts/vortex_trials_topology.py and the server's Vortex Trials subtree permits (#12).

    python scripts/test_vortex_trials_topology.py PATH_TO_DEBUG_EXE [--port 19988]

Needs a server built with ServerCache::m_vortexTrialAreas (ee6f920f, live since 2026-11-03); registered in
run_bugfix_regressions.py since that deploy.

Offline: install into a hard-linked copy of deploy writes the area and dungeon changes and F_DUNGEON_MST
DUNGEON_VERSION, a second install writes nothing, `check` is complete, and deploy/ is untouched.
Wire (that copy plus gen_vortex_trials.py's archive): UserInfo permits area 100003, Winter Paradise's area, dungeon
and mission, and every Vortex Trials area with a built fight (the five children of 100003 among them); the area table
it serves puts 80013400 under 100003; and no area of the subtree is permitted unless a fight sits under it.
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
import vortex_trials_topology as topo  # noqa: E402
from bf_testkit import ROOT, Fixture, IsolatedServer, Client, Checker, qa_dir  # noqa: E402

DEPLOY = ROOT / 'deploy'


def digest(paths):
    return hashlib.sha256(b''.join(p.read_bytes() for p in paths)).hexdigest()


def mirror(dst: Path):
    if dst.exists():
        shutil.rmtree(dst)
    for d in ('mst', 'archive', 'game_content', 'system'):
        for p in (DEPLOY / d).rglob('*'):
            if p.is_file():
                q = dst / p.relative_to(DEPLOY)
                q.parent.mkdir(parents=True, exist_ok=True)
                os.link(p, q)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19988)
    args = ap.parse_args()
    check = Checker()
    watched = [DEPLOY / 'mst' / 'area_mst.json', DEPLOY / 'mst' / 'dungeon_mst.json',
               DEPLOY / 'archive' / 'mission.json', DEPLOY / 'archive' / 'ai.json',
               DEPLOY / 'game_content' / 'content' / 'F_DUNGEON_MST' / 'manifest.json']
    before = digest(watched)
    root = qa_dir('vortex_trials_topology') / 'root'
    mirror(root)
    applied = not topo.install(DEPLOY, None, check_only=True)[1]     # deploy carries it (live since 2026-11-03)
    log, problems = topo.install(root, None)
    names = sorted(Path(p).name for p in log)
    if applied:
        check('deploy already carries the topology: install on its copy writes nothing', not log and not problems,
              (names, problems))
    else:
        check('install writes the area table, the dungeon table and F_DUNGEON_MST DUNGEON_VERSION',
              not problems and 'area_mst.json' in names and 'dungeon_mst.json' in names and 'manifest.json' in names
              and any(n.startswith(f'Ver{topo.DUNGEON_VERSION}_') for n in names), (names, problems))
    check('a second install writes nothing; check is complete',
          topo.install(root, None)[0] == [] and topo.install(root, None, check_only=True)[1] == [])
    (_, areas), = json.loads((root / 'mst' / 'area_mst.json').read_text(encoding='utf-8')).items()
    (_, dungeons), = json.loads((root / 'mst' / 'dungeon_mst.json').read_text(encoding='utf-8')).items()
    wp = next(a for a in areas if a['VjCY7rX4'] == '80013400')
    wd = next(d for d in dungeons if d['MHx05sXt'] == '80013400')
    check('area 80013400 "Winter Paradise" under 100003, its dungeon moved into it',
          (wp['sHMai2ZG'], wp['V84mzqoX'], wd['VjCY7rX4']) == ('100003', 'Winter Paradise', '80013400'))
    r = subprocess.run([sys.executable, str(HERE / 'gen_vortex_trials.py'), '--archive', str(root / 'archive'),
                        '--out', str(root / 'archive')], capture_output=True, text=True)
    check('the trials generate into the copy', r.returncode == 0, r.stderr[-200:])
    check('deploy/ is untouched (hard-linked copy written by temp + rename)', digest(watched) == before)

    fx = Fixture.create(qa_dir('vortex_trials_topology_run'), port=args.port, archive_root=root / 'archive',
                        mst_root=root / 'mst', document_root=root / 'game_content')
    with IsolatedServer(args.exe, fx):
        reply = Client(fx).call('UserInfo', body={})
    got = {'area': set(), 'dungeon': set(), 'mission': set()}
    for key in ('yXNM8kL3', 'Y73tHKS8', 'Y73mHKS8'):
        for row in reply.get(key, []):
            for k, kind in (('VjCY7rX4', 'area'), ('MHx05sXt', 'dungeon'), ('j28VNcUW', 'mission')):
                if k in row:
                    got[kind].add(row[k])
    served = {a['VjCY7rX4']: a for a in reply.get('3SG2wX0R', [])}
    check('Winter Paradise: area, dungeon and mission 83013400 permitted; the served area is under 100003',
          {'100003', '80013400'} <= got['area'] and '80013400' in got['dungeon'] and '83013400' in got['mission']
          and served.get('80013400', {}).get('sHMai2ZG') == '100003', {k: len(v) for k, v in got.items()})
    children = {'800150', '800154', '800167', '800169', '80013400'}
    check('the five children of Vortex Trials are permitted', children <= got['area'], sorted(children - got['area']))
    parent = {a['VjCY7rX4']: a.get('sHMai2ZG') for a in areas}

    def under_trials(aid, seen=()):
        p = parent.get(aid)
        return p == '100003' or (p not in (None, '', '0') and p not in seen and under_trials(p, seen + (aid,)))
    subtree = {a for a in parent if under_trials(a) and int(a) >= 200000}
    by_area = {}
    for d in dungeons:
        by_area.setdefault(d['VjCY7rX4'], []).append(d['MHx05sXt'])
    built = {a for a in subtree if any(dg in got['dungeon'] for dg in by_area.get(a, []))}
    for a in list(built):
        p = parent.get(a)
        while p in subtree:
            built.add(p)
            p = parent.get(p)
    stray = sorted((got['area'] & subtree) - built)
    check('no Vortex Trials area is permitted without a fight under it', not stray, stray[:5])
    return check.summary('test_vortex_trials_topology')


if __name__ == '__main__':
    sys.exit(main())
