"""#12: the Vortex Trials into the mission and AI archives.

    python scripts/gen_vortex_trials.py --check [--archive DIR]   # exit 1 if DIR's mission.json / ai.json differ
    python scripts/gen_vortex_trials.py --out DIR [--archive DIR]  # write mission.json and ai.json into DIR

Idempotent; only the missions below are written, and nothing without --out (deploy/archive changes are a release
action).  Run it BEFORE gen_material_drops.py, which must be rerun whenever mission.json is replaced.  The fights
are only reachable on a server with the Vortex Trials subtree permits (ServerCache::m_vortexTrialAreas,
2026-11-03) and, for Winter Paradise, the topology of scripts/vortex_trials_topology.py.

THE SOURCES.  scripts/data/vortex_trials_wiki.json: 39 of the 41 missions matched to their wiki quest (latest
revision, the end-of-service version; Evan, November 2: "a wiki scrape / community authoring effort if nothing
exists on the wiki"), with each boss's phase HP, the unit whose art it can fight with, and its named skills found in
the client's own F_SKILL_MST by exact name.  scripts/data/vortex_trials_skills.json (scripts/extract_trial_skills.py):
the same pages' skills per section with the wiki's "Used ..." notes.  The client's own battle scripts
(F_MISSION_SCRIPT_MST, LocalState Ver288_F2Dz3QHU.dat), read 2026-11-03 into SCRIPT_CHAINS below.

WHAT IS SOURCED: the boss line-ups, every phase HP the wiki gives, the skills (the client's rows, so their effects
are the original kits), each script's monster ids and phase order, "enemies move first", and the costs / rewards
(mission_mst, client-side).  WHAT IS AUTHORED (marked here, listed in docs/MISSING_CONTENT.md, for community tuning):

  * the AI.  The wiki states every trigger in prose.  ai_rows() reads the notes it can map -- "turn N" (act N, once),
    "every N turns" / "turn N & every K turns afterwards" (actbetween K), "every turn" (each turn), "HP < N%" /
    "reaches N%" (hp_pr_under, once) -- and puts every other skill in a 20 % pool, at most once a turn, then a
    normal attack.  A skill that instantly defeats the party is used ONLY when the wiki gives it a turn (an enrage);
    Idle lines only at their stated turn.  A script's later phase counts its turns from the battle's start (see
    ai_rows(relative)).  Not modelled: shields "still up", adds alive, BB counters, marked units, Doom counters.
  * ATK / DEF (no source gives them): by energy tier, TIERS; the Zel / Karma drops (boss_row).
  * HP the wiki leaves blank (BLANK_HP), and the third phase the client scripts of the normal Savage Lion and Curse
    Master have but the wiki omits (the previous phase's HP).
  * the art of the bosses the game has no sprite for (STAND_INS): a unit of the boss's element and kind.
  * three fights the scripts would leave unwinnable or that the scripts cannot reach (Chilling Beauties x2, the
    True God-King): fresh monster ids the scripts do not name, phases merged into one HP bar.
  * 8301541 "Return of the Goddess" (area Weaver of Friendships): its wiki page does not exist ("missing" in the
    round-8 scrape); the Skeleton line-up of the other 10-energy openers (8301470/80/90), the same three waves.
  * those openers' Skeleton HP (the wiki names only the Skeleton): SKELETON_PATHS.

SCRIPT CHAINS.  The opening monster is fielded under the script's own id; the later phases go in the record's
script_monsters (MonsterMst/AI rows without a battle-group row; changeMonster finds them by id).  'hp': the script
swaps at < 1 % HP itself (trigger 3) and keeps the monster at 1 HP meanwhile (cmd 7), like Research Lab Trial 003.
'flag': the swap is trigger 5 = the monster's AI flag 3 (libgame.so BattleList::getAiFlgEvent), so each phase's AI
raises flag 3 once below 2 % HP.

83013400 Winter Paradise -- rev 665774: energy 30, 4 battles, EXP 7,500 (mission_mst agrees); "All units in this
       dungeon can be captured".  Each wave "spawns one of the following groups"; the archive fields one fixed
       line-up per wave, so each wave takes ONE of the wiki's groups (INTERPRETATION): wave 1 the four element
       Imps (the first group), wave 2 three Burst Frogs (the group the archive already fields), wave 3 Void
       Tablet Emgif, boss Imbued Tablet Elgif (boss1).  Emgif is new to the archive: Elgif's row with monster
       68500 and unit 60812.  HP, ATK and DEF are the copied rows' (AUTHORED where those were).  Not modelled:
       "can only be accessed once a day" (no per-mission daily limit exists here).
"""
import argparse
import copy
import json
import os
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from gen_research_lab import cond, dump, row  # noqa: E402

