"""Research Lab AI regressions against the client-AI model (scripts/ai_model.py).

    python scripts/test_research_lab_ai.py

Reads the live deploy/archive/ai.json and mission.json, so it checks what the
server will actually send.  Evidence level: a pass means the authored rows
implement the documented behaviour under the DECODED client grammar -- it is
not an in-game observation.
"""
import json
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ai_model import AuthoringError, Monster  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
AIS = {a['id']: a for a in json.loads((ROOT / 'deploy/archive/ai.json').read_text(encoding='utf-8'))}
LAB = {m['id']: m for m in json.loads((ROOT / 'deploy/archive/mission.json').read_text(encoding='utf-8'))
       if m['id'] in (2000000, 2000001, 2000002)}
MISSION = LAB[2000002]
MONSTERS = {m['id']: m for rec in LAB.values()
            for m in rec['stages'][0]['battle_monsters'] + rec.get('script_monsters', [])}

# Skill indexes, 1-based in each monster's `skills` order.
MAGNETRON, GRAVITON, NEOPLASMA = 1, 2, 3
BANISHMENT, NECRO_CURSE, BLACK_FORCE, EVIL_HOLE, DEATH_GATE = 1, 2, 3, 4, 5
GENESIS, RUNE, SACRED_SONG, DESTINY, METEOR, RESURRECTION, ENDLESS = 1, 2, 3, 4, 5, 6, 7

failures = 0


def check(label, cond, detail=''):
    global failures
    if cond:
        print(f'PASS  {label}')
    else:
        failures += 1
        print(f'FAIL  {label}  {detail}')


def make(monster_id, seed=1):
    m = MONSTERS[monster_id]
    return Monster(AIS[m['ai_id']], act_min=m['act_min'], act_max=m['act_max'],
                   act_rate=m.get('act_rate', 100.0), max_hp=m['hp'], rng=random.Random(seed))


def skills_used(log):
    return [s for kind, s in log if kind == 'skill']


def special(log, specials):
    return [s for s in skills_used(log) if s in specials]


# --- archive wiring ---------------------------------------------------------
check('mission 2000002 is one battle (MissionMst battle_count 1)', len(MISSION['stages']) == 1)
check('Juggernaut opens the battle; Abaddon and Maxwell are scripted',
      [m['id'] for m in MISSION['stages'][0]['battle_monsters']] == [1002]
      and [m['id'] for m in MISSION['script_monsters']] == [1102, 54501])
check('skill lists are the Trial band',
      [s['skill_id'] for s in MONSTERS[1002]['skills']] == [1902, 1904, 1903]
      and [s['skill_id'] for s in MONSTERS[1102]['skills']] == [1954, 1955, 1952, 1953, 1956]
      and [s['skill_id'] for s in MONSTERS[54501]['skills']] == [2052, 2056, 2054, 2059, 2055, 2057, 2058])
check('Maxwell HP is the wiki value', MONSTERS[54501]['hp'] == 1_000_000)

# --- Maxwell's 20-turn schedule (Template:Maxwell Turns) ---------------------
SCHEDULED = {SACRED_SONG, RUNE, DESTINY}
expected = {4: [SACRED_SONG, RUNE], 5: [DESTINY], 8: [SACRED_SONG, RUNE], 10: [DESTINY],
            12: [SACRED_SONG, RUNE], 15: [DESTINY], 16: [SACRED_SONG, RUNE],
            20: [DESTINY, SACRED_SONG, RUNE]}
mx = make(54501)
mx.set_hp_pct(90)
seen = {}
for turn in range(1, 41):
    log = mx.take_turn(turn)
    seen[turn] = log
    want = expected.get(((turn - 1) % 20) + 1, [])
    got = special(log, SCHEDULED)
    if got != want:
        check(f'Maxwell turn {turn} scheduled casts', False, f'want {want} got {got}')
        break
else:
    check('Maxwell turns 1-40 cast Sacred Song+Rune on 4k, Destiny on 5k, all three on 20k', True)
check('a scheduled turn is only the schedule (turn_end closes it)',
      all(len(seen[t]) == len(expected[((t - 1) % 20) + 1]) + 1 for t in (4, 5, 20, 24, 25, 40)),
      str(seen[20]))
