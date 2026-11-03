"""Model of the summon gate's white overlay (#24): which animations are in the
client's global SuperAnimList, frame by frame, and when SummonsAnimeScene2 turns
the whole screen white.

This is a MODEL of the client, not the client.  It reproduces the rules read
out of libgame.so (arm64; the Windows client runs the same code) so that a gate
script can be judged by the lifecycle it produces instead of by its text.  A
pass here is modeled evidence only; whether the real client draws the gates is
still a player check.

The overlay (SummonsAnimeScene2::updateEvent @0x169D080):
  after ScriptEngine::process, when SuperAnimList::getCount() == 3 and no
  overlay exists yet, take getObjectAtIndex(2); if its path contains
  "Touch.sam" or "Hold.sam" do nothing, else once its GetCurFrame() >= 45
  create GraphicUtils::fillRect(...0xffffff) over the whole screen.  Its
  opacity then rises by 255/15 per frame and it is never removed by this
  scene -- it covers everything until the reveal scene replaces it.  The rule
  counts ALL animations in the global list, not only the gate's.  This is
  rule='stock'.

  rule='state' is the replacement decision of the proxy DLL's gate flash fix
  (offline-proxy OfflineMod.win32/content/cp_gate_flash_logic.cpp, x86 patch
  at 0x8760EF): no overlay yet, and an enabled animation whose path contains
  "GachaGateOpen" -- the Open script's gate and its blend, the only animations
  drawn when the final gate opens -- that was seen BELOW frame 45 in this
  scene has reached frame 45.  Nothing else in the list matters; an opening
  animation first seen at or past frame 45 (a leftover) can never fire, and a
  frame with no opening animation forgets what was armed.  The overlay, its
  fade and everything after are the game's own code under both rules.

The list (SuperAnimList, 0xFE9360..):
  * addAnime / addAnime2 append the gate, then its additive "blend" layer as a
    second entry (0xFE9D58 / 0xFEB2E4).
  * id=43 (advParam -> 0xFCB364) setEnable(false)s the named gate, which
    propagates to its blend (SuperAnimEx::setEnable 0xFF5BDC); id=20
    clearScreen (0xFCD590) does the same for the id=21 touch prompts only.
    Disabled entries leave the list at the next SuperAnimList::process
    (0xFEA128), which GameScene::update (0x1601548) and BaseScene::draw
    (0xFFF1C0) both call -- removal is DEFERRED, never inside the script step.
  * every tap appends ripple.sam (BaseScene::startTouchEffect 0x10012D0, after
    synchronously removing earlier ripples); it plays start/delete (7 frames)
    and auto-removes (SuperAnimEx::process 0xFF56FC); BaseScene::draw also
    drops finished ripples each frame (0xFFF188).
  * other screens' animations can already be in the list when the gates start
    ("baseline"): sceneFadeOut (0xFFF1F4) purges non-character animations
    unless LoadingSamUtil::isConnectPop(), and ConnectScene sets that flag
    right before returning to GachaActionScene (0x1B5DB64) -- i.e. on every
    summon.  Known members: the header's StaminaBarOverflowAdd.sam while energy
    is above max (GameScene::updateHeader 0x1604E2C, setHeaderControl
    0x1627F20) and downloading_icon.sam while downloads are pending
    (GameScene::onSceneVisible 0x16008C8, removed by GameScene::update
    0x16019D4).  The model takes the baseline as a parameter.

The script (SummonsAnimeScene2::initialize 0x169C3F4 appends effect_before,
effect and effect_after into ONE script for ScriptEngine::initEngine):
  * commands are the text between "*" and "#" (AdventureSystem::advSet
    0xFD4198), so bytes between commands -- a leading BOM -- are skipped;
  * PLAYWAIT x waits x*60 script steps (AdventureSystem::advPlayWait 0xFD79A8);
  * STOP (id -1) waits until every id=42 animation drawn since the last STOP
    either has a "loop" section or has finished (ScriptEngine::advStopStart
    0xFCE75C, isFinishedCmd 0xFCE49C, SuperAnimEx::isLoop 0xFF58E0);
  * MSGWAIT waits for a tap; a tap anywhere else is ignored by the script
    (AdventureSystem::advTouch 0xFD7D24 needs an open message).
Animations advance at their own 24 fps; a section that ends moves start ->
loop (if any) -> delete (if any) -> finished (SuperAnimEx::OnAnimSectionEnd
0xFF5C0C); a looping animation keeps looping.
"""
from __future__ import annotations

