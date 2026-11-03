"""Wire tests for the startup coverage report (/offline_mod/content/v1/coverage, Session 16).

The broker answers, for one generation of its index, whether every name of both startup sets of
<system_root>/content_startup.json (scripts/startup_coverage.py) is owned and servable.  These
tests run isolated servers on fresh saves over a small content root under
out/qa/current/session16/coverage_wire -- never the live save -- and try to make the report claim
completeness it does not have.  Every verdict is checked three ways: the report's own `complete`
line, the DLL's rule (content_provider_client_test.parse_coverage, which mirrors cp_core.cpp
ParseCoverage) and an independent offline evaluation (startup_coverage.evaluate) -- they must agree.

  baseline        complete 1 for the client build it was derived for, proved by the DLL's rule; the
                  report names the epoch, generation, policy and manifest digests it describes
  build           no build, another build: complete 0 (an unsupported client)
  missing file    a required file removed: complete 0 at a NEWER generation; restored: complete 1
  shared member   a member two bundles share withdrawn: both bundles' names unavailable; recovery
  collision       a second, different copy of a required name appears: ambiguous; recovery
  absent          a required name in no source is a problem (QC 2026-10-20): complete 0 with a
                  `P login absent` row; when such a file appears unowned it is unowned; gone again,
                  absent again -- never accepted
  unknown list    a list the manifest could not derive: complete 0
  client files    the report lists inputs.client_files (inputs N, one I row each: the client's own
                  master tables and package files, which the DLL hashes); a manifest naming none
                  of them is complete 0 (Session 18)
  server tables   the manifest names the response tables the fixture server serves (deploy/mst,
                  read only), which its status lists (T rows, server_binding 1); a wrong digest or
                  none named is complete 0 with startup_coverage.server_binding's reason, word for
                  word (Session 18 correction; scripts/test_content_server_tables.py changes the
                  tables themselves on a copy)
  detail          the default lists 25 names per class; detail=1 lists every one
  manifests       none, a disabled broker, and every manifest the broker must refuse
                  (test_startup_coverage_manifest.SCHEMA cases): QC's login-only and empty-set
                  manifests, unknown / malformed / duplicate sets, reasonless or contradictory
                  exclusions, stray absent notes, unknown keys, another format, no build,
                  malformed client files -- complete 0 with manifest_error, and the offline
                  evaluation refuses them too

With --real POLICY MANIFEST it also starts the broker over the real deploy/game_content (read
only) and checks its report against the offline evaluation of the same policy and manifest: the
verdict, every class count, every problem name and every unknown list (about 3 minutes at C7
scale).  The verdict belongs to the content: incomplete while any list is unknown or any required
name is not owned.

    python scripts/test_content_coverage_wire.py EXE [--real POLICY.json MANIFEST.json]
"""
from __future__ import annotations

import copy
import hashlib
import json
import os
import shutil
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Fixture, IsolatedServer  # noqa: E402
from content_provider_client_test import parse_coverage as dll_rule  # noqa: E402
import startup_coverage as sc  # noqa: E402
from test_content_broker_wire import Broker  # noqa: E402

PORT = 19966
BUILD = 'TEST1234-00ABCDEF-00123456'
WORK = ROOT / 'out' / 'qa' / 'current' / 'session16' / 'coverage_wire'
REAL = ROOT / 'deploy' / 'game_content'
# What every fixture server here serves: deploy/mst's response tables, and the area table it synthesizes
# from a working folder without game_content -- so no translated banner (Session 19).
ISOLATED_SERVED = sc.served_tables(banner_dir=None)


def sha_file(path: Path) -> str:
    return 'sha256:' + hashlib.sha256(Path(path).read_bytes()).hexdigest()


