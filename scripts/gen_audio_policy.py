"""Phase C5 candidate policy: game_content's sounds, served to the audio reader from RAM (Session 15).

    python scripts/gen_audio_policy.py --out POLICY_JSON [--base BASE_POLICY] [--report REPORT_JSON]

Every .mp3 under deploy/game_content/content (sound/, sound/bgm/, sound/se/)
becomes its own one-member bundle `snd_<name>`, added to the base policy (for
example C3) or alone.  deploy/ is only read; the policy is written disabled
(the harness or the maintainer switches it on).

**Pair it only with a proxy DLL that hooks the audio reader** (Session 15:
`offline-proxy/OfflineMod.win32/content/cp_hooks*.cpp`, the EXE's fopen_s /
fread / fclose imports; the log line "audio reader on").  The reader (x86
0xEC0030) bypasses getFileData: an older DLL would report owned sounds as
downloaded, the game would stop downloading them, and an uncached one would be
silent.  A DLL whose audio patch failed keeps sound names legacy by itself;
the test harness refuses an audio policy with a DLL not known to hook audio.

Why it is safe to own them.  The reader decodes the whole file with
fopen_s(&f, path, "rb"), fread(buf, 1, 210, f) until 0, fclose(f) -- no seek,
no other stream call (tests/fixtures/check_audio_reader.py) -- every time the
engine (re)loads a sound (SimpleAudioEngine -> MediaStreamer::Initialize ->
the handler, effects and music alike; the game's sound manager unloads its
oldest effect and decodes it again on the next play).  Cached copies in
LocalState are byte-identical to game_content's files (plain MP3, no
FileCrypt), so owning them changes no sound for a client that downloaded them;
for an uncached client it removes the download.  The 42 sounds the app
package also ships are game_content's bytes for 39 of them; the other three
(the title music and voice) are what the game downloads and plays today once
cached, so owning them serves that copy from the first play.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONTENT = ROOT / 'deploy' / 'game_content' / 'content'
APPX = ROOT.parent / 'BraveFrontierAppxClient' / 'Assets'
SOUND_EXTENSIONS = ('.mp3',)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def declared(policy: dict) -> set[str]:
    return {m.lower() for ms in policy.get('bundles', {}).values() for m in ms}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', required=True, type=Path, help='policy JSON to write (outside deploy/)')
    ap.add_argument('--base', type=Path, help='policy to extend (e.g. the C3 effects policy); default: sounds only')
    ap.add_argument('--report', type=Path, help='inventory report to write')
    args = ap.parse_args()
    out = args.out.resolve()
    if (ROOT / 'deploy').resolve() in out.parents:
        raise SystemExit('refusing: write the candidate policy outside deploy/')

    sounds = sorted(p for p in CONTENT.rglob('*') if p.is_file() and p.suffix.lower() in SOUND_EXTENSIONS)
    by_name: dict[str, Path] = {}
    for p in sounds:
        key = p.name.lower()
        if key in by_name:
            raise SystemExit(f'two sounds share the flat name {p.name}: {by_name[key]} and {p}')
        by_name[key] = p
    if not sounds:
        raise SystemExit(f'no sounds under {CONTENT}')

    if args.base:
        base = json.loads(args.base.read_text(encoding='utf-8'))
        owned = declared(base)
        already = sorted(n for n in owned if n.endswith(SOUND_EXTENSIONS) or n.endswith('.wav'))
        if already:
            raise SystemExit(f'the base policy already owns sounds: {already[:5]}')
        clash = sorted(n for n in by_name if n in owned)
        if clash:
            raise SystemExit(f'flat-name clash with the base policy: {clash[:5]}')
        if any(b.startswith('snd_') for b in base.get('bundles', {})):
            raise SystemExit('the base policy already has snd_* bundles')
        policy = json.loads(json.dumps(base))
    else:
        policy = {'enabled': False, 'root': None, 'scope': [], 'aliases': {}, 'extensions': [], 'exclude': [],
                  'bundles': {}}
    rels = [p.relative_to(CONTENT).as_posix() for p in sounds]
    policy['scope'] = list(policy.get('scope', [])) + rels
    exts = list(policy.get('extensions', []))
    for ext in SOUND_EXTENSIONS:
        if ext not in exts:
            exts.append(ext)
    policy['extensions'] = exts
    for p in sounds:
        policy['bundles'][f'snd_{p.stem}'] = [p.name]
    policy['enabled'] = False
    if policy.get('root') is None:
        policy.pop('root', None)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(policy, indent=1), encoding='utf-8')

    packaged = {p.name.lower(): p for p in APPX.rglob('*') if p.is_file() and p.suffix.lower() in SOUND_EXTENSIONS} \
        if APPX.is_dir() else {}
    differ = sorted(n for n, p in packaged.items() if n in by_name and sha(p) != sha(by_name[n]))
    sizes = sorted(((p.stat().st_size, p.name) for p in sounds), reverse=True)
    report = {
        'policy': str(out), 'policy_sha256': sha(out), 'base': str(args.base) if args.base else None,
        'base_sha256': sha(args.base) if args.base else None,
        'sounds': len(sounds), 'by_folder': {}, 'bytes': sum(s for s, _ in sizes), 'largest': sizes[:5],
        'owned_names': len(declared(policy)),
        'packaged': len(packaged), 'packaged_identical': len(packaged) - len(differ), 'packaged_differ': differ,
    }
    for rel in rels:
        folder = rel.rsplit('/', 1)[0]
        report['by_folder'][folder] = report['by_folder'].get(folder, 0) + 1
    if args.report:
        args.report.write_text(json.dumps(report, indent=1), encoding='utf-8')
    print(f'{out}: {len(sounds)} sounds as {len(sounds)} snd_* bundles ({report["by_folder"]}; '
          f'{report["bytes"] / 1e6:.1f} MB, largest {sizes[0][1]} {sizes[0][0] / 1e6:.1f} MB); '
          f'{report["owned_names"]} owned names in all; sha256 {report["policy_sha256"]}')
    print(f'packaged sounds: {len(packaged)}, {len(packaged) - len(differ)} byte-identical to game_content, '
          f'differ: {", ".join(differ) or "none"}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