import math
import re
import struct
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONTENT = ROOT / 'deploy' / 'game_content' / 'content'
APPX_RESOURCES = ROOT.parent / 'BraveFrontierAppxClient' / 'Assets' / 'Resources'

WHITE_FRAME = 45            # cmp w0, #0x2d @0x169D378
WHITE_COUNT = 3             # cmp w0, #3 @0x169D144
WHITE_INDEX = 2             # getObjectAtIndex(2) @0x169D20C
EXCLUDED = ('Touch.sam', 'Hold.sam')
OPENING_MARKER = 'gachagateopen'   # rule='state': GachaGateOpen2..6, GachaGateOpenAdd, GachaGateOpen6Add
ARMING = True                      # rule='state': only openings seen below WHITE_FRAME may fire (tests mutate it)
RIPPLE = 'ripple.sam'
ANIM_FPS = 24               # every animation involved has rate 24

# Sections of every animation the gate scripts and the known baseline members
# draw, as read from the .sam files (frame-name labels; parse_sam_sections
# below re-derives them and the regression compares).  name -> [(section,
# first frame, last frame)].
_LOOPING_GATE = [('start', 0, 19), ('loop', 20, 63), ('delete', 64, 64)]
_CHANGE = [('start', 0, 49), ('delete', 50, 50)]
_OPEN = [('start', 0, 59), ('delete', 60, 60)]
SECTIONS = {
    'Touch.sam': [('start', 0, 0), ('loop', 1, 22), ('delete', 23, 32)],
    'Hold.sam': [('start', 0, 0), ('loop', 1, 22), ('delete', 23, 32)],
    'GachaGateClose2.sam': _LOOPING_GATE,
    'GachaGateClose3.sam': _LOOPING_GATE,
    'GachaGateClose4.sam': _LOOPING_GATE,
    'GachaGateClose5.sam': _LOOPING_GATE,
    'GachaGateClose6.sam': [('start', 0, 19), ('loop', 20, 112), ('delete', 113, 113)],
    'GachaGateClose6Add.sam': [('start', 0, 19), ('loop', 20, 112), ('delete', 113, 113)],
    'GachaGateChange3_4.sam': _CHANGE,
    'GachaGateChange3_5.sam': _CHANGE,
    'GachaGateChange4_5.sam': _CHANGE,
    'GachaGateChange5_6.sam': _CHANGE,
    'GachaGateOpen2.sam': _OPEN,
    'GachaGateOpen3.sam': _OPEN,
    'GachaGateOpen4.sam': _OPEN,
    'GachaGateOpen5.sam': _OPEN,
    'GachaGateOpen6.sam': _OPEN,
    'GachaGateOpen6Add.sam': _OPEN,
    'GachaGateOpenAdd.sam': [('start', 0, 78), ('delete', 79, 79)],
    'ripple.sam': [('start', 0, 4), ('delete', 5, 6)],
    'StaminaBarOverflowAdd.sam': [('start', 0, 0), ('loop', 1, 120), ('delete', 121, 121)],
    'downloading_icon.sam': [('start', 0, 0), ('loop', 1, 18), ('delete', 19, 20)],
}
BASELINE_KINDS = {'overflow': 'StaminaBarOverflowAdd.sam', 'download': 'downloading_icon.sam'}


# ---------------------------------------------------------------- .sam sections

