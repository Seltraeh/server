"""Tests for scripts/gen_boss_icon_offsets.py, offline (deploy/ is only read).

    python scripts/test_gen_boss_icon_offsets.py

The generator changes nothing but `visuals.hp_disp_pos`, only on rows that carried the invented "0,-60", and is
idempotent; the three bosses round 8 v12 put on screen keep the values the client showed below the HUD bar
(Juggernaut 0,26 / Emperor 0,81 / Maxwell 0,93); every value is at least the model and at least the clamp;
the Battle Simulator and the bosses without measurable art keep their value; --check reports and nothing is
written without --out.  Holds before and after deploy/archive is regenerated.
"""
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_boss_icon_offsets as g  # noqa: E402
from bf_testkit import Checker  # noqa: E402

# Round 8 v12 (S19 client, 831x934 window): icon centres at window y 130 / 104 / 112, under the zel bar's 68
# and the SPARK/CRITICAL line's 90, on each boss's head or upper body.
CLIENT_VERIFIED = {(85, 'Juggernaut'): '0,26', (465, 'Emperor of Amdahl'): '0,81',
                   (365, 'Creator Maxwell'): '0,93'}
UNMEASURABLE = ('Cursed Estia', 'Xie Jing')


def rows(missions):
    for m in missions:
        for b in [b for s in m['stages'] for b in s['battle_monsters']] + list(m.get('script_monsters') or []):
            yield m['id'], b


def without_offsets(missions):
    for _, b in rows(missions):
        if b.get('visuals'):
            b['visuals'].pop('hp_disp_pos', None)
    return missions


def main():
    check = Checker()
    raw = g.MISSIONS.read_bytes()
    out, changed, skipped = g.generate(raw)
    again, changed2, _ = g.generate(out)
    before, after = json.loads(raw), json.loads(out)

    check('only hp_disp_pos changes', without_offsets(json.loads(raw)) == without_offsets(json.loads(out)))
    check('idempotent', again == out and not changed2, f'{len(changed2)} change(s) on a second pass')
    moved = [(mid, b['name']) for (mid, b), (_, a) in zip(rows(before), rows(after))
             if (b.get('visuals') or {}).get('hp_disp_pos') != (a.get('visuals') or {}).get('hp_disp_pos')]
    check('only rows carrying the invented offset change',
          all(b['visuals']['hp_disp_pos'] == g.INVENTED for (_, b), (_, a) in zip(rows(before), rows(after))
              if (b.get('visuals') or {}).get('hp_disp_pos') != (a.get('visuals') or {}).get('hp_disp_pos')),
          moved)
    check('the report names exactly the changed rows', sorted(moved) == sorted((m, n) for m, n, _ in changed),
          (len(moved), len(changed)))

    final = {(mid, b['name']): b['visuals'].get('hp_disp_pos') for mid, b in rows(after) if b.get('visuals')}
    for key, want in CLIENT_VERIFIED.items():
        check(f'client-verified {key[1]} (mission {key[0]}) = {want}', final.get(key) == want, final.get(key))

    bad = []
    for mid, b in rows(after):
        v = b.get('visuals') or {}
        if mid == g.SIMULATOR or v.get('hp_disp_pos') in (None, g.INVENTED):
            continue
        model = g.offset_for(v)
        clamped = g.offset_for(v, b.get('position', ''))
        y = int(v['hp_disp_pos'].split(',')[1])
        if model is not None and not (y == clamped and y >= model):
            bad.append((mid, b['name'], y, model, clamped))
    check('every boss offset = max(model, clamp)', not bad, bad[:5])
    check('the clamp lowers at least one giant (the reason it exists)',
          any(g.offset_for(b['visuals'], b['position']) > g.offset_for(b['visuals'])
              for _, b in rows(after) if b.get('visuals') and g.offset_for(b['visuals']) is not None))

    sim_before = [b.get('visuals') for mid, b in rows(before) if mid == g.SIMULATOR]
    sim_after = [b.get('visuals') for mid, b in rows(after) if mid == g.SIMULATOR]
    check('the Battle Simulator is untouched', sim_before == sim_after)
    kept = [(mid, b['name']) for mid, b in rows(after) if b['name'].startswith(UNMEASURABLE)]
    check('bosses without measurable art keep their value',
          kept and all(final[k] == g.INVENTED for k in kept), [(k, final[k]) for k in kept][:3])
    emperor = next(b['visuals'] for mid, b in rows(after) if (mid, b['name']) == (465, 'Emperor of Amdahl'))
    check('a row without a usable position falls back to the model',
          g.offset_for(emperor, '') == g.offset_for(emperor, 'x:y') == g.offset_for(emperor) == 12,
          (g.offset_for(emperor, ''), g.offset_for(emperor)))

    # gen_research_lab copies story bosses' visuals into the trials; it must re-derive the same offset for the
    # trial's own position, or the two generators' --checks could never both pass once deploy is regenerated.
    import gen_research_lab as lab
    by_id = {m['id']: m for m in after}
    disagree = []
    for build in lab.TRIALS:
        record = build(dict(by_id))[0]
        want = {(b['id'], b['name']): (b.get('visuals') or {}).get('hp_disp_pos') for _, b in rows([by_id[record['id']]])}
        got = {(b['id'], b['name']): (b.get('visuals') or {}).get('hp_disp_pos') for _, b in rows([record])}
        if want != got:
            disagree.append((record['id'], {k: (want[k], got.get(k)) for k in want if want[k] != got.get(k)}))
    check('the Research Lab generator re-derives the same offsets for its trials', not disagree, disagree[:2])

    script = HERE / 'gen_boss_icon_offsets.py'
    res = subprocess.run([sys.executable, '-I', str(script), '--check'], capture_output=True, text=True)
    check('--check agrees with the archive', (res.returncode == 0) == (out == raw), res.stdout[-200:])
    res = subprocess.run([sys.executable, '-I', str(script)], capture_output=True, text=True)
    check('nothing is written without --out', res.returncode != 0 and g.MISSIONS.read_bytes() == raw,
          res.stderr[-200:])
    return check.summary('test_gen_boss_icon_offsets')


if __name__ == '__main__':
    sys.exit(main())
