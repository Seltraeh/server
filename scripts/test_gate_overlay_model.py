"""#24 double-gate whiteout: the gate scripts judged by the overlay lifecycle
they produce in scripts/gate_overlay_model.py, not by their text.

    python scripts/test_gate_overlay_model.py [--scripts CANDIDATE_DIR]

MODELED evidence only (no server, no client): the model encodes the client's
own rule (SummonsAnimeScene2::updateEvent @0x169D080 arm64 / 0x875F20 x86) and
list lifecycle; whether the real client draws the gates is the player's check.

A. Model fidelity: the section table equals what parse_sam_sections reads from
   every .sam the gate scripts and baseline members use.
B. Negative controls -- the fault reproduces: with the current served scripts
   (deploy/game_content, Session 9 patch) and two other animations in the
   client's list, every DOUBLE gate whites out during its second (Change) gate
   and no single gate does; with one, every gate flashes during its Open (the
   designed transition); with none, nothing whitens.  The stock scripts
   (reconstructed by inverting tools/patch_gacha_scripts.py, checked against
   the four stock files the app package ships) fail the same way, so the fault
   predates that patch.
C. The guarded scripts (scripts/gen_gate_scripts.py, generated here into a
   temporary folder; --scripts also checks a candidate folder) never whiten
   before the Open part, for 0-4 extra animations, normal / rapid / late taps,
   60 and 30 fps, with and without the tap ripple, and with a download icon
   leaving mid-summon.
D. Under the stock rule the guard changes nothing else: with one extra
   animation every effect flashes during its Open exactly as the current
   scripts do; Open scripts are byte for byte the current ones.  This is an
   EQUIVALENCE check of the Session 14 guard (the fallback while the DLL fix
   is absent), NOT the acceptance criterion: the stock rule itself never
   whitens with 0 or 2+ other animations -- the missing final white the player
   saw -- and G is what the fix must satisfy.
E. Mutations are caught: no guard, a guard drawn BEFORE the gate, a guard never
   removed -- each makes a check in C or D fail.
F. Generator: idempotent, refuses deploy/ and client packages, valid shape.
G. The gate flash fix (rule='state', the proxy DLL's replacement decision,
   offline-proxy cp_gate_flash_logic.cpp) on the served, the stock and the
   guarded scripts, for every effect, 0-4 other animations, unrelated looping
   animations, leftover opening animations, normal / rapid / late taps, 30 /
   60 / 144 fps, ripple on/off and a download icon leaving mid-summon:
     POSITIVE  every summon whitens, during its Open part, at frame 45 of the
               final gate's opening (45/24 s after it is drawn), before the
               reveal;
     NEGATIVE  no summon whitens before its Open part -- never over a gate
               still waiting for its tap.
   Where the stock rule already worked (exactly one other animation: the 5-star
   case the player accepted) the white comes on the same frame.  The stock
   rule fails both halves under the player's conditions (negative control),
   and three mutations of the fix (no arming, any gate, blend-only marker) are
   caught.
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gate_overlay_model as m  # noqa: E402
import gen_gate_scripts as g  # noqa: E402
from bf_testkit import Checker  # noqa: E402

CURRENT = m.CONTENT / 'gacha'
OVERFLOW = 'StaminaBarOverflowAdd.sam'
ICON = 'downloading_icon.sam'
BASELINES = {0: [], 1: [OVERFLOW], 2: [OVERFLOW, OVERFLOW], '2b': [OVERFLOW, ICON],
             3: [OVERFLOW, OVERFLOW, ICON], 4: [OVERFLOW] * 4}


def play(folder: Path, base, **kw) -> dict[int, m.Result]:
    """Every effect through the model; kw goes to simulate (rule='state' for the DLL fix)."""
    return {e['id']: m.simulate(m.effect_scripts(e, folder), baseline=base, **kw) for e in m.load_effects()}


def doubles() -> set[int]:
    return {e['id'] for e in m.load_effects() if e['change']}


def invert_patch(name: str, text: str) -> str:
    """The stock script tools/patch_gacha_scripts.py started from (its inverse)."""
    lines = text.replace('\ufeff', '').replace('\r\n', '\n').split('\n')
    if name.startswith('gOpen'):
        out = [re.sub(r'(id=46,param=)light\d+:', r'\1light:', l) for l in lines]
        return '\n'.join(out)
    drop = set(g.LEFTOVERS) | {g.GUARD_DRAW, g.GUARD_REMOVE, '*,type=PARAM,id=26,param=0,#',
                               '*,type=PARAM,id=18,param=all,#', g.PROMPT_REMOVE}
    out = []
    for l in lines:
        if l in drop:
            continue
        mt = re.match(r'^\*,type=PARAM,id=42,param=touch_prompt:([0-9.-]+):([0-9.-]+):Touch\.sam:none,#$', l)
        if mt:
            out.append(f'*,type=PARAM,id=21,param={mt.group(1)}:{mt.group(2)},#')
            continue
        out.append(l)
        if 'type=MSGWAIT' in l:
            out.append('*,type=PARAM,id=20,param=0,#')
    return '\n'.join(out)


def write_variant(src: Path, dst: Path, transform) -> Path:
    dst.mkdir(parents=True, exist_ok=True)
    for p in sorted(src.glob('g*.txt')):
        (dst / p.name).write_text(transform(p.name, p.read_text(encoding='utf-8-sig')), encoding='utf-8')
    return dst


def matrix(folder: Path):
    """Every (condition, effect) whose overlay appears before the Open part."""
    bad = []
    for b, base in BASELINES.items():
        for taps in ('normal', 'rapid', 'late'):
            for fps in (60.0, 30.0):
                for ripple in (True, False):
                    for eid, r in play(folder, base, taps=taps, fps=fps, ripple=ripple).items():
                        if r.premature:
                            bad.append((b, taps, fps, ripple, eid, r.white_part, r.white_anim))
    for eid, r in play(folder, [OVERFLOW, OVERFLOW, ICON], icon_gone_at=2.5).items():
        if r.premature:
            bad.append(('icon leaves at 2.5 s', eid, r.white_part))
    return bad


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--scripts', type=Path, help='also judge this candidate folder of gate scripts')
    args = ap.parse_args()
    check = Checker()
    if not CURRENT.is_dir():
        print(f'SKIP: no served gate scripts at {CURRENT}')
        return check.summary('test_gate_overlay_model')
    effects = m.load_effects()
    double_ids = doubles()

    # ---- A. fidelity --------------------------------------------------------------
    # Resolve once: recursively walking game_content for every section name
    # makes this model test spend minutes repeating the same directory scan.
    section_files = {}
    wanted = {name.lower() for name in m.SECTIONS}
    for path in m.CONTENT.glob('**/*.sam'):
        if path.name.lower() in wanted:
            section_files.setdefault(path.name.lower(), path)
    for name, table in sorted(m.SECTIONS.items()):
        source = section_files.get(name.lower())
        candidates = ([source] if source else []) + ([m.APPX_RESOURCES / name] if (m.APPX_RESOURCES / name).is_file() else [])
        if not candidates:
            print(f'SKIP  A: {name}: no copy on this machine')
            continue
        got = m.parse_sam_sections(candidates[0].read_bytes())
        check(f'A: {name} sections as the model has them', got == table, (got, table))
    used = set()
    for e in effects:
        for _part, text in m.effect_scripts(e, CURRENT):
            for c in m.parse(text, ''):
                if c.op in (42, 21) and c.kind == 'PARAM':
                    used.update(p for p in c.param.split(':')[3:5] if p.endswith('.sam'))
    check('A: every animation the served scripts draw is in the model', used <= set(m.SECTIONS), sorted(used - set(m.SECTIONS)))

    # ---- B. negative controls ------------------------------------------------------
    for label, base in (('two other animations (2x energy-overflow glow)', BASELINES[2]),
                        ('two other animations (overflow glow + download icon)', BASELINES['2b'])):
        res = play(CURRENT, base)
        whiteout = {eid for eid, r in res.items() if r.premature}
        check(f'B: current scripts, {label}: every double gate whites out', whiteout == double_ids,
              sorted(whiteout ^ double_ids))
        check(f'B: current scripts, {label}: it happens during the second (Change) gate',
              all(res[eid].white_part == 'change' for eid in double_ids),
              {eid: res[eid].white_part for eid in double_ids})
        check(f'B: current scripts, {label}: single gates never whiten',
              all(res[e['id']].white_at is None for e in effects if not e['change']))
    res = play(CURRENT, BASELINES[1])
    check('B: current scripts, one other animation: every gate flashes during its Open (designed)',
          all(r.white_part == 'open' for r in res.values()), {k: r.white_part for k, r in res.items()})
    res = play(CURRENT, [])
    check('B: current scripts, no other animation: nothing whitens', all(r.white_at is None for r in res.values()))

    with tempfile.TemporaryDirectory(prefix='gate_overlay_') as tmp:
        tmp = Path(tmp)
        stock = write_variant(CURRENT, tmp / 'stock', invert_patch)
        for name in ('gWaitGold.txt', 'gWaitRed.txt', 'gOpenGold(R).txt', 'gOpenRed(SR).txt'):
            shipped = m.APPX_RESOURCES / name
            if shipped.is_file():
                want = [c.strip() for c in m.commands(shipped.read_text(encoding='utf-8-sig'))]
                have = [c.strip() for c in m.commands((stock / name).read_text(encoding='utf-8'))]
                check(f'B: the reconstructed stock {name} equals the app package copy', want == have, (want, have))
        res = play(stock, BASELINES[2])
        whiteout = {eid for eid, r in res.items() if r.premature}
        check('B: stock scripts, two other animations: every double gate whites out (predates the patch)',
              whiteout == double_ids, sorted(whiteout ^ double_ids))

        # ---- C/D. the guarded scripts ----------------------------------------------
        guarded = tmp / 'guarded'
        g.generate(CURRENT, guarded)
        folders = [('generated', guarded)] + ([('candidate', args.scripts)] if args.scripts else [])
        for label, folder in folders:
            check(f'C: {label}: shape', not g.check(folder), g.check(folder))
            bad = matrix(folder)
            check(f'C: {label}: no whiteout before the Open in any modeled condition '
                  f'({len(BASELINES)} baselines x 3 tap patterns x 2 fps x ripple on/off, plus a leaving icon)',
                  not bad, bad[:8])
            res = play(folder, BASELINES[1])
            check(f'D: {label}: stock rule, one other animation -- every effect still flashes during its Open',
                  all(r.white_part == 'open' for r in res.values()), {k: r.white_part for k, r in res.items()})
            current_flash = play(CURRENT, BASELINES[1])
            check(f'D: {label}: stock rule -- the guard does not move the flash (equivalence, not acceptance)',
                  all(abs(res[k].white_at - current_flash[k].white_at) < 0.05 for k in res),
                  {k: (res[k].white_at, current_flash[k].white_at) for k in res})
            res = play(folder, [])
            check(f'D: {label}: stock rule, no other animation -- no white either (the known stock defect G fixes)',
                  all(r.white_at is None for r in res.values()))
            same = [p.name for p in sorted(CURRENT.glob('gOpen*.txt'))
                    if (folder / p.name).read_bytes() == p.read_bytes()]
            check(f'D: {label}: every Open script is byte for byte the current one', len(same) == 8, same)

        # ---- E. mutations ------------------------------------------------------------
        def no_guard(name, text):
            return '\n'.join(l for l in text.split('\n') if l != g.GUARD_DRAW)

        def guard_first(name, text):
            lines = [l for l in text.split('\n') if l != g.GUARD_DRAW]
            k = next(i for i, l in enumerate(lines) if g.GATE_DRAW.match(l))
            return '\n'.join(lines[:k] + [g.GUARD_DRAW] + lines[k:])

        def never_removed(name, text):
            lines = text.split('\n')
            k = max(i for i, l in enumerate(lines) if l == g.GUARD_REMOVE)
            return '\n'.join(lines[:k] + lines[k + 1:])

        for label, fn in (('no guard', no_guard), ('guard drawn before the gate', guard_first),
                          ('guard never removed', never_removed)):
            folder = write_variant(guarded, tmp / re.sub(r'\W+', '_', label),
                                   lambda n, t, fn=fn: fn(n, t) if g.is_guarded_kind(n) else t)
            # Open scripts must stay byte-identical to the guarded set's (the variant writer re-encodes).
            for p in guarded.glob('gOpen*.txt'):
                shutil.copyfile(p, folder / p.name)
            caught = []
            if matrix(folder):
                caught.append('C whiteout')
            res = play(folder, BASELINES[1])
            if not all(r.white_part == 'open' for r in res.values()):
                caught.append('D flash')
            if g.check(folder):
                caught.append('shape')
            check(f'E: mutation "{label}" is caught', any(c in caught for c in ('C whiteout', 'D flash')), caught)

        # ---- F. generator ------------------------------------------------------------
        before = {p.name: p.read_bytes() for p in guarded.glob('g*.txt')}
        g.generate(CURRENT, guarded)
        check('F: regenerating is byte-identical', before == {p.name: p.read_bytes() for p in guarded.glob('g*.txt')})
        again = tmp / 'again'
        g.generate(guarded, again)
        check('F: guarding an already guarded folder changes nothing',
              all((again / n).read_bytes() == b for n, b in before.items()))
        for target in (m.ROOT / 'deploy' / 'game_content' / 'content' / 'gacha_test',
                       Path(__import__('os').environ.get('LOCALAPPDATA', 'C:/x')) / 'Packages' / 'x'):
            try:
                g.generate(CURRENT, target)
                refused = False
            except SystemExit:
                refused = True
            check(f'F: refuses to write {target}', refused and not target.exists())
        try:
            g.guard('\ufeff' + (CURRENT / 'gWaitGold.txt').read_text(encoding='utf-8-sig'))
            bom = False
        except ValueError:
            bom = True
        check('F: a byte-order mark in a Wait/Change script is refused', bom)

        # ---- G. the gate flash fix (rule='state') ----------------------------------------
        state_checks(check, [('served', CURRENT), ('stock', stock), ('guarded', guarded)]
                     + ([('candidate', args.scripts)] if args.scripts else []), effects, double_ids)
    return check.summary('test_gate_overlay_model')


UNRELATED = 'unrelated_loop.sam'     # a synthetic looping animation on another screen
LEFTOVERS = [('GachaGateOpen6Add.sam', 10.0), ('GachaGateOpenAdd.sam', 10.0), ('GachaGateOpen5.sam', 10.0)]
STATE_BASELINES = dict(BASELINES)
STATE_BASELINES.update({
    'unrelated x5': [UNRELATED] * 5,
    'unrelated + overflow + icon': [(UNRELATED, 0.4), OVERFLOW, ICON],
    'leftover openings': LEFTOVERS,
    'leftovers + overflow + icon': LEFTOVERS + [OVERFLOW, ICON],
})
OPEN_DELAY = m.WHITE_FRAME / m.ANIM_FPS


def state_violations(res: dict[int, m.Result], fps: float) -> list:
    """(effect, what) for every summon that breaks the POSITIVE or NEGATIVE half of G."""
    bad = []
    for eid, r in res.items():
        if r.white_at is None:
            bad.append((eid, 'no white'))
        elif r.white_part != 'open':
            bad.append((eid, f'white in {r.white_part}'))
        elif r.open_drawn_at is None or not (OPEN_DELAY - 1e-6 <= r.white_at - r.open_drawn_at
                                             <= OPEN_DELAY + 1.0 / fps + 1e-6):
            bad.append((eid, f'white {r.white_at} vs opening drawn {r.open_drawn_at}'))
        elif r.white_frame != m.WHITE_FRAME:
            bad.append((eid, f'white at frame {r.white_frame}'))
        elif r.reveal_at is None or r.white_at >= r.reveal_at:
            bad.append((eid, f'white {r.white_at} not before reveal {r.reveal_at}'))
    return bad


QUICK_BASELINES = (0, 1, '2b', 'leftovers + overflow + icon')


def state_matrix(folder: Path, rule: str = 'state', full: bool = True) -> tuple[int, list]:
    """full: every baseline x normal/rapid/late x 30/60/144 fps x ripple on/off; else a quick
    subset (4 baselines x normal/rapid at 60 fps) for the negative control and the mutations."""
    runs, bad = 0, []
    baselines = STATE_BASELINES if full else {k: STATE_BASELINES[k] for k in QUICK_BASELINES}
    for b, base in baselines.items():
        for taps in (('normal', 'rapid', 'late') if full else ('normal', 'rapid')):
            for fps in ((60.0, 30.0, 144.0) if full else (60.0,)):
                for ripple in ((True, False) if full else (True,)):
                    res = play(folder, base, taps=taps, fps=fps, ripple=ripple, rule=rule)
                    runs += len(res)
                    bad += [(b, taps, fps, ripple) + v for v in state_violations(res, fps)]
    res = play(folder, [OVERFLOW, OVERFLOW, ICON], icon_gone_at=2.5, rule=rule)
    runs += len(res)
    bad += [('icon leaves at 2.5 s',) + v for v in state_violations(res, 60.0)]
    return runs, bad


def state_checks(check, folders, effects, double_ids) -> None:
    m.SECTIONS[UNRELATED] = [('start', 0, 0), ('loop', 1, 40), ('delete', 41, 41)]
    try:
        for label, folder in folders:
            full = label == 'served'      # the scripts the client has cached and the C3 policy leaves to it
            runs, bad = state_matrix(folder, full=full)
            scope = (f'{len(STATE_BASELINES)} baselines x 3 tap patterns x 3 fps x ripple on/off, plus a leaving icon'
                     if full else f'{len(QUICK_BASELINES)} baselines x normal/rapid taps at 60 fps, plus a leaving icon')
            check(f'G: {label} scripts: every summon whitens at frame {m.WHITE_FRAME} of the final opening, before the '
                  f'reveal, and never before its Open part ({runs} summons: {scope})', not bad, bad[:8])
        # The 5-star case the player accepted: wherever the stock rule gives the designed white (in the
        # Open part), the fix gives it on the same frame.  Where the stock rule fails even with one
        # other animation (30 fps: the single black gate whitens before its tap; rapid taps: a ripple
        # makes the count 4 and the white never comes) the fix still meets the G matrix above.
        compared, moved, stock_failures = 0, [], []
        for taps in ('normal', 'rapid', 'late'):
            for fps in (60.0, 30.0):
                stock_res = play(CURRENT, BASELINES[1], taps=taps, fps=fps)
                state_res = play(CURRENT, BASELINES[1], taps=taps, fps=fps, rule='state')
                for k, r in stock_res.items():
                    if r.white_part == 'open':
                        compared += 1
                        if r.white_at != state_res[k].white_at:
                            moved.append((taps, fps, k, r.white_at, state_res[k].white_at))
                    else:
                        stock_failures.append((taps, fps, k, r.white_part))
        check(f'G: one other animation (the stock rule\'s working case, e.g. the accepted 5-star): in all {compared} '
              'summons where the stock rule whitens during the Open, the fix whitens on the same frame',
              compared > 0 and not moved, moved[:6])
        check('G: negative control -- even with one other animation the stock rule fails some summons '
              '(30 fps: early white over the single black gate; rapid taps: no white)',
              any(f[3] == 'wait' for f in stock_failures) and any(f[3] is None for f in stock_failures),
              stock_failures[:6])
        # Negative control: the stock rule breaks both halves under the player's conditions.
        res = play(CURRENT, BASELINES['2b'])
        early = sorted(k for k, r in res.items() if r.premature)
        missing = sorted(k for k, r in res.items() if r.white_at is None)
        singles = {e['id'] for e in effects if not e['change']}
        check('G: negative control -- stock rule, overflow glow + download icon: every double gate whitens early '
              '(the Omni report) and every single gate never whitens (the 7-star report)',
              set(early) == double_ids and set(missing) == singles, (early, missing))
        check('G: negative control -- stock rule, nothing else on screen: no summon whitens at all',
              all(r.white_at is None for r in play(CURRENT, []).values()))
        _runs, stock_bad = state_matrix(CURRENT, rule='stock', full=False)
        check('G: negative control -- the stock rule fails the G matrix', len(stock_bad) > 0, len(stock_bad))
        # Mutations of the fix.
        for label, attr, value, want in (
                ('no arming (a leftover opening may fire)', 'ARMING', False, 'early'),
                ('any gate animation counts', 'OPENING_MARKER', 'gachagate', 'early'),
                ('only the blend GachaGateOpenAdd counts', 'OPENING_MARKER', 'gachagateopenadd', 'missing')):
            old = getattr(m, attr)
            setattr(m, attr, value)
            try:
                _runs, bad = state_matrix(CURRENT, full=False)
            finally:
                setattr(m, attr, old)
            kinds = {'early' if 'white in' in v[-1] or 'vs opening' in v[-1] else 'missing' if v[-1] == 'no white'
                     else 'other' for v in bad}
            check(f'G: mutation "{label}" is caught ({want} white)', want in kinds, sorted(kinds))
    finally:
        del m.SECTIONS[UNRELATED]


if __name__ == '__main__':
    raise SystemExit(main())