def coverage(broker: Broker, build: str | None = BUILD, detail: bool = False) -> dict:
    """The raw report fields, plus 'proved'/'why' under the DLL's own rule for this build."""
    params = {}
    if build is not None:
        params['build'] = build
    if detail:
        params['detail'] = '1'
    code, _, body = broker.get('/coverage', params or None)
    text = body.decode()
    out = {'code': code, 'text': text, 'sets': {}, 'P': [], 'U': [], 'I': [], 'end': text.endswith('end\n')}
    for line in text.splitlines():
        parts = line.split('\t')
        if parts[0] in ('P', 'U', 'I') and len(parts) > 1:
            out[parts[0]].append(tuple(parts[1:]))
        elif line.startswith('set '):
            words = line.split(' ')
            out['sets'][words[1]] = {words[i]: int(words[i + 1]) for i in range(2, len(words) - 1, 2)}
        elif ' ' in line:
            k, v = line.split(' ', 1)
            out[k] = v
    rule = dll_rule(text, build) if code == 200 else None
    out['readable'] = rule is not None
    out['proved'] = bool(rule and rule['proved'])
    out['why'] = rule['why'] if rule else 'not a report'
    return out


def wait_coverage(broker: Broker, pred, timeout: float = 20.0, **kw) -> dict:
    deadline = time.time() + timeout
    cov = coverage(broker, **kw)
    while not pred(cov) and time.time() < deadline:
        time.sleep(0.2)
        cov = coverage(broker, **kw)
    return cov


def put(path: Path, data: bytes) -> bytes:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return data


SCRIPT1 = (b'*,type=PARAM,id=6,param=/sound/shared.mp3,#\r\n'
           b'*,type=PARAM,id=1,param=bg1:page1.png,#\r\n*,type=STOP,id=-1,#\r\n')
SCRIPT2 = b'*,type=PARAM,id=33,param=shared.mp3,#\r\n*,type=STOP,id=-1,#\r\n'


def build_tree(base: Path) -> dict:
    if base.exists():
        shutil.rmtree(base, ignore_errors=True)
    content = base / 'game_content' / 'content'
    system = base / 'system'
    system.mkdir(parents=True)
    files = {
        'login/a_login.png': b'\x89PNG\r\n\x1a\nlogin',
        '_dlcbundle/layout_scene.csv': b'0,0,640,960,0,0,\n',
        'sound/shared.mp3': b'ID3shared',
        'event/page1.png': b'\x89PNG\r\n\x1a\npage1',
        'event/script1.txt': SCRIPT1,
        'event/script2.txt': SCRIPT2,
        'common/b_bg.png': b'\x89PNG\r\n\x1a\nbackground',
        'extra/not_required.png': b'\x89PNG\r\n\x1a\nunrequired',
    }
    for rel, data in files.items():
        put(content / rel, data)
    for real in (ROOT / 'deploy' / 'system').glob('*.json'):
        if real.name not in ('content_provider.json', 'content_startup.json'):
            shutil.copyfile(real, system / real.name)
    policy = {
        'enabled': True,
        'scope': [rel for rel in files if not rel.startswith('extra/')],
        'aliases': {},
        'extensions': ['.png', '.csv', '.mp3', '.txt'],
        'exclude': ['UserDefault.xml', 'XMLFileProcessor.xml', '*.dat', '*.xml', '*.ini', '*.tmp', '*.json', '*.zip',
                    'Thumbs.db', '.DS_*'],
        'bundles': {'s16_txt_script1': ['script1.txt', 'shared.mp3', 'page1.png'],
                    's16_txt_script2': ['script2.txt', 'shared.mp3'],
                    's16_file_a_login': ['a_login.png'], 's16_file_layout': ['layout_scene.csv'],
                    's16_file_b_bg': ['b_bg.png']},
    }
    # The client's own files the manifest says it was derived from (Session 18): a master table in a
    # LocalState and the package's resourceMap.  The broker only lists them; the DLL hashes its copies
    # (scripts/test_content_startup_live.py hands these folders to the proxy suite).
    client = base / 'client'
    table = put(client / 'LocalState' / 'Ver1_Fixture1.dat', b'a master table of the fixture client')
    rmap = put(client / 'package' / 'Assets' / 'Resources' / 'resourceMap', b'1:a_login.png,\r\n')
    client_files = [
        {'root': 'localstate', 'path': 'Ver1_Fixture1.dat', 'sha256': hashlib.sha256(table).hexdigest()},
        {'root': 'package', 'path': 'Assets/Resources/resourceMap', 'sha256': hashlib.sha256(rmap).hexdigest()},
    ]
    # The response tables the fixture server serves (its mst_root is deploy/mst, read only): the broker
    # binds the manifest to them (Session 18 correction).  The sets here are a wire fixture, not names
    # those tables derive -- scripts/test_content_server_tables.py derives them for real.
    server_files = sc.server_file_entries(ISOLATED_SERVED)
    # Complete as written: both sets non-empty, every name owned, nothing absent, the one exclusion
    # (with its reason) in no set, the client files and the server tables named.
    manifest = {
        'format': 'bfcp-startup 1', 'build': BUILD, 'generated': 'test',
        'inputs': {'fixture': True, 'client_files': client_files, 'server_files': server_files},
        'sets': {'login': ['a_login.png', 'layout_scene.csv', 'page1.png', 'script1.txt', 'shared.mp3'],
                 'background': ['b_bg.png', 'script2.txt', 'shared.mp3']},
        'unknown': [], 'excluded': {'thumbs.db': 'the client reports it present itself'},
        'absent': {},
    }
    return {'base': base, 'content': content, 'system': system, 'policy': policy, 'manifest': manifest,
            'localstate': client / 'LocalState', 'package': client / 'package'}


