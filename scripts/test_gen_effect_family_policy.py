"""Deterministic test of scripts/gen_effect_family_policy.py on a synthetic tree (no server, no real content).

    python scripts/test_gen_effect_family_policy.py

Builds a small content root, a fake app-package resource folder and a base
policy under out/qa/current/gen_effect_family_policy/, runs the generator as a
subprocess and checks every rule its docstring states:

  - a SuperAnim definition is owned with ALL its pages, under either header
    layout (16-bit fields, or the 32-bit layout of the unit/monster animations);
  - definitions sharing a page form ONE bundle; no name is in two bundles;
  - a missing page, a page whose copies differ, or a definition whose own copies
    differ keeps the whole group legacy; identical copies are fine;
  - a packaged copy with different bytes keeps the group legacy, an identical
    one does not; a plist that differs only in CR LF reads the same (text mode);
  - a particle plist is owned alone when its texture is embedded, with its
    texture when that file exists, and not at all when the named texture exists
    nowhere and nothing is embedded; sprite-frame plists are not particles;
  - the base policy stays an exact prefix and its names are never re-owned.
"""
from __future__ import annotations

import json
import plistlib
import shutil
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, qa_dir  # noqa: E402

GEN = ROOT / 'scripts' / 'gen_effect_family_policy.py'


def sam(pages: list[str], wide: bool = False) -> bytes:
    """A version-1 SuperAnim definition naming pages (header fields 16- or 32-bit)."""
    out = b'MAS.' + struct.pack('<I', 1) + bytes([24])
    out += struct.pack('<iiii', 0, 0, 12800, 12800) if wide else struct.pack('<hhhh', 0, 0, 12800, 12800)
    out += struct.pack('<H', len(pages))
    for p in pages:
        out += struct.pack('<H', len(p)) + p.encode() + struct.pack('<HH', 10, 10)
        out += struct.pack('<iiii', 1310720, 0, 0, 1310720) + struct.pack('<hh', 0, 0)
    return out + b'\x00' * 32          # frames etc. (not read by the generator)


def particle(texture: str = '', embedded: bool = True, crlf: bool = False) -> bytes:
    d = {'maxParticles': 10, 'emitterType': 0, 'textureFileName': texture}
    if embedded:
        d['textureImageData'] = 'H4sIAAAAAAAAAwMAAAAAAAAAAAA='
    data = plistlib.dumps(d)
    return data.replace(b'\n', b'\r\n') if crlf else data


def write(root: Path, rel: str, data: bytes) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data)