check('non-scheduled turns take exactly one action above 70%',
      all(len(seen[t]) == 1 and seen[t][0][1] in (None, GENESIS) for t in (1, 2, 3, 6, 7, 9)))

# --- Maxwell HP thresholds ------------------------------------------------------
def maxwell_at(pct, turn, seed=3, force=None, fresh=True, mx=None):
    mx = mx or make(54501, seed)
    mx.set_hp_pct(pct)
    return mx, mx.take_turn(turn, force_percent=force)

_, log = maxwell_at(30.0, 5)
check('HP exactly 30%: Destiny once', special(log, {DESTINY}) == [DESTINY], str(log))
_, log = maxwell_at(29.99, 5)
check('HP 29.99%: Destiny twice', special(log, {DESTINY}) == [DESTINY, DESTINY], str(log))
_, log = maxwell_at(20.0, 3)
check('HP exactly 20%: no Endless', ENDLESS not in skills_used(log), str(log))
m, log = maxwell_at(19.99, 3)
check('HP 19.99%: Endless', skills_used(log) == [ENDLESS], str(log))
log = m.take_turn(6)
check('Endless is once only', ENDLESS not in skills_used(log), str(log))

_, log = maxwell_at(19.0, 4)
check('Endless on a Sacred Song turn replaces Sacred Song + Rune', skills_used(log) == [ENDLESS], str(log))
_, log = maxwell_at(25.0, 5)
check('HP 25% on turn 5: Destiny twice, then the turn ends', skills_used(log) == [DESTINY, DESTINY], str(log))
_, log = maxwell_at(19.0, 20)
check('turn 20 below 20%: Destiny twice, then Endless, no Sacred Song/Rune',
      skills_used(log) == [DESTINY, DESTINY, ENDLESS], str(log))

# 70% and 50% gates: forced rolls show which random skills are eligible.
_, log = maxwell_at(70.0, 1, force=True)
check('HP exactly 70%: Meteor not yet eligible', METEOR not in skills_used(log), str(log))
m = make(54501, 5)
m.set_hp_pct(69.9)
pool = {skills_used(m.take_turn(t, force_percent=True))[0] for t in (1, 2, 3)}
check('HP 69.9%: Meteor eligible (forced roll)', METEOR in pool, str(pool))
m = make(54501, 5)
m.set_hp_pct(49.9)
check('HP 49.9%: Resurrection outranks Meteor when both roll', skills_used(m.take_turn(1, force_percent=True)) == [RESURRECTION])

# Several thresholds crossed before one enemy turn (100% -> 15%).
m = make(54501, 7)
m.set_hp_pct(15)
turns = [skills_used(m.take_turn(t)) for t in (1, 2, 3)]
check('100%->15% in one hit: Endless on the first turn after the crossing', turns[0] == [ENDLESS], str(turns))
check('... the 30% Genesis follows on the next free turn', turns[1] == [GENESIS], str(turns))
check('... and Endless does not repeat', ENDLESS not in turns[2], str(turns))

# Late arrival: the party turn counter, not the monster's, drives the cycle.
m = make(54501, 9)
m.set_hp_pct(95)
casts = {t: special(m.take_turn(t), SCHEDULED) for t in range(13, 21)}
check('Maxwell arriving at turn 13 still casts on 15, 16 and 20',
      casts[15] == [DESTINY] and casts[16] == [SACRED_SONG, RUNE]
      and casts[20] == [DESTINY, SACRED_SONG, RUNE] and casts[13] == [] and casts[14] == [], str(casts))

# --- Demon Abaddon -------------------------------------------------------------
def abaddon_turn(pct_series, seed=11, start_turn=1):
    a = make(1102, seed)
    out = []
    for i, pct in enumerate(pct_series):
        a.set_hp_pct(pct)
        out.append(a.take_turn(start_turn + i))
    return out

logs = abaddon_turn([50.0, 49.9, 45, 29.9, 25, 10])
dg = [skills_used(l).count(DEATH_GATE) for l in logs]
check('Abaddon at exactly 50%: no Death Gate', dg[0] == 0, str(dg))
check('Abaddon 49.9%: Death Gate twice in one phase, then never again at 45%', dg[1] == 2 and dg[2] == 0, str(dg))
check('Abaddon 29.9%: Death Gate four times, once only', dg[3] == 4 and dg[4] == 0 and dg[5] == 0, str(dg))
check('Death Gates are free: the regular budget still acts after a burst',
      len(logs[1]) > 2 and len(logs[3]) > 4, f'{len(logs[1])} {len(logs[3])}')

