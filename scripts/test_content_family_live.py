"""Proxy content provider against a REAL broker serving a generated family policy.

    python scripts/test_content_family_live.py SERVER_EXE PROXY_TEST_EXE POLICY_JSON TRACE_JSON
                                               [--effect-report C3_REPORT.json]

Starts an isolated server (fresh save, port 19962) whose broker serves
POLICY_JSON (scripts/gen_unit_family_policy.py, gen_effect_family_policy.py)
over the REAL, read-only deploy/game_content tree, then runs the offline-proxy's
content_provider_tests with BF_FAMILY_PORT / BF_FAMILY_ROOT / BF_FAMILY_EXPECT,
so its family case uses the DLL's own WinINet client, provider and hooks
against it:

  - the broker's full index lists exactly the policy's names;
  - every name a traced client session downloaded on its first visits
    (TRACE_JSON, out/qa/current/session11/trace_first_visits.py) that the
    policy now owns is owned, served byte-exact from its policy path and kept
    out of the download queue;
  - every other traced download stays with the legacy path and is queued.

With --policy-report (the report of scripts/startup_coverage.py policy, Phase C7)
every name its generator left legacy -- a root whose closure is incomplete, a
character animation missing a member -- must be absent from the policy, and each
one game_content holds is a not-owned control (unowned, asked of the game's own
existence check, queued).  A policy that owns every traced download (C7 owns all
5,630 of the Session 11 trace) still has to give the run controls that exist on
disk: legacy trace downloads, or these.

With --effect-report (the report of gen_effect_family_policy.py) the Phase C3
families are read the way the game reads them, every one of their names:

  - each SuperAnim definition as getLocalPath builds it ("<LocalState>\\X.sam",
    "rb"), present to the dependency processor's isFileExist, and each of its
    pages as LoadSuperAnimMainDef builds it ("<LocalState>/<page>", "rb");
  - each particle plist in the text mode CCSAXParser reads it ("rt");
  - each read equals what the CRT returns for the real file in that mode, none
    reaches the client cache's reader or a download queue, nothing is written;
  - every member of a component the generator left legacy (collisions,
    missing pages, a differing packaged copy) stays with the game's own path.

With --script-bundle NAME (Phase C4, scripts/gen_gate_script_policy.py) every
member of that bundle -- the summon gate scripts -- is read the way
SummonsAnimeScene2::initialize reads it: getLocalPath ("<LocalState>\\X.txt")
then getCsvList, i.e. getFileData(path, "r") in text mode, and must equal the
CRT's text-mode read of the policy root's copy; a script the candidate changed
must also differ from the served game_content copy (the provider serves the
candidate, not what the client cached).  A policy with its own "root" (C4) is
served from, and compared against, that root.

Nothing under deploy/ is written (the broker never writes; the save is fresh).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Fixture, IsolatedServer, qa_dir  # noqa: E402

PORT = 19962
CONTENT = ROOT / 'deploy' / 'game_content' / 'content'


def traced_names(trace: dict) -> dict:
    """flat name -> 'branch #change origin->destination' for every demand download in the trace."""
    out = {}
    for branch, result in trace.items():
        for proc in result['processes']:
            for w in proc['windows']:
                if not w['transitional']:
                    continue
                for name in w['files']:
                    out.setdefault(name, f'{branch} #{w["change"]} {w["origin"]}->{w["destination"]}')
    return out


def effect_expectations(policy: dict, paths: dict, report: dict) -> tuple[list[str], list[str], Counter]:
    """('read ...' / 'fileexist ...' lines, legacy names, counts) for the C3 bundles (sam_*, prt_*)."""
    lines, counts = [], Counter()
    for bundle, members in policy['bundles'].items():
        if bundle.startswith('sam_'):
            for m in members:
                rel = paths[m.lower()]
                if m.lower().endswith('.sam'):
                    lines += [f'read {m} {rel} rb b\n', f'fileexist {m}\n']
                    counts['sam definitions'] += 1
                else:
                    lines.append(f'read {m} {rel} rb f\n')
                    counts['sam pages'] += 1
        elif bundle.startswith('prt_'):
            for m in members:
                lines.append(f'read {m} {paths[m.lower()]} {"rt" if m.lower().endswith(".plist") else "rb"} b\n')
                counts['particle plists'] += 1
    legacy = set()
    for family in ('sam', 'particle'):
        for definition, problems in report['legacy'].get(family, {}).items():
            if any('owned by the base policy' in why for why in problems.values()):
                continue                    # left as the base policy has it (the Town)
            legacy.add(definition)
            legacy.update(n for n, why in problems.items() if not why.startswith('missing'))
    declared = {m.lower() for ms in policy['bundles'].values() for m in ms}
    return lines, sorted(n for n in legacy if n.lower() not in declared), counts


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('server_exe', type=Path)
    ap.add_argument('proxy_tests', type=Path)
    ap.add_argument('policy', type=Path)
    ap.add_argument('trace', type=Path)
    ap.add_argument('--effect-report', type=Path, help='gen_effect_family_policy.py report: read every C3 name')
    ap.add_argument('--script-bundle', help='read every member of this bundle as a script ("r", getLocalPath)')
    ap.add_argument('--policy-report', type=Path,
                    help='startup_coverage.py policy report: what its generator left legacy stays unowned')
    args = ap.parse_args()
    server_exe, proxy_tests, policy_path, trace_path = args.server_exe, args.proxy_tests, args.policy, args.trace
    check = Checker()
    work = qa_dir('content_family_live')
    work.mkdir(parents=True, exist_ok=True)
    policy = json.loads(policy_path.read_text(encoding='utf-8'))
    root = Path(policy['root']) if policy.get('root') else CONTENT
    declared = {m.lower() for ms in policy['bundles'].values() for m in ms}
    paths = {Path(p).name.lower(): p for p in policy['scope'] if '*' not in p and '?' not in p}
    traced = traced_names(json.loads(trace_path.read_text(encoding='utf-8')))
    present = sorted(n for n in traced if n.lower() in declared and n.lower() in paths)
    legacy = sorted(n for n in traced if n.lower() not in declared)
    on_disk = {}                              # lower flat name -> its spelling in game_content
    for _dir, _sub, files in os.walk(CONTENT):
        for f in files:
            on_disk.setdefault(f.lower(), f)
    refused = []
    if args.policy_report:
        report = json.loads(args.policy_report.read_text(encoding='utf-8'))
        left = sorted(set(report.get('legacy', {})) | set(report.get('refused', [])))
        owned_anyway = [n for n in left if n.lower() in declared]
        check(f'none of the {len(left)} names the generator left legacy is owned by the policy', bool(left)
              and not owned_anyway, owned_anyway[:10])
        refused = [on_disk[n.lower()] for n in left if n.lower() in on_disk and n.lower() not in declared]
        print(f'{args.policy_report.name}: {len(left)} names left legacy, {len(refused)} of them in game_content'
              ' (not-owned controls)')
    check('the trace names owned downloads', bool(present), len(present))
    controls = [n for n in legacy if n.lower() in on_disk] + refused
    check('not-owned controls that game_content holds: legacy trace downloads or names the generator left legacy',
          bool(controls), (len(legacy), len(refused)))
    notowned = legacy + [n for n in refused if n.lower() not in {x.lower() for x in legacy}]
    reads, effect_legacy, counts = [], [], Counter()
    if args.effect_report:
        reads, effect_legacy, counts = effect_expectations(
            policy, paths, json.loads(args.effect_report.read_text(encoding='utf-8')))
        check('the C3 families give reads and legacy names to check', bool(reads) and bool(effect_legacy),
              (dict(counts), len(effect_legacy)))
    if args.script_bundle:
        members = policy['bundles'].get(args.script_bundle, [])
        check(f'bundle {args.script_bundle} has members, all with a scope path', bool(members)
              and all(m.lower() in paths for m in members), members)
        changed = [m for m in members if (root / paths[m.lower()]).read_bytes() != (CONTENT / paths[m.lower()]).read_bytes()]
        print(f'{args.script_bundle}: {len(members)} scripts, {len(changed)} differ from game_content: {", ".join(changed)}')
        check(f'{args.script_bundle}: the candidate root changes some scripts (else this proves nothing new)',
              bool(changed) or root == CONTENT, changed)
        reads += [f'read {m} {paths[m.lower()]} r b\n' for m in members]
        counts['scripts'] += len(members)
    expect = work / 'expect.txt'
    expect.write_text(f'owned {len(declared)}\n'
                      + ''.join(f'present {n} {paths[n.lower()]}\n' for n in present)
                      + ''.join(f'notowned {n}\n' for n in notowned + [n for n in effect_legacy if n not in notowned])
                      + ''.join(reads), encoding='utf-8')
    print(f'{policy_path.name}: {len(declared)} owned names; trace: {len(traced)} demand downloads, '
          f'{len(present)} now owned, {len(legacy)} legacy; {len(notowned)} not-owned control(s) in all')
    if args.effect_report:
        n_reads = sum(1 for r in reads if r.startswith('read '))
        print(f'C3: {n_reads} game reads ({dict(counts)}); {len(effect_legacy)} names left legacy by the generator: '
              + ', '.join(effect_legacy[:12]))
    for n in present[:60]:
        print(f'  owned now: {n:40s} {traced[n]}')
    if len(present) > 60:
        print(f'  ... and {len(present) - 60} more')

    system = work / 'system'
    if system.exists():
        shutil.rmtree(system)
    shutil.copytree(ROOT / 'deploy' / 'system', system)
    (system / 'content_provider.json').write_text(json.dumps(dict(policy, enabled=True), indent=1), encoding='utf-8')
    fx = Fixture.create(work / 'server', port=PORT, fresh=True,
                        document_root=ROOT / 'deploy' / 'game_content', system_root=system)
    env = dict(os.environ, BF_FAMILY_PORT=str(PORT), BF_FAMILY_ROOT=str(root), BF_FAMILY_EXPECT=str(expect),
               BF_APPX_DIR=str(ROOT.parent / 'BraveFrontierAppxClient'))
    env.pop('BF_BROKER_PORT', None)
    env.pop('BF_BROKER_ROOT', None)
    with IsolatedServer(server_exe, fx, log_name='server.log'):
        run = subprocess.run([str(proxy_tests)], env=env, capture_output=True, text=True, timeout=900)
    out = run.stdout + run.stderr
    (work / 'content_provider_tests.log').write_text(out, encoding='utf-8')
    print(out.strip()[-3000:])
    m = re.search(r'content_provider_tests: (\d+) passed, (\d+) failed, (\d+) skipped', out)
    check('proxy tests completed', m is not None, out[-300:])
    check('proxy tests: no failures', m and m.group(2) == '0' and run.returncode == 0, m and m.group(0))
    check('family policy case ran (not skipped)', 'SKIP  family policy integration' not in out)
    check('every traced owned name was served byte-exact',
          f'family policy: {len(present)} traced name(s) served byte-exact' in out)
    if args.effect_report or args.script_bundle:
        n_reads = sum(1 for r in reads if r.startswith('read '))
        check(f'every game read ({n_reads}: {dict(counts)}) was served exactly as the CRT reads the file',
              re.search(rf'family policy: {n_reads} game read\(s\) byte-exact', out) is not None,
              re.findall(r'family policy: \d+ game read[^\n]*', out))
    log = (fx.dir / 'server.log').read_text(encoding='utf-8', errors='replace')
    served = set(p.rsplit('/', 1)[-1].lower() for p in re.findall(r'content broker: served (\S+)', log))
    check('the server log shows the broker serving every traced owned name',
          {n.lower() for n in present} <= served, sorted({n.lower() for n in present} - served)[:10])
    check('the broker indexed the policy without collisions or incomplete bundles',
          re.search(rf'content broker: indexed \d+ file\(s\) in \d+ ms; {len(declared)} owned, 0 collision', log) is not None,
          re.findall(r'content broker: indexed[^\n]*', log))
    return check.summary('test_content_family_live')


if __name__ == '__main__':
    raise SystemExit(main())