ROOT = HERE.parent
ARCHIVE = ROOT / 'deploy' / 'archive'
WIKI = HERE / 'data' / 'vortex_trials_wiki.json'
SKILLS = HERE / 'data' / 'vortex_trials_skills.json'
CAPTURE = 40          # Evan's offline capture floor (MissionArchiver, 2026-09-11)
FORMATION = {1: ['180:302'], 2: ['180:260', '150:350'], 3: ['180:302', '120:248', '96:352'],
             4: ['160:240', '215:300', '160:360', '105:300']}

# mission -> waves; each monster is (archive name to copy, count) or (archive name to copy, count, new id, new name,
# new unit) for a monster the archive never fielded.
TRIALS = {
    83013400: [
        [('Vigor Imp Molin', 1), ('Power Imp Pakpak', 1), ('Guard Imp Ganju', 1), ('Healing Imp Fwahl', 1)],
        [('Burst Frog', 3)],
        [('Imbued Tablet Elgif', 1, 68500, 'Void Tablet Emgif', 60812)],
        [('Imbued Tablet Elgif', 1)],
    ],
    # AUTHORED: no wiki page ("Return of the Goddess" is missing); the 10-energy openers' Skeleton line-up.
    8301541: [
        [('Skeleton', 3)],
        [('Skeleton', 3)],
        [('Skeleton King', 1), ('Skeleton', 2)],
    ],
}

# ---- the boss trials -------------------------------------------------------------------------------------------

# ATK / DEF by energy (AUTHORED; no source gives them): the archive's late bosses run 580-2000 ATK at 700k-13M HP.
TIERS = [(10, 2000, 800), (20, 2500, 1000), (25, 3000, 1500), (50, 5000, 2500), (100, 6000, 3000)]

# The bosses the game has no sprite for -> a unit of their element and kind (AUTHORED stand-ins).
STAND_INS = {
    'Savage Lion Baragan': 10735,        # Ember Hellion Adel (Fire beastman)
    'Curse Master': 61137,               # Accursed Calamity Velma (Dark)
    'Elhaza': 61137,
    'Warmonger Zaad': 40445,             # Beast God Exvehl (Thunder)
    'Illusionist Melfel': 830288,        # Steel Magus Dessit (Earth)
    'Vizier of Chaos': 61026,            # Wicked Sorcerer Gregor (Dark)
    'True God-King (Body)': 50014,       # God Atro (Light)
    'True God-King (Head)': 50114,       # God Knight Will (Light)
    'Chilling Beauty Rene': 20013,       # Ice Queen Selena (Water)
    'Corrupted Dryad': 30092, 'Defiled Dryad': 30092,      # Dryad
    'Hugin': 40083, 'Munin': 40083,      # Crow Tengu
    'Aura of Rejuvenation': 60812, 'Abnormal Aura of Time': 60812,  # the Imbued Tablet
    'Tainted Rivermaw': 20112,           # Leviathan (Water beast)
    'Corrupted Fleshsnapper': 820358,    # Crocodile Lord Kawab
    'Fledging Necromancer': 60163, 'Fanatic Necromancer': 60497,    # Necromancer Lemia / Lilly Matah
    'Undead Entity': 60083,              # Lich
    'Wild Hoof': 50383,                  # Horseman Sodis
    'Claws of Osiris': 60051,            # Skeleton King
}

