"""Generate Phase C3 of the content provider: SuperAnim definitions and particle plists.

    python scripts/gen_effect_family_policy.py --base C2_POLICY.json --out POLICY.json [--report REPORT.json]
                                               [--content deploy/game_content/content]
                                               [--app-resources ../BraveFrontierAppxClient/Assets/Resources]

C3 appends after a base policy -- normally the C2 policy of
scripts/gen_unit_family_policy.py -- and changes nothing in it: the base's
scope, aliases, bundles, extensions and exclusions stay an exact prefix.  Each
family was traced through the client before it is owned
(docs/OFFLINE_CONTENT_PHASE_C3.md; x86 RVAs of BraveFrontier.Windows.exe
fb4f3c25, arm64 = libgame.so):

  sam       every SuperAnim definition (*.sam anywhere under the content root)
            together with every page its image table names.  The game resolves
            the .sam with CommonUtils::getLocalPath (0x236950: existsLocalFile,
            then the LocalState path) and reads it with getFileData "rb" in
            SuperAnimDefMgr::LoadSuperAnimMainDef (0x62B270; the only '.SAM'
            magic check in the EXE); each page is "<dir of the .sam>/<page>"
            (arm64 0xFF0BA4) loaded by SuperAnimSpriteMgr (0x632AC0:
            CCSpriteFrameCache::spriteFrameByName, else CCTextureCache::addImage
            -> getFileData).  The client's own downloader finds the same
            dependencies the same way: FileLoader::processSamAndCutSceneFile
            (0x106B10) -> SuperAnim::getSamSupportingFiles (0x62CBA0, getFileData
            "rb") -> existsLocalFile(page) -> downloadBundlePriority.  So a
            definition is owned only with ALL its pages: one all-or-nothing bundle
            per connected group of definitions (41 pages are shared between two or
            three definitions), so bundles never overlap.  The image table is read
            with either header layout the loader accepts: short fields (its
            default) or 32-bit fields (LoadSuperAnimMainDef's mode 2, the unit and
            monster animations); a file that parses under neither, or both with
            different tables, stays legacy.
  particle  every particle plist (a dictionary with maxParticles / emitterType).
            GameParticle::create (0x629990; 56 call sites, each passing
            getLocalPath(name) or the title screen's isFileExist variant) ->
            CCParticleSystem::initWithFile -> CCFileUtils::
            createCCDictionaryWithContentsOfFile (cocos 0x135470) -> getFileData
            "rt" (text mode, which the provider reproduces).  Its texture is
            textureFileName next to the plist when that file exists, else the
            embedded textureImageData: a named texture that exists joins the
            plist's bundle; one that exists nowhere, with no embedded data, keeps
            the plist legacy.  One single-name bundle otherwise, so every owned
            name is a bundle member (scripts/check_content_policy.py).

A component is left to the legacy path, and reported, when any member: is
missing; has a basename whose copies differ (a collision) without an alias in
the base; is not a safe flat name; is already owned by the base policy (left
as the base has it); or is a name the app package ships
(Assets/Resources) with different bytes -- owning it would replace the
bundled copy the game reads when it has never downloaded that name.  A bundled
copy that reads identically is owned: what is drawn does not change.  "Reads"
is the reader's view: plists are parsed from a text-mode read, so a bundled
copy that differs only in CR LF line ends (120 particle plists) is the same.

Every LocalState copy of these families is stored raw (sampled byte-identical
to game_content), and the EXE's only FileCrypt decode loops are in
CommonUtils::getTexture (conditional) and the two-image reader (served encoded
by return address), so the provider's raw bytes are what each reader expects.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import plistlib
import re
import struct
import sys
from collections import Counter, defaultdict
from fnmatch import fnmatch
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP_RESOURCES = ROOT.parent / 'BraveFrontierAppxClient' / 'Assets' / 'Resources'
SAM_MAGIC = b'MAS.'
PAGE_EXTENSIONS = ('.png', '.jpg')


def load_json_with_comments(path: Path) -> dict:
    text = path.read_text(encoding='utf-8-sig')
    text = re.sub(r'^\s*//.*$', '', text, flags=re.M)
    return json.loads(text)


def safe_flat_name(name: str) -> bool:
    """The provider's IsSafeFlatName: what the client's flat LocalState namespace can hold."""
    return (0 < len(name) <= 255 and name not in ('.', '..') and not re.search(r'[\\/:\x00-\x1f]', name)
            and not name.endswith(('.', ' ')))


def sam_pages(data: bytes, wide: bool) -> list[str] | None:
    """The image table under one header layout, or None when it does not parse cleanly."""
    pos = 9 + (16 if wide else 8)               # magic, version, rate, then x/y/w/h
    if pos + 2 > len(data):
        return None
    count = struct.unpack_from('<H', data, pos)[0]
    pos += 2
    names = []
    for _ in range(count):
        if pos + 2 > len(data):
            return None
        length = struct.unpack_from('<H', data, pos)[0]
        raw = data[pos + 2:pos + 2 + length]
        if len(raw) != length or not raw or any(c < 32 or c > 126 for c in raw):
            return None
        name = raw.decode('ascii')
        if not name.lower().endswith(PAGE_EXTENSIONS):
            return None
        pos += 2 + length + 4 + 16 + 4           # name, w/h, 2x2 matrix, translation
        if pos > len(data):
            return None
        names.append(name)
    return names


def parse_sam(data: bytes) -> tuple[str, list[str] | None]:
    """('short' | 'wide', pages) or (reason, None)."""
    if len(data) < 17 or data[:4] != SAM_MAGIC or struct.unpack_from('<I', data, 4)[0] != 1:
        return 'not a version-1 SuperAnim definition', None
    short, wide = sam_pages(data, False), sam_pages(data, True)
    if short is not None and (wide is None or wide == short):
        return 'short', short
    if wide is not None and short is None:
        return 'wide', wide
    if short is None:
        return 'image table parses under neither header layout', None
    return 'image table parses differently under both header layouts', None


class Tree:
    """Every file under the content root by flat basename, resolved like the broker."""

    def __init__(self, content: Path, aliases: dict):
        self.content = content
        self.aliases = {k.lower(): v for k, v in aliases.items()}
        self.where = defaultdict(list)
        for dirpath, _dirs, files in os.walk(content):
            rel_dir = Path(dirpath).relative_to(content).as_posix()
            for f in files:
                self.where[f.lower()].append(f'{rel_dir}/{f}' if rel_dir != '.' else f)
        self._digest = {}

    def digest(self, rel: str) -> str:
        if rel not in self._digest:
            self._digest[rel] = hashlib.sha256((self.content / rel).read_bytes()).hexdigest()
        return self._digest[rel]

    def resolve(self, name: str) -> tuple[str | None, str]:
        """(relative path the broker would serve, 'unique' | 'identical' | 'alias') or (None, why)."""
        key = name.lower()
        if key in self.aliases:
            return self.aliases[key], 'alias'
        rels = sorted(self.where.get(key, []))
        if not rels:
            return None, 'missing'
        if len(rels) == 1:
            return rels[0], 'unique'
        if len({self.digest(r) for r in rels}) == 1:
            return rels[0], 'identical'
        return None, 'collision: ' + ' | '.join(rels)


class BaseOwnership:
    """What the base policy already owns (exact paths, globs, aliases, bundle members)."""

    def __init__(self, policy: dict, tree: Tree):
        self.names = {m.lower() for ms in policy.get('bundles', {}).values() for m in ms}
        self.names |= {k.lower() for k in policy.get('aliases', {})}
        for path in policy.get('scope', []):
            if '*' in path or '?' in path:
                rx = path.lower()
                for name, rels in tree.where.items():
                    if any(fnmatch(r.lower(), rx) for r in rels):
                        self.names.add(name)
            else:
                self.names.add(Path(path).name.lower())

    def owns(self, name: str) -> bool:
        return name.lower() in self.names


def app_resources(folder: Path) -> dict:
    return {p.name.lower(): p for p in folder.iterdir() if p.is_file()} if folder.is_dir() else {}


def reader_view(name: str, data: bytes) -> bytes:
    """The bytes a reader of this name receives: plists and CSVs are read in MSVC text mode
    ("rt" / "r": CR LF -> LF, CTRL+Z ends the data), everything else raw ("rb")."""
    if not name.lower().endswith(('.plist', '.csv', '.txt')):
        return data
    end = data.find(b'\x1a')
    return (data if end < 0 else data[:end]).replace(b'\r\n', b'\n')


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', required=True, help='policy to append to (normally the C2 policy)')
    ap.add_argument('--out', required=True)
    ap.add_argument('--report')
    ap.add_argument('--content', default=str(ROOT / 'deploy/game_content/content'))
    ap.add_argument('--app-resources', default=str(APP_RESOURCES),
                    help="the client package's Assets/Resources (bundled copies the game reads when it never downloaded a name)")
    args = ap.parse_args()

    base = load_json_with_comments(Path(args.base))
    tree = Tree(Path(args.content), base.get('aliases', {}))
    owned_by_base = BaseOwnership(base, tree)
    bundled = app_resources(Path(args.app_resources))
    if not bundled:
        print(f'app resources not found at {args.app_resources}: refusing to guess which names the package ships')
        return 2
    allowed = {e.lower() for e in base.get('extensions', [])} | {'.plist'}
    excluded = [g.lower() for g in base.get('exclude', [])]

    def member_problem(name: str) -> str | None:
        """Why one flat name cannot be owned, or None."""
        if not safe_flat_name(name):
            return 'not a safe flat name'
        low = name.lower()
        if os.path.splitext(low)[1] not in allowed or any(fnmatch(low, g) for g in excluded):
            return 'extension not served'
        rel, how = tree.resolve(name)
        if rel is None:
            return how
        if owned_by_base.owns(name):
            return 'owned by the base policy'
        app = bundled.get(low)
        if app is not None and (reader_view(low, app.read_bytes())
                                != reader_view(low, (tree.content / rel).read_bytes())):
            return 'the app package ships different bytes'
        return None

    legacy = defaultdict(dict)          # family -> {definition: {member: why}}
    stats = Counter()

    # ---- SuperAnim definitions and their pages ------------------------------
    definitions = {}                    # lower .sam name -> (name, rel, layout, [pages])
    for key in sorted(k for k in tree.where if k.endswith('.sam')):
        name = Path(tree.where[key][0]).name
        rel, how = tree.resolve(name)
        if rel is None:
            legacy['sam'][name] = {name: how}
            continue
        layout, pages = parse_sam((tree.content / rel).read_bytes())
        if pages is None:
            legacy['sam'][name] = {name: layout}
            continue
        stats[f'sam layout {layout}'] += 1
        definitions[key] = (Path(rel).name, rel, layout, pages)

    # Definitions sharing a page are one component: all of them are owned
    # together or not at all, and no page is in two bundles.
    parent = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[max(ra, rb)] = min(ra, rb)

    for key, (_name, _rel, _layout, pages) in definitions.items():
        find(key)
        for page in pages:
            union(key, 'page:' + page.lower())
    components = defaultdict(list)
    for key in definitions:
        components[find(key)].append(key)

    families = []                       # (bundle, [(name, rel)], family)
    for root_key in sorted(components):
        keys = sorted(components[root_key])
        members, problems = {}, {}
        for key in keys:
            name, rel, _layout, pages = definitions[key]
            for member in [name] + pages:
                why = member_problem(member)
                if why:
                    problems[member] = why
                else:
                    members.setdefault(member.lower(), (member, tree.resolve(member)[0]))
        if problems:
            for key in keys:
                legacy['sam'][definitions[key][0]] = problems
            stats['sam components left legacy'] += 1
            continue
        bundle = 'sam_' + Path(definitions[keys[0]][0]).stem
        families.append((bundle, sorted(members.values(), key=lambda m: m[0].lower()), 'sam'))
        stats['sam definitions owned'] += len(keys)

    # ---- particle plists ------------------------------------------------------
    sam_owned = {m[0].lower() for _b, ms, _f in families for m in ms}
    particles = {}                      # lower plist name -> [member names] (the plist first)
    for key in sorted(k for k in tree.where if k.endswith('.plist')):
        name = Path(tree.where[key][0]).name
        rel, how = tree.resolve(name)
        if rel is None:
            legacy['particle'][name] = {name: how}
            continue
        try:
            plist = plistlib.loads((tree.content / rel).read_bytes())
        except Exception as ex:   # noqa: BLE001 -- any parse failure keeps the file legacy
            legacy['particle'][name] = {name: f'not a property list ({type(ex).__name__})'}
            continue
        if not isinstance(plist, dict) or not ({'maxParticles', 'emitterType'} & set(plist)):
            stats['plists that are not particle systems (sprite frames etc.)'] += 1
            continue
        members = [name]
        texture = Path(str(plist.get('textureFileName', '')).replace('\\', '/')).name
        if texture:
            texture_rel, _ = tree.resolve(texture)
            if texture_rel is not None:
                members.append(texture)       # tried first: addImage(<plist dir>/<texture>)
            elif not plist.get('textureImageData'):
                legacy['particle'][name] = {texture: 'named texture exists nowhere and nothing is embedded'}
                continue
        particles[key] = members

    # A plist whose texture is another plist's file joins that plist's bundle.
    for key, members in particles.items():
        find('prt:' + key)
        for m in members:
            union('prt:' + key, 'prt-member:' + m.lower())
    groups = defaultdict(list)
    for key in particles:
        groups[find('prt:' + key)].append(key)
    for root_key in sorted(groups):
        keys = sorted(groups[root_key])
        names = {}
        for key in keys:
            for m in particles[key]:
                names.setdefault(m.lower(), m)
        problems = {m: why for m in names.values() if (why := member_problem(m))}
        problems.update({m: 'shares a member with a SuperAnim bundle' for m in names.values()
                         if m.lower() in sam_owned})
        if problems:
            for key in keys:
                legacy['particle'][particles[key][0]] = problems
            continue
        members = sorted(((m, tree.resolve(m)[0]) for m in names.values()), key=lambda m: m[0].lower())
        families.append(('prt_' + Path(particles[keys[0]][0]).stem, members, 'particle'))
        stats['particle plists owned'] += len(keys)

    # ---- the policy: the base, then C3 appended --------------------------------
    policy = dict(base)
    scope = list(base.get('scope', []))
    bundles = dict(base.get('bundles', {}))
    present = {p.lower() for p in scope}
    seen_members = {m.lower(): b for b, ms in bundles.items() for m in ms}
    merged = {}
    for bundle, members, family in families:
        assert bundle not in bundles and bundle not in merged, f'bundle name {bundle} already used'
        merged[bundle] = (members, family)
    for bundle, (members, family) in merged.items():
        for name, rel in members:
            owner = seen_members.get(name.lower())
            assert owner is None, f'{name} would be in two bundles ({owner}, {bundle})'
            seen_members[name.lower()] = bundle
            if rel.lower() not in present:
                scope.append(rel)
                present.add(rel.lower())
        bundles[bundle] = [name for name, _rel in members]
    extensions = list(base.get('extensions', []))
    if '.plist' not in [e.lower() for e in extensions]:
        extensions.append('.plist')
    policy['scope'] = scope
    policy['bundles'] = bundles
    policy['extensions'] = extensions
    Path(args.out).write_text(json.dumps(policy, indent=1, ensure_ascii=False), encoding='utf-8')

    files = Counter()
    bundle_count = Counter()
    for bundle, (members, family) in merged.items():
        files[family] += len(members)
        bundle_count[family] += 1
    reasons = Counter()
    for family, defs in legacy.items():
        for _d, problems in defs.items():
            for why in set(problems.values()):
                reasons[f'{family}: {why.split(":")[0]}'] += 1
    report = {
        'base': str(args.base),
        'base_names': len(owned_by_base.names),
        'c3': {
            'bundles_by_family': dict(bundle_count),
            'files_by_family': dict(files),
            'files': sum(files.values()),
            'policy_scope_entries': len(scope),
            'policy_bundles': len(bundles),
        },
        'stats': dict(stats),
        'legacy_reasons': dict(reasons.most_common()),
        'legacy': {k: v for k, v in legacy.items()},
    }
    if args.report:
        Path(args.report).write_text(json.dumps(report, indent=1), encoding='utf-8')
    print(f'C3: {report["c3"]["files"]} more files in {sum(bundle_count.values())} bundles '
          f'{dict(bundle_count)} (files {dict(files)}); policy scope now {len(scope)} entries, {len(bundles)} bundles')
    print('stats:', dict(stats))
    print('left legacy:', dict(reasons.most_common()))
    return 0


if __name__ == '__main__':
    sys.exit(main())
