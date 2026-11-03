"""Battle sprites for units whose part sheets do not survive: unit_anime_<id>.png + unit_cgg_<id>.csv + cgs files.

A unit is drawn from unit_anime_<id>.png cut up by unit_cgg_<id>.csv (one line per frame: `frame,parts,` then per part
`x,y,0,0,100,0,srcX,srcY,w,h,0,` with x/y the part's top-left relative to the unit's feet) and played by the
idle/move/atk cgs files (one line per step: `frame,0,0,ticks,`).  Two stand-ins, both AUTHORED:

  animated(gifs, totals)  the wiki's rendered animations (Unit_ills_anime/atk/move_<id>.gif, one GIF per animation,
                          each cropped to its own content).  Every GIF frame becomes one part.  The idle GIF sets the
                          feet (bottom centre of its frames' union box); the attack and move GIFs are placed by
                          matching their first frame onto the idle pose (alpha cross-correlation), else feet on
                          feet.  Each animation's ticks are the GIF delays scaled to the datamine's total length
                          for it, so hits stay where the unit's damage frames put them.
  static(full, totals)    the full art's figure, fitted to SPRITE_MAX px, feet on the anchor: one frame per
                          animation (the EU stand-in, scripts/eu_units.py).

render(sheet, cgg, frame) draws one cgg frame back onto a canvas, so a test can compare it with the GIF it came from.
"""
from __future__ import annotations

import io

import numpy as np
from PIL import Image, ImageSequence

SPRITE_MAX = 150
SHEET_WIDTH = 1024
MATCH_MIN = 0.6          # alpha overlap (IoU) below which a registration falls back to feet on feet


DARK = 40               # max(R,G,B) below this is background in a frame the GIF stored without transparency
DARK_SHARE = 0.25


def gif_frames(path) -> list[tuple[Image.Image, int]]:
    """(RGBA frame, delay ms) per frame.  Some wiki GIFs store effect frames (speed trails) with no transparency
    index, so their background is opaque near-black: when such a frame is mostly near-black, that is keyed out."""
    im = Image.open(path)
    out = []
    for fr in ImageSequence.Iterator(im):
        rgba = fr.convert('RGBA')
        if fr.info.get('transparency') is None:
            a = np.asarray(rgba).copy()
            dark = (a[..., 3] > 0) & (a[..., :3].max(axis=2) < DARK)
            if dark.sum() > DARK_SHARE * max(1, (a[..., 3] > 0).sum()):
                a[dark, 3] = 0
                rgba = Image.fromarray(a, 'RGBA')
        out.append((rgba, int(fr.info.get('duration') or 0)))
    return out


def _union_box(frames):
    boxes = [f.getchannel('A').getbbox() for f, _ in frames]
    boxes = [b for b in boxes if b]
    if not boxes:
        return None
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))


def _mask(img):
    return (np.asarray(img.getchannel('A'), dtype=np.float64) > 0).astype(np.float64)


def register(idle0: Image.Image, other0: Image.Image) -> tuple[int, int, float]:
    """(dx, dy, IoU): where other0's canvas origin sits in idle0's canvas so that their figures overlap best."""
    a, b = _mask(idle0), _mask(other0)
    H, W = a.shape[0] + b.shape[0], a.shape[1] + b.shape[1]
    corr = np.fft.irfft2(np.fft.rfft2(a, s=(H, W)) * np.conj(np.fft.rfft2(b, s=(H, W))), s=(H, W))
    y, x = np.unravel_index(np.argmax(corr), corr.shape)
    dy = y if y < a.shape[0] else y - H
    dx = x if x < a.shape[1] else x - W
    inter = corr[y, x]
    union = a.sum() + b.sum() - inter
    return int(dx), int(dy), float(inter / union) if union else 0.0


def _ticks(durations: list[int], total: int) -> list[int]:
    """GIF delays scaled to `total` ticks, each at least 1, summing to max(total, len)."""
    ms = [max(d, 10) for d in durations]
    total = max(int(total or 0), len(ms))
    raw = [m * total / sum(ms) for m in ms]
    out = [max(1, int(r)) for r in raw]
    i = 0
    while sum(out) < total:
        out[i % len(out)] += 1
        i += 1
    while sum(out) > total and max(out) > 1:
        j = out.index(max(out))
        out[j] -= 1
    return out


class Sheet:
    """Shelf packer: parts in rows, 1 px apart, SHEET_WIDTH wide."""

    def __init__(self):
        self.parts, self.x, self.y, self.row_h = [], 1, 1, 0

    def add(self, img: Image.Image) -> tuple[int, int]:
        w, h = img.size
        if self.x + w + 1 > SHEET_WIDTH:
            self.x, self.y, self.row_h = 1, self.y + self.row_h + 1, 0
        pos = (self.x, self.y)
        self.parts.append((img, pos))
        self.x += w + 1
        self.row_h = max(self.row_h, h)
        return pos

    def png(self) -> bytes:
        height = self.y + self.row_h + 1
        sheet = Image.new('RGBA', (SHEET_WIDTH, max(2, height)), (0, 0, 0, 0))
        for img, pos in self.parts:
            sheet.paste(img, pos)
        buf = io.BytesIO()
        sheet.save(buf, format='PNG', optimize=False)
        return buf.getvalue()