def parse_sam_sections(data: bytes) -> list[tuple[str, int, int]]:
    """Walk a version-1 SuperAnim definition to EOF and return its sections.

    Header, image table and frame stream as SuperAnimCore's
    LoadSuperAnimMainDef reads them (the client's copy, arm64 0xFF0228 region,
    i16 header layout); sections are the frame-name labels, each running to
    the frame before the next.  Raises ValueError unless the walk ends exactly
    at EOF, so a misread cannot pass silently."""
    pos = 0

    def take(fmt):
        nonlocal pos
        v = struct.unpack_from(fmt, data, pos)
        pos += struct.calcsize(fmt)
        return v if len(v) > 1 else v[0]

    def string():
        nonlocal pos
        n = take('<H')
        s = data[pos:pos + n]
        pos += n
        return s.decode('latin-1')

    if data[:4] != b'MAS.':
        raise ValueError('not a SuperAnim definition')
    pos = 4
    if take('<i') != 1:
        raise ValueError('version')
    take('<B')
    take('<4h')
    for _ in range(take('<H')):
        string()
        take('<HH')
        take('<4i')
        take('<2h')
    frames = take('<H')
    labels = []
    for index in range(frames):
        flags = take('<B')
        if flags & 1:                                   # removes
            n = take('<B')
            n = take('<H') if n == 255 else n
            for _ in range(n):
                if take('<H') >= 2047:
                    take('<i')
        if flags & 2:                                   # adds
            n = take('<B')
            n = take('<H') if n == 255 else n
            for _ in range(n):
                if take('<H') & 0x07FF == 2047:
                    take('<i')
                take('<B')
        if flags & 4:                                   # moves
            n = take('<B')
            n = take('<H') if n == 255 else n
            for _ in range(n):
                fo = take('<H')
                if fo & 0x03FF == 1023:
                    take('<i')
                if fo & 0x1000:
                    take('<4i')
                elif fo & 0x4000:
                    take('<h')
                take('<2i' if fo & 0x0800 else '<2h')
                if fo & 0x8000:
                    take('<4h')
                if fo & 0x2000:
                    take('<4B')
        if flags & 8:                                   # frame name
            labels.append((string(), index))
    if pos != len(data):
        raise ValueError(f'{len(data) - pos} trailing bytes')
    return [(name, start, labels[i + 1][1] - 1 if i + 1 < len(labels) else frames - 1)
            for i, (name, start) in enumerate(labels)]


# ----------------------------------------------------------------- scripts

def commands(text: str) -> list[str]:
    """The script's commands: the text between each '*' and the next '#'."""
    return re.findall(r'\*([^#]*)#', text.replace('﻿', ''))


@dataclass
class Command:
    kind: str                 # PARAM, PLAYWAIT, STOP, MSGDRAW, MSGWAIT
    op: int | None
    param: str
    part: str                 # wait / change / open


def parse(text: str, part: str) -> list[Command]:
    out = []
    for raw in commands(text):
        fields = dict(f.split('=', 1) for f in raw.split(',') if '=' in f)
        kind = fields.get('type', '')
        op = int(fields['id']) if kind == 'PARAM' and fields.get('id', '').lstrip('-').isdigit() else None
        out.append(Command(kind, op, fields.get('param', ''), part))
    return out


# ------------------------------------------------------------------- model

@dataclass(eq=False)
class Anim:
    path: str
    sections: list
    blend: 'Anim | None' = None
    auto_remove: bool = False
    tag: int = 0
    enabled: bool = True
    finished: bool = False
    section: int = 0
    t: float = 0.0            # seconds into the current section

    @property
    def name(self) -> str:
        return Path(self.path).name

    def has(self, section: str) -> bool:
        return any(s[0] == section for s in self.sections)

    def frame(self) -> int:
        _name, first, last = self.sections[self.section]
        return min(first + int(self.t * ANIM_FPS + 1e-9), last)

    def advance(self, dt: float) -> None:
        if self.finished and not self.has('loop'):
            return
        self.t += dt
        while True:
            name, first, last = self.sections[self.section]
            length = (last - first + 1) / ANIM_FPS
            if self.t < length - 1e-9:
                return
            self.t -= length
            nxt = self._after(name)
            if nxt is None:
                self.t = length                          # rests on its last frame
                return
            self.section = nxt

    def _after(self, name: str) -> int | None:
        index = {s[0]: i for i, s in enumerate(self.sections)}
        if name == 'start':
            if 'loop' in index:
                return index['loop']
            if 'delete' in index:
                return index['delete']
            self.finished = True
            return None
        if name == 'loop':
            self.finished = True                         # set at every loop end, then loops again
            return index['loop']
        self.finished = True                             # delete (or anything else) ends it
        return None


def sections_for(name: str) -> list:
    if name in SECTIONS:
        return SECTIONS[name]
    raise KeyError(f'no section table for {name}; add it to SECTIONS (read it with parse_sam_sections)')


@dataclass
class Result:
    white_at: float | None = None
    white_part: str | None = None
    white_anim: str | None = None
    white_frame: int | None = None      # the deciding animation's frame when the overlay was created
    open_drawn_at: float | None = None  # first opening-gate animation appended (the final gate starts opening)
    stock_white_at: float | None = None # rule='state' runs: when the stock rule would have fired (comparison only)
    stock_white_part: str | None = None
    reveal_at: float | None = None
    parts_seen: list = field(default_factory=list)
    trace: list = field(default_factory=list)

    @property
    def premature(self) -> bool:
        """White before the Open part started: it hides a gate the player has not tapped through."""
        return self.white_part is not None and self.white_part != 'open'


