"""Live startup coverage: the proxy's provider and its startup decisions against a REAL broker.

    python scripts/test_content_startup_live.py SERVER_EXE PROXY_TESTS_EXE [--real POLICY.json MANIFEST.json]

Runs offline-proxy's content_provider_tests.exe (TestLiveStartupCoverage, BF_COVERAGE_* variables)
against an isolated server on a fresh save (Session 16, docs/OFFLINE_STARTUP_TRANSITION_PLAN.md
section 8):

  small fixture  (the tree of scripts/test_content_coverage_wire.py)
    complete     -> the C++ parser reads the report as complete for the provider's own generation;
                    the census and the background-download prompt are left out
    a required file parked -> the broker's next generation is incomplete; a provider started
                    now adopts it, and both decisions fall back to the game's own behaviour
    restored     -> complete again
    another copy of the master table in the fixture client's LocalState, then another version
                    beside the right one -> the report is complete, but it was not derived from this
                    client's own files: both decisions fall back (Session 18); the table restored ->
                    left out again
    item / area     (Session 19) the manifest also binds the client's F_ITEM_MST and F_DUNGEON_MST
                    (copies of the server's staged tables in the fixture LocalState): held -> left
                    out; the item table changed, then another dungeon version beside it -> fallback;
                    restored -> left out; the dungeon table's binding omitted while inputs.tables
                    records reading it -> the broker refuses the manifest, the DLL falls back
  --real         the generated policy and manifest over deploy/game_content (read only) with the
                 supported client's build identity, expecting whatever an independent offline
                 evaluation of that policy says (scripts/startup_coverage.py evaluate): complete ->
                 both left out; incomplete -> the census built and the prompt left to the player.
                 The C7 of 2026-10-20 is incomplete (about 3 minutes)

Every binary run also re-runs the unit suite; its exit code and its "0 failed" line both count.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker  # noqa: E402
import test_content_coverage_wire as cov  # noqa: E402

PORT = 19968
REAL_BUILD = '6253F827-01756000-00ED273D'


def run_tests(tests_exe: Path, build: str, expect: str, client: dict | None = None) -> tuple[int, str]:
    """client: the folders the decisions check the report's client files in (Session 18)."""
    env = dict(os.environ, BF_COVERAGE_PORT=str(PORT), BF_COVERAGE_BUILD=build, BF_COVERAGE_EXPECT=expect,
               BF_APPX_DIR=str(ROOT.parent / 'BraveFrontierAppxClient'))
    if client:
        env.update(BF_COVERAGE_LOCALSTATE=str(client['localstate']), BF_COVERAGE_PACKAGE=str(client['package']))
    r = subprocess.run([str(tests_exe)], env=env, capture_output=True, text=True, timeout=900)
    return r.returncode, r.stdout + r.stderr


def judge(check: Checker, label: str, code: int, out: str, expect: str) -> None:
    """expect: complete (both left out), incomplete (built and asked), or other-client (a complete
    report for client files this client does not hold: built and asked)."""
    summary = re.search(r'content_provider_tests: (\d+) passed, (\d+) failed', out)
    live = re.search(r'live startup coverage: (\w+), generation (\d+).*census ([\w ]+), prompt ([\w ]+)', out)
    fails = [l for l in out.splitlines() if l.startswith('FAIL')]
    check(f'{label}: the proxy suite passes with the live case', code == 0 and summary and summary.group(2) == '0',
          fails[:5] or out[-400:])
    want = ('left out', 'left out') if expect == 'complete' else ('built', 'asked')
    report = 'complete' if expect == 'other-client' else expect
    check(f'{label}: the C++ side read "{report}" -> census {want[0]}, prompt {want[1]}',
          live is not None and live.group(1) == report and live.group(3) == want[0] and live.group(4) == want[1],
          live.group(0) if live else 'no live line')


