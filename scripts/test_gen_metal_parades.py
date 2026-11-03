"""Tests for scripts/gen_metal_parades.py (no server; deploy/ is only read).

    python scripts/test_gen_metal_parades.py

The generator is run in memory on deploy/archive/mission.json, whether or not deploy already carries its
output.  Checked: Super Metal Parade!'s boss wave is the SMP page's "1 God and 2 Kings" formation, built as the
normal parade's boss wave one tier up (Evan's choice, 2026-10-30): Dark God + Water King + Thunder King, same
positions, wiki HP, DEF 999999, capture 60 %.  Super's battles 1-4, Mega's and the normal parade's records
come out as the generator's tier rule makes them, every other mission is byte-for-byte the source's,
regenerating is idempotent, and --out refuses deploy/.
"""
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_metal_parades as g  # noqa: E402
from bf_testkit import Checker  # noqa: E402


def main() -> int:
    check = Checker()
    raw = g.MISSIONS.read_bytes()
    before = {m['id']: m for m in json.loads(raw)}
    out = g.generate(raw)
    after = {m['id']: m for m in json.loads(out)}
    base_boss = before[g.BASE]['stages'][4]['battle_monsters']
    boss = after[g.SUPER]['stages'][4]
    names = [b['name'] for b in boss['battle_monsters']]
    check('Super\'s boss wave is "1 God and 2 Kings": Dark God + Water King + Thunder King',
          names == ['Dark God', 'Water King', 'Thunder King'] and boss['is_boss'], names)
    check('... the normal parade\'s boss wave one tier up: same positions, wiki HP, DEF 999999, capture 60 %',
          [b['position'] for b in boss['battle_monsters']] == [b['position'] for b in base_boss]
          and [b['hp'] for b in boss['battle_monsters']] == [100, 35, 35]
          and all(b['def'] == 999999 and b['unit_drop_chance'] == 60 and b['unit_drop_id'] == b['unit_id']
                  for b in boss['battle_monsters']),
          [(b['position'], b['hp'], b['def'], b['unit_drop_chance']) for b in boss['battle_monsters']])
    check('... with the archive\'s own God and King rows (61650/60134, 21850/20203, 41850/40203)',
          [(b['id'], b['unit_id']) for b in boss['battle_monsters']] == [(61650, 60134), (21850, 20203), (41850, 40203)])
    tiers = {g.SUPER: {'King', 'God'}, g.MEGA: {'God', 'Crystal'}}
    for mission, allowed in tiers.items():
        seen = {b['name'].split(' ', 1)[1] for s in after[mission]['stages'] for b in s['battle_monsters']}
        check(f'{mission}: only {sorted(allowed)} appear, five battles', seen == allowed
              and len(after[mission]['stages']) == 5, seen)
    check('Mega keeps its lone Dark Crystal finale',
          [b['name'] for b in after[g.MEGA]['stages'][4]['battle_monsters']] == ['Dark Crystal'])
    check('the normal parade is untouched', after[g.BASE] == before[g.BASE])
    others = [k for k in before if k not in (g.SUPER, g.MEGA) and before[k] != after[k]]
    check('every other mission is unchanged', not others and set(after) == set(before), others[:5])
    check('regenerating the output is idempotent', g.generate(out) == out)
    probe = g.ROOT / 'deploy' / 'archive' / 'gen_metal_parades_probe.json'
    run = subprocess.run([sys.executable, str(HERE / 'gen_metal_parades.py'), '--out', str(probe)],
                         capture_output=True, text=True)
    check('--out inside deploy/ is refused and nothing is written',
          run.returncode != 0 and 'inside deploy' in (run.stderr + run.stdout) and not probe.exists(),
          (run.returncode, run.stderr[-200:]))
    check('deploy/archive/mission.json was only read', g.MISSIONS.read_bytes() == raw)
    return check.summary('gen_metal_parades')


if __name__ == '__main__':
    raise SystemExit(main())