logs = abaddon_turn([60, 25, 25, 25])
dg = [skills_used(l).count(DEATH_GATE) for l in logs]
check('60% -> 25% in one hit: four Death Gates, the unused 50% burst retires (authored)',
      dg == [0, 4, 0, 0], str(dg))

logs = abaddon_turn([90] * 12, start_turn=7)
eh = [t for t, l in zip(range(7, 19), logs) if EVIL_HOLE in skills_used(l)]
check('Abaddon arriving at turn 7: Evil Hole on 8, 12 and 16 only', eh == [8, 12, 16], str(eh))

# --- Juggernaut -----------------------------------------------------------------
j = make(1002, 13)
j.set_hp_pct(80)
neo = {t: skills_used(j.take_turn(t)).count(NEOPLASMA) for t in range(1, 17)}
check('Neoplasma Blast once on 4, 8, 12, 16 even with a second action',
      all(neo[t] == (1 if t % 4 == 0 else 0) for t in neo), str(neo))
j = make(1002, 17)
counts = [len(j.take_turn(t)) for t in range(1, 200)]
check('Juggernaut takes 1-2 actions (act_rate 50)', set(counts) == {1, 2}, str(set(counts)))

# --- Trial No. 001: Brave Knight Karl -> Ice Warrior Karl ------------------------
FROZEN_AXE, FLOOD, VERDANT, LIGHTNING = 1, 2, 3, 4
IW_VERDANT, IW_LIGHTNING, FLASHING, BLUE_EXEC, CURSED_SLASH, TIDAL = 1, 2, 3, 4, 5, 6
check('Trial 001: Karl opens; Ice Warrior Karl is scripted',
      [m['id'] for m in LAB[2000000]['stages'][0]['battle_monsters']] == [22651]
      and [m['id'] for m in LAB[2000000]['script_monsters']] == [22701])
check('Trial 001: HP 400,000 then 500,000 (wiki)', MONSTERS[22651]['hp'] == 400_000 and MONSTERS[22701]['hp'] == 500_000)

k = make(22651, 21)
k.set_hp_pct(90)
axes = {t: skills_used(k.take_turn(t)).count(FROZEN_AXE) for t in range(1, 21)}
check('Brave Knight Karl: Frozen Axe once on 5, 10, 15, 20', all(axes[t] == (1 if t % 5 == 0 else 0) for t in axes), str(axes))
k = make(22651, 22)
k.set_hp_pct(75.0)
pool = set(skills_used(k.take_turn(1, force_percent=True)))
check('BKK at exactly 75%: no Flood Offensive, no axes', not pool & {FLOOD, VERDANT, LIGHTNING}, str(pool))
k.set_hp_pct(74.9)
log = k.take_turn(2, force_percent=True)
check('BKK at 74.9%: Flood Offensive (free) then an axe', skills_used(log)[:2] == [FLOOD, VERDANT], str(log))
check('... Flood Offensive is once only', FLOOD not in skills_used(k.take_turn(3, force_percent=True)))

iw = make(22701, 23)
iw.set_hp_pct(100)
check('Ice Warrior Karl at full HP: only the two axes or an attack',
      set(skills_used(iw.take_turn(1, force_percent=True))) <= {IW_VERDANT, IW_LIGHTNING})
iw.set_hp_pct(50.0)
check('IWK at exactly 50%: no Blue Execution', BLUE_EXEC not in skills_used(iw.take_turn(2, force_percent=True)))
iw.set_hp_pct(49.9)
check('IWK at 49.9%: Blue Execution', skills_used(iw.take_turn(3))[:1] == [BLUE_EXEC])
check('... once only', BLUE_EXEC not in skills_used(iw.take_turn(4, force_percent=True)))
iw.set_hp_pct(14.9)
log = iw.take_turn(5, force_percent=True)
check('IWK at 14.9%: Tidal Offensive (free) first', skills_used(log)[:1] == [TIDAL], str(log))
check('... once only', TIDAL not in skills_used(iw.take_turn(6, force_percent=True)))

