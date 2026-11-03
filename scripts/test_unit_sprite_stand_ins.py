"""Tests for scripts/unit_sprite_stand_ins.py: the authored battle sprites for units whose part sheets do not survive.

    python scripts/test_unit_sprite_stand_ins.py

Offline, on synthetic GIFs (no wiki file needed):
  * every idle frame drawn back from the sheet + part file equals the GIF frame with its feet on the anchor;
  * an attack GIF on its own canvas is registered onto the idle pose (its first frame lands on the idle figure);
  * each animation's ticks sum to the datamine's total length for it, every step at least one tick;
  * a frame stored without transparency whose background is near-black is keyed out, a normal frame keeps its
    dark outline;
  * no cgs step names a frame the part file lacks; a missing move GIF plays the first idle frame;
  * the static stand-in is one frame, feet on the anchor, at most SPRITE_MAX px; small full art is scaled up by a whole
    factor, large art is returned byte-identical; the same input gives the same bytes (installs are idempotent).
"""
import io
import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import unit_sprite_stand_ins as s  # noqa: E402
from bf_testkit import Checker, qa_dir  # noqa: E402


def figure(size, at, color, outline=True):
    im = Image.new('RGBA', size, (0, 0, 0, 0))
    x, y = at
    a = np.asarray(im).copy()
    a[y:y + 30, x:x + 12] = color
    if outline:
        a[y:y + 30, x] = (5, 5, 5, 255)
    return Image.fromarray(a, 'RGBA')


def save_gif(frames, durations, path, transparent=True):
    pal = [f.convert('RGB').quantize(colors=250) for f in frames]
    if transparent:
        out = []
        for f in frames:
            p = f.convert('RGBA')
            bg = Image.new('RGBA', p.size, (1, 2, 3, 255))
            bg.alpha_composite(p)
            out.append(bg.convert('RGB'))
        out[0].save(path, save_all=True, append_images=out[1:], duration=durations, loop=0,
                    transparency=0, disposal=2, optimize=False)
        # re-open and rebuild with (1,2,3) as transparency
        im = Image.open(path)
        frames2 = []
        for i in range(im.n_frames):
            im.seek(i)
            rgba = im.convert('RGBA')
            arr = np.asarray(rgba).copy()
            mask = (arr[..., 0] == 1) & (arr[..., 1] == 2) & (arr[..., 2] == 3)
            arr[mask, 3] = 0
            frames2.append(Image.fromarray(arr, 'RGBA'))
        frames2[0].save(path, save_all=True, append_images=frames2[1:], duration=durations, loop=0, disposal=2)
    else:
        pal[0].save(path, save_all=True, append_images=pal[1:], duration=durations, loop=0)


