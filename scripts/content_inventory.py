"""Inventory of the local-content boundary: what game_content holds, what a policy owns, what the client asks for.

    python scripts/content_inventory.py --policy POLICY.json --out DIR
                                        [--base-policy OLDER.json ...] [--trace TRACE.json]
                                        [--package PFN ...] [--sample-stored N]

Read only.  Writes DIR/inventory.json (every name with its class) and DIR/inventory.md
(the tables docs/OFFLINE_CONTENT_PHASE_C3.md quotes).  Inputs:

  content     deploy/game_content/content, walked once: every file by folder,
              extension and flat basename.  A basename with several copies is
              "identical" (one digest) or a collision (differing bytes) unless the
              policy aliases it -- the client's LocalState is flat, so it can hold
              only one of them.
  policy      a generated policy (scripts/gen_unit_family_policy.py,
              gen_effect_family_policy.py), resolved exactly like the broker's
              owned set (exact scope paths, globs, aliases, bundle members).
              --base-policy adds a column per older phase for comparison.
  requests    what the client consumes, from its own data, never from a guess:
              * the package's Assets/Resources/resourceMap (scene id -> the
                _dlcbundle files DLCManagerComplementor fetches on entry) and the
                names the package ships itself (never downloaded);
              * each --package's LocalState census (names only; default: every
                gumi.BraveFrontier* package): what that client actually stored --
                downloads, plus its own state files;
              * --trace: the Session 11 first-visit downloads, per screen.
  readers     the read path of each kind of file, with the evidence for it
              (READERS below): routed through the hooked getFileData, or read by
              something the provider cannot see.

"Unavailable" is a name the client requests that game_content does not hold;
"existing unowned" is a name game_content holds that the policy leaves to the
client's own download path.  Counts are inventory metrics, not a percentage of
any project.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from collections import Counter, defaultdict
from fnmatch import fnmatch
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / 'deploy' / 'game_content' / 'content'
APPX = ROOT.parent / 'BraveFrontierAppxClient'
PACKAGES = Path(os.environ.get('LOCALAPPDATA', '')) / 'Packages'

# How each kind of file is read by the Windows client (x86 RVAs of BraveFrontier.Windows.exe
# fb4f3c25 / libcocos2d 5488d108; arm64 = libgame.so semantics).  "hooked" = every read goes
# through CCFileUtils::getFileData (cocos 0x135EB0), which the provider hooks; the provider can
# own such a family once its dependencies are known.  "bypass" = the provider never sees it.
READERS = {
    '.png': ('hooked', 'CommonUtils::getTexture 0x2353D0 (FileCrypt decode only when forced or the PNG/JPEG magic '
                       'is absent) or CCTextureCache::addImage (cocos 0x16A590) -> getFileData "rb"; the two-image '
                       'reader 0x65FB70 decodes unconditionally (served encoded by return address). These are the '
                       'EXE\'s only FileCrypt loops (0x2355A4, 0x65FD93, 0x65FE44; 0x222F22 encodes downloads).'),
    '.jpg': ('hooked', 'as .png'),
    '.csv': ('hooked', 'CommonUtils::getCsvList 0x235C60 -> getFileData "r" (text mode); layouts via '
                       'GameScene::loadLayout (downloaded copy if FileLoader::isFileAlreadyDownloaded, else the '
                       'packaged one)'),
    '.sam': ('hooked', 'SuperAnimDefMgr::LoadSuperAnimMainDef 0x62B270 (the only .SAM magic check) and '
                       'SuperAnim::getSamSupportingFiles 0x62CBA0 -> getFileData "rb"; pages "<dir>/<page>" via '
                       'SuperAnimSpriteMgr 0x632AC0 -> CCTextureCache::addImage'),
    '.plist': ('hooked', 'CCSAXParser::parse (cocos 0x1381B0) / createCCDictionaryWithContentsOfFile (0x135470) '
                         '-> getFileData "rt"; particles via GameParticle::create 0x629990 (56 sites, getLocalPath)'),
    '.txt': ('hooked', 'cut-scene / tutorial / gacha scripts: CommonUtils::getCsvList(ForLocal) -> getFileData "r" '
                       '(arm64 GameUtils::getTutorialScript; x86 processSamAndCutSceneFile 0x106B10 calls '
                       '0x235C60). Scripts name further assets (param=/dungeon/...jpg): a dependency family'),
    '.fnt': ('hooked', 'CCBMFontConfiguration (cocos 0x114E90) -> CCString::createWithContentsOfFile 0x108870 '
                       '-> getFileData "rb"; atlas pages via addImage'),
    '.mp3': ('hooked', 'the EXE\'s audio reader 0xEC0030 (SGAudioHandler slot 0): fopen_s "rb", fread(1, 210) '
                       'until 0, fclose -- never getFileData; served from RAM by the Session 15 DLL\'s '
                       'fopen_s / fread / fclose import hooks (offline-proxy tests/fixtures/check_audio_reader.py). '
                       'Older DLLs bypass it: never own sounds without that hook'),
    '.zip': ('bypass', 'ZIP extractor 0x391EE0: cocos unzOpen (its own CRT open) and fopen "wb" of every entry '
                       'into LocalState (sgtext_*.csv: mutable client state)'),
    '.dat': ('state', 'MST parts and account key files (Ver*.dat, version channel); refused by the provider'),
    '.json': ('state', 'server-side MST sources; refused by the provider'),
    '.mp4': ('not traced', 'video playback; no getFileData user found, not routed'),
    '.ttf': ('not traced', 'fonts; not routed'),
    '.gif': ('not traced', 'no consumer found'),
    '.css': ('not traced', 'web view asset; not routed'),
    '.ini': ('state', 'refused by the provider'),
    '.tmp': ('state', 'refused by the provider'),
    '.cgs': ('not traced', 'stray copies (the unit CGS family is .csv)'),
    '.cgg': ('not traced', 'stray copies (the unit CGG family is .csv)'),
}
STATE_NAME = re.compile(r'(\.dat|\.xml|\.ini|\.tmp|\.json)$|^thumbs\.db$|^\.ds_|^telemetry$', re.I)


def load_policy(path: Path) -> dict:
    text = path.read_text(encoding='utf-8-sig')
    return json.loads(re.sub(r'^\s*//.*$', '', text, flags=re.M))


def owned_names(policy: dict, where: dict) -> dict:
    """lower flat name -> relative path, as the broker resolves the policy (scope, globs, aliases)."""
    aliases = {k.lower(): v for k, v in policy.get('aliases', {}).items()}
    exact, globs = {}, []
    for p in policy.get('scope', []):
        if '*' in p or '?' in p:
            globs.append(p.lower())
        else:
            exact.setdefault(Path(p).name.lower(), p)
    out = {}
    for name, rels in where.items():
        if name in aliases:
            out[name] = aliases[name]
        elif name in exact:
            out[name] = exact[name]
        elif globs and any(fnmatch(r.lower(), g) for r in rels for g in globs):
            out[name] = next(r for r in rels if any(fnmatch(r.lower(), g) for g in globs))
    members = {m.lower() for ms in policy.get('bundles', {}).values() for m in ms}
    return {n: r for n, r in out.items() if n in members}


def family_of(rel: str) -> str:
    parts = rel.split('/')
    if parts[0] in ('unit', 'monster', 'effect', 'dbb', 'raid', 'grand', 'common') and len(parts) > 2:
        return '/'.join(parts[:2])
    return parts[0] if len(parts) > 1 else '(root)'


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--policy', required=True)
    ap.add_argument('--base-policy', action='append', default=[])
    ap.add_argument('--out', required=True)
    ap.add_argument('--trace')
    ap.add_argument('--package', action='append', default=[])
    ap.add_argument('--content', default=str(CONTENT))
    ap.add_argument('--appx', default=str(APPX))
    ap.add_argument('--sample-stored', type=int, default=0,
                    help='compare up to N cached copies per family with game_content (raw / FileCrypt / other)')
    args = ap.parse_args()
    content, appx, out = Path(args.content), Path(args.appx), Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    where, size = defaultdict(list), {}
    for dirpath, _d, files in os.walk(content):
        rel_dir = Path(dirpath).relative_to(content).as_posix()
        for f in files:
            rel = f'{rel_dir}/{f}' if rel_dir != '.' else f
            where[f.lower()].append(rel)
            size[rel] = os.path.getsize(content / rel)
    digests = {}

    def digest(rel):
        if rel not in digests:
            digests[rel] = hashlib.sha256((content / rel).read_bytes()).hexdigest()
        return digests[rel]

    policy = load_policy(Path(args.policy))
    aliases = {k.lower() for k in policy.get('aliases', {})}
    duplicates = {n: rels for n, rels in where.items() if len(rels) > 1}
    collisions = {n: rels for n, rels in duplicates.items() if len({digest(r) for r in rels}) > 1}
    owned = owned_names(policy, where)
    phases = {Path(p).stem: set(owned_names(load_policy(Path(p)), where)) for p in args.base_policy}

    bundled = {p.name.lower() for p in (appx / 'Assets' / 'Resources').iterdir() if p.is_file()}
    resource_map = {}
    for line in (appx / 'Assets' / 'Resources' / 'resourceMap').read_text(encoding='utf-8', errors='replace').splitlines():
        if ':' in line:
            scene, files = line.split(':', 1)
            resource_map[scene.strip()] = [f.strip() for f in files.split(',') if f.strip()]

    packages = args.package or sorted(p.name for p in PACKAGES.glob('gumi.BraveFrontier*') if (p / 'LocalState').is_dir())
    census = {}
    for pfn in packages:
        ls = PACKAGES / pfn / 'LocalState'
        if not ls.is_dir():
            print(f'package {pfn}: no LocalState at {ls}; left out of the census')
            continue
        census[pfn] = {e.name.lower(): e.stat().st_size for e in os.scandir(ls) if e.is_file()}
    packages = list(census)

    # ---- families ------------------------------------------------------------------------
    primary = max(census, key=lambda p: len(census[p])) if census else None   # the fullest legacy cache
    fam = defaultdict(lambda: Counter())
    fam_ext = defaultdict(Counter)
    for name, rels in where.items():
        for rel in rels:
            f = family_of(rel)
            ext = os.path.splitext(name)[1] or '(none)'
            c = fam[f]
            c['files'] += 1
            c['bytes'] += size[rel]
            fam_ext[f][ext] += 1
        rel0 = sorted(rels)[0]
        c = fam[family_of(owned.get(name, rel0))]
        c['names'] += 1
        if name in owned:
            c['owned'] += 1
        elif name in collisions and name not in aliases:
            c['collision'] += 1
        else:
            c['existing unowned'] += 1
        for label, names in phases.items():
            if name in names:
                c[f'owned in {label}'] += 1
        if primary and name in census[primary]:
            c['cached by the client'] += 1

    # ---- requests the client makes, and whether game_content can answer them --------------
    rm_names = {f.lower() for files in resource_map.values() for f in files}
    rm_unavailable = sorted(n for n in rm_names if n not in where and n not in bundled)
    rm_bundled_only = sorted(n for n in rm_names if n not in where and n in bundled)
    per_package = {}
    for pfn, names in census.items():
        in_content = [n for n in names if n in where]
        state = [n for n in names if n not in where and STATE_NAME.search(n)]
        extracted = [n for n in names if n not in where and n.startswith('sgtext_') and n.endswith('.csv')]
        other = sorted(n for n in names if n not in where and n not in state and n not in extracted)
        per_package[pfn] = {
            'entries': len(names), 'content_names': len(in_content),
            'owned_names': sum(1 for n in in_content if n in owned),
            'legacy_content_names': sum(1 for n in in_content if n not in owned),
            'client_state_names': len(state), 'zip_extracted_names': len(extracted),
            'other_non_content': other,
            'owned_size_mismatch': sorted(n for n in in_content if n in owned and names[n] != size[owned[n]])[:50],
        }
    never_cached = None
    if census:
        never_cached = Counter(family_of(sorted(where[n])[0]) for n in where if n not in census[primary])

    trace = {}
    if args.trace:
        t = json.loads(Path(args.trace).read_text(encoding='utf-8'))
        seen = {}
        for branch, result in t.items():
            for proc in result['processes']:
                for w in proc['windows']:
                    if w.get('transitional'):
                        for n in w['files']:
                            seen.setdefault(n.lower(), f'{branch} #{w["change"]} {w["origin"]}->{w["destination"]}')
        legacy_by = Counter()
        for n in seen:
            if n not in owned:
                legacy_by[(family_of(sorted(where[n])[0]) if n in where else '(not in game_content)')
                          + ' ' + (os.path.splitext(n)[1] or '')] += 1
        trace = {
            'traced_demand_downloads': len(seen),
            'owned_now': sum(1 for n in seen if n in owned),
            'owned_by_phase': {label: sum(1 for n in seen if n in names) for label, names in phases.items()},
            'legacy_by_family': dict(legacy_by.most_common()),
            'unavailable': sorted(n for n in seen if n not in where),
        }

    stored = {}
    if args.sample_stored and census:
        ls = PACKAGES / primary / 'LocalState'
        seen_fam = Counter()
        result = defaultdict(Counter)
        for name in sorted(where):
            rel = sorted(where[name])[0]
            f = family_of(rel) + ' ' + os.path.splitext(name)[1]
            if name not in census[primary] or seen_fam[f] >= args.sample_stored:
                continue
            seen_fam[f] += 1
            a = (content / rel).read_bytes()
            b = (ls / name).read_bytes()
            if a == b:
                kind = 'raw'
            elif len(a) == len(b) and bytes((c - i * i) & 0xFF for i, c in enumerate(b[:2048])) == a[:2048]:
                kind = 'FileCrypt'
            elif len(a) != len(b):
                kind = 'other size (stale or other version)'
            else:
                kind = 'other bytes'
            result[f][kind] += 1
        stored = {k: dict(v) for k, v in sorted(result.items())}

    report = {
        'inputs': {
            'content': str(content), 'files': sum(len(v) for v in where.values()), 'distinct_names': len(where),
            'bytes': sum(size.values()),
            'policy': str(args.policy), 'policy_sha256': hashlib.sha256(Path(args.policy).read_bytes()).hexdigest(),
            'base_policies': args.base_policy, 'packages': packages,
            'app_resources': len(bundled), 'resource_map_scenes': len(resource_map),
        },
        'flat_namespace': {
            'names_with_several_copies': len(duplicates),
            'identical_copies': len(duplicates) - len(collisions),
            'collisions': len(collisions),
            'collisions_aliased_by_policy': sorted(n for n in collisions if n in aliases),
            'collision_list': {n: rels for n, rels in sorted(collisions.items())},
        },
        'owned': {'names': len(owned), 'by_phase': {k: len(v) for k, v in phases.items()}},
        'families': {f: dict(c) | {'extensions': dict(fam_ext[f].most_common())} for f, c in
                     sorted(fam.items(), key=lambda kv: -kv[1]['files'])},
        'readers': {ext: {'class': cls, 'evidence': ev} for ext, (cls, ev) in READERS.items()},
        'requests': {
            'resource_map_names': len(rm_names),
            'resource_map_owned': sum(1 for n in rm_names if n in owned),
            'resource_map_bundled_only': rm_bundled_only,
            'resource_map_unavailable': rm_unavailable,
            'packages': per_package,
            'content_names_never_cached_by_largest_census': dict(never_cached.most_common()) if never_cached else {},
        },
        'first_visit_trace': trace,
        'stored_form_sample': stored,
    }
    (out / 'inventory.json').write_text(json.dumps(report, indent=1), encoding='utf-8')

    lines = [f'# Content inventory ({Path(args.policy).name})', '',
             f'content: {report["inputs"]["files"]} files, {report["inputs"]["distinct_names"]} flat names, '
             f'{report["inputs"]["bytes"] / 1e9:.2f} GB; owned {len(owned)} '
             + ' '.join(f'({k}: {len(v)})' for k, v in phases.items()), '',
             f'flat namespace: {len(duplicates)} names with several copies, {len(collisions)} collisions '
             f'({len(report["flat_namespace"]["collisions_aliased_by_policy"])} aliased by the policy)', '',
             '| family | files | names | owned | existing unowned | collision | cached by client | extensions |',
             '|---|---:|---:|---:|---:|---:|---:|---|']
    for f, c in report['families'].items():
        ext = ', '.join(f'{e} {n}' for e, n in list(c['extensions'].items())[:4])
        lines.append(f'| {f} | {c.get("files", 0)} | {c.get("names", 0)} | {c.get("owned", 0)} | '
                     f'{c.get("existing unowned", 0)} | {c.get("collision", 0)} | {c.get("cached by the client", 0)} | {ext} |')
    lines += ['', '| reader | class | evidence |', '|---|---|---|']
    lines += [f'| {e} | {r["class"]} | {r["evidence"]} |' for e, r in report['readers'].items()]
    rq = report['requests']
    lines += ['', f'resourceMap: {rq["resource_map_names"]} names, {rq["resource_map_owned"]} owned, '
                  f'{len(rq["resource_map_bundled_only"])} only in the app package, '
                  f'{len(rq["resource_map_unavailable"])} unavailable: {", ".join(rq["resource_map_unavailable"][:20])}', '',
              '| package | entries | content names | owned | legacy content | client state | zip-extracted | other |',
              '|---|---:|---:|---:|---:|---:|---:|---|']
    for pfn, p in rq['packages'].items():
        lines.append(f'| {pfn} | {p["entries"]} | {p["content_names"]} | {p["owned_names"]} | '
                     f'{p["legacy_content_names"]} | {p["client_state_names"]} | {p["zip_extracted_names"]} | '
                     f'{", ".join(p["other_non_content"][:5])} |')
    if trace:
        lines += ['', f'first-visit trace: {trace["traced_demand_downloads"]} demand downloads, {trace["owned_now"]} '
                      f'owned now ({trace["owned_by_phase"]}); unavailable {len(trace["unavailable"])}', '',
                  '| still legacy | names |', '|---|---:|']
        lines += [f'| {k} | {v} |' for k, v in list(trace['legacy_by_family'].items())[:25]]
    if stored:
        lines += ['', '| stored form (sample) | result |', '|---|---|']
        lines += [f'| {k} | {v} |' for k, v in stored.items()]
    (out / 'inventory.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('\n'.join(lines[:6]))
    print(f'wrote {out / "inventory.json"} and {out / "inventory.md"}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