# HP the wiki leaves blank (AUTHORED: the nearest sourced fight of the same energy).
BLANK_HP = {8301551: 8_000_000, 8301570: 45_000_000, 8301471: 60_000_000, 8301481: 60_000_000,
            8301482: 90_000_000, 8301491: 120_000_000, 8301500: 180_000_000}

# The client scripts' phase chains (F_MISSION_SCRIPT_MST, 2026-11-03).
SCRIPT_CHAINS = {
    8301560: ('flag', [140002000, 140002001, 140002002]),
    8301561: ('flag', [140002004, 140002005, 140002006]),
    8301640: ('hp', [140002014, 140002015, 140002016]),
    8301641: ('hp', [140002010, 140002011, 140002012]),
    8301650: ('hp', [140002103, 140002104, 140002105]),
    8301651: ('hp', [140002100, 140002101, 140002102]),
    20001500: ('hp', [20001511, 20001512, 20001513]),
    20001501: ('hp', [20001501, 20001502, 20001503]),
    20001600: ('hp', [20001611, 20001612, 20001613]),
    20001601: ('hp', [20001601, 20001602, 20001603]),
    20002201: ('hp', [20002201, 20002202, 20002203]),
    20002202: ('hp', [20002211, 20002212, 20002213]),
    25010001: ('hp', [25010011, 25010012, 25010013]),
    25010002: ('hp', [25010021, 25010022, 25010023]),
}
# A script that names one monster without changing it (dialogue only): that monster takes the script's id.
SCRIPT_IDS = {8301551: 8500309, 8301471: 140000004}

