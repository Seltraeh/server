"""#18: author the story bosses' skills, AI and ailment profile from the wiki, into the mission and AI archives.

    python scripts/gen_story_bosses.py --check [--archive DIR]   # exit 1 if DIR's mission.json / ai.json differ
    python scripts/gen_story_bosses.py --out DIR [--archive DIR]  # write both files into DIR

Idempotent; only the named monster's `skills`, `ai_id`, `ailment_resists` and (where given) `hp`, `act_max` and
`act_rate`, and its AI record, are written, and
nothing without --out (deploy/archive changes are a release action).  Every value is either sourced (the
revision is named) or marked AUTHORED / INTERPRETATION, the round-8 rule for numbers the source does not give
(docs/REMAINING_CONTENT_SCOPE.md, Decision 2).  Story AIs take ids 30000000 + the mission id.

#18-a  Juggernaut, mission 85 "The Forged Gigantor" -- Tower of Mistral rev 674494 (snapshot in
       out/qa/current/review_round6_20261031/Tower_of_Mistral.json): 50,000 HP, Thunder, energy 8, 8 battles,
       180 EXP (the archive already matches all of them), "Causes Weakness", "Can be injured, cursed, and
       poisoned".  The archive had no skill and AI 1 "Attack random".
       * Resists: immune to Weak, Sick and Paralysis -- INTERPRETATION of the wiki's "can be injured, cursed,
         and poisoned" as the complete list (gen_research_lab's susceptible_only convention).
       * "Causes Weakness": skill 9600 Atom Cannon -- AUTHORED choice.  Juggernaut's own band 1900-1904 never
         inflicts Weakness (proc 11 "6,15" is Paralysis 15 %); 9600 is the monster-range Thunder AoE, 12 hits,
         Weak 45 % (its one level row), drawing the band's 400150 effect, and no other monster uses it.
       * AI 30000085: Atom Cannon on 30 % of turns, else attack -- AUTHORED rate (the wiki gives none).

#18-b/c (round 8, November 1) -- the other eight story bosses.  Sources: each dungeon page's FULL revision
       history (about 620 revisions, scanned by out/qa/current/fresh_round8_20261031/tools/wiki_boss_hp.py, raw
       JSON kept beside it) and the boss notes cached in tools/wiki_bosses/boss_catalog.json.  Every skill is the
       boss's OWN named burst, found by name in the monster range of F_SKILL_MST (the wiki names them).
       * HP: sourced only for Defense System 180,000 (Tower of Destruction, revs 64402..675483) and Granvalm
         200,000 (The White Tower, revs 63619..675502).  No revision gives the others.  Demon Abaddon's 370,000
         in the archive is the TRIAL 03 boss (Trial of the Gods), not the Tower of Morgan story fight.  The
         sourced story bosses rise almost linearly with mission EXP (Juggernaut 180 -> 50,000; Defense System
         5,900 -> 180,000; the same line puts Granvalm's 6,200 at 187,000 against its sourced 200,000), so the
         unsourced ones are that line at their own EXP, rounded to 5,000 -- AUTHORED: Abaddon 70,000, Maxwell
         and Emperor of Amdahl 115,000, Demon Zebra and Grantos 130,000, Cardes 150,000.
       * Turn patterns the wiki states are kept exactly; every percentage is AUTHORED (the wiki says "often",
         "nearly every turn" or nothing).  Resists follow the wiki's susceptibility lines (INTERPRETATION as the
         complete list, as for Juggernaut); Creator Maxwell "can be inflicted with all status ailments" = none.
       * Not modelled (the client data has no such parameter): ailments the wiki puts on ordinary attacks
         (Defense System Injury, Granvalm Paralysis, Zebra Curse/Injury, Cardes Curse), Emperor's Paralysis/
         Poison on its bursts (5480/5481 carry only damage) and Maxwell's Weakness (2052/2053 likewise).

SKILLS NEED A SERVER THAT SENDS 8aiBoHg5 (round 8, November 1).  The client files a skill whose ToZj4zyn is 0
(most of the monster range) as battle-temporary and empties that list on entering Home, so a monster skill crashes the Windows client at
battle start (v13, v14) unless MissionStart re-sends its row (MissionStartResp.skills, MissionArchiver).
Client run v16 on that server: Juggernaut cast Atom Cannon with its name shown and the quest cleared.  Ship
this generator's output only with or after such a server (docs/RELEASE_READINESS_2026-10-31.md).
"""
import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from gen_research_lab import AILMENTS, cond, dump, row, susceptible_only  # noqa: E402

ROOT = HERE.parent
ARCHIVE = ROOT / 'deploy' / 'archive'

