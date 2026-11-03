"""Tiers the Super and Mega Metal Parade waves the way the wiki describes (#39).

python scripts/gen_metal_parades.py          # rewrite deploy/archive/mission.json
python scripts/gen_metal_parades.py --check  # exit 1 if the archive differs
python scripts/gen_metal_parades.py --out DIR/mission.json   # write the result elsewhere (never deploy/)

EVIDENCE -- Global wiki, one page per parade (each lists the tiers and their HP):
  Metal Parade!       Parade:Metal rev 630717  1000 EXP  Ghost 10, King 35, God 100, Crystal 300
  Super Metal Parade! Parade:SMP   rev 630719  3000 EXP  "No Metal Ghosts (or similar) will be
                                                         encountered here"  King 35, God 100, Crystal 300
  Mega Metal Parade!  Parade:MMP   rev 630716  7000 EXP  "No Metal Ghosts and Kings (or similar)
                                                         will be encountered here"  God 100, Crystal 300
The archive had copied the normal parade's Ghost/King waves into both harder
parades, which is exactly the report.

EXP IS LEFT ALONE.  The game's own mission_mst (the table the quest screen
shows) says 1000 / 3000 / 5000; the Mega page says 7000.  That is a version
difference this build's data does not resolve, so the archive keeps agreeing
with its own MST (validate_missions.py H4) and the page's figure is recorded
in docs/CLAUDE_SESSION_2_HANDOFF.md instead.

AUTHORED -- the pages give tiers and HP but no wave layout.  So the waves of
Super and Mega are the normal parade's (100600, left untouched) with every slot
moved up the same ladder the pages describe, same element, same position, same
drops: Super one step (Ghost->King, King->God), Mega two (Ghost->God,
King->Crystal).  Super takes all five waves that way, so its boss wave is the
normal parade's Dark King + Water Ghost + Thunder Ghost one tier up: Dark (Metal)
God + Water King + Thunder King.  That is the SMP page's "1 God and 2 Kings"
formation (its only other one is "1 Crystal"; listed in every revision from 64415,
2014-08-07, to 630719), chosen by Evan on 2026-10-30 over the lone Dark God this
generator used to keep.  Mega keeps its lone Dark Crystal finale: the MMP page
lists no boss formation.  ATK is each tier's existing parade value; DEF 999999 is
the parade convention for the pages' "cannot be hit for more than 1 damage per
hit" (not a sourced number); capture chance stays at the existing 60, since no
page gives rates.  Monster ids and art are the archive's own for each unit.
"""
import argparse
import copy
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MISSIONS = ROOT / 'deploy' / 'archive' / 'mission.json'
BASE, SUPER, MEGA = 100600, 100601, 100614
ELEMENTS = ('Fire', 'Water', 'Earth', 'Thunder', 'Light', 'Dark')

# (monster id, unit id) of each tier's parade enemy, per element -- all already
# in the archive (Crystals other than Dark come from the story missions' rows).
KING = {'Fire': (12000, 10203), 'Water': (21850, 20203), 'Earth': (31850, 30203),
        'Thunder': (41850, 40203), 'Light': (52200, 50203), 'Dark': (61600, 60133)}
GOD = {'Fire': (12050, 10204), 'Water': (21900, 20204), 'Earth': (31900, 30204),
       'Thunder': (41900, 40204), 'Light': (52250, 50204), 'Dark': (61650, 60134)}
CRYSTAL = {'Fire': (13400, 10344), 'Water': (23250, 20334), 'Earth': (33250, 30324),
           'Thunder': (43550, 40324), 'Light': (53300, 50364), 'Dark': (63000, 60334)}
STATS = {'King': (35, 260), 'God': (100, 420), 'Crystal': (300, 520)}   # HP (wiki), ATK (parade)
TABLE = {'King': KING, 'God': GOD, 'Crystal': CRYSTAL}
LADDER = {SUPER: {'Ghost': 'King', 'King': 'God'}, MEGA: {'Ghost': 'God', 'King': 'Crystal'}}


def tier_up(monster, mission):
    element, tier = monster['name'].split(' ', 1)
    if element not in ELEMENTS or tier not in LADDER[mission]:
        raise SystemExit(f'unexpected base-parade monster {monster["name"]!r}')
    new_tier = LADDER[mission][tier]
    monster_id, unit_id = TABLE[new_tier][element]
    hp, atk = STATS[new_tier]
    out = copy.deepcopy(monster)
    out.update({'id': monster_id, 'name': f'{element} {new_tier}', 'unit_id': unit_id, 'hp': hp, 'atk': atk,
                'def': 999999, 'unit_drop_id': unit_id})
    return out


def rebuild(by_id):
    base = by_id[BASE]
    for mission in (SUPER, MEGA):
        rec = by_id[mission]
        tiered = 5 if mission == SUPER else 4       # Super's boss wave too; Mega keeps its finale
        stages = [copy.deepcopy(s) for s in base['stages'][:tiered]]
        for stage in stages:
            stage['battle_monsters'] = [tier_up(m, mission) for m in stage['battle_monsters']]
        rec['stages'] = stages + rec['stages'][tiered:]


def dump(data):
    text = json.dumps(data, indent=1, ensure_ascii=False).replace('\n', '\r\n') + '\r\n'
    return text.encode('utf-8')


def generate(raw: bytes) -> bytes:
    by_id = {m['id']: m for m in json.loads(raw)}
    # Super's finale is the lone Dark God this generator used to keep, or the God + 2 Kings it now writes.
    finale = {m: [b['name'] for b in by_id[m]['stages'][4]['battle_monsters']] for m in (SUPER, MEGA)}
    if finale[SUPER] not in (['Dark God'], ['Dark God', 'Water King', 'Thunder King']) \
            or finale[MEGA] != ['Dark Crystal']:
        raise SystemExit(f'unexpected finales {finale}')
    rebuild(by_id)
    return dump([by_id[k] for k in sorted(by_id)])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--source', type=Path, default=MISSIONS)
    ap.add_argument('--out', type=Path, help='write here instead of over --source (a new or identical file)')
    args = ap.parse_args()
    raw = args.source.read_bytes()
    new = generate(raw)
    if args.check:
        print('up to date' if new == raw else f'stale: {args.source.name}')
        return 0 if new == raw else 1
    target = (args.out or args.source).resolve()
    if args.out:
        deploy = (ROOT / 'deploy').resolve()
        if target == deploy or deploy in target.parents:
            raise SystemExit(f'refusing --out {target}: inside deploy/ (omit --out to rewrite the source)')
        if target.exists() and target.read_bytes() != new:
            raise SystemExit(f'refusing --out {target}: it exists with different bytes')
    if not target.exists() or target.read_bytes() != new:
        target.write_bytes(new)
        print(f'wrote {target}')
    else:
        print('nothing to write (already current)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
