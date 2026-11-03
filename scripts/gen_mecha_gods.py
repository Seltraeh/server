"""#17: the six Lv. 6 Sanctums as "The Mecha Gods Awaken" (the end-of-service version), into the mission and AI archives.

    python scripts/gen_mecha_gods.py --check [--archive DIR]   # exit 1 if DIR's mission.json / ai.json differ
    python scripts/gen_mecha_gods.py --out DIR [--archive DIR]  # write both files into DIR

Idempotent; only missions 100320-100325 and their six AI records are written, and nothing without --out
(deploy/archive changes are a release action).  Run it BEFORE gen_material_drops.py, which must be rerun
whenever mission.json is replaced.  Ship it only with or after a server that sends 8aiBoHg5 (the bosses cast
monster skills; see gen_story_bosses.py).

SOURCE: The Mecha Gods Awaken, rev 659707 (2020-07-28, the page's latest; all 44 revisions back to 144806 in
2015 were read, out/qa/current/fresh_round8_20261031/wiki/history/).  Its footer: "As of November 1 2018, this
dungeon is now permanently available everyday in the Vortex", so this is the end-of-service content (Evan,
November 2: "We want the EOS functionality for all systems").  The archive's earlier Totem/Idol/Miracle Totem
line-up was tools/gen_vortex_battles.py's AUTHORED guess ("No wiki line-up survives": the page is not named
after the Sanctums), not an older version.
  * Sourced, per element: energy 70, 3 battles, EXP 7,000 (mission_mst already agrees); the boss (unit 10354 /
    20344 / 30334 / 40334 / 50394 / 60344, alone); room 2's line-up; the boss's two named skills and how often
    the wiki says they come; the room-2 susceptibility lines; "Each monster can act up to 4 times per turn"
    (High Elf: "up to three times per turn").
  * Client data: each boss's monster id is the client dictionary's first "<Element> Mecha God"
    (sgtext_monsters MST_MONSTERS_<id>_NAME: 15850, 25800, 35950, 46850, 55900, 66000 -- INTERPRETATION: the
    dictionary has 3-5 ids per Mecha God and no sign of which event used which).  The skills are the only
    monster-range rows carrying both names, an adjacent pair per element: Explosion 1054 / Volcanic Cannon
    1055, Diamond Dust 1154 / Freezing Breeze 1155, Vortex 1254 / Insane Climate 1255, Lightning Bolt 1554 /
    Unruly Thunder 1555, Southern Cross 1704 / Falling Star 1705, Invasion 1854 / Ominous Shade 1855.  Room-2
    monsters are COPIED from the archive's own row for that unit (its lowest monster id), never invented.
  * AUTHORED (the wiki gives no HP, ATK, DEF, capture rate or percentage in any revision) -- starting points
    for the community to tune (Evan, 2026-09-18: "invent the battles and the community will fine tune"):
      - boss HP 700,000: the sourced Vortex boss line Guardians of Lore Lv. 3 / Lv. 5 (5,200 EXP -> 420,000;
        8,800 EXP -> 1,000,000) at 7,000 EXP is 710,000, rounded down;
      - boss ATK 580 / DEF 100: the archive's late-story bosses of the nearest EXP (Defense System, Granvalm);
      - room-2 HP 100,000: the archive's usual HP for these same units in 5,000+ EXP Vortex rooms;
      - capture 40 %, unit_drop_type 0 (server-rolled): Evan's offline capture floor (2026-09-11);
      - act_rate 50 % per extra action (1-4 a turn), every skill at most once a turn, the "frequently" /
        unstated skill percentages, and room 1 (kept from the archive: the element's Idol and Totem).
  * Not modelled: room-2 monsters' own skills (High Elf's Vortex, Meltia's Diamond Dust and Planet Shower: the
    monster range holds several same-named rows and nothing ties one to these monsters) and High Elf's Poison.
"""
import argparse
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from gen_research_lab import AILMENTS, cond, dump, row, susceptible_only  # noqa: E402

