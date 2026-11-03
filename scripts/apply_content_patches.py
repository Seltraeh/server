"""Maintained corrections to the git-ignored game content tree, applied by hash.

    python scripts/apply_content_patches.py --content-root DIR            report each patch's state (read only)
    python scripts/apply_content_patches.py --content-root DIR --apply    correct every file still original

The content tree (deploy/game_content/ here, or a release's populated content folder) is not in git, so a
correction made in it would vanish from any other checkout.  Each patch below names one file under the
content root (the server's document root, the folder holding content/), the SHA-256 of the shipped bytes
and of the corrected bytes, and the edit between them.  Each file is reported as:

  patched    the corrected bytes: nothing to do (running --apply again changes nothing)
  original   the shipped bytes: --apply writes the corrected bytes
  absent     no such file: reported, never created
  unknown    any other bytes: refused and never touched -- someone else changed this file, and a patch
             written against other bytes cannot be trusted to mean the same thing there

--apply never edits a file in place: it writes the corrected bytes to a new file beside it and renames it
over the path, so a hard link elsewhere (a QA fixture sharing the shipped bytes) keeps them.  The result is
hashed before the rename; a mismatch writes nothing.

Exit 0 when every present file is patched (after --apply, if given), 1 when any is original (without
--apply) or unknown, 2 on a usage error.  --json OUT writes the report.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Patch:
    path: str            # relative to the content root, forward slashes
    original: str        # SHA-256 of the shipped file
    patched: str         # SHA-256 of the corrected file
    after_line: int      # insert after this 1-based line of the original...
    expect: bytes        # ...which must be exactly these bytes (line ending included)
    insert: bytes        # the inserted line(s), in the file's own encoding and line endings
    why: str

    def apply(self, data: bytes) -> bytes:
        lines = data.splitlines(keepends=True)
        if len(lines) < self.after_line or lines[self.after_line - 1] != self.expect:
            raise ValueError(f'{self.path}: line {self.after_line} is not {self.expect!r}')
        return b''.join(lines[:self.after_line]) + self.insert + b''.join(lines[self.after_line:])


PATCHES = (
    Patch(
        path='content/tuto/tuto8.txt',
        original='cfa85733ad854131e48be8bcd0624ee6d7556bd6db57f532cf058f2b505a26a9',
        patched='271511d995619d20956883bdc77d6b0253d86a8784cc97ba3b2471c65f89f16b',
        after_line=22,
        expect=b'*,type=STOP,id=-1,#\r\n',
        insert=b'*,type=PLAYWAIT,param=0.5,#\r\n',
        why='Fresh accounts froze on the Grand Gaia quest select (tuto8). Lines 19-24 wait for the quest '
            'select, then create the plate arrow and its tap area (tag -20000) on the scene\'s first '
            'frame; two frames later the scene sends GetAchievementInfo behind a modal ConnectScene, and '
            'when that pops, DungeonSelectScene2::onSceneVisible -> GameScene::removeMask deletes the tap '
            'area, so the tap the script waits for can never happen.  PLAYWAIT is a frame count, not a '
            'clock (arm64 AdventureSystem::advPlayWait 0xFD79A8: param x 60, one per advPlay call), and '
            'the quest select runs no script frame while the modal is up (the director draws only the '
            'running ConnectScene, and BaseScene::draw 0xFFF078 drives each scene\'s frame), so the 30 '
            'frames end after the pop however slow the reply.  The request goes out on the scene\'s '
            'second frame; this holds only while that stays under 30.  Client-confirmed 2026-10-24, and '
            'on 2026-10-28 with the quest and mission selects\' replies held 5 s: the arrow appeared '
            'only after the release and stayed, while the shipped script under the same hold lost it.'),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def check(root: Path, patch: Patch) -> tuple[str, str | None]:
    """The state of one patch's file: (patched | original | absent | unknown, its SHA-256)."""
    target = root / patch.path
    if not target.is_file():
        return 'absent', None
    digest = sha256(target.read_bytes())
    if digest == patch.patched:
        return 'patched', digest
    if digest == patch.original:
        return 'original', digest
    return 'unknown', digest


def write_replacing(target: Path, data: bytes) -> None:
    """A new file renamed over the path: never an in-place edit of a (possibly hard-linked) file."""
    temp = target.with_name(target.name + '.patching')
    temp.write_bytes(data)
    try:
        os.replace(temp, target)
    except OSError:
        temp.unlink(missing_ok=True)
        raise


def run(root: Path, apply: bool) -> list[dict]:
    report = []
    for patch in PATCHES:
        state, digest = check(root, patch)
        entry = {'path': patch.path, 'state': state, 'sha256': digest, 'action': 'none'}
        if state == 'original' and apply:
            fixed = patch.apply((root / patch.path).read_bytes())
            if sha256(fixed) != patch.patched:
                entry.update(state='unknown', action=f'refused: the edit produced {sha256(fixed)}, '
                                                     f'not {patch.patched}')
            else:
                write_replacing(root / patch.path, fixed)
                after, digest = check(root, patch)
                entry.update(state=after, sha256=digest, action='applied')
        elif state == 'unknown':
            entry['action'] = 'refused: not the shipped or the corrected bytes; left untouched'
        report.append(entry)
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--content-root', required=True, type=Path,
                        help='the document root that holds content/ (for example deploy/game_content)')
    parser.add_argument('--apply', action='store_true', help='correct every file still at its shipped bytes')
    parser.add_argument('--json', type=Path, help='also write the report here')
    args = parser.parse_args(argv)
    root = args.content_root
    if not (root / 'content').is_dir():
        print(f'{root}: no content/ folder; not a content root', file=sys.stderr)
        return 2
    report = run(root, args.apply)
    for e in report:
        print(f'{e["state"]:<9} {e["path"]}  {e["sha256"] or "-"}  {e["action"]}')
    if args.json:
        args.json.write_text(json.dumps({'content_root': str(root.resolve()), 'patches': report}, indent=1),
                             encoding='utf-8')
    bad = [e for e in report if e['state'] in ('original', 'unknown')]
    return 1 if bad else 0


if __name__ == '__main__':
    raise SystemExit(main())
