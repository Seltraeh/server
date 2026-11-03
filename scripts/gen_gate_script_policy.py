"""Phase C4 candidate policy: the summon gate scripts, served by the content
provider from a candidate root (Session 14, #24).

    python scripts/gen_gate_script_policy.py --base C3_POLICY --scripts GATE_SCRIPTS_DIR
                                             --root OUT_ROOT --out POLICY_JSON [--report REPORT_JSON]

Why a policy.  The client caches the 17 gate scripts in its LocalState and
never re-checks them (existsLocalFile has no version), so corrected scripts
cannot reach an installed client through the download path without deleting
its cache -- which this project does not do.  Owned by the provider, they are
read through the hooks instead: SummonsAnimeScene2::initialize loads each with
CommonUtils::getLocalPath + getCsvList (x86 0x236950 / 0x235C60, arm64
getCsvListForLocal), i.e. getFileData("<LocalState>\\<name>", "r") -- the same
text-mode path the provider already serves for unit CSVs (Phase C1, client
confirmed).  SummonsAnimeScene2::downloadFiles queues the scripts with
addLoadFilePriority (arm64 0x169C130..), which the C1 enqueue filter drops for
owned names, so nothing is downloaded and the cached copies are simply not read
while the provider is active. With the provider dormant or the C3 policy
selected the scripts become legacy again (rollback). A withdrawn C4 bundle
remains owned-but-unavailable and does NOT fall back to cached scripts.

Dependencies.  Every SuperAnim and particle plist the scripts name must already
be owned (Phase C3), or the bundle would mix provider and cache content; the
generator refuses otherwise.  Sounds (.mp3) are NEVER owned: the audio reader
bypasses getFileData (x86 0xEC0030).  Owning a script has one consequence for
sounds, recorded in the report: FileLoader::processSamAndCutSceneFile (arm64
0x1CD9BC0) parses a DOWNLOADED script's PARAM lines and queues its missing
dependencies, so a script that is never downloaded no longer pulls the sounds
only it names; a script sound that is neither cached nor packaged then falls
back to the packaged-resource path and is silent (the id=34 handler plays
LocalState if existsLocalFile, else the resource path; it never downloads).

The root.  <OUT_ROOT>/content mirrors exactly the files the policy owns: hard
links to deploy/game_content/content for the base policy's names and for the
unchanged Open scripts, and new files for the 9 guarded Wait/Change scripts
(scripts/gen_gate_scripts.py).  deploy/ is only read.  The policy's "root" is
that absolute content folder; the shipped policy is not touched.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_gate_scripts  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
CONTENT = ROOT / 'deploy' / 'game_content' / 'content'
APPX = ROOT.parent / 'BraveFrontierAppxClient' / 'Assets' / 'Resources'
EFFECT_MST = ROOT / 'deploy' / 'mst' / 'gacha_effect_mst.json'
BUNDLE = 'gate_scripts'


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def owned_paths(policy: dict) -> dict[str, str]:
    """flat name (lower) -> relative path, resolved as the broker resolves the policy."""
    names = {m.lower() for ms in policy['bundles'].values() for m in ms}
    aliases = {k.lower(): v for k, v in policy.get('aliases', {}).items()}
    exact, globs = {}, []
    for entry in policy['scope']:
        if '*' in entry or '?' in entry:
            globs.append(entry)
        else:
            exact.setdefault(Path(entry).name.lower(), entry)
    out = {}
    for name in names:
        if name in aliases:
            out[name] = aliases[name]
        elif name in exact:
            out[name] = exact[name]
    missing = names - set(out)
    for pattern in globs:
        for cand in CONTENT.glob(pattern):
            if cand.name.lower() in missing:
                out[cand.name.lower()] = cand.relative_to(CONTENT).as_posix()
    unresolved = names - set(out)
    if unresolved:
        raise SystemExit(f'base policy names that resolve to no file: {sorted(unresolved)[:10]}')
    return out


def gate_script_names() -> list[str]:
    rows = next(iter(json.loads(EFFECT_MST.read_text(encoding='utf-8')).values()))
    names = {r[c] for r in rows for c in ('7ZNcmYS2', 'YTx3c1jQ', 'tj0i9JhC') if r[c]}
    return sorted(names)


def references(text: str) -> list[tuple[str, str]]:
    """(kind, file) for every file a gate script names (SuperAnim, plist, sound)."""
    out = []
    for op, param in gen_gate_scripts.ops(text):
        parts = param.split(':')
        if op == 42 and len(parts) >= 5:
            out += [('sam', p) for p in parts[3:5] if p and p != 'none']
        elif op == 46 and len(parts) >= 5:
            out.append(('plist', parts[4]))
        elif op == 34 and param:
            out.append(('sound', param))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--base', required=True, type=Path, help='the C3 policy (gen_effect_family_policy.py)')
    ap.add_argument('--scripts', required=True, type=Path, help='output of gen_gate_scripts.py')
    ap.add_argument('--root', required=True, type=Path, help='folder for the candidate root (rebuilt)')
    ap.add_argument('--out', required=True, type=Path, help='policy JSON to write')
    ap.add_argument('--report', type=Path, help='dependency/identity report to write')
    args = ap.parse_args()

    root = args.root.resolve()
    if root == (ROOT / 'deploy').resolve() or (ROOT / 'deploy').resolve() in root.parents:
        raise SystemExit('refusing: the candidate root may not be inside deploy/')
    if not (ROOT / 'out').resolve() in root.parents:
        raise SystemExit('refusing: keep the candidate root under out/')
    problems = gen_gate_scripts.check(args.scripts)
    if problems:
        raise SystemExit(f'{args.scripts} is not a valid gen_gate_scripts.py output: {problems}')

    base = json.loads(args.base.read_text(encoding='utf-8'))
    if BUNDLE in base['bundles']:
        raise SystemExit(f'the base policy already has a {BUNDLE} bundle')
    owned = owned_paths(base)
    scripts = gate_script_names()
    clash = [s for s in scripts if s.lower() in owned]
    if clash:
        raise SystemExit(f'base policy already owns {clash}')

    report = {'base': str(args.base), 'base_sha256': sha(args.base), 'scripts': {}, 'dependencies': {},
              'sound_dependencies': {}}
    for name in scripts:
        served = args.scripts / name
        original = CONTENT / 'gacha' / name
        if not served.is_file() or not original.is_file():
            raise SystemExit(f'{name}: missing from {args.scripts} or {original.parent}')
        text = served.read_text(encoding='utf-8-sig')
        report['scripts'][name] = {'served_sha256': sha(served), 'original_sha256': sha(original),
                                   'changed': sha(served) != sha(original)}
        for kind, ref in references(text):
            key = ref.lower()
            if kind == 'sound':
                cached_in = []
                if (CONTENT / 'sound' / ref).is_file():
                    cached_in.append('game_content (legacy download)')
                if (APPX / ref).is_file():
                    cached_in.append('app package')
                report['sound_dependencies'].setdefault(ref, {'available': cached_in, 'named_by': []})['named_by'].append(name)
                if key in owned:
                    raise SystemExit(f'{name}: sound {ref} is owned by the base policy (never own .mp3)')
                continue
            if key not in owned:
                raise SystemExit(f'{name}: {kind} {ref} is not owned by the base policy; '
                                 'the bundle would mix provider and cache content')
            report['dependencies'].setdefault(ref, {'kind': kind, 'rel': owned[key], 'named_by': []})['named_by'].append(name)

    # The mirror: exactly the policy's files.
    content = root / 'content'
    if root.exists():
        shutil.rmtree(root)
    linked = written = 0
    for rel in sorted(set(owned.values())):
        dst = content / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        os.link(CONTENT / rel, dst)
        linked += 1
    for name in scripts:
        dst = content / 'gacha' / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        if report['scripts'][name]['changed']:
            shutil.copyfile(args.scripts / name, dst)
            written += 1
        else:
            os.link(CONTENT / 'gacha' / name, dst)
            linked += 1

    policy = json.loads(json.dumps(base))
    policy['scope'] = base['scope'] + [f'gacha/{n}' for n in scripts]
    if '.txt' not in policy['extensions']:
        policy['extensions'] = base['extensions'] + ['.txt']
    policy['bundles'][BUNDLE] = scripts
    policy['root'] = content.as_posix()
    policy['enabled'] = False
    args.out.write_text(json.dumps(policy, indent=1), encoding='utf-8')
    report.update({'policy': str(args.out), 'policy_sha256': sha(args.out), 'root': content.as_posix(),
                   'linked': linked, 'written': written,
                   'owned_names': len({m.lower() for ms in policy['bundles'].values() for m in ms})})
    if args.report:
        args.report.write_text(json.dumps(report, indent=1), encoding='utf-8')
    unavailable = {s: v for s, v in report['sound_dependencies'].items() if not v['available']}
    print(f'{args.out}: {report["owned_names"]} owned names ({len(scripts)} gate scripts in bundle {BUNDLE}, '
          f'{sum(r["changed"] for r in report["scripts"].values())} changed); root {content} '
          f'({linked} hard links, {written} new files); sha256 {report["policy_sha256"]}')
    print(f'dependencies: {len(report["dependencies"])} owned SuperAnim/plist names; sounds left to the game: '
          + ', '.join(f'{s} ({"/".join(v["available"]) or "NOWHERE"})' for s, v in sorted(report['sound_dependencies'].items())))
    return 1 if unavailable else 0


if __name__ == '__main__':
    sys.exit(main())