# --- Trial No. 002: Grahdens ------------------------------------------------------
G = dict(IGNITE=1, SLASH=2, ROCK=3, VOLCANO=4, SOUL=5, TIRED=6, SACRED=7, SHOWER=8, HOLY=9,
         DARKNESS=10, GAZE=11, LIGHT=12, VANISH=13, APOC=14, BARRAGE=15, STEEL=16, ZERO=17)
check('Trial 002: Grahdens alone, 500,000 HP', [m['id'] for m in LAB[2000001]['stages'][0]['battle_monsters']] == [63051]
      and 'script_monsters' not in LAB[2000001] and MONSTERS[63051]['hp'] == 500_000)

g = make(63051, 31)
g.set_hp_pct(100)
log = [skills_used(g.take_turn(t)) for t in range(1, 7)]
check('Grahdens turn 1: Soul Rejection', log[0] == [G['SOUL']], str(log[0]))
check('turn 6 at full HP: the break ("I\'m tired")', log[5] == [G['TIRED']], str(log[5]))
check('... it opens the after-break pool (flag 2)', g.flags[1] == 1)
after = {skills_used(g.take_turn(t, force_percent=True))[0] for t in (7, 8)}
check('after the break, Holy Light leads the forced pool', after == {G['HOLY']}, str(after))

g = make(63051, 32)
g.set_hp_pct(69.0)
turns = [skills_used(g.take_turn(t)) for t in range(1, 8)]
check('below 70% before turn 6: no break at all', not any(G['TIRED'] in t for t in turns), str(turns))

g = make(63051, 33)
g.set_hp_pct(80.0)
check('exactly 80%: no Darkness Change', G['DARKNESS'] not in skills_used(g.take_turn(2)))
seq = []
for pct in (79.9, 69.9, 39.9, 29.9, 19.9, 14.9, 4.9):
    g.set_hp_pct(pct)
    seq.append(skills_used(g.take_turn(2 + len(seq) + 1)))
check('HP ladder one step at a time: 80 Darkness, 70 Soul, 40 Apocalypse, 30 Barrage, 20 Shower+Steel, 15 Sacred, 5 Shower',
      seq[0][:1] == [G['DARKNESS']] and seq[1][:1] == [G['SOUL']] and seq[2][:1] == [G['APOC']]
      and seq[3][:1] == [G['BARRAGE']] and seq[4][:2] == [G['SHOWER'], G['STEEL']]
      and seq[5][:1] == [G['SACRED']] and seq[6][:1] == [G['SHOWER']], str(seq))

g = make(63051, 34)
g.set_hp_pct(10)
cascade = [skills_used(g.take_turn(t)) for t in range(2, 7)]
check('100% -> 10% in one hit: one threshold move per turn, in threshold order',
      [c[0] for c in cascade[:4]] == [G['DARKNESS'], G['SOUL'], G['APOC'], G['BARRAGE']]
      and cascade[3][:4] == [G['BARRAGE'], G['SHOWER'], G['STEEL'], G['SACRED']], str(cascade))

g = make(63051, 35)
g.set_hp_pct(0.5)
tail = [g.take_turn(t) for t in range(2, 6)]
check('below 1%: Apocalypse Zero, then two idle turns, then normal',
      skills_used(tail[0])[:1] == [G['ZERO']] and tail[1] == [('wait', None)] and tail[2] == [('wait', None)]
      and tail[3][0][0] != 'wait' and G['ZERO'] not in skills_used(tail[3]), str(tail))

# --- The Creation God (2100004) ---------------------------------------------------
# Wiki rev 663846 + Template:MaxwellSZTurns rev 535212; structure from the
# client's F_MISSION_SCRIPT_MST row 2100004 (see docs/features/CREATION_GOD.md).
CG = {m['id']: m for m in json.loads((ROOT / 'deploy/archive/mission.json').read_text(encoding='utf-8'))
      if m['id'] == 2100004}[2100004]
CGM = {m['id']: m for st in CG['stages'] for m in st['battle_monsters']}
CGM.update({m['id']: m for m in CG.get('script_monsters', [])})