# mission -> the wave: [(boss name, sections, HP or None, add?)].  Sections: '*' = every section not claimed by a
# named monster; 'Phase N' sections go to the chain's Nth phase (see build()).  A chain mission lists ONE boss.
MISSIONS = {
    8301690: [('Herald of Yggdrasil', '*', 20_500_000, False), ('Corrupted Dryad', (), 4_500_000, True),
              ('Defiled Dryad', (), 4_500_000, True)],
    8301691: [('Prescient Goddess Frigg', '*', 36_000_000, False)],
    8301692: [('Creation God Odin', '*', 42_000_000, False), ('Hugin', ('Hugin',), 4_000_000, True),
              ('Munin', ('Munin',), 4_000_000, True)],
    8301550: [('Vizier of Chaos', '*', 109_890_000, False)],
    8301551: [('Ember Elder Fiora', '*', None, False)],
    8301552: [('Ptah, Master of Truth', '*', 120_000_000, False)],
    8301553: [('Aesys, Great of Magic', '*', 80_000_000, False)],
    8301570: [("Ma'at, Arbiter of Order", ("Ma'at, Arbiter of Order", 'Unsorted'), None, False),
              ('Set the Deceiver', ('Set the Deceiver',), None, False),
              ('Aura of Rejuvenation', ('Aura of Rejuvenation',), 3_000_000, True),
              ('Abnormal Aura of Time', ('Abnormal Aura of Time',), 3_000_000, True)],
    8301571: [('Disciple Zurg', '*', 95_000_000, False)],
    8301572: [('True God-King (Body)', '*', 115_000_000, False),
              ('True God-King (Head)', ('Lingering PowerB', 'Lingering PowerC'), 18_000_000, False)],
    8301560: [('Savage Lion Baragan', '*', None, False)],
    8301561: [('Savage Lion Baragan', '*', None, False)],
    8301640: [('Curse Master', '*', None, False)],
    8301641: [('Curse Master', '*', None, False)],
    8301650: [('Warmonger Zaad', '*', None, False)],
    8301651: [('Warmonger Zaad', '*', None, False)],
    8301710: [('Illusionist Melfel', '*', 9_750_000, False)],
    8301711: [('Illusionist Melfel', '*', 42_500_000, False)],
    20001300: [('Chilling Beauty Hilda', '*', 20_000_000, False), ('Chilling Beauty Rene', '*', 20_000_000, False)],
    20001301: [('Chilling Beauty Hilda', '*', 70_000_000, False), ('Chilling Beauty Rene', '*', 70_000_000, False)],
    20001500: [('The Silver Knight Joshua', '*', None, False)],
    20001501: [('The Silver Knight Joshua', '*', None, False)],
    20001600: [('Scorching Blade Roy', '*', None, False)],
    20001601: [('Scorching Blade Roy', '*', None, False)],
    20002201: [('The Righteous Sheriff Gilbert', '*', None, False), ('Wild Hoof', ('Wild Hoof',), 300_000, True)],
    20002202: [('The Righteous Sheriff Gilbert', '*', None, False), ('Wild Hoof', ('Wild Hoof',), 10_000_000, True)],
    25010001: [('Teresa, the Fighting Nun', '*', None, False)],
    25010002: [('Teresa, the Fighting Nun', '*', None, False)],
    8301500: [('Osiris', '*', None, False), ('Claws of Osiris', (), 20_000_000, True)],
    8301470: None, 8301480: None, 8301490: None,     # skeleton waves, below
    8301471: [('Divine Rogue Hetepheres', ('Divine Rogue Hetepheres',), None, False),
              ('Tainted Rivermaw', ('Tainted Rivermaw',), 30_000_000, False)],
    8301472: [('Malvan', '*', 120_000_000, False), ('Divine Rogue Hetepheres', (), 50_000_000, False),
              ('Corrupted Fleshsnapper', (), 20_000_000, True)],
    8301481: [('Polevik', '*', None, False), ('Fledging Necromancer', ('Fledging Necromancer',), 20_000_000, True),
              ('Undead Entity', ('Undead Entity',), 20_000_000, True)],
    8301482: [('Polevik', '*', None, False), ('Fanatic Necromancer', ('Fanatic Necromancer',), 30_000_000, True),
              ('Undead Entity', ('Undead Entity',), 30_000_000, True)],
    8301491: [('Zahhak', '*', None, False)],
    8301492: [('Lulujann', '*', 163_200_000, False)],
}
# Boss HP authored against the wiki's figures (AUTHORED): the Chilling Beauties fight without their script, so each
# beauty's two phases are one bar (8M + 12M; Hard 20M + 50M); 8301472's page gives one 170M, split between Malvan
# and Hetepheres (120M + 50M).
MERGED_HP = {20001300, 20001301, 8301472}
# The Burden of Aesys path openers: three waves of the archive's Skeletons (the wiki's only boss), HP AUTHORED.
SKELETON_PATHS = {8301470: 1_500_000, 8301480: 1_500_000, 8301490: 1_500_000}
# The unit each fighting boss is drawn as, where the dataset names one for a different spelling.
UNIT_OF = {'The Silver Knight Joshua': 850858, 'Scorching Blade Roy': 810858, 'The Righteous Sheriff Gilbert': 840958,
           'Teresa, the Fighting Nun': 830958, 'Chilling Beauty Hilda': 820938,
           'Ptah, Master of Truth': 850688}   # the dataset found no unit; the game has Ptah's own
POOL_PERCENT = 20.0
EVERY_TURN_PERCENT = 50.0
MAX_SKILLS = 24
AI_BASE = 1_000_000_000


def mission_row(mst, mid):
    return next(r for r in next(iter(json.loads((ROOT / 'deploy/mst/mission_mst.json').read_text(
        encoding='utf-8')).values())) if r['j28VNcUW'] == str(mid)) if mst is None else mst[str(mid)]


def templates(missions, skip):
    out = {}
    for m in missions:
        if m['id'] in skip:
            continue
        for s in m['stages']:
            for b in s['battle_monsters']:
                key = b['name'].strip()
                if key not in out or b['id'] < out[key]['id']:
                    out[key] = b
    return out


