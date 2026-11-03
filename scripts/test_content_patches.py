"""scripts/apply_content_patches.py on disposable content roots (no server).

    python scripts/test_content_patches.py

Every case runs on copies under out/qa/current/content_patches; the real content root is only read.  The
shipped tuto8.txt bytes come from deploy/game_content: when that file is the corrected one, the inserted
line is removed and the result must hash to the shipped bytes before anything is tested with it.  Without
a content tree the real-bytes cases are reported as skipped, never as passed.
"""
from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, qa_dir  # noqa: E402
import apply_content_patches as acp  # noqa: E402

WORK = qa_dir('content_patches')
REAL_ROOT = ROOT / 'deploy' / 'game_content'
TUTO8 = next(p for p in acp.PATCHES if p.path == 'content/tuto/tuto8.txt')


def shipped_tuto8() -> bytes | None:
    """The shipped bytes, recovered read-only from the real content root (it may hold either version)."""
    target = REAL_ROOT / TUTO8.path
    if not target.is_file():
        return None
    data = target.read_bytes()
    if acp.sha256(data) == TUTO8.original:
        return data
    if acp.sha256(data) == TUTO8.patched:
        lines = data.splitlines(keepends=True)
        if lines[TUTO8.after_line] == TUTO8.insert:
            data = b''.join(lines[:TUTO8.after_line] + lines[TUTO8.after_line + 1:])
            return data if acp.sha256(data) == TUTO8.original else None
    return None


def root_with(name: str, data: bytes | None) -> Path:
    root = WORK / name
    (root / 'content' / 'tuto').mkdir(parents=True)
    if data is not None:
        (root / TUTO8.path).write_bytes(data)
    return root


def main() -> int:
    check = Checker()
    if WORK.exists():
        shutil.rmtree(WORK)
    WORK.mkdir(parents=True)

    # The edit itself, on synthetic lines.
    try:
        TUTO8.apply(b'a\r\n' * 30)
        check('Patch.apply refuses when its anchor line differs', False, 'no ValueError')
    except ValueError:
        check('Patch.apply refuses when its anchor line differs', True)
    check('a root without content/ is a usage error (exit 2), nothing created',
          acp.main(['--content-root', str(WORK / 'nothing')]) == 2 and not (WORK / 'nothing').exists())
    absent = root_with('absent', None)
    check('absent: reported, exit 0, never created',
          acp.run(absent, apply=True)[0]['state'] == 'absent' and not (absent / TUTO8.path).exists())

    shipped = shipped_tuto8()
    if shipped is None:
        print(f'SKIP  real-bytes cases: {REAL_ROOT / TUTO8.path} is absent or neither the shipped nor the '
              'corrected bytes')
        return check.summary('test_content_patches')
    check('the shipped tuto8.txt is recovered from the content root and hashes to the recorded original',
          acp.sha256(shipped) == TUTO8.original)

    root = root_with('original', shipped)
    target = root / TUTO8.path
    check('original, report only: state original, exit 1, file unchanged',
          acp.main(['--content-root', str(root)]) == 1 and target.read_bytes() == shipped)
    twin = WORK / 'hardlinked_twin.txt'
    os.link(target, twin)
    rep = acp.run(root, apply=True)[0]
    fixed = target.read_bytes()
    check('original --apply: corrected bytes with the recorded hash', rep['state'] == 'patched'
          and rep['action'] == 'applied' and acp.sha256(fixed) == TUTO8.patched, rep)
    check('--apply replaced the path: a hard link to the shipped bytes keeps them',
          twin.read_bytes() == shipped and os.stat(twin).st_nlink == 1)
    lines = fixed.splitlines(keepends=True)
    check('the UTF-8 BOM and CRLF endings are kept; exactly one line added, as line 23',
          fixed.startswith(b'\xef\xbb\xbf') and all(l.endswith(b'\r\n') for l in lines)
          and len(lines) == len(shipped.splitlines()) + 1 and lines[22] == TUTO8.insert)
    stamp = os.stat(target).st_mtime_ns
    again = acp.run(root, apply=True)[0]
    check('--apply again is a no-op (idempotent): state patched, action none, file not rewritten',
          again['state'] == 'patched' and again['action'] == 'none' and os.stat(target).st_mtime_ns == stamp)
    check('a patched root reports exit 0', acp.main(['--content-root', str(root)]) == 0)
    check('no temporary file is left beside it', not list(target.parent.glob('*.patching')))

    changed = bytearray(shipped)
    changed[-5] ^= 0x20
    unknown = root_with('unknown', bytes(changed))
    out = acp.main(['--content-root', str(unknown), '--apply'])
    check('unknown bytes: --apply refuses (exit 1) and leaves the file untouched',
          out == 1 and (unknown / TUTO8.path).read_bytes() == bytes(changed))
    variant = root_with('variant', shipped.replace(b'\r\n', b'\n'))
    check('the same script with LF endings is unknown too (a patch is bound to exact bytes)',
          acp.run(variant, apply=True)[0]['state'] == 'unknown')

    real = acp.run(REAL_ROOT, apply=False)[0]
    print(f'note: {REAL_ROOT}: {real["state"]} ({real["sha256"]}); read only')
    return check.summary('test_content_patches')


if __name__ == '__main__':
    raise SystemExit(main())