def cg(monster_id, seed=1):
    m = CGM[monster_id]
    return Monster(AIS[m['ai_id']], act_min=m['act_min'], act_max=m['act_max'],
                   act_rate=m.get('act_rate', 100.0), max_hp=m['hp'], rng=random.Random(seed))


# The ids the client's own script names: 7=1004 / 6=1104 in battle 1,
# 7=54508 / 6=5000851 in battle 2.  changeMonster finds them by id alone.
check('Creation God: two battles, opening on 1004 and 54508 (MissionMst battle_count 2)',
      [[m['id'] for m in st['battle_monsters']] for st in CG['stages']] == [[1004], [54508]]
      and [st['is_boss'] for st in CG['stages']] == [False, True], str(CG['stages'][0].keys()))
check('Creation God: Abaddon 1104 and Inception God Maxwell 5000851 are script_monsters',
      [m['id'] for m in CG['script_monsters']] == [1104, 5000851])
check('Creation God: HP is the wiki\'s 4M / 6M / 13M / 22.2M',
      [CGM[i]['hp'] for i in (1004, 1104, 54508, 5000851)] == [4_000_000, 6_000_000, 13_000_000, 22_200_000])
check('Creation God: skills are the re-issue band 5001050-5001094',
      [s['skill_id'] for s in CGM[1004]['skills']] == [5001050, 5001052, 5001053, 5001051]
      and [s['skill_id'] for s in CGM[1104]['skills']] == [5001060, 5001061, 5001062, 5001063, 5001065, 5001064, 5001066]
      and [s['skill_id'] for s in CGM[54508]['skills']] == [5001070, 5001074, 5001072, 5001077, 5001073, 5001075, 5001076]
      and [s['skill_id'] for s in CGM[5000851]['skills']] == [5001080, 5001085, 5001083, 5001084, 5001086, 5001087,
                                                               5001094, 5001082, 5001090, 5001091, 5001092, 5001093])
check('Creation God: both Maxwells are unit-backed (50525, 51147)',
      CGM[54508]['unit_id'] == 50525 and CGM[5000851]['unit_id'] == 51147
      and 'visuals' not in CGM[54508] and 'visuals' not in CGM[5000851])

# Juggernaut 1004 -- 1 Magnetron, 2 Graviton, 3 Magnetism, 4 Neoplasma.
J_NEO = 4
j = cg(1004, 21)
j.set_hp_pct(80)
neo = {t: skills_used(j.take_turn(t)).count(J_NEO) for t in range(1, 21)}
check('CG Juggernaut: Neoplasma Blast once on every 4th turn and never between',
      all(neo[t] == (1 if t % 4 == 0 else 0) for t in neo), str(neo))
j = cg(1004, 22)
pool = set()
for t in range(1, 60):
    pool |= set(skills_used(j.take_turn(t, force_percent=None)))
check('CG Juggernaut: Magnetron, Graviton and Magnetism all turn up', {1, 2, 3} <= pool, str(pool))

# Demon Abaddon 1104 -- 6 Banishment every 3 turns, 7 Death Gate x4 below
# 66% and x4 below 30%.
A_BAN, A_DG = 6, 7


def cg_abaddon(pcts, start_turn=1, seed=31):
    a = cg(1104, seed)
    out = []
    for i, pct in enumerate(pcts):
        a.set_hp_pct(pct)
        out.append(a.take_turn(start_turn + i))
    return out


logs = cg_abaddon([66.0, 65.9, 50, 29.9, 25, 10])
dg = [skills_used(l).count(A_DG) for l in logs]
check('CG Abaddon: exactly 66% -> no Death Gate; 65.9% -> four in one turn; none at 50%',
      dg[:3] == [4 * 0, 4, 0], str(dg))
check('CG Abaddon: 29.9% -> four more, once only', dg[3:] == [4, 0, 0], str(dg))
logs = cg_abaddon([70, 29.9, 25, 10])
dg = [skills_used(l).count(A_DG) for l in logs]
check('CG Abaddon: 70% -> 29.9% in one hit is four Death Gates, not eight (authored precedence)',
      dg == [0, 4, 0, 0], str(dg))