def tier(energy: int) -> tuple[int, int]:
    atk, df = TIERS[0][1:]
    for e, a, d in TIERS:
        if energy >= e:
            atk, df = a, d
    return atk, df


def pick_skill(ids: list[int], cluster: str) -> int | None:
    """The mission's own row among a name's matches: the one sharing the mission's id prefix, else the largest."""
    if not ids:
        return None
    own = [i for i in ids if str(i).startswith(cluster)]
    return (own or sorted(ids))[-1]


def cluster_of(skills: list[dict]) -> str:
    singles = [str(s['skill_ids'][0]) for s in skills if len(s['skill_ids']) == 1]
    if not singles:
        return ''
    counts = {}
    for s in singles:
        counts[s[:5]] = counts.get(s[:5], 0) + 1
    return max(counts, key=counts.get)


TURN_CYCLE = re.compile(r'turn (\d+)\s*(?:&|and)\s*every (\d+) turns?', re.I)
EVERY = re.compile(r'every (\d+) turns', re.I)
TURN = re.compile(r'(?:^|\b)(?:on |at )?turn (\d+)\b', re.I)
HP_UNDER = re.compile(r'(?:hp|health)\s*(?:<|below|under)\s*(\d+)\s*%|<\s*(\d+)\s*%\s*hp', re.I)
HP_REACHES = re.compile(r'(?:hp|health) reaches (\d+)\s*%', re.I)
EVERY_TURN = re.compile(r'\bevery turn\b|once every turn', re.I)


def trigger(text: str):
    """(conditions, once) the wiki note maps to, or None for the pool."""
    if m := TURN_CYCLE.search(text):
        return [cond('actbetween', int(m.group(2)))], False
    if m := EVERY.search(text):
        return [cond('actbetween', int(m.group(1)))], False
    if m := HP_UNDER.search(text):
        return [cond('hp_pr_under', int(m.group(1) or m.group(2)))], True
    if m := HP_REACHES.search(text):
        return [cond('hp_pr_under', int(m.group(1)) + 1)], True
    if (m := TURN.search(text)) and 'after' not in text.lower():
        return [cond('act', int(m.group(1)))], True
    if EVERY_TURN.search(text):
        return [], False
    return None


def ai_rows(skills: list[dict], phase_flag: bool, relative: bool = False) -> tuple[list, list]:
    """(skill ids in slot order, AI actions) for one monster's skill entries ({id, effect, notes}).

    relative: the monster enters mid-battle (a script's later phase).  The wiki counts its turns from the phase's
    start, the AI's `act` from the battle's (the counter survives the swap, and no term counts a monster's own
    turns), so its "Turn 1" becomes its first chance (once) and any later fixed turn joins the pool (AUTHORED)."""
    timed, pooled, ids = [], [], []
    for s in skills:
        text = '; '.join(s['notes'])
        idle = s['effect'].lower().startswith('idle')
        lethal = 'instantly defeat' in s['effect'].lower()
        t = trigger(text)
        if relative and t is not None and any(c['type'] == 'act' for c in t[0]):
            first = any(c['type'] == 'act' and c['parameter'] == 1 for c in t[0])
            t = ([c for c in t[0] if c['type'] != 'act'], True) if first else None
            if t is None and (idle or lethal):
                continue
        if (idle or lethal) and (t is None or not any(c['type'] == 'act' for c in t[0])):
            continue
        if s['id'] in ids:
            slot = ids.index(s['id']) + 1
        elif len(ids) < MAX_SKILLS:
            ids.append(s['id'])
            slot = len(ids)
        else:
            continue
        self_target = 'to self' in s['effect'].lower() and 'attack' not in s['effect'].lower()
        if t is None:
            pooled.append((slot, self_target))
        else:
            timed.append((slot, t, idle, self_target))
    actions, n = [], 1
    if phase_flag:
        # the script swaps this phase out when AI flag 3 is up (trigger 5); the script keeps it at 1 HP until then
        actions.append(row(n, [cond('hp_pr_under', 2), cond('limited_act', 1)], action='wait', flags=(3, 1)))
        n += 1
    for slot, (conds, once), idle, self_target in timed:
        limit = [cond('limited_act', 1)] if once or idle else [cond('turn_limited_act', 1)]
        # "every turn" (no condition of its own) at EVERY_TURN_PERCENT, so it does not take every action (AUTHORED)
        actions.append(row(n, [cond('skill', slot)] + conds + limit, target=1 if self_target else 2,
                           counts=not idle, percent=100.0 if conds or once else EVERY_TURN_PERCENT))
        n += 1
    for slot, self_target in pooled:
        actions.append(row(n, [cond('skill', slot), cond('turn_limited_act', 1)], percent=POOL_PERCENT,
                           target=1 if self_target else 2))
        n += 1
    actions.append(row(n, [], action='attack'))
    return ids, actions