def main():
    check = Checker()
    tmp = qa_dir('unit_sprite_stand_ins')
    if tmp.exists():
        shutil.rmtree(tmp)
    tmp.mkdir(parents=True)
    idle = [figure((40, 50), (10 + i, 15), (200, 40, 40, 255)) for i in range(4)]
    save_gif(idle, [200, 200, 200, 200], tmp / 'idle.gif')
    atk = [figure((120, 60), (30, 25), (200, 40, 40, 255))] + [figure((120, 60), (30 + 20 * i, 25), (60, 200, 40, 255))
                                                             for i in range(1, 5)]
    save_gif(atk, [100, 80, 80, 80, 300], tmp / 'atk.gif')
    totals = {'idle': 60, 'atk': 90, 'move': 9}
    png, cgg, cgs, report = s.animated({'idle': tmp / 'idle.gif', 'atk': tmp / 'atk.gif', 'move': None}, totals)

    frames = s.gif_frames(tmp / 'idle.gif')
    box = s._union_box(frames)
    feet = ((box[0] + box[2]) // 2, box[3])
    idle_steps = [int(line.split(',')[0]) for line in cgs['idle'].decode().splitlines()]
    bad = 0
    for n, f in enumerate(idle_steps):
        ref = Image.new('RGBA', (400, 300))
        ref.alpha_composite(frames[n][0], (200 - feet[0], 250 - feet[1]))
        if not np.array_equal(np.asarray(s.render(png, cgg, f)), np.asarray(ref)):
            bad += 1
    check('every idle frame drawn from the sheet and part file equals the GIF frame, feet on the anchor', bad == 0, bad)

    atk_steps = [int(line.split(',')[0]) for line in cgs['atk'].decode().splitlines()]
    a0 = np.asarray(s.render(png, cgg, atk_steps[0]))[..., 3] > 0
    i0 = np.asarray(s.render(png, cgg, idle_steps[0]))[..., 3] > 0
    iou = (a0 & i0).sum() / max(1, (a0 | i0).sum())
    check('the attack GIF\'s first frame lands on the idle figure (registered, not feet-on-feet)',
          iou > 0.8 and 'matched' in report.get('atk', ''), (round(float(iou), 2), report))

    sums = {k: sum(int(line.split(',')[3]) for line in v.decode().splitlines()) for k, v in cgs.items()}
    mins = {k: min(int(line.split(',')[3]) for line in v.decode().splitlines()) for k, v in cgs.items()}
    check('each animation\'s ticks sum to its datamine total, every step at least 1',
          sums == totals and min(mins.values()) >= 1, (sums, mins))

    nframes = len(cgg.decode().splitlines())
    used = {int(line.split(',')[0]) for v in cgs.values() for line in v.decode().splitlines()}
    check('no cgs step names a frame the part file lacks', max(used) < nframes, (max(used), nframes))
    check('no move GIF: the move animation is the first idle frame',
          [int(line.split(',')[0]) for line in cgs['move'].decode().splitlines()] == idle_steps[:1])

    # a frame with no transparency and a near-black background (the wiki's speed-trail frames)
    dark = Image.new('RGB', (80, 40), (10, 8, 12))
    dark.paste((250, 220, 40), (30, 10, 50, 30))
    dark.quantize(colors=16).save(tmp / 'dark.gif')
    (img, _), = s.gif_frames(tmp / 'dark.gif')
    a = np.asarray(img)
    check('a no-transparency frame whose background is near-black loses that background, keeps the bright part',
          a[0, 0, 3] == 0 and a[20, 40, 3] == 255)
    (img2, _), = s.gif_frames(tmp / 'idle.gif')[:1]
    check('a normal frame keeps its dark outline', np.asarray(img2)[20, 10, 3] == 255 and np.asarray(img2)[20, 10, 0] < 40)

    full = Image.new('RGBA', (220, 300), (0, 0, 0, 0))
    full.paste((90, 120, 200, 255), (60, 40, 160, 290))
    full.save(tmp / 'full_small.png')
    big = Image.new('RGBA', (900, 700), (0, 0, 0, 0))
    big.paste((90, 120, 200, 255), (300, 100, 600, 690))
    big.save(tmp / 'full_big.png')
    up = Image.open(io.BytesIO(s.full_art_png(tmp / 'full_small.png')))
    check('small full art is scaled up by the smallest whole factor reaching FULL_MIN; large art is untouched',
          up.size == (440, 600) and s.full_art_png(tmp / 'full_big.png') == (tmp / 'full_big.png').read_bytes(), up.size)

    spng, scgg, scgs, _ = s.static(tmp / 'full_big.png', totals)
    sheet = Image.open(io.BytesIO(spng))
    v = [int(x) for x in scgg.decode().rstrip().rstrip(',').split(',')]
    check('the static stand-in: one frame, feet on the anchor, at most SPRITE_MAX px',
          len(scgg.decode().splitlines()) == 1 and v[1] == 1 and v[3] == -v[11] and max(v[10], v[11]) <= s.SPRITE_MAX
          and sheet.width >= v[10], v)
    check('the same input gives the same bytes',
          s.animated({'idle': tmp / 'idle.gif', 'atk': tmp / 'atk.gif', 'move': None}, totals)[:3] == (png, cgg, cgs)
          and s.static(tmp / 'full_big.png', totals)[:3] == (spng, scgg, scgs))
    return check.summary('unit sprite stand-ins')


if __name__ == '__main__':
    sys.exit(main())