# Swapped in on turn 5 (Juggernaut fell on turn 4): the count is the fight's.
logs = cg_abaddon([90] * 10, start_turn=5)
ban = [t for t, l in zip(range(5, 15), logs) if A_BAN in skills_used(l)]
check('CG Abaddon arriving on turn 5: Banishment on 6, 9 and 12 (count from the fight start)',
      ban == [6, 9, 12], str(ban))

# Creator Maxwell 54508 -- 1 Genesis, 2 Rune, 3 Sacred Song, 4 Destiny,
# 5 Meteor, 6 Resurrection, 7 Endless.
C_GEN, C_RUNE, C_SONG, C_DEST, C_MET, C_RES, C_END = 1, 2, 3, 4, 5, 6, 7
C_SCHED = {C_SONG, C_RUNE, C_DEST}
m = cg(54508, 41)
m.set_hp_pct(95)
bad = []
for turn in range(1, 41):
    log = m.take_turn(turn)
    want = sorted(([C_SONG, C_RUNE] if turn % 4 == 0 else [])
                  + [C_DEST] * ((turn % 5 == 0) + (turn % 10 == 0)))
    got = sorted(special(log, C_SCHED))
    if got != want:
        bad.append((turn, want, got))
    if want and len(log) != len(want) + 1:     # the schedule, then turn_end
        bad.append((turn, 'extra action', log))
check('CG Creator Maxwell: Song+Rune every 4th, Destiny every 5th and twice every 10th, over 40 turns',
      not bad, str(bad[:3]))
m = cg(54508, 42)
m.set_hp_pct(75.0)
check('CG Creator Maxwell: exactly 75% -> no Resurrection', C_RES not in skills_used(m.take_turn(1)))
m.set_hp_pct(70.0)
t2 = skills_used(m.take_turn(2))
check('CG Creator Maxwell: exactly 70% -> Resurrection (<75%), no Endless', t2 == [C_RES], str(t2))
m.set_hp_pct(69.9)
t3 = skills_used(m.take_turn(3))
t6 = skills_used(m.take_turn(6))
check('CG Creator Maxwell: 69.9% -> Endless once; neither repeats',
      t3 == [C_END] and C_END not in t6 and C_RES not in t6, f'{t3} {t6}')
m = cg(54508, 43)
m.set_hp_pct(69.9)
t8 = skills_used(m.take_turn(8))
t9 = skills_used(m.take_turn(9))
check('CG Creator Maxwell: crossing 70% on a Sacred Song turn keeps the schedule; Endless comes next turn',
      sorted(t8) == sorted([C_SONG, C_RUNE]) and t9 == [C_END], f'{t8} {t9}')

# Inception God Maxwell 5000851.
I_PG, I_CM, I_VG, I_SA, I_AR, I_DR, I_OD, I_EX, I_RO, I_RB, I_DC, I_UC = range(1, 13)
I_SCHED = {I_SA, I_AR, I_DR}
m = cg(5000851, 51)
m.set_hp_pct(95)
bad = []
for turn in range(13, 41):          # Creator Maxwell fell on turn 12
    log = m.take_turn(turn)
    used = skills_used(log)
    want = sorted(([I_SA, I_AR] if turn % 4 == 0 else [])
                  + [I_DR] * ((turn % 5 == 0) + (turn % 10 == 0)))
    if sorted(special(log, I_SCHED)) != want or used.count(I_PG) != 1 or len(log) != len(want) + 2:
        bad.append((turn, want, log))
check('CG Inception God: the fight\'s turn count drives SA+Akashic (4th), Destiny Rune (5th, twice 10th); '
      'Primal Genesis and one action every turn', not bad, str(bad[:2]))
dr_rows = [r for r in AIS[CGM[5000851]['ai_id']]['actions']
           if {'type': 'skill', 'parameter': I_DR} in r['self_conditions']]
check('CG Inception God: the two Destiny Runes hit the highest- and the lowest-HP unit',
      [r['search_term'] for r in dr_rows] == ['hp_max', 'hp_min'], str([r['search_term'] for r in dr_rows]))
m = cg(5000851, 52)
m.set_hp_pct(50.0)
check('CG Inception God: exactly 50% -> no Verse Genesis (forced rolls)',
      I_VG not in skills_used(m.take_turn(1, force_percent=True)))