def boss_row(mid, name, unit, hp, atk, df, ai_id, skill_ids, add):
    return {
        'id': mid, 'name': name, 'unit_id': unit, 'position': '180:302', 'hp': hp, 'atk': atk, 'def': df,
        'ai_id': ai_id, 'act_min': 1, 'act_max': 1 if add else 2, 'wait': 5,
        'unit_drop_id': 0, 'unit_drop_level': 0, 'unit_drop_type': 0, 'unit_drop_chance': 0, 'authored_capture': True,
        # AUTHORED: the archive's richest Vortex boss drops (200 / 125, five rolls); adds the next tier
        'zel_max_drop': 120 if add else 200, 'zel_drop_count': 5, 'karma_max_drop': 75 if add else 125,
        'karma_drop_count': 5,
        'treasure_chest_chance': 0, 'treasure_drops': [],
        'skills': [{'skill_id': i, 'show_name': True} for i in skill_ids], 'act_rate': 50.0,
    }


def section_skills(entry, sections_wanted, claimed, wiki_skills, cluster):
    by_name = {s['name']: s['skill_ids'] for s in wiki_skills}
    out = []
    for sec in entry['sections']:
        mine = (sections_wanted == '*' and sec['section'] not in claimed) or sec['section'] in (
            sections_wanted if sections_wanted != '*' else ())
        if not mine:
            continue
        for k in sec['skills']:
            sid = pick_skill(by_name.get(k['name'], []), cluster)
            if sid is not None:
                out.append({'id': sid, 'effect': k['effect'], 'notes': k['notes'], 'section': sec['section']})
    return out


def phase_of(section: str):
    """The phase a 'Phase N' / 'Phase One' section names, else None (shared by every phase)."""
    m = re.match(r'phase (\d+|one|two|three)\b', section.strip(), re.I)
    if not m:
        return None
    word = m.group(1).lower()
    return int(word) if word.isdigit() else {'one': 1, 'two': 2, 'three': 3}[word]


