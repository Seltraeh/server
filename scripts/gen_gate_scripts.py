"""Generate the summon gate scripts with a gate guard (#24, Session 14).

    python scripts/gen_gate_scripts.py --out DIR [--src DIR] [--check]

Why.  SummonsAnimeScene2::updateEvent (libgame.so @0x169D080) whites out the
whole screen when the GLOBAL SuperAnimList holds exactly three animations, the
third is not Touch.sam / Hold.sam, and it has reached frame 45.  It counts
every animation in the client, not just the gate's, and other screens leave
some in the list (scripts/gate_overlay_model.py has the details and
addresses).  With two of those already present, a double gate's Change
animation is the third entry by the time it reaches frame 45 -- the screen
turns white over the second gate.  With one present, the Open's blend layer is
the third and the white flash covers the reveal, as designed.

The fix.  Each Wait and Change script draws, right after its gate, a named
anime `gate_guard` using Touch.sam placed far off screen, and removes it with
the gate after the tap.  While the gate is up the guard always follows it in
the list, so a gate (or its blend) can be the third entry only when the guard
makes the count four -- whatever else is in the list.  The rule never fires
on a Touch.sam, so the guard cannot trigger it either.  The Open scripts are
left exactly as they are, so the designed flash still happens when the client
would have shown it.  Nothing is hidden: no white effect is suppressed, no
gate transition skipped, and the guard is invisible.

Input is the current (already patched, tools/patch_gacha_scripts.py) content
folder, read only.  Output is a separate folder: the 9 Wait/Change scripts
rewritten, the 8 Open scripts copied byte for byte.  The generator refuses to
write into deploy/ or any client LocalState: delivering the result is a
separate, deliberate step (a provider policy root in Session 14).  It is
idempotent, and --check re-reads the output and validates its shape.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SRC = ROOT / 'deploy' / 'game_content' / 'content' / 'gacha'

GUARD = 'gate_guard'
# Far off the 320x480 script canvas; Touch.sam is already a dependency of
# every Wait/Change script (the visible touch prompt).
GUARD_DRAW = f'*,type=PARAM,id=42,param={GUARD}:-1000:-1000:Touch.sam:none,#'
GUARD_REMOVE = f'*,type=PARAM,id=43,param={GUARD},#'
PROMPT_REMOVE = '*,type=PARAM,id=43,param=touch_prompt,#'
# What tools/patch_gacha_scripts.py puts first: the previous Open's gate and
# particles.  test_summon_reveal_wire.py section D requires these five first.
LEFTOVERS = ['*,type=PARAM,id=43,param=door_open,#'] + \
            [f'*,type=PARAM,id=47,param={n},#' for n in ('light', 'light2', 'light3', 'light4')]
GATE_DRAW = re.compile(r'^\*,type=PARAM,id=42,param=door_(appear|change):')
SCRIPT_GLOBS = ('gWait*.txt', 'gChange(*.txt', 'gOpen*.txt')


def is_guarded_kind(name: str) -> bool:
    return name.startswith('gWait') or name.startswith('gChange(')


def guard(text: str) -> str:
    """Add the guard to one Wait/Change script (idempotent)."""
    if '﻿' in text:
        raise ValueError('byte-order mark in a Wait/Change script (patch_gacha_scripts.py removes them)')
    lines = [l for l in text.replace('\r\n', '\n').split('\n') if l not in (GUARD_DRAW, GUARD_REMOVE)]
    if lines[:len(LEFTOVERS)] != LEFTOVERS:
        raise ValueError('does not start with the leftover cleanup of tools/patch_gacha_scripts.py')
    out = lines[:len(LEFTOVERS)] + [GUARD_REMOVE]           # a guard a previous script left
    for line in lines[len(LEFTOVERS):]:
        out.append(line)
        if GATE_DRAW.match(line):
            out.append(GUARD_DRAW)
        elif line == PROMPT_REMOVE:
            out.append(GUARD_REMOVE)
    return '\n'.join(out)


def ops(text: str) -> list[tuple[int, str]]:
    found = []
    for raw in re.findall(r'\*([^#]*)#', text.replace('﻿', '')):
        f = dict(x.split('=', 1) for x in raw.split(',') if '=' in x)
        if f.get('type') == 'PARAM' and f.get('id', '').lstrip('-').isdigit():
            found.append((int(f['id']), f.get('param', '')))
    return found


def shape_problems(name: str, text: str) -> list[str]:
    """Why a generated Wait/Change script is not the intended shape ([] = fine)."""
    problems = []
    o = ops(text)
    if '﻿' in text:
        problems.append('byte-order mark')
    if o[:5] != [(43, 'door_open')] + [(47, n) for n in ('light', 'light2', 'light3', 'light4')]:
        problems.append('leftover cleanup is not first')
    draws = [k for k, (i, p) in enumerate(o) if i == 42 and p.startswith(f'{GUARD}:')]
    gates = [k for k, (i, p) in enumerate(o) if i == 42 and re.match(r'door_(appear|change):', p)]
    if len(draws) != 1 or len(gates) != 1 or draws[0] != gates[0] + 1:
        problems.append(f'guard must be drawn once, right after the gate (gates {gates}, guards {draws})')
    else:
        guard_param = o[draws[0]][1].split(':')
        if guard_param[3:] != ['Touch.sam', 'none'] or float(guard_param[1]) > -320 or float(guard_param[2]) > -480:
            problems.append(f'guard is not an off-screen Touch.sam: {o[draws[0]][1]}')
    removes = [k for k, (i, p) in enumerate(o) if (i, p) == (43, GUARD)]
    prompt_removes = [k for k, (i, p) in enumerate(o) if (i, p) == (43, 'touch_prompt')]
    if len(removes) != 2 or removes[0] != 5 or not prompt_removes or removes[1] != prompt_removes[0] + 1:
        problems.append(f'guard must be removed first thing and right after the prompt (at {removes})')
    if any(i == 20 for i, _ in o):
        problems.append('id=20 clearScreen is back')
    drawn = {p.split(':')[0] for i, p in o if i == 42}
    removed = {p for i, p in o if i == 43}
    if not drawn <= removed:
        problems.append(f'drawn but never removed: {sorted(drawn - removed)}')
    return problems


def refuse(out: Path) -> str | None:
    out = out.resolve()
    deploy = (ROOT / 'deploy').resolve()
    packages = Path(os.environ.get('LOCALAPPDATA', '')) / 'Packages'
    if out == deploy or deploy in out.parents:
        return f'{out} is inside deploy/: the served content is changed only by a deliberate deployment'
    if packages.exists() and (out == packages.resolve() or packages.resolve() in out.parents):
        return f'{out} is inside a client package: client caches are never written'
    return None


def sources(src: Path) -> list[Path]:
    found = sorted({p for g in SCRIPT_GLOBS for p in src.glob(g)})
    if len(found) != 17:
        raise SystemExit(f'expected the 17 summon gate scripts under {src}, found {len(found)}')
    return found


def generate(src: Path, out: Path) -> dict:
    why = refuse(out)
    if why:
        raise SystemExit(f'refusing: {why}')
    out.mkdir(parents=True, exist_ok=True)
    report = {'source': str(src), 'files': {}}
    for path in sources(src):
        raw = path.read_bytes()
        if is_guarded_kind(path.name):
            text = guard(raw.decode('utf-8'))
            problems = shape_problems(path.name, text)
            if problems:
                raise SystemExit(f'{path.name}: {problems}')
            data = text.encode('utf-8')
            kind = 'guarded'
        else:
            data = raw                                   # Open scripts: byte for byte
            kind = 'copied'
        target = out / path.name
        if not target.exists() or target.read_bytes() != data:
            tmp = target.with_name(target.name + '.tmp')
            tmp.write_bytes(data)
            os.replace(tmp, target)
        report['files'][path.name] = {'kind': kind, 'source_sha256': hashlib.sha256(raw).hexdigest(),
                                      'sha256': hashlib.sha256(data).hexdigest()}
    return report


def check(out: Path) -> list[str]:
    problems = []
    for path in sources(out):
        if is_guarded_kind(path.name):
            problems += [f'{path.name}: {p}' for p in shape_problems(path.name, path.read_text(encoding='utf-8'))]
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--src', default=str(DEFAULT_SRC), help='folder with the current gate scripts (read only)')
    ap.add_argument('--out', required=True, help='output folder (never inside deploy/ or a client package)')
    ap.add_argument('--check', action='store_true', help='only validate an existing output folder')
    args = ap.parse_args()
    out = Path(args.out)
    if args.check:
        problems = check(out)
        for p in problems:
            print('PROBLEM', p)
        print(f'{len(problems)} problem(s) in {out}')
        return 1 if problems else 0
    report = generate(Path(args.src), out)
    (out / 'gate_scripts_report.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
    for name, row in report['files'].items():
        print(f"{row['kind']:8} {name:28} {row['source_sha256'][:16]} -> {row['sha256'][:16]}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