ROOT = HERE.parent
ARCHIVE = ROOT / 'deploy' / 'archive'
REVID = 659707

BOSS_HP, BOSS_ATK, BOSS_DEF = 700_000, 580, 100
ROOM_HP = 100_000
CAPTURE = 40
ACT_MAX, ACT_RATE = 4, 50.0
FREQUENT, PLAIN = 25.0, 15.0        # per action, AUTHORED: "used frequently" / no frequency given
BOSS_POSITION = '180:302'
FORMATION = {3: ['180:302', '120:248', '96:352'], 4: ['160:240', '215:300', '160:360', '105:300']}

# mission, element, boss monster id, boss unit, room 2 [(name, count, susceptible or None, act_max)],
# skills [(id, name, cadence)], cadence = 'every3' | 'frequent' | None
SANCTUMS = [
    (100320, 'Fire', 15850, 10354,
     [('Dragon Graven', 2, None, ACT_MAX), ('Dalimaone', 1, None, ACT_MAX)],
     [(1054, 'Explosion', None), (1055, 'Volcanic Cannon', 'frequent')]),
    (100321, 'Water', 25800, 20344,
     [('Siren', 2, ('paralysis',), ACT_MAX), ('Meltia', 1, ('paralysis',), ACT_MAX)],
     [(1154, 'Diamond Dust', 'frequent'), (1155, 'Freezing Breeze', 'every3')]),
    (100322, 'Earth', 35950, 30334,
     [('High Elf', 2, ('weak',), 3), ('Lemenara', 1, ('weak',), ACT_MAX)],
     [(1254, 'Vortex', 'frequent'), (1255, 'Insane Climate', 'every3')]),
    (100323, 'Thunder', 46850, 40334,
     [('Great Falcon Ziz', 2, None, ACT_MAX), ('Zazabis', 1, None, ACT_MAX)],
     [(1554, 'Lightning Bolt', None), (1555, 'Unruly Thunder', 'every3')]),
    (100324, 'Light', 55900, 50394,
     [('Valkyrie', 1, None, ACT_MAX), ('Sky Hero Athena', 2, None, ACT_MAX), ('Legendary Jona', 1, None, ACT_MAX)],
     [(1704, 'Southern Cross', None), (1705, 'Falling Star', None)]),
    (100325, 'Dark', 66000, 60344,
     [('Lich', 2, ('sick', 'poison', 'paralysis', 'curse'), ACT_MAX), ('Vampire', 1, ('poison',), ACT_MAX),
      ('Legion', 1, ('injury',), ACT_MAX)],
     [(1854, 'Invasion', None), (1855, 'Ominous Shade', 'every3')]),
]


def ai_record(mission, element, skills):
    """Skill 1 is the powerful AoE, skill 2 the massive one; each at most once a turn, else a normal attack."""
    rows, n = [], 1
    for slot, (_sid, _name, cadence) in enumerate(skills, start=1):
        if cadence == 'every3':
            rows.insert(0, (slot, [cond('skill', slot), cond('actbetween', 3), cond('turn_limited_act', 1)], 100.0))
        else:
            rows.append((slot, [cond('skill', slot), cond('turn_limited_act', 1)],
                         FREQUENT if cadence == 'frequent' else PLAIN))
    actions = []
    for _slot, conds, pct in rows:
        actions.append(row(n, conds, percent=pct))
        n += 1
    actions.append(row(n, [], action='attack'))
    return {'id': 30_000_000 + mission, 'name': f'Sanctum of {element} Lv. 6 - {element} Mecha God', 'actions': actions}


def templates(missions):
    """Monster name (stripped) -> the archive row with the lowest monster id carrying it."""
    out = {}
    for m in missions:
        if 100320 <= m['id'] <= 100325:
            continue
        for s in m['stages']:
            for b in s['battle_monsters']:
                key = b['name'].strip()
                if key not in out or b['id'] < out[key]['id']:
                    out[key] = b
    return out


def acting(b, act_max):
    b['act_min'], b['act_max'], b['act_rate'] = 1, act_max, ACT_RATE
    return b


