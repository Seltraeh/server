"""Give the archive's special bosses a status-icon offset measured from their own sprites (round 8).

    python scripts/gen_boss_icon_offsets.py --check [--archive DIR]   # exit 1 if DIR/mission.json differs
    python scripts/gen_boss_icon_offsets.py --out PATH [--archive DIR]

The bosses with their own art (Juggernaut, Demon Abaddon, Creator Maxwell, the four story bosses, Granvalm,
Grantos) were archived with the invented hp_disp_pos "0,-60" the Training Ground dummies had, which puts their
buff/debuff icons ~190 px above the sprite (round 8, R8 v1).  The client places the icon at
    feet y - H + y_off                                   (BattleUnit::getStateIconPoint, arm64 0x11350B4)
H being the height of the union of the idle frames' cgg part rectangles.  Over 336 shipped units the UnitMst
offsets they carry fit y_off = H + visible_top + MARGIN (visible_top = highest opaque pixel of the idle
frames, negative = up), MARGIN 3.5, median error 13.5 units; on the Training Ground dummy the same rule gives
~32 against the client-verified 30.  So each boss sprite gets that y_off, x 0.  tools/icon_offset_model.py
re-derives MARGIN.

That rule alone hides a giant boss's icons: their visible top is at or above the top of the battlefield, so the
icon lands under the zel/karma bar or off screen (round 8 v9/v10: Klaus's burst debuffed Emperor and Maxwell
and no icon showed).  Run v11 put each icon at its feet (y_off = H) and measured it: the icon centre sits
pos_y/2 + K - (H - y_off) battle units below the field's top, pos_y from the monster's "x:y" position and
K 15.7 / 18.3 / 21.8 for Juggernaut / Emperor / Maxwell.  The centre must stay FIELD_TOP units down to clear
the bar and the SPARK/CRITICAL line, so y_off is at least H - pos_y/2 + CLAMP (FIELD_TOP 38.4 less K ~19),
which puts a giant's icons on its upper body instead.

Only rows still carrying "0,-60" change, and only where the sprite can be measured: the Battle Simulator's
dummies belong to scripts/gen_battle_simulator.py, Cursed Estia has no cgg (anm_cgg '') and Xie Jing no art,
so those keep their value (reported).  Idempotent; only `visuals.hp_disp_pos` is written.
"""
import argparse
import json
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
MISSIONS = ROOT / 'deploy' / 'archive' / 'mission.json'
CONTENT = ROOT / 'deploy' / 'game_content' / 'content' / 'monster'
INVENTED = '0,-60'
MARGIN = 3.5
CLAMP = 20
SIMULATOR = 6000000


def cgg_frames(path: Path) -> dict:
    frames = {}
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        v = [x for x in line.strip().split(',') if x != '']
        if len(v) < 2 or not v[0].lstrip('-').isdigit():
            continue
        parts = []
        for i in range(int(v[1])):
            p = v[2 + i * 11: 2 + (i + 1) * 11]
            if len(p) >= 10:
                parts.append(tuple(int(p[k]) for k in (0, 1, 6, 7, 8, 9)))
        frames[int(v[0])] = parts
    return frames


def offset_for(visuals: dict, position: str = '') -> int | None:
    """The modelled y offset for a sprite at `position`, or None when its art cannot be measured."""
    cgg, img, idle = (visuals.get(k, '') for k in ('anm_cgg', 'img_a', 'cgs_idle'))
    paths = (CONTENT / 'cgg' / cgg, CONTENT / 'img' / img, CONTENT / 'cgs' / idle)
    if not (cgg and img and idle) or not all(p.is_file() for p in paths):
        return None
    frames = cgg_frames(paths[0])
    used = sorted({int(line.split(',')[0]) for line in paths[2].read_text(encoding='utf-8').splitlines()
                   if line.split(',')[0].strip().lstrip('-').isdigit()} & set(frames)) or sorted(frames)[:1]
    alpha = Image.open(paths[1]).convert('RGBA').getchannel('A')
    top = bottom = visible = None
    for f in used:
        for x, y, sx, sy, w, h in frames[f]:
            top = y if top is None else min(top, y)
            bottom = y + h if bottom is None else max(bottom, y + h)
            box = alpha.crop((sx, sy, sx + w, sy + h)).getbbox()
            if box:
                visible = y + box[1] if visible is None else min(visible, y + box[1])
    if top is None or visible is None:
        return None
    y = int(round(bottom - top + visible + MARGIN))
    pos = position.split(':')
    if len(pos) == 2 and pos[1].strip().lstrip('-').isdigit():
        y = max(y, int(round(bottom - top - int(pos[1]) / 2 + CLAMP)))
    return y


def generate(raw: bytes):
    missions = json.loads(raw)
    changed, skipped = [], []
    for m in missions:
        if m['id'] == SIMULATOR:
            continue
        monsters = [b for s in m['stages'] for b in s['battle_monsters']] + list(m.get('script_monsters') or [])
        for b in monsters:
            v = b.get('visuals')
            if not v or v.get('hp_disp_pos') != INVENTED:
                continue
            y = offset_for(v, b.get('position', ''))
            if y is None:
                skipped.append((m['id'], b['name']))
                continue
            v['hp_disp_pos'] = f'0,{y}'
            changed.append((m['id'], b['name'], y))
    text = json.dumps(missions, indent=1, ensure_ascii=False).replace('\n', '\r\n') + '\r\n'
    return text.encode('utf-8'), changed, skipped


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--archive', type=Path, default=MISSIONS.parent)
    ap.add_argument('--out', type=Path)
    a = ap.parse_args()
    raw = (a.archive / MISSIONS.name).read_bytes()
    new, changed, skipped = generate(raw)
    for mid, name, y in changed:
        print(f'  {mid} {name}: 0,{y}')
    if skipped:
        print(f'  kept "{INVENTED}" (art not measurable): {sorted(set(n for _, n in skipped))}')
    if a.check:
        print('up to date' if new == raw else f'stale: {a.archive / MISSIONS.name}')
        return 0 if new == raw else 1
    if a.out is None:
        sys.exit('nothing written: pass --out PATH')
    a.out.write_bytes(new)
    print(f'wrote {a.out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