def main() -> int:
    check = Checker()
    work = qa_dir('gen_effect_family_policy')
    shutil.rmtree(work, ignore_errors=True)
    content, app = work / 'content', work / 'Resources'
    app.mkdir(parents=True)
    png = lambda tag: b'\x89PNG\r\n\x1a\n' + tag.encode()   # noqa: E731

    # Owned: a short-layout definition with two pages, and a wide one with one.
    write(content, 'sam/Short/Short.sam', sam(['Short_10x10.png', 'Short_20x20.jpg']))
    write(content, 'sam/Short/Short_10x10.png', png('s1'))
    write(content, 'sam/Short/Short_20x20.jpg', b'\xff\xd8jpeg')
    write(content, 'unit_sam/unit_1/unit_anime_1.sam', sam(['unit_anime_1_8x8.png'], wide=True))
    write(content, 'unit_sam/unit_1/unit_anime_1_8x8.png', png('u1'))
    write(content, 'unit/cgs/unit_anime_1.sam', sam(['unit_anime_1_8x8.png'], wide=True))   # identical copy
    # Two definitions sharing a page: one bundle.
    write(content, 'sam/A/A.sam', sam(['A_1.png', 'Shared_1.png']))
    write(content, 'sam/A/A_1.png', png('a'))
    write(content, 'sam/A/Shared_1.png', png('shared'))
    write(content, 'sam/B/B.sam', sam(['B_1.png', 'Shared_1.png']))
    write(content, 'sam/B/B_1.png', png('b'))
    write(content, 'sam/B/Shared_1.png', png('shared'))              # identical duplicate of the shared page
    # Legacy: a missing page; a page whose copies differ; a definition whose copies differ.
    write(content, 'sam/Miss/Miss.sam', sam(['Miss_1.png', 'Gone_1.png']))
    write(content, 'sam/Miss/Miss_1.png', png('m'))
    write(content, 'sam/Clash/Clash.sam', sam(['Clash_1.png']))
    write(content, 'sam/Clash/Clash_1.png', png('one'))
    write(content, 'title/Clash_1.png', png('two'))
    write(content, 'sam/Twice/Twice.sam', sam(['Twice_1.png']))
    write(content, 'sam/Twice.sam/Twice.sam', sam(['Twice_1.png', 'Twice_2.png']))
    write(content, 'sam/Twice/Twice_1.png', png('t'))
    # Packaged copies: identical (owned) and different (legacy).
    write(content, 'sam/Pkg/Pkg.sam', sam(['Pkg_1.png']))
    write(content, 'sam/Pkg/Pkg_1.png', png('pkg'))
    (app / 'Pkg.sam').write_bytes(sam(['Pkg_1.png']))
    write(content, 'sam/PkgDiff/PkgDiff.sam', sam(['PkgDiff_1.png']))
    write(content, 'sam/PkgDiff/PkgDiff_1.png', png('new'))
    (app / 'PkgDiff_1.png').write_bytes(png('old'))
    # Already owned by the base: left as the base has it.
    write(content, 'sam/Base/Base.sam', sam(['Base_1.png']))
    write(content, 'sam/Base/Base_1.png', png('base'))
    # Particles.
    write(content, 'effect/plist/prt_embedded.plist', particle('missing_tex.png'))
    write(content, 'effect/plist/prt_withtex.plist', particle('prt_tex.png', embedded=False))
    write(content, 'effect/img/prt_tex.png', png('tex'))
    write(content, 'effect/plist/prt_broken.plist', particle('nowhere.png', embedded=False))
    write(content, 'effect/plist/prt_crlf.plist', particle('', crlf=False))
    (app / 'prt_crlf.plist').write_bytes(particle('', crlf=True))
    write(content, 'footer/frames.plist', plistlib.dumps({'frames': {}, 'metadata': {'textureFileName': 'f.png'}}))

    base = {'enabled': False, 'scope': ['sam/Base/Base.sam', 'sam/Base/Base_1.png'], 'aliases': {},
            'extensions': ['.sam', '.png', '.jpg', '.csv'], 'exclude': ['*.dat', '*.xml', '*.json'],
            'bundles': {'town': ['Base.sam', 'Base_1.png']}}
    (work / 'base.json').write_text(json.dumps(base), encoding='utf-8')
    run = subprocess.run([sys.executable, str(GEN), '--base', str(work / 'base.json'), '--out', str(work / 'c3.json'),
                          '--report', str(work / 'report.json'), '--content', str(content), '--app-resources', str(app)],
                         capture_output=True, text=True)
    check('generator ran', run.returncode == 0, run.stdout[-500:] + run.stderr[-1500:])
    if run.returncode != 0:
        return check.summary('test_gen_effect_family_policy')
    policy = json.loads((work / 'c3.json').read_text(encoding='utf-8'))
    report = json.loads((work / 'report.json').read_text(encoding='utf-8'))
    bundles = policy['bundles']
    member_of = {}
    for b, ms in bundles.items():
        for m in ms:
            member_of.setdefault(m.lower(), []).append(b)
    owned = set(member_of)

    def bundle_with(name):
        return next((set(m.lower() for m in ms) for ms in bundles.values() if name.lower() in {x.lower() for x in ms}), set())

    check('the base is an exact prefix (scope, bundles, aliases, exclusions)',
          policy['scope'][:2] == base['scope'] and list(bundles)[0] == 'town' and bundles['town'] == base['bundles']['town']
          and policy['aliases'] == {} and policy['exclude'] == base['exclude'], policy['scope'][:3])
    check('.plist is appended to the extensions, the base\'s kept in order',
          policy['extensions'] == base['extensions'] + ['.plist'], policy['extensions'])
    check('a short-layout definition is owned with all its pages, in one bundle',
          bundle_with('Short.sam') == {'short.sam', 'short_10x10.png', 'short_20x20.jpg'}, bundle_with('Short.sam'))
    check('a wide-layout (32-bit) definition is read and owned with its page',
          bundle_with('unit_anime_1.sam') == {'unit_anime_1.sam', 'unit_anime_1_8x8.png'}, bundle_with('unit_anime_1.sam'))
    check('a definition whose two copies are identical is owned once',
          'unit_anime_1.sam' in owned and len(member_of['unit_anime_1.sam']) == 1)
    check('definitions sharing a page are one bundle',
          bundle_with('A.sam') == {'a.sam', 'a_1.png', 'shared_1.png', 'b.sam', 'b_1.png'}, bundle_with('A.sam'))
    check('no name is in two bundles', all(len(v) == 1 for v in member_of.values()),
          {k: v for k, v in member_of.items() if len(v) > 1})
    check('a missing page keeps the whole definition legacy', not ({'miss.sam', 'miss_1.png'} & owned))
    check('a page whose copies differ keeps the definition legacy', not ({'clash.sam', 'clash_1.png'} & owned))
    check('a definition whose own copies differ stays legacy', 'twice.sam' not in owned and 'twice_1.png' not in owned)
    check('an identical packaged copy does not prevent ownership', {'pkg.sam', 'pkg_1.png'} <= owned)
    check('a packaged copy with different bytes keeps the group legacy', not ({'pkgdiff.sam', 'pkgdiff_1.png'} & owned))
    check('the base\'s names are not re-owned (one bundle: the base\'s)', member_of.get('base.sam') == ['town'])
    check('a particle with an embedded texture is owned alone', bundle_with('prt_embedded.plist') == {'prt_embedded.plist'})
    check('a particle whose texture file exists is owned with it',
          bundle_with('prt_withtex.plist') == {'prt_withtex.plist', 'prt_tex.png'}, bundle_with('prt_withtex.plist'))
    check('a particle naming a texture that exists nowhere, with nothing embedded, stays legacy',
          'prt_broken.plist' not in owned)
    check('a packaged plist that differs only in CR LF reads the same and is owned', 'prt_crlf.plist' in owned)
    check('a sprite-frame plist is not a particle', 'frames.plist' not in owned)
    legacy = report['legacy']
    check('the report names each legacy definition with its reason',
          'Miss.sam' in legacy.get('sam', {}) and 'Clash.sam' in legacy.get('sam', {})
          and 'PkgDiff.sam' in legacy.get('sam', {}) and 'prt_broken.plist' in legacy.get('particle', {}),
          {k: sorted(v) for k, v in legacy.items()})
    check('every scope path added is a file of the content root',
          all((content / p).is_file() for p in policy['scope'][2:]), [p for p in policy['scope'][2:] if not (content / p).is_file()])
    return check.summary('test_gen_effect_family_policy')


if __name__ == '__main__':
    raise SystemExit(main())