def generate(missions_raw: bytes, ais_raw: bytes):
    missions = json.loads(missions_raw)
    ais = {a['id']: a for a in json.loads(ais_raw)}
    by_id = {m['id']: m for m in missions}
    pool = templates(missions)
    report = []
    for mission_id, element, monster_id, unit, room2, skills in SANCTUMS:
        m = by_id.get(mission_id)
        if m is None or len(m['stages']) != 3:
            raise SystemExit(f'mission {mission_id}: expected an archived 3-battle record')
        # room 1 stays (authored filler); every monster acts up to 4 times a turn (the page's footer)
        for b in m['stages'][0]['battle_monsters']:
            acting(b, ACT_MAX)
        line = []
        for name, count, susceptible, act_max in room2:
            if name not in pool:
                raise SystemExit(f'mission {mission_id}: no archived row fields {name!r} to copy')
            for _ in range(count):
                b = acting(copy.deepcopy(pool[name]), act_max)
                b['hp'] = ROOM_HP
                b.pop('skills', None)
                b.pop('material_drops', None)
                if susceptible:
                    b['ailment_resists'] = susceptible_only(*susceptible)
                else:
                    b.pop('ailment_resists', None)
                line.append(b)
        for b, pos in zip(line, FORMATION[len(line)]):
            b['position'] = pos
        m['stages'][1]['battle_monsters'] = line
        ai = ai_record(mission_id, element, skills)
        ais[ai['id']] = ai
        old_boss = m['stages'][2]['battle_monsters'][0]
        boss = acting({
            'id': monster_id, 'name': f'{element} Mecha God', 'unit_id': unit, 'position': BOSS_POSITION,
            'hp': BOSS_HP, 'atk': BOSS_ATK, 'def': BOSS_DEF, 'ai_id': ai['id'], 'act_min': 1, 'act_max': ACT_MAX,
            'wait': 5, 'unit_drop_id': unit, 'unit_drop_level': 1, 'unit_drop_type': 0, 'unit_drop_chance': CAPTURE,
            'zel_max_drop': old_boss['zel_max_drop'], 'zel_drop_count': old_boss['zel_drop_count'],
            'karma_max_drop': old_boss['karma_max_drop'], 'karma_drop_count': old_boss['karma_drop_count'],
            'treasure_chest_chance': 0, 'treasure_drops': [],
            'skills': [{'skill_id': sid, 'show_name': True} for sid, _n, _c in skills],
        }, ACT_MAX)
        m['stages'][2] = {'is_boss': True, 'first_attack_rate': m['stages'][2].get('first_attack_rate', 0),
                          'battle_monsters': [boss]}
        report.append((mission_id, boss['name'], [s for s, _n, _c in skills], ai['id'],
                       [b['name'].strip() for b in line]))
    return dump(missions, 1, False), dump([ais[k] for k in sorted(ais)], 4, True), report


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--archive', type=Path, default=ARCHIVE)
    ap.add_argument('--out', type=Path)
    a = ap.parse_args()
    missions_raw = (a.archive / 'mission.json').read_bytes()
    ais_raw = (a.archive / 'ai.json').read_bytes()
    new_missions, new_ais, report = generate(missions_raw, ais_raw)
    for mid, name, skills, ai, room2 in report:
        print(f'  {mid} {name}: skills {skills}, AI {ai}, room 2 {room2}')
    stale = [n for n, old, new in (('mission.json', missions_raw, new_missions), ('ai.json', ais_raw, new_ais))
             if old != new]
    if a.check:
        print('up to date' if not stale else 'stale: ' + ', '.join(stale))
        return 1 if stale else 0
    if a.out is None:
        sys.exit('nothing written: pass --out DIR')
    a.out.mkdir(parents=True, exist_ok=True)
    (a.out / 'mission.json').write_bytes(new_missions)
    (a.out / 'ai.json').write_bytes(new_ais)
    print(f'wrote {a.out / "mission.json"}, {a.out / "ai.json"}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
