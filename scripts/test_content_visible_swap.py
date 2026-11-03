"""The client checklist's Archive replacement and recovery, end to end without a client.

    python scripts/test_content_visible_swap.py SERVER_EXE PROXY_TEST_EXE POLICY_JSON
                                                [--name unit_ills_thum_810048.png]
                                                [--like unit_ills_thum_820278.png]

Builds the harness's DISPOSABLE root for POLICY_JSON with the harness's own
build_disposable (hard links to the real files, no bytes copied) in a folder of
its own (out/qa/current/content_visible_swap, never a test package's), starts an
isolated server (fresh save, port 19963) whose broker serves that root, and runs
the offline-proxy's content_provider_tests with BF_SWAP_* set, so its visible-swap
case stands in for the game: one provider, the real hook code over fake game
functions.  Between that case's steps this driver runs the harness's own
commands, exactly as the client checklist does:

    swap NAME --like LIKE      restore      park NAME      restore

and the proxy case asserts, for each, that the broker published it, that the
next scene change released exactly the decoded copy the tile was drawn from,
and what the next read returns (docs/CLAUDE_SESSION_12_HANDOFF.md, Priority 2).

The default target is Blaze's Archive tile (No. 8318), unit_ills_thum_810048.png:
the image UnitDictionaryScene::setUnitDictionary actually draws.  Session 11
swapped unit_ills_thum_810048_100.png, the DBB variant only owned-unit lists
draw, so nothing visible could change.

Nothing under deploy/ is written: the broker never writes, the save is fresh,
and the real files of both units' bundles are checked byte-identical with
unchanged mtimes afterwards.
"""
from __future__ import annotations

import argparse
import contextlib
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parent))
import content_provider_client_test as harness  # noqa: E402
from bf_testkit import (LIVE_SAVE, ROOT, Checker, Fixture, IsolatedServer, fps_cap_check,  # noqa: E402
                        qa_dir)

PORT = 19963
STEPS = ('swap', 'restore', 'park', 'restore2')