m.set_hp_pct(49.9)
check('CG Inception God: 49.9% -> Verse Genesis eligible (forced rolls)',
      I_VG in skills_used(m.take_turn(2, force_percent=True)))

# Overdrive below 15%, then Existence, each turn cancelling everything else.
m = cg(5000851, 53)
m.set_hp_pct(15.0)
check('CG Inception God: exactly 15% -> no Overdrive', I_OD not in skills_used(m.take_turn(1)))
m.set_hp_pct(14.9)
od = m.take_turn(20)                 # a turn with the whole schedule due
ex = m.take_turn(21)
after = m.take_turn(22)
check('CG Inception God: 14.9% -> Overdrive alone, even on turn 20', od == [('skill', I_OD)], str(od))
check('CG Inception God: the next turn is Existence alone', ex == [('skill', I_EX)], str(ex))
check('CG Inception God: then normal play, and neither repeats',
      I_OD not in skills_used(after) and I_EX not in skills_used(after) and I_PG in skills_used(after), str(after))

# UBB answers.
m = cg(5000851, 54)
m.set_hp_pct(80)
answers = []
for turn in (1, 2, 3, 6, 7):
    m.opposing = {'ubb_use'}
    answers.append([s for s in skills_used(m.take_turn(turn)) if s in (I_RO, I_RB, I_DC, I_UC)])
check('CG Inception God: each UBB -> Return to the Origin, then the 1st/2nd/3rd buff; nothing new after',
      answers == [[I_RO, I_RB], [I_RO, I_DC], [I_RO, I_UC], [I_RO], [I_RO]], str(answers))
m = cg(5000851, 55)
m.set_hp_pct(80)
quiet = [s for t in range(1, 11) for s in skills_used(m.take_turn(t)) if s in (I_RO, I_RB, I_DC, I_UC)]
check('CG Inception God: no UBB, no answer', quiet == [], str(quiet))
m = cg(5000851, 56)
m.set_hp_pct(14.9)
m.take_turn(1)                       # Overdrive
m.opposing = {'ubb_use'}             # the player's UBB in the Overdrive window
ex = m.take_turn(2)
m.opposing = set()
check('CG Inception God: a UBB in the Overdrive window is not answered (Existence turn)',
      ex == [('skill', I_EX)], str(ex))
m.opposing = {'sbb_use'}
check('CG Inception God: an SBB does not count as a UBB',
      not {I_RO, I_RB} & set(skills_used(m.take_turn(3))))

for record in (AIS[CGM[i]['ai_id']] for i in (1004, 1104, 54508, 5000851)):
    m = Monster(record, act_min=1, act_max=3)
    try:
        for turn in range(1, 41):
            for pct in (100, 69, 49, 29, 14, 5):
                m.set_hp_pct(pct)
                m.opposing = {'ubb_use'} if turn % 3 == 0 else set()
                m.take_turn(turn)
        check(f'CG AI {record["id"]} never hangs, UBBs included', True)
    except AuthoringError as e:
        check(f'CG AI {record["id"]} never hangs', False, str(e))

# --- authoring hazards -----------------------------------------------------------
bad = {'id': 1, 'name': 'endless free row', 'actions': [
    {'priority': 1, 'percent': 100.0, 'act_target': 2, 'search_term': 'random',
     'self_conditions': [{'type': 'skill', 'parameter': 1}], 'party_conditions': [],
     'action': {'type': 'skill', 'flag_changes': [], 'unknown_bool': False,
                'unknown_int_1': 0, 'unknown_int_2': 0}}]}
try:
    Monster(bad).take_turn(1)
    check('an unlimited free row is caught', False)
except AuthoringError:
    check('an unlimited free row is caught', True)

for record in AIS.values():
    m = Monster(record, act_min=1, act_max=3)
    try:
        for turn in range(1, 41):
            for pct in (100, 69, 49, 29, 19, 5):
                m.set_hp_pct(pct)
                m.take_turn(turn)
        check(f'AI {record["id"]} never hangs across 40 turns x 6 HP bands', True)
    except AuthoringError as e:
        check(f'AI {record["id"]} never hangs', False, str(e))

print(f'\n{failures} failure(s)')
sys.exit(1 if failures else 0)