def _cgg_line(i: int, x: int, y: int, sx: int, sy: int, w: int, h: int) -> str:
    return f'{i},1,{x},{y},0,0,100,0,{sx},{sy},{w},{h},0,\r\n'


def _cgs(frames: list[int], ticks: list[int]) -> bytes:
    return ''.join(f'{f},0,0,{t},\r\n' for f, t in zip(frames, ticks)).encode('ascii')


def animated(gifs: dict, totals: dict) -> tuple[bytes, bytes, dict, dict]:
    """gifs: {'idle': path, 'atk': path, 'move': path or None}; totals: {'idle'/'atk'/'move': ticks}.
    Returns (sheet png, cgg csv, {kind: cgs csv}, report)."""
    anims = {k: gif_frames(p) for k, p in gifs.items() if p}
    idle = anims['idle']
    box = _union_box(idle) or (0, 0) + idle[0][0].size
    feet = ((box[0] + box[2]) // 2, box[3])
    origins, report = {'idle': (0, 0)}, {}
    for kind in ('atk', 'move'):
        if kind not in anims:
            continue
        dx, dy, iou = register(idle[0][0], anims[kind][0][0])
        if iou < MATCH_MIN:
            ob = _union_box(anims[kind][:1]) or (0, 0) + anims[kind][0][0].size
            dx, dy = feet[0] - (ob[0] + ob[2]) // 2, feet[1] - ob[3]
            report[kind] = f'feet on feet (first-frame match {iou:.2f} < {MATCH_MIN})'
        else:
            report[kind] = f'matched to idle (IoU {iou:.2f})'
        origins[kind] = (dx, dy)
    sheet, cgg, index, cgs = Sheet(), [], 0, {}
    for kind in ('idle', 'atk', 'move'):
        if kind not in anims:
            continue
        ox, oy = origins[kind]
        frames, durations = [], []
        for img, dur in anims[kind]:
            b = img.getchannel('A').getbbox()
            if b is None:                       # an empty frame: keep its time on the previous picture
                if frames:
                    durations[-1] += dur
                continue
            part = img.crop(b)
            sx, sy = sheet.add(part)
            cgg.append(_cgg_line(index, b[0] + ox - feet[0], b[1] + oy - feet[1], sx, sy, part.width, part.height))
            frames.append(index)
            durations.append(dur)
            index += 1
        cgs[kind] = _cgs(frames, _ticks(durations, totals.get(kind, 0)))
    if 'move' not in cgs:
        idle_frames = [int(line.split(',')[0]) for line in cgs['idle'].decode().splitlines()]
        cgs['move'] = _cgs(idle_frames[:1], [max(1, int(totals.get('move') or 1))])
        report['move'] = 'no move GIF: the first idle frame'
    return sheet.png(), ''.join(cgg).encode('ascii'), cgs, report


def static(full_path, totals: dict) -> tuple[bytes, bytes, dict, dict]:
    # Client run v24 (November 2) measured it: about 9-12k visible px, against about 29k for a real 7-star sprite, so
    # the size is in range; a tighter cut around the character read as a boxed cut-out and was not kept.
    im = Image.open(full_path).convert('RGBA')
    box = im.getchannel('A').getbbox() or (0, 0) + im.size
    im = im.crop(box)
    k = min(SPRITE_MAX / im.width, SPRITE_MAX / im.height, 1.0)
    im = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), Image.LANCZOS)
    sheet = Sheet()
    sx, sy = sheet.add(im)
    cgg = _cgg_line(0, -(im.width // 2), -im.height, sx, sy, im.width, im.height).encode('ascii')
    cgs = {kind: _cgs([0], [max(1, int(totals.get(kind) or 60))]) for kind in ('idle', 'atk', 'move')}
    return sheet.png(), cgg, cgs, {'all': 'static figure from the full art'}


FULL_MIN = 600


def full_art_png(path) -> bytes:
    """The full art as installed: some wiki uploads are small (the DC collab's are about 200x300; Global's median is
    about 700x360), and the screen crops computed on them come out a few pixels wide.  Art whose longer side is
    under FULL_MIN is scaled up by the smallest whole factor that reaches it (Lanczos); the rest is kept as published."""
    raw = open(path, 'rb').read()
    im = Image.open(io.BytesIO(raw))
    k = -(-FULL_MIN // max(im.size))
    if k <= 1:
        return raw
    out = im.convert('RGBA').resize((im.width * k, im.height * k), Image.LANCZOS)
    buf = io.BytesIO()
    out.save(buf, format='PNG', optimize=False)
    return buf.getvalue()


def render(sheet_png: bytes, cgg: bytes, frame: int, canvas=(400, 300), feet=(200, 250)) -> Image.Image:
    sheet = Image.open(io.BytesIO(sheet_png)).convert('RGBA')
    out = Image.new('RGBA', canvas, (0, 0, 0, 0))
    for line in cgg.decode('ascii').splitlines():
        v = [int(x) for x in line.rstrip(',').split(',')]
        if v[0] != frame:
            continue
        for p in range(v[1]):
            x, y, _, _, _, _, sx, sy, w, h, _ = v[2 + p * 11: 13 + p * 11]
            out.alpha_composite(sheet.crop((sx, sy, sx + w, sy + h)), (feet[0] + x, feet[1] + y))
    return out