def simulate(scripts: list[tuple[str, str]], *, baseline: list = (), fps: float = 60.0,
             taps: str = 'normal', ripple: bool = True, icon_gone_at: float | None = None,
             tap_delay: float = 0.6, limit: float = 30.0, keep_trace: bool = False,
             rule: str = 'stock') -> Result:
    """Play one summon: `scripts` = [(part, text)] in Wait / Change / Open order.

    baseline: animation names already in the list when the gates start (e.g.
    ['StaminaBarOverflowAdd.sam', 'downloading_icon.sam']).  taps: 'normal'
    taps `tap_delay` s after each tap prompt appears; 'rapid' taps every 0.15 s
    from the start; 'late' waits 4 s at each prompt.  icon_gone_at: seconds at
    which a baseline downloading_icon.sam is removed (downloads finished).
    A baseline entry may be (name, seconds): that animation has already played
    for `seconds` (a leftover resting on its last frame, a loop mid-cycle).
    rule: 'stock' (the client's count rule) or 'state' (the DLL's gate flash
    fix); see the module docstring."""
    if rule not in ('stock', 'state'):
        raise ValueError(rule)
    program = [c for part, text in scripts for c in parse(text, part)]
    dt = 1.0 / fps
    alist: list[Anim] = []
    for entry in baseline:
        name, played = (entry, 0.0) if isinstance(entry, str) else entry
        a = Anim(name, sections_for(name))
        a.advance(played)
        alist.append(a)
    armed: list[Anim] = []
    named: dict[str, Anim] = {}
    touch_list: list[Anim] = []
    since_stop: list[Anim] = []
    res = Result()
    ip = 0
    wait_left = None            # PLAYWAIT counter
    msg_since = None            # time the current MSGWAIT started
    tapped = False
    t = 0.0
    next_rapid = 0.0

    def append(name: str, blend: str | None = None) -> Anim:
        main = Anim(name, sections_for(name))
        alist.append(main)
        if blend:
            b = Anim(blend, sections_for(blend))
            main.blend = b
            alist.append(b)
        return main

    def disable(a: Anim) -> None:
        while a is not None:
            a.enabled = False
            a = a.blend

    def process_list() -> None:
        for a in alist:
            if a.enabled and a.finished and a.auto_remove and not a.has('loop'):
                a.enabled = False
        alist[:] = [a for a in alist if a.enabled]

    def run_script() -> None:
        nonlocal ip, wait_left, msg_since, tapped
        while ip < len(program):
            c = program[ip]
            if c.part not in res.parts_seen:
                res.parts_seen.append(c.part)
            if c.kind == 'PLAYWAIT':
                if wait_left is None:
                    wait_left = int(round(float(c.param) * 60))
                    return
                wait_left -= 1
                if wait_left > 0:
                    return
                wait_left = None
            elif c.kind == 'STOP':
                if not all(a.has('loop') or a.finished for a in since_stop):
                    return
                since_stop.clear()
            elif c.kind == 'MSGWAIT':
                if msg_since is None:
                    msg_since = t
                    tapped = False
                if not tapped:
                    return
                msg_since = None
                tapped = False
            elif c.kind == 'PARAM' and c.op == 42:
                name, _x, _y, a_sam, b_sam = (c.param.split(':') + ['', '', '', '', ''])[:5]
                anim = append(a_sam, None if b_sam in ('', 'none') else b_sam)
                named[name] = anim                       # replaces the mapping, never disables the old one
                if res.open_drawn_at is None and OPENING_MARKER in a_sam.lower():
                    res.open_drawn_at = round(t, 3)
                since_stop.append(anim)
            elif c.kind == 'PARAM' and c.op == 43:
                anim = named.pop(c.param, None)
                if anim is not None:
                    disable(anim)
            elif c.kind == 'PARAM' and c.op == 21:
                parts = c.param.split(':')
                anim = append('Hold.sam' if len(parts) > 2 and parts[2] not in ('', '0') else 'Touch.sam')
                touch_list.append(anim)
            elif c.kind == 'PARAM' and c.op == 20:
                for anim in touch_list:
                    disable(anim)
                touch_list.clear()
            ip += 1

    while t < limit:
        # input
        do_tap = False
        if taps == 'rapid':
            if t >= next_rapid:
                do_tap = True
                next_rapid = t + 0.15
        elif msg_since is not None:
            delay = 4.0 if taps == 'late' else tap_delay
            do_tap = t - msg_since >= delay - 1e-9 and not tapped
        if do_tap:
            if ripple:
                for a in alist:
                    if a.name == RIPPLE:
                        a.enabled = False
                alist[:] = [a for a in alist if a.enabled]
                r = Anim(RIPPLE, sections_for(RIPPLE), auto_remove=True, tag=-1000000)
                alist.append(r)
            if msg_since is not None:
                tapped = True
        if icon_gone_at is not None and t >= icon_gone_at:
            alist[:] = [a for a in alist if a.name != 'downloading_icon.sam']
            icon_gone_at = None
        # GameScene::update
        process_list()
        # SummonsAnimeScene2::updateEvent
        if ip >= len(program) and wait_left is None:
            res.reveal_at = round(t, 3)
            break
        run_script()
        part = program[min(ip, len(program) - 1)].part
        stock = None
        if len(alist) == WHITE_COUNT:
            cand = alist[WHITE_INDEX]
            if not any(x in cand.path for x in EXCLUDED) and cand.frame() >= WHITE_FRAME:
                stock = cand
        if rule == 'stock':
            if res.white_at is None and stock is not None:
                res.white_at, res.white_part, res.white_anim = round(t, 3), part, stock.name
                res.white_frame = stock.frame()
        elif res.white_at is None:
            if stock is not None and res.stock_white_at is None:
                res.stock_white_at, res.stock_white_part = round(t, 3), part
            fire = None
            opening = [a for a in alist if a.enabled and OPENING_MARKER in a.name.lower()]
            for a in opening:
                armed_now = any(a is b for b in armed)
                if a.frame() < WHITE_FRAME:
                    if not armed_now:
                        armed.append(a)
                elif (armed_now or not ARMING) and fire is None:
                    fire = a
            if fire is not None:
                res.white_at, res.white_part, res.white_anim = round(t, 3), part, fire.name
                res.white_frame = fire.frame()
                armed.clear()
            elif not opening:
                armed.clear()
        if keep_trace:
            res.trace.append((round(t, 3), program[min(ip, len(program) - 1)].part,
                              [(a.name, a.frame()) for a in alist]))
        # BaseScene::draw
        for a in alist:
            if a.name == RIPPLE and a.finished:
                a.enabled = False
        process_list()
        # node updates (the animations' own clocks)
        for a in alist:
            a.advance(dt)
        t += dt
    return res