def build(mission_id, wiki, entry, mst_row, units):
    """(stages, script_monsters, ai records, notes) for one boss trial."""
    energy = int(mst_row['A8DEK5ob'])
    atk, df = tier(energy)
    cluster = cluster_of(wiki['skills'])
    if entry is None:
        # no quest block on the page (8301472): the dataset's matched skills, in one pool for the first boss
        entry = {'sections': [{'section': '', 'facts': [], 'skills': [
            {'name': s['name'], 'effect': '', 'notes': []} for s in wiki['skills']]}]}
    spec = MISSIONS[mission_id]
    claimed = {s for _n, secs, _h, _a in spec if secs != '*' for s in secs}
    phases_hp = list(wiki['phase_hp'])
    ais, script, line, notes = [], [], [], []
    for k, (name, secs, hp, add) in enumerate(spec):
        unit = UNIT_OF.get(name) or next((b['unit'] for b in wiki['bosses'] if b['name'] == name and b['unit']), None)
        unit = unit or STAND_INS.get(name)
        if unit is None or str(unit) not in units:
            raise SystemExit(f'{mission_id} {name}: no unit with art to draw it ({unit})')
        if unit in STAND_INS.values() and name in STAND_INS:
            notes.append(f'{name}: stand-in art {unit}')
        skills = section_skills(entry, secs, claimed, wiki['skills'], cluster) if entry else []
        chain = SCRIPT_CHAINS.get(mission_id) if k == 0 else None
        if chain:
            kind, ids = chain
            per_phase = {p: [s for s in skills if phase_of(s['section']) in (None, p)] for p in (1, 2, 3)}
            hp_list = phases_hp + [phases_hp[-1]] * (3 - len(phases_hp))
            if len(phases_hp) < 3:
                notes.append(f'phase 3 HP {hp_list[2]:,} AUTHORED (the wiki gives {len(phases_hp)} phase(s))')
            for p, mid in enumerate(ids, start=1):
                sids, actions = ai_rows(per_phase[p], kind == 'flag' and p < len(ids), relative=p > 1)
                ai_id = AI_BASE + (mission_id % 100_000_000) * 10 + p
                ais.append({'id': ai_id, 'name': f'{wiki["name"]} - {name} phase {p}', 'actions': actions})
                b = boss_row(mid, name, unit, hp_list[p - 1], atk, df, ai_id, sids, False)
                (line if p == 1 else script).append(b)
            continue
        if hp is None:
            hp = (phases_hp[k] if k < len(phases_hp) else None) or BLANK_HP.get(mission_id)
            if hp is None:
                raise SystemExit(f'{mission_id} {name}: no HP')
            if mission_id in BLANK_HP and not phases_hp:
                notes.append(f'{name}: HP {hp:,} AUTHORED')
        sids, actions = ai_rows(skills, False)
        ai_id = AI_BASE + (mission_id % 100_000_000) * 10 + 4 + k   # 1-3 are a chain's phases
        ais.append({'id': ai_id, 'name': f'{wiki["name"]} - {name}', 'actions': actions})
        mid = SCRIPT_IDS.get(mission_id) if k == (1 if mission_id == 8301471 else 0) else None
        line.append(boss_row(mid or mission_id * 10 + k + 1, name, unit, hp, atk if not add else atk // 2,
                             df if not add else df // 2, ai_id, sids, add))
    for b, pos in zip(line, FORMATION[len(line)]):
        b['position'] = pos
    for b in script:
        b['position'] = line[0]['position']
    first = 100 if wiki['rules'].get('enemies_move_first') else 0
    return [{'is_boss': True, 'first_attack_rate': first, 'battle_monsters': line}], script, ais, notes


def generate(missions_raw: bytes, ais_raw: bytes, mst=None):
    missions = json.loads(missions_raw)
    ai_records = {a['id']: a for a in json.loads(ais_raw)}
    pool = templates(missions, set(TRIALS) | set(MISSIONS))
    by_id = {m['id']: m for m in missions}
    wiki = {m['mission']: m for m in json.loads(WIKI.read_text(encoding='utf-8'))['missions']}
    skills = {m['mission']: m for m in json.loads(SKILLS.read_text(encoding='utf-8'))['missions']}
    units = {str(u['id']) for u in json.loads((ARCHIVE / 'unit.json').read_text(encoding='utf-8'))}
    report = []

    def place(mid, record):
        if mid in by_id:
            missions[missions.index(by_id[mid])] = record
        else:
            missions.append(record)
            by_id[mid] = record

    def base_record(mid, row_):
        return {'id': mid, 'name': row_['0iAIR2LP'].strip(), 'zel': int(row_['Rs7bCE3t']),
                'karma': int(row_['HTVh8a65']), 'exp': int(row_['d96tuT2E']), 'energy_cost': int(row_['A8DEK5ob'])}

    copied = dict(TRIALS)
    for mid, hp in SKELETON_PATHS.items():
        copied[mid] = [[('Skeleton', 3)], [('Skeleton', 3)], [('Skeleton King', 1), ('Skeleton', 2)]]
    for mid, waves in copied.items():
        row_ = mission_row(mst, mid)
        stages = []
        for i, wave in enumerate(waves):
            line = []
            for spec in wave:
                name, count = spec[0], spec[1]
                if name not in pool:
                    raise SystemExit(f'mission {mid}: no archived row fields {name!r} to copy')
                for _ in range(count):
                    b = copy.deepcopy(pool[name])
                    if len(spec) > 2:
                        b['id'], b['name'], b['unit_id'] = spec[2], spec[3], spec[4]
                    b.pop('material_drops', None)
                    b['unit_drop_id'], b['unit_drop_level'] = b['unit_id'], max(b.get('unit_drop_level') or 0, 1)
                    b['unit_drop_chance'] = max(b.get('unit_drop_chance') or 0, CAPTURE)
                    if mid in SKELETON_PATHS:
                        b['hp'] = SKELETON_PATHS[mid] * (2 if i == len(waves) - 1 else 1)
                    line.append(b)
            for b, pos in zip(line, FORMATION[len(line)]):
                b['position'] = pos
            stages.append({'is_boss': i == len(waves) - 1, 'first_attack_rate': 0, 'battle_monsters': line})
        record = dict(base_record(mid, row_), stages=stages)
        if len(stages) != int(row_['69vnphig']):
            raise SystemExit(f'mission {mid}: {len(stages)} waves but mission_mst says {row_["69vnphig"]}')
        place(mid, record)
        report.append((mid, record['name'], [[b['name'].strip() for b in s['battle_monsters']] for s in stages], []))

    for mid, spec in MISSIONS.items():
        if spec is None:
            continue
        row_ = mission_row(mst, mid)
        stages, script, ais, notes = build(mid, wiki[mid], skills.get(mid), row_, units)
        if len(stages) != int(row_['69vnphig']):
            raise SystemExit(f'mission {mid}: {len(stages)} waves but mission_mst says {row_["69vnphig"]}')
        record = dict(base_record(mid, row_), stages=stages)
        if script:
            record['script_monsters'] = script
        place(mid, record)
        for a in ais:
            ai_records[a['id']] = a
        report.append((mid, record['name'], [[f"{b['name']} {b['hp']:,}" for b in s['battle_monsters']]
                                             for s in stages] + ([[f"script {b['id']} {b['hp']:,}"
                                                                   for b in script]] if script else []), notes))
    missions.sort(key=lambda m: m['id'])
    return (dump(missions, 1, False), dump([ai_records[k] for k in sorted(ai_records)], 4, True), report)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--archive', type=Path, default=ARCHIVE)
    ap.add_argument('--out', type=Path)
    a = ap.parse_args()
    missions_raw = (a.archive / 'mission.json').read_bytes()
    ais_raw = (a.archive / 'ai.json').read_bytes()
    new_missions, new_ais, report = generate(missions_raw, ais_raw)
    for mid, name, waves, notes in report:
        print(f'  {mid} {name}: {waves}' + (f'  [{"; ".join(notes)}]' if notes else ''))
    stale = [n for n, old, new in (('mission.json', missions_raw, new_missions), ('ai.json', ais_raw, new_ais))
             if old != new]
    if a.check:
        print('up to date' if not stale else 'stale: ' + ', '.join(stale))
        return 1 if stale else 0
    if a.out is None:
        sys.exit('nothing written: pass --out DIR')
    a.out.mkdir(parents=True, exist_ok=True)
    for name, data in (('mission.json', new_missions), ('ai.json', new_ais)):
        # temp + rename: a QA mirror's files are hard links into deploy/, and an in-place write goes through them
        # (2026-11-03: it did, and deploy's archive was restored from the release staging copy)
        tmp = a.out / f'.{name}.tmp'
        tmp.write_bytes(data)
        os.replace(tmp, a.out / name)
    print(f'wrote {a.out / "mission.json"}, {a.out / "ai.json"}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