def sha(path: Path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def use_own_work_folder() -> Path:
    """Point the harness at a folder no test package uses, and at this test's port."""
    work = qa_dir('content_visible_swap')
    work.mkdir(parents=True, exist_ok=True)
    harness.WORK = work
    harness.LAYOUT_RECORD = work / 'layout.json'
    harness.layout_port = lambda layout=None: (PORT, 'visible swap test (no client)')
    return work


def run_quietly(fn, args) -> tuple[int, list[str]]:
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        rc = fn(args)
    lines = out.getvalue().strip().splitlines()
    for line in lines:
        print(f'    harness: {line}', flush=True)
    return rc, lines


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('server_exe')
    ap.add_argument('proxy_tests')
    ap.add_argument('policy')
    ap.add_argument('--name', default='unit_ills_thum_810048.png')
    ap.add_argument('--like', default='unit_ills_thum_820278.png')
    ap.add_argument('--page', action='store_true',
                    help='NAME is a SuperAnim page: the game reads it as "<LocalState>/<page>" (LoadSuperAnimMainDef)')
    args = ap.parse_args()
    check = Checker()
    work = use_own_work_folder()
    policy_path = Path(args.policy).resolve()
    policy = json.loads(policy_path.read_text(encoding='utf-8'))
    name, like = args.name.lower(), args.like.lower()
    targets = harness.owned_targets(policy)
    bundle = next((ms for ms in policy['bundles'].values() if name in {m.lower() for m in ms}), [])
    like_bundle = next((ms for ms in policy['bundles'].values() if like in {m.lower() for m in ms}), [])
    check(f'{name} and {like} are owned by the policy, in different bundles',
          name in targets and like in targets and bundle and like_bundle and bundle != like_bundle,
          (name in targets, like in targets, bundle, like_bundle))
    if check.failures:
        return check.summary('test_content_visible_swap')

    # A mirror left with changes by an interrupted run is rebuilt from the originals.
    state = harness.disposable_state()
    if state.get('changes'):
        print(f'earlier run left changes {sorted(state["changes"])}: rebuilding the mirror')
        harness.disposable_paths()[1].unlink()
    root = harness.build_disposable(policy, str(policy_path))
    involved = {m.lower(): harness.CONTENT / targets[m.lower()] for m in list(bundle) + list(like_bundle)}
    before = {n: (sha(p), os.stat(p).st_mtime_ns) for n, p in involved.items()}
    live_before = sha(LIVE_SAVE)

    signals = work / 'signals'
    if signals.exists():
        shutil.rmtree(signals)
    signals.mkdir()
    expect = work / 'expect.txt'
    expect.write_text(f'name {name} {targets[name]}\nlike {like} {targets[like]}\n'
                      + ''.join(f'bundle {m.lower()}\n' for m in bundle)
                      + ('sep f\n' if args.page else ''), encoding='utf-8')
    system = work / 'system'
    if system.exists():
        shutil.rmtree(system)
    shutil.copytree(ROOT / 'deploy' / 'system', system)
    served = dict(policy, root=str(root), enabled=True)
    (system / 'content_provider.json').write_text(json.dumps(served, indent=1), encoding='utf-8')
    fx = Fixture.create(work / 'server', port=PORT, fresh=True,
                        document_root=ROOT / 'deploy' / 'game_content', system_root=system)
    env = dict(os.environ, BF_SWAP_PORT=str(PORT), BF_SWAP_CONTENT=str(harness.CONTENT),
               BF_SWAP_EXPECT=str(expect), BF_SWAP_SIGNALS=str(signals),
               BF_APPX_DIR=str(ROOT.parent / 'BraveFrontierAppxClient'))
    for k in ('BF_BROKER_PORT', 'BF_BROKER_ROOT', 'BF_FAMILY_PORT', 'BF_FAMILY_ROOT', 'BF_FAMILY_EXPECT'):
        env.pop(k, None)

    commands = {
        'swap': (harness.cmd_swap, SimpleNamespace(name=name, like=like)),
        'restore': (harness.cmd_restore, SimpleNamespace(name=None)),
        'park': (harness.cmd_park, SimpleNamespace(name=name)),
        'restore2': (harness.cmd_restore, SimpleNamespace(name=None)),
    }
    said, timings = {}, {}
    log = work / 'content_provider_tests.log'
    with IsolatedServer(args.server_exe, fx, checks=[fps_cap_check(fx), harness.broker_check('on', None)],
                        timeout=600.0) as server, open(log, 'w', encoding='utf-8') as out:
        print(f'server pid {server.proc.pid} ready on {PORT}; broker serves {root}', flush=True)
        proc = subprocess.Popen([str(args.proxy_tests)], env=env, stdout=out, stderr=subprocess.STDOUT)
        try:
            for step in STEPS:
                ready = signals / f'{step}.ready'
                deadline = time.time() + 900
                while not ready.exists() and proc.poll() is None and time.time() < deadline:
                    time.sleep(0.2)
                if not ready.exists():
                    print(f'the proxy case never asked for {step} (exit {proc.poll()})')
                    break
                fn, ns = commands[step]
                start = time.time()
                rc, lines = run_quietly(fn, ns)
                timings[step] = round(time.time() - start, 1)
                said[step] = lines[-1:] if lines else []
                tmp = signals / f'{step}.done.tmp'
                tmp.write_text(str(rc), encoding='utf-8')
                os.replace(tmp, signals / f'{step}.done')
                if rc != 0:
                    break
            proc.wait(timeout=900)
        finally:
            if proc.poll() is None:
                (signals / 'abort').write_text('driver stopped', encoding='utf-8')
                try:
                    proc.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
    text = log.read_text(encoding='utf-8', errors='replace')
    print(text.strip()[-2500:])
    m = re.search(r'content_provider_tests: (\d+) passed, (\d+) failed, (\d+) skipped', text)
    check('proxy tests completed', m is not None, text[-300:])
    check('proxy tests: no failures', m and m.group(2) == '0' and proc.returncode == 0, m and m.group(0))
    check('the visible-swap case ran (not skipped)', 'SKIP  visible swap integration' not in text)
    check('the case completed all four steps',
          f'visible swap: {name} swapped (like {like}), restored, parked ({len(bundle)} bundle names withdrawn), restored'
          in text and (not args.page or "('<dir>/<page>' path)" in text), text[-600:])
    check('each harness command reported the broker publishing it',
          all(said.get(s) and 'published by the broker' in said[s][0] for s in STEPS), said)
    print(f'  publication: {timings}')

    rc, lines = run_quietly(harness.cmd_disposable, SimpleNamespace())
    check('disposable: every unchanged file is the original itself, no change left', rc == 0
          and any('changes: none' in l for l in lines), lines[-2:])
    after = {n: (sha(p), os.stat(p).st_mtime_ns) for n, p in involved.items()}
    check(f'the real files of both bundles ({len(involved)}) are byte-identical with unchanged mtimes',
          before == after, sorted(n for n in before if before[n] != after.get(n)))
    check('the live save is byte-identical', sha(LIVE_SAVE) == live_before)
    check(f'port {PORT} is free again', not IsolatedServer._listeners(PORT))
    return check.summary('test_content_visible_swap')


if __name__ == '__main__':
    raise SystemExit(main())