# ------------------------------------------------------------ gate effects

def load_effects(mst: Path = ROOT / 'deploy' / 'mst' / 'gacha_effect_mst.json') -> list[dict]:
    import json
    rows = next(iter(json.loads(mst.read_text(encoding='utf-8')).values()))
    return [{'id': int(r['u0vkt9yH']), 'rarity': int(r['7ofj5xa1']), 'weight': int(r['ug9xV4Fz']),
             'wait': r['7ZNcmYS2'], 'change': r['YTx3c1jQ'], 'open': r['tj0i9JhC']} for r in rows]


def effect_scripts(effect: dict, folder: Path) -> list[tuple[str, str]]:
    out = []
    for part in ('wait', 'change', 'open'):
        if effect[part]:
            out.append((part, (folder / effect[part]).read_text(encoding='utf-8-sig')))
    return out


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description='Play every summon effect through the overlay model.')
    ap.add_argument('--scripts', default=str(CONTENT / 'gacha'), help='folder holding the gate scripts')
    ap.add_argument('--baseline', default='', help='comma list of kinds: overflow, download (repeat for more)')
    ap.add_argument('--taps', default='normal', choices=('normal', 'rapid', 'late'))
    ap.add_argument('--fps', type=float, default=60.0)
    ap.add_argument('--rule', default='stock', choices=('stock', 'state'),
                    help="stock: the client's count rule; state: the DLL's gate flash fix")
    args = ap.parse_args()
    base = [BASELINE_KINDS[k] for k in args.baseline.split(',') if k]
    for eff in load_effects():
        r = simulate(effect_scripts(eff, Path(args.scripts)), baseline=base, fps=args.fps, taps=args.taps,
                     rule=args.rule)
        kind = 'double' if eff['change'] else 'single'
        white = 'none' if r.white_at is None else f'{r.white_at:.2f}s in {r.white_part} ({r.white_anim} frame {r.white_frame})'
        stock = ''
        if args.rule == 'state':
            stock = '; stock rule ' + ('never' if r.stock_white_at is None else f'{r.stock_white_at:.2f}s in {r.stock_white_part}')
        print(f"effect {eff['id']:>3} rarity {eff['rarity']} {kind:6}: white {white}; "
              f"opening drawn {r.open_drawn_at}; reveal {r.reveal_at}{stock}")