def write_system(fx: dict, policy: dict | None, manifest) -> None:
    for name in ('content_provider.json', 'content_startup.json'):
        p = fx['system'] / name
        if p.exists():
            p.unlink()
    if policy is not None:
        (fx['system'] / 'content_provider.json').write_text(json.dumps(policy, indent=1), encoding='utf-8')
    if manifest is not None:
        text = manifest if isinstance(manifest, str) else json.dumps(manifest, indent=1)
        (fx['system'] / 'content_startup.json').write_text(text, encoding='utf-8')


def server(exe: Path, fx: dict, label: str, document_root: Path | None = None) -> IsolatedServer:
    sfx = Fixture.create(WORK / f'server_{label}', port=PORT, fresh=True,
                         document_root=document_root or fx['base'] / 'game_content', system_root=fx['system'])
    return IsolatedServer(exe, sfx, log_name='server.log')


def agree(check: Checker, label: str, cov: dict, policy: dict, manifest: dict, content: Path) -> dict:
    """The broker's live report against the offline evaluation of the same tree, name by name."""
    offline = sc.evaluate(policy, manifest, sc.Content(content, policy.get('aliases', {})), ISOLATED_SERVED)
    live = {(s, c, n) for s, c, n in cov['P']}
    expected = {(s, c, n) for s, v in offline['sets'].items() for c, names in v['problems'].items() for n in names}
    counts_ok = all(cov['sets'].get(s, {}).get('required') == sum(k for c, k in v.items() if c != 'problems')
                    and all(cov['sets'].get(s, {}).get(c, 0) == v.get(c, 0)
                            for c in ('ok',) + sc_classes())
                    for s, v in offline['sets'].items())
    verdict = cov.get('complete') == ('1' if offline['complete'] else '0') and cov['proved'] == offline['complete']
    check(f'{label}: the broker, the DLL\'s rule and the offline evaluation agree '
          f'({"complete" if offline["complete"] else "incomplete"})',
          verdict and counts_ok and live == expected
          and len(cov['U']) == len(offline['unknown']),
          {'broker': cov.get('complete'), 'dll': cov['proved'], 'offline': offline['complete'],
           'only live': sorted(live - expected)[:6], 'only offline': sorted(expected - live)[:6],
           'sets': cov['sets'], 'offline sets': {s: {c: n for c, n in v.items() if c != 'problems'}
                                                for s, v in offline['sets'].items()}})
    return offline


def sc_classes() -> tuple:
    return ('absent', 'unowned', 'missing', 'ambiguous', 'unavailable')