def main() -> int:
    args = sys.argv[1:]
    real = None
    if '--real' in args:
        i = args.index('--real')
        real = (Path(args[i + 1]), Path(args[i + 2]))
        del args[i:i + 3]
    exe, tests_exe = Path(args[0]), Path(args[1])
    check = Checker()
    cov.PORT = PORT
    cov.WORK = ROOT / 'out' / 'qa' / 'current' / 'session16' / 'startup_live'
    cov.WORK.mkdir(parents=True, exist_ok=True)
    fx = cov.build_tree(cov.WORK / 'tree')
    cov.write_system(fx, fx['policy'], fx['manifest'])
    broker = cov.Broker(PORT)
    with cov.server(exe, fx, 'small'):
        broker.wait_ready()
        code, out = run_tests(tests_exe, cov.BUILD, 'complete', fx)
        judge(check, 'small fixture, complete', code, out, 'complete')
        page = fx['content'] / 'event' / 'page1.png'
        parked = cov.WORK / 'parked_page1.png'
        os.replace(page, parked)
        report = cov.wait_coverage(broker, lambda c: c.get('complete') == '0')
        check('small fixture: the broker reports the parked file', report.get('complete') == '0', report.get('reason'))
        code, out = run_tests(tests_exe, cov.BUILD, 'incomplete', fx)
        judge(check, 'small fixture, a required file parked', code, out, 'incomplete')
        code, out = run_tests(tests_exe, 'FFFFFFFF-00000000-00000000', 'incomplete', fx)
        judge(check, 'small fixture, another client build', code, out, 'incomplete')
        os.replace(parked, page)
        report = cov.wait_coverage(broker, lambda c: c.get('complete') == '1')
        code, out = run_tests(tests_exe, cov.BUILD, 'complete', fx)
        judge(check, 'small fixture, restored', code, out, 'complete')
        # Session 18: the same complete report on a client whose master table is not the manifest's.
        table = fx['localstate'] / 'Ver1_Fixture1.dat'
        original = table.read_bytes()
        table.write_bytes(original + b' (another copy of the table)')
        code, out = run_tests(tests_exe, cov.BUILD, 'other-client', fx)
        judge(check, 'small fixture, a client holding another copy of the master table', code, out, 'other-client')
        (fx['localstate'] / 'Ver2_Fixture1.dat').write_bytes(b'a newer version beside the right one')
        table.write_bytes(original)
        code, out = run_tests(tests_exe, cov.BUILD, 'other-client', fx)
        judge(check, 'small fixture, another version of the table in LocalState', code, out, 'other-client')
        (fx['localstate'] / 'Ver2_Fixture1.dat').unlink()
        code, out = run_tests(tests_exe, cov.BUILD, 'complete', fx)
        judge(check, 'small fixture, the client\'s own table again', code, out, 'complete')

    # Session 19: the item and area lists read the client's own F_ITEM_MST and F_DUNGEON_MST.  The fixture
    # client holds copies of the server's STAGED tables (deploy/game_content/content/<TABLE>/, never a
    # player's LocalState); the manifest binds them, and inputs.tables records reading them.
    import hashlib
    staged = {table: ROOT / 'deploy' / 'game_content' / 'content' / table / name
              for table, name in (('F_ITEM_MST', 'Ver1028_83JWTCGy.dat'), ('F_DUNGEON_MST', 'Ver1003_B57MJYoF.dat'))}
    copies = {}
    for table, src in staged.items():
        copies[table] = fx['localstate'] / src.name
        copies[table].write_bytes(src.read_bytes())
    bound = json.loads(json.dumps(fx['manifest']))
    bound['inputs']['client_files'] = bound['inputs']['client_files'] + [
        {'root': 'localstate', 'path': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in copies.values()]
    bound['inputs']['tables'] = {'F_ITEM_MST': {'version': 1028, 'parts': [copies['F_ITEM_MST'].name]},
                                 'M_AREA_MST': {'served': '3SG2wX0R', 'joined': [
                                     {'version': 1003, 'parts': [copies['F_DUNGEON_MST'].name]}]}}
    cov.write_system(fx, fx['policy'], bound)
    with cov.server(exe, fx, 'item_area'):
        broker.wait_ready()
        code, out = run_tests(tests_exe, cov.BUILD, 'complete', fx)
        judge(check, 'item/area tables bound, the client holds them', code, out, 'complete')
        item = copies['F_ITEM_MST'].read_bytes()
        copies['F_ITEM_MST'].write_bytes(item[:-8] + b'AAAAAAA=')
        code, out = run_tests(tests_exe, cov.BUILD, 'other-client', fx)
        judge(check, 'item/area: the client\'s F_ITEM_MST is not the one the manifest was derived from (a stale '
              'binding)', code, out, 'other-client')
        copies['F_ITEM_MST'].write_bytes(item)
        older = fx['localstate'] / 'Ver1002_B57MJYoF.dat'
        older.write_bytes(b'an older version of the dungeon table beside the right one')
        code, out = run_tests(tests_exe, cov.BUILD, 'other-client', fx)
        judge(check, 'item/area: another version of F_DUNGEON_MST in LocalState', code, out, 'other-client')
        older.unlink()
        code, out = run_tests(tests_exe, cov.BUILD, 'complete', fx)
        judge(check, 'item/area: the client\'s own tables again', code, out, 'complete')
    omitted = json.loads(json.dumps(bound))
    omitted['inputs']['client_files'] = [f for f in omitted['inputs']['client_files'] if 'B57MJYoF' not in f['path']]
    cov.write_system(fx, fx['policy'], omitted)
    with cov.server(exe, fx, 'item_area_omitted'):
        broker.wait_ready()
        report = cov.coverage(broker)
        check('item/area: inputs.tables records the dungeon table, client_files does not bind it -> the broker refuses '
              'the manifest', report.get('manifest') == 'none' and 'inputs.tables' in report.get('manifest_error', ''),
              {k: report.get(k) for k in ('complete', 'manifest', 'manifest_error')})
        code, out = run_tests(tests_exe, cov.BUILD, 'incomplete', fx)
        judge(check, 'item/area: an omitted client binding', code, out, 'incomplete')
    for p in copies.values():
        p.unlink()
    cov.write_system(fx, fx['policy'], fx['manifest'])
    if real is not None:
        import startup_coverage as sc
        policy = json.loads(real[0].read_text(encoding='utf-8'))
        manifest = json.loads(real[1].read_text(encoding='utf-8'))
        offline = sc.evaluate(policy, manifest, sc.Content(sc.CONTENT, policy.get('aliases', {})), cov.ISOLATED_SERVED)
        expect = 'complete' if offline['complete'] else 'incomplete'
        policy['enabled'] = True
        cov.write_system(fx, policy, real[1].read_text(encoding='utf-8'))
        with cov.server(exe, fx, 'real', document_root=cov.REAL):
            broker.wait_ready(timeout=300)
            code, out = run_tests(tests_exe, REAL_BUILD, expect)
            judge(check, f'real tree ({real[0].name}; the offline evaluation says {expect})', code, out, expect)
            print(next((l for l in out.splitlines() if l.startswith('live startup coverage')), ''))
    return check.summary('test_content_startup_live')


if __name__ == '__main__':
    raise SystemExit(main())