BOSSES = [
    {
        'mission': 85, 'monster': 1000, 'name': 'Juggernaut',
        'skills': [9600],
        'ai': {'id': 30000085, 'name': 'Story - Juggernaut (The Forged Gigantor)', 'actions': [
            row(1, [cond('skill', 1)], percent=30.0),
            row(2, [], action='attack'),
        ]},
        'resists': susceptible_only('injury', 'curse', 'poison'),
    },
    # Tower of Morgan "The Eroding Barricade".  "Casts AOE nearly every turn", "When health drops below 50%, he
    # casts a slightly stronger AOE BB", "BB can cause Curse", "Can be Sickened".  1 Black Force 1952 (AoE),
    # 2 Evil Hole 1953 (the band's stronger AoE), 3 Necro Curse 1955 (the band's curse).  One action a turn:
    # the archive's unsourced 3 would make "nearly every turn" about three AoEs a turn.
    {
        'mission': 265, 'monster': 1100, 'name': 'Demon Abaddon', 'hp': 70_000, 'act_max': 1,
        'skills': [1952, 1953, 1955],
        'ai': {'id': 30000265, 'name': 'Story - Demon Abaddon (The Eroding Barricade)', 'actions': [
            row(1, [cond('skill', 2), cond('hp_pr_under', 50)], percent=70.0),
            row(2, [cond('skill', 1)], percent=70.0),
            row(3, [cond('skill', 3)], percent=50.0),
            row(4, [], action='attack'),
        ]},
        'resists': susceptible_only('sick'),
    },
    # St. Lamia Palace "Creator of the World".  "Endless ... casted at every 5th turn", "Genesis ... used more
    # often".  The band 2050-2059 holds two Genesis and three Endless; the Lab's 2058 (700 %) belongs to a 10x
    # trial, so the story pair is the band's ordinary one: 1 Genesis 2052, 2 Endless 2053 -- AUTHORED choice.
    {
        'mission': 365, 'monster': 1200, 'name': 'Creator Maxwell', 'hp': 115_000,
        'skills': [2052, 2053],
        'ai': {'id': 30000365, 'name': 'Story - Creator Maxwell (Creator of the World)', 'actions': [
            row(1, [cond('skill', 2), cond('actbetween', 5), cond('turn_limited_act', 1)]),
            row(2, [cond('skill', 1)], percent=50.0),
            row(3, [], action='attack'),
        ]},
        'resists': {a: 0 for a in AILMENTS},
    },
    # Amdahl Castle "Malevolent Ceremony".  "Blood Cyclone ... Used rather often", "Amdahl Vision ... casted every
    # 4th turn", "Can be weakened, paralyzed and cursed".  1 Blood Cyclone 5480, 2 Amdahl Vision 5481.
    {
        'mission': 465, 'monster': 1400, 'name': 'Emperor of Amdahl', 'hp': 115_000,
        'skills': [5480, 5481],
        'ai': {'id': 30000465, 'name': 'Story - Emperor of Amdahl (Malevolent Ceremony)', 'actions': [
            row(1, [cond('skill', 2), cond('actbetween', 4), cond('turn_limited_act', 1)]),
            row(2, [cond('skill', 1)], percent=50.0),
            row(3, [], action='attack'),
        ]},
        'resists': susceptible_only('weak', 'paralysis', 'curse'),
    },
    # Sky Fort Solaris "The Mad God Descends".  "Chaos Rage - AOE BB", "Gravity Nova - Powerful AOE BB used in
    # every five turns", "Can be inflicted with Injury, Sickness and Weakness".  1 Chaos Rage 5640 (its data
    # adds Injury 15 % / Curse 10 %), 2 Gravity Nova 5650.
    {
        'mission': 565, 'monster': 1500, 'name': 'Demon Zebra', 'hp': 130_000,
        'skills': [5640, 5650],
        'ai': {'id': 30000565, 'name': 'Story - Demon Zebra (The Mad God Descends)', 'actions': [
            row(1, [cond('skill', 2), cond('actbetween', 5), cond('turn_limited_act', 1)]),
            row(2, [cond('skill', 1)], percent=40.0),
            row(3, [], action='attack'),
        ]},
        'resists': susceptible_only('injury', 'sick', 'weak'),
    },
    # Spirit World Palmyna "The Open Seal".  "Endless will be cast on turns 1,6,11, and intervals of 5", "Heals
    # 30,000 HP every 5 turns, starting on the 5th turn", "Susceptible to injury".  1 Hidden Dimension 5660
    # (AoE), 2 Foreign Light 5670 (the band's self-heal; its data heals 35,000-40,000), 3 Endless 5680.
    # turn % 5 == 1 is not a term, so the heal (turns 5, 10, ...) raises AI flag 1 and Endless answers it the
    # next turn (6, 11, ...), clearing it; a turn-1 row casts the first Endless.
    {
        'mission': 666, 'monster': 1600, 'name': 'Cardes the Malevolent', 'hp': 150_000,
        'skills': [5660, 5670, 5680],
        'ai': {'id': 30000666, 'name': 'Story - Cardes the Malevolent (The Open Seal)', 'actions': [
            row(1, [cond('skill', 3), cond('act', 1)]),
            row(2, [cond('skill', 3), cond('flg_on', 1)], flags=(1, 0)),
            row(3, [cond('skill', 2), cond('actbetween', 5), cond('turn_limited_act', 1)], target=1, flags=(1, 1)),
            row(4, [cond('skill', 1)], percent=40.0),
            row(5, [], action='attack'),
        ]},
        'resists': susceptible_only('injury'),
    },
    # Tower of Destruction "The Corpse Producing Machine" (HP sourced).  "Destruct Drive - 7 combo powerful
    # random Thunder attack", "Genocide Order - 20 combo ... Used every 5 turn", "Heavy Barrage - 12 combo ...
    # all enemies", "System Repair - Heals 50,000 HP to self, used when below 50% HP", "Susceptible to
    # Weakness".  1 Destruct Drive 7700, 2 Heavy Barrage 7701, 3 System Repair 7702, 4 Genocide Order 7703.
    # The repair fires once (AUTHORED: the wiki does not say how often).
    {
        'mission': 776, 'monster': 1700, 'name': 'Defense System', 'hp': 180_000,
        'skills': [7700, 7701, 7702, 7703],
        'ai': {'id': 30000776, 'name': 'Story - Defense System (The Corpse Producing Machine)', 'actions': [
            row(1, [cond('skill', 4), cond('actbetween', 5), cond('turn_limited_act', 1)]),
            row(2, [cond('skill', 3), cond('hp_pr_under', 50), cond('limited_act', 1)], target=1),
            row(3, [cond('skill', 1)], percent=30.0),
            row(4, [cond('skill', 2)], percent=30.0),
            row(5, [], action='attack'),
        ]},
        'resists': susceptible_only('weak'),
    },
    # The White Tower "The Roar of Triumph" (HP sourced).  "Tail Swing - AOE BB", "Crash Nail - STBB", "Crunch
    # Rack - Powerful AOE BB used every 3rd turn", "Can spam Tail Swing and Crash Nail in one turn", "Can be
    # inflicted with Paralysis and Weakness", "Can be poisoned".  1 Tail Swing 9311, 2 Crash Nail 9310,
    # 3 Crunch Rack 9312.  Two actions on half the turns (AUTHORED rate, as the Lab's bosses).
    {
        'mission': 866, 'monster': 1800, 'name': 'Granvalm', 'hp': 200_000, 'act_max': 2, 'act_rate': 50.0,
        'skills': [9311, 9310, 9312],
        'ai': {'id': 30000866, 'name': 'Story - Granvalm (The Roar of Triumph)', 'actions': [
            row(1, [cond('skill', 3), cond('actbetween', 3), cond('turn_limited_act', 1)]),
            row(2, [cond('skill', 1)], percent=40.0),
            row(3, [cond('skill', 2)], percent=40.0),
            row(4, [], action='attack'),
        ]},
        'resists': susceptible_only('paralysis', 'weak', 'poison'),
    },
    # Giant's Ruins "The Giant God Awakes".  "Fatal Blow - AOE BB that's used often", "Grand Destroyer -
    # Powerful AOE BB that's used in every fourth turn", "Can be Weakened and Injured".  1 Fatal Blow 5150,
    # 2 Grand Destroyer 5151.
    {
        'mission': 20067, 'monster': 1300, 'name': 'Grantos', 'hp': 130_000,
        'skills': [5150, 5151],
        'ai': {'id': 30020067, 'name': 'Story - Grantos (The Giant God Awakes)', 'actions': [
            row(1, [cond('skill', 2), cond('actbetween', 4), cond('turn_limited_act', 1)]),
            row(2, [cond('skill', 1)], percent=50.0),
            row(3, [], action='attack'),
        ]},
        'resists': susceptible_only('weak', 'injury'),
    },
]