def main() -> int:
    args = sys.argv[1:]
    real = None
    if '--real' in args:
        i = args.index('--real')
        real = (Path(args[i + 1]), Path(args[i + 2]))
        del args[i:i + 3]
    exe = Path(args[0] if args else ROOT / 'out/qa/current/session16/bin/gimuserverw.exe')
    check = Checker()
    WORK.mkdir(parents=True, exist_ok=True)
    fx = build_tree(WORK / 'tree')
    content = fx['content']
    policy, manifest = fx['policy'], fx['manifest']
    check('the fixture manifest is one the offline schema accepts', not sc.validate_manifest(manifest),
          sc.validate_manifest(manifest))
    write_system(fx, policy, manifest)
    broker = Broker(PORT)

    with server(exe, fx, 'main'):
        st = broker.wait_ready()
        cov = coverage(broker)
        check('baseline: 200, terminated, protocol line', cov['code'] == 200 and cov['end']
              and cov['text'].startswith('bfcp 2\ncoverage 1\n'), cov['text'][:200])
        check('baseline: complete for the build the manifest was derived for -- and PROVED by the DLL\'s rule',
              cov.get('complete') == '1' and cov.get('build_match') == '1' and cov['proved'], (cov['why'], cov['text']))
        check('baseline: the report names the epoch and generation it describes',
              cov.get('epoch') == st['epoch'] and cov.get('generation') == st['generation'], (cov.get('generation'), st))
        check('baseline: policy and manifest digests are the files the broker loaded',
              cov.get('policy') == sha_file(fx['system'] / 'content_provider.json')
              and cov.get('manifest') == sha_file(fx['system'] / 'content_startup.json'), cov['text'])
        check('baseline: login and background counts (the excluded name is in no set)',
              cov['sets'].get('login') == {'required': 5, 'ok': 5, 'absent': 0, 'unowned': 0, 'missing': 0,
                                           'ambiguous': 0, 'unavailable': 0}
              and cov['sets'].get('background', {}).get('required') == 3 and cov['sets']['background']['ok'] == 3
              and cov.get('excluded') == '1', cov['sets'])
        check('baseline: the report lists the client files the manifest names (inputs 2, one I row each)',
              cov.get('inputs') == '2' and cov['I'] == [(f['root'], f['path'], 'sha256:' + f['sha256'])
                                                       for f in manifest['inputs']['client_files']], cov['text'])
        agree(check, 'baseline', cov, policy, manifest, content)
        again = coverage(broker)
        check('same generation, same answer', again['text'] == cov['text'])
        _c, _h, body = broker.get('/status')
        status = body.decode().splitlines()
        tables = {sc.server_input_name(p[1], p[2]): p[3] for p in (l.split('\t') for l in status)
                  if p[0] == 'T' and len(p) == 4 and p[1] in sc.SERVER_ROOTS}
        check('baseline: the status lists the response tables the server serves (T rows, the digests of the bytes it '
              'parsed, and of the area table\'s served projection) and the manifest bound to them (server_binding 1)',
              tables == {sc.server_input_name(f['root'], f['path']): 'sha256:' + f['sha256']
                         for f in manifest['inputs']['server_files']}
              and 'server_binding 1' in status and f'server_tables {len(tables)}' in status
              and sorted(tables) == sorted(sc.server_sources()),
              (tables, [l for l in status if l.startswith('server_')]))

        none = coverage(broker, build=None)
        check('no client build: not complete', none.get('complete') == '0' and not none['proved']
              and 'no client build' in none.get('reason', ''), none.get('reason'))
        other = coverage(broker, build='6253F827-01756000-00ED273E')
        check('another client build: not complete, build_match 0', other.get('complete') == '0' and not other['proved']
              and other.get('build_match') == '0' and 'derived for client build' in other.get('reason', ''),
              other.get('reason'))

        # A required file removed: the report moves to a newer generation and says incomplete.
        g0 = int(cov['generation'])
        parked = WORK / 'parked_page1.png'
        os.replace(content / 'event' / 'page1.png', parked)
        cov = wait_coverage(broker, lambda c: c.get('complete') == '0')
        check('missing file: incomplete at a newer generation', cov.get('complete') == '0' and not cov['proved']
              and int(cov['generation']) > g0, (cov.get('generation'), cov.get('reason')))
        probs = set(cov['P'])
        check('missing file: its bundle is withdrawn -- script and page unavailable',
              ('login', 'unavailable', 'script1.txt') in probs and ('login', 'unavailable', 'page1.png') in probs, probs)
        check('missing file: the member it shares withdraws the other bundle too (script2 unavailable)',
              ('background', 'unavailable', 'script2.txt') in probs
              and ('background', 'unavailable', 'shared.mp3') in probs, probs)
        agree(check, 'missing file', cov, policy, manifest, content)
        os.replace(parked, content / 'event' / 'page1.png')
        g1 = int(cov['generation'])
        cov = wait_coverage(broker, lambda c: c.get('complete') == '1')
        check('missing file: restored -> complete again, newer generation', cov.get('complete') == '1' and cov['proved']
              and int(cov['generation']) > g1, (cov.get('generation'), cov.get('reason')))

        # A member two bundles share withdrawn directly.
        g1 = int(cov['generation'])
        os.replace(content / 'sound' / 'shared.mp3', WORK / 'parked_shared.mp3')
        cov = wait_coverage(broker, lambda c: c.get('complete') == '0')
        probs = set(cov['P'])
        check('shared member: both scripts unavailable, incomplete', cov.get('complete') == '0'
              and ('login', 'unavailable', 'script1.txt') in probs and ('background', 'unavailable', 'script2.txt') in probs
              and int(cov['generation']) > g1, (probs, cov.get('generation')))
        agree(check, 'shared member', cov, policy, manifest, content)
        os.replace(WORK / 'parked_shared.mp3', content / 'sound' / 'shared.mp3')
        cov = wait_coverage(broker, lambda c: c.get('complete') == '1')
        check('shared member: restored -> complete', cov.get('complete') == '1' and cov['proved'], cov.get('reason'))

        # A second, different copy of a required name: ambiguous in the flat namespace.
        g1 = int(cov['generation'])
        put(content / 'other' / 'b_bg.png', b'\x89PNG\r\n\x1a\nANOTHER background')
        cov = wait_coverage(broker, lambda c: c.get('complete') == '0')
        check('collision: a differing copy makes the name ambiguous, incomplete',
              cov.get('complete') == '0' and ('background', 'ambiguous', 'b_bg.png') in set(cov['P'])
              and int(cov['generation']) > g1, (cov['P'], cov.get('reason')))
        agree(check, 'collision', cov, policy, manifest, content)
        (content / 'other' / 'b_bg.png').unlink()
        cov = wait_coverage(broker, lambda c: c.get('complete') == '1')
        check('collision: removed -> complete', cov.get('complete') == '1' and cov['proved'], cov.get('reason'))

    # A required name in no source is a gap, never an exemption (QC 2026-10-20 finding 3).
    absent = copy.deepcopy(manifest)
    absent['sets']['login'] = sorted(absent['sets']['login'] + ['absent_x.png'])
    absent['absent'] = {'absent_x.png': 'absent from every source (test)'}
    write_system(fx, policy, absent)
    with server(exe, fx, 'absent'):
        broker.wait_ready()
        cov = coverage(broker)
        check('absent: complete 0, "set login: 1 absent", a P login absent row; the DLL\'s rule agrees',
              cov.get('complete') == '0' and not cov['proved'] and 'set login: 1 absent' in cov.get('reason', '')
              and ('login', 'absent', 'absent_x.png') in set(cov['P']) and cov['sets']['login']['absent'] == 1,
              (cov.get('reason'), cov['P'], cov['sets'].get('login')))
        agree(check, 'absent', cov, policy, absent, content)
        put(content / 'extra' / 'absent_x.png', b'\x89PNG\r\n\x1a\nnow it exists')
        cov = wait_coverage(broker, lambda c: ('login', 'unowned', 'absent_x.png') in set(c['P']))
        check('absent appears: a file now exists but is not owned -> unowned, still incomplete',
              cov.get('complete') == '0' and ('login', 'unowned', 'absent_x.png') in set(cov['P'])
              and cov['sets']['login']['absent'] == 0, (cov['P'], cov['sets'].get('login')))
        agree(check, 'absent appears', cov, policy, absent, content)
        (content / 'extra' / 'absent_x.png').unlink()
        cov = wait_coverage(broker, lambda c: ('login', 'absent', 'absent_x.png') in set(c['P']))
        check('absent gone again: absent again -- incomplete, never accepted',
              cov.get('complete') == '0' and not cov['proved'] and cov['sets']['login']['absent'] == 1, cov['sets'].get('login'))

    # A list the manifest could not derive.
    unknown = dict(copy.deepcopy(manifest), unknown=['category x: names enumerated from existing files'])
    write_system(fx, policy, unknown)
    with server(exe, fx, 'unknown'):
        broker.wait_ready()
        cov = coverage(broker)
        check('unknown list: complete 0, the U row, the DLL\'s rule agrees',
              cov.get('complete') == '0' and not cov['proved'] and 'could not derive' in cov.get('reason', '')
              and cov['U'] == [('category x: names enumerated from existing files',)], (cov.get('reason'), cov['U']))
        agree(check, 'unknown list', cov, policy, unknown, content)

    # Session 18: a manifest that names none of the client's own files cannot be bound to a client.
    unbound = copy.deepcopy(manifest)
    del unbound['inputs']['client_files']
    write_system(fx, policy, unbound)
    with server(exe, fx, 'unbound'):
        broker.wait_ready()
        cov = coverage(broker)
        check('no client files named: complete 0 with the reason, inputs 0 and no I row, never proved',
              cov.get('complete') == '0' and not cov['proved'] and cov.get('inputs') == '0' and not cov['I']
              and "names none of the client's own inputs" in cov.get('reason', ''), (cov.get('reason'), cov['I']))
        agree(check, 'no client files named', cov, policy, unbound, content)

    # Session 18 correction: a manifest derived from other bytes of a served table, or naming none.
    served = ISOLATED_SERVED
    stale = copy.deepcopy(manifest)
    stale['inputs']['server_files'] = [dict(f, sha256='0' * 64) if f['path'] == 'banner_info_mst.json' else f
                                       for f in stale['inputs']['server_files']]
    stale_area = copy.deepcopy(manifest)
    stale_area['inputs']['server_files'] = [dict(f, sha256='1' * 64) if f['root'] == 'served' else f
                                            for f in stale_area['inputs']['server_files']]
    no_area = copy.deepcopy(manifest)
    no_area['inputs']['server_files'] = [f for f in no_area['inputs']['server_files'] if f['root'] != 'served']
    no_tables = copy.deepcopy(manifest)
    del no_tables['inputs']['server_files']
    for label, man in (('a served table with another digest', stale),
                       ('the area table derived from other served rows (Session 19)', stale_area),
                       ('the served area table not named (Session 19)', no_area), ('no server tables named', no_tables)):
        write_system(fx, policy, man)
        with server(exe, fx, 'server_tables'):
            broker.wait_ready()
            cov = coverage(broker)
            status = broker.status()
        want = sc.server_binding(man, served)
        check(f'server tables, {label}: complete 0 with startup_coverage.server_binding\'s reason word for word, '
              'server_binding 0, never proved', cov.get('complete') == '0' and not cov['proved']
              and cov.get('reason') == want and bool(want) and status.get('server_binding') == '0',
              (cov.get('reason'), want, status.get('server_binding')))
        agree(check, f'server tables, {label}', cov, policy, man, content)

    # detail=1: more problems than the default listing shows (in a required set, not an extra one).
    many = copy.deepcopy(manifest)
    many['sets']['background'] = sorted(many['sets']['background'] + [f'never_{i:02d}.png' for i in range(30)])
    write_system(fx, policy, many)
    with server(exe, fx, 'detail'):
        broker.wait_ready()
        short, full = coverage(broker), coverage(broker, detail=True)
        check('detail: 30 missing names, 25 listed by default, all 30 with detail=1',
              short['sets'].get('background', {}).get('missing') == 30
              and len([p for p in short['P'] if p[0] == 'background']) == 25
              and len([p for p in full['P'] if p[0] == 'background']) == 30 and short.get('complete') == '0',
              (short['sets'].get('background'), len(short['P']), len(full['P'])))
        agree(check, 'detail', full, policy, many, content)

    from test_startup_coverage_manifest import schema_cases
    cases = [('no manifest', policy, None,
              lambda c: c.get('manifest') == 'none' and 'no startup manifest' in c.get('reason', '')),
             ('a disabled broker', dict(policy, enabled=False), manifest,
              lambda c: c.get('enabled') == '0' and c.get('reason') == 'broker disabled')]
    cases += [(label, policy, bad, lambda c: c.get('manifest') == 'none' and bool(c.get('manifest_error')))
              for label, bad in schema_cases(copy.deepcopy(manifest))]
    for n, (label, pol, man, judge) in enumerate(cases):
        write_system(fx, pol, man)
        with server(exe, fx, f'case{n}'):
            broker.wait_ready()
            cov = coverage(broker)
        refused_offline = man is None or bool(sc.validate_manifest(man)) or not pol.get('enabled', True)
        check(f'{label}: complete 0 with the reason, never proved; the offline schema refuses it too',
              cov.get('complete') == '0' and not cov['proved'] and judge(cov) and refused_offline,
              {k: cov.get(k) for k in ('complete', 'reason', 'manifest', 'manifest_error', 'enabled')})

    if real is not None:
        # The verdict is the content's, not the test's: the broker's live report over the real tree
        # must match, name by name, the offline evaluation (startup_coverage.evaluate resolves the
        # policy as ContentBroker::recomputeOwned does).
        policy_file, manifest_file = real
        rpolicy = json.loads(policy_file.read_text(encoding='utf-8'))
        rmanifest = json.loads(manifest_file.read_text(encoding='utf-8'))
        check(f'real manifest ({manifest_file.name}) is one the schema accepts', not sc.validate_manifest(rmanifest),
              sc.validate_manifest(rmanifest))
        rcontent = sc.Content(sc.CONTENT, rpolicy.get('aliases', {}))
        offline = sc.evaluate(rpolicy, rmanifest, rcontent, ISOLATED_SERVED)
        rpolicy['enabled'] = True
        write_system(fx, rpolicy, manifest_file.read_text(encoding='utf-8'))
        with server(exe, fx, 'real', document_root=REAL):
            broker.wait_ready(timeout=300)
            cov = coverage(broker, build=rmanifest['build'], detail=True)
            status = broker.status()
        check('real tree: the manifest is bound to the response tables the server serves (server_binding 1): '
              f'"{offline.get("server_binding") or "bound"}" offline',
              status.get('server_binding') == '1' and offline.get('server_binding') == '', status.get('server_binding'))
        want = '1' if offline['complete'] else '0'
        check(f'real tree ({policy_file.name}): the broker says complete {cov.get("complete")}, the DLL\'s rule '
              f'{"proved" if cov["proved"] else "not proved"}, the offline evaluation {want}',
              cov.get('complete') == want and cov['proved'] == offline['complete'] and cov.get('build_match') == '1',
              {k: cov.get(k) for k in ('complete', 'reason', 'generation', 'build_match')} | {'why': cov['why']})
        rfiles = (rmanifest.get('inputs') or {}).get('client_files') or []
        check(f'real tree: the report lists the manifest\'s {len(rfiles)} client file(s), one I row each',
              cov.get('inputs') == str(len(rfiles))
              and cov['I'] == [(f['root'], f['path'], 'sha256:' + f['sha256']) for f in rfiles], cov['I'][:3])
        for set_name, classes in offline['sets'].items():
            counts = {k: n for k, n in classes.items() if k != 'problems'}
            got = cov['sets'].get(set_name, {})
            check(f'real tree, {set_name}: the broker counts every class as the offline evaluation does {counts}',
                  got.get('required') == sum(counts.values())
                  and all(got.get(k, 0) == counts.get(k, 0) for k in set(got) - {'required'} | set(counts)), got)
        live = {(s, c, n) for s, c, n in cov['P']}
        expected = {(s, c, n) for s, v in offline['sets'].items() for c, names in v['problems'].items() for n in names}
        check(f'real tree: the same {len(expected)} problem name(s), name by name (absent names among them)',
              live == expected, {'only live': sorted(live - expected)[:10], 'only offline': sorted(expected - live)[:10]})
        check(f'real tree: the same {len(offline["unknown"])} unknown list(s)',
              [u for (u,) in cov['U']] == offline['unknown'] and cov.get('unknown') == str(len(offline['unknown'])),
              (cov.get('unknown'), len(offline['unknown'])))
        if want == '0':
            check('real tree: the report gives its reason', bool(cov.get('reason')), cov.get('reason'))
        print(f'real tree: complete {cov.get("complete")}, {cov["sets"]}; generation {cov.get("generation")},'
              f' manifest {cov.get("manifest", "")[:23]}; unknown {cov.get("unknown")}; reason {cov.get("reason", "-")}')
        by_class = {}
        for s, c, n in sorted(live):
            by_class.setdefault((s, c), []).append(n)
        for (s, c), names in sorted(by_class.items()):
            print(f'  {s} {c}: {len(names)}: {", ".join(names[:8])}')
    return check.summary('test_content_coverage_wire')


if __name__ == '__main__':
    raise SystemExit(main())