def generate(missions_raw: bytes, ais_raw: bytes):
    missions = json.loads(missions_raw)
    ais = {a['id']: a for a in json.loads(ais_raw)}
    by_id = {m['id']: m for m in missions}
    report = []
    for boss in BOSSES:
        mission = by_id.get(boss['mission'])
        targets = [b for s in (mission or {}).get('stages', []) for b in s['battle_monsters']
                   if b['id'] == boss['monster'] and b['name'] == boss['name']]
        if len(targets) != 1:
            raise SystemExit(f"mission {boss['mission']}: expected one {boss['name']} {boss['monster']}, "
                             f'found {len(targets)}')
        b = targets[0]
        if boss.get('skills'):
            b['skills'] = [{'skill_id': i, 'show_name': True} for i in boss['skills']]
        if boss.get('ai'):
            b['ai_id'] = boss['ai']['id']
            ais[boss['ai']['id']] = boss['ai']
        b['ailment_resists'] = {k: boss['resists'][k] for k in AILMENTS}
        for key in ('hp', 'act_max', 'act_rate'):
            if key in boss:
                b[key] = boss[key]
        report.append((boss['mission'], boss['name'], boss.get('skills', []), b['ai_id']))
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
    for mid, name, skills, ai in report:
        print(f'  {mid} {name}: skills {skills}, AI {ai}')
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
