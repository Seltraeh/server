"""Author Randall's Battle Simulator (Summoners' Training Ground, mission 6000000).

Idempotent: rerunning replaces the same records.  `--check` reports whether
deploy/archive/mission.json and ai.json already match without writing anything.

    python scripts/gen_battle_simulator.py [--check]

WHAT THE SIMULATOR IS (evidence, 2026-09-29)
  * Global wiki "Summoners' Training Ground" rev 599590 (2018-12-02), read
    through the MediaWiki API: the Battle Simulator in the Survey Office of
    Imperial Capital Randall.  Six enemies, Enemy 1 cannot be switched off,
    element / HP / DEF / self-heal / revival / BC-HC / crit and weakness
    resistance are chosen by the player, "enemies will not attack back",
    items are fixed at 99 and not taken from the inventory, no energy.
  * libgame.so (arm64): GameUtils::isSandbag @0x1EC18D8 is
    DungeonMst.dungeon_type == 6, and the only such dungeon is 6000000
    (F_MISSION_MST 6000000 "Summoners' Training Ground": one battle, energy 0,
    zel/karma/exp 0).  SandbagStartConnectScene::initConnect @0x15D8D80 sends an
    ordinary MissionStart for it; SandbagResultScene::initConnect @0x15BD1E4
    sends NOTHING, so MissionStart is the simulator's whole server contract.
  * MonsterParty::entryMonstersSandbag @0x10C1504 walks the battle's monster
    group slot by slot, builds "1000000" + (element - 1) from the player's
    element setting and calls MonsterUnit::initialize(.., that id)
    @0x1155FDC, which dereferences MonsterMstList::getObject(id) without a
    null check.  The six dummies are therefore monsters 10000000-10000005
    ("Enemy 1".."Enemy 6" in the client's own monster string table), and
    every one of them must be described by the MissionStart response.

WHY IT CRASHED.  No archive record existed, so MissionStart served mission 10's
five waves relabelled as 6000000 (the BATTLE-CONTENT FALLBACK).  None of the
dummy ids were in the response, so the first MonsterUnit::initialize read a
null MonsterMst.

WHAT IS AUTHORED HERE (not recovered verbatim; the original F_MONSTER_MST rows
are not among the decoded tables)
  * Art: the client content ships exactly one dummy atlas per element,
    monster/img/unit_anime_{1..6}0000.png, sharing monster/cgg/unit_cgg_10000.csv
    and an idle-only monster/cgs/unit_idle_cgs_10000.csv (no attack animation).
    They match the wiki's screenshots (File:Battle simulator 1.jpg / 3.jpg:
    orange = Fire ... purple = Dark) and the sandbag_enemy01-06.png setting
    icons.  Mapping element e -> unit_anime_{e}0000.png is asset-matched.
  * THE DUMMIES NEVER ACT, by AI: their own AI 60000001 has one row, action
    `turn_end`.  BattleUnit::setAiTargetList @0x10FD940 matches that word as an
    inlined 64-bit constant (@0x10FE4CC, which is why there is no literal),
    targets the monster itself and sets action type 4: the turn ends with no
    move and no attack.  That is the wiki's "enemies will not attack back", and
    the client content agrees -- the dummies ship an idle animation and no
    move or attack one.
    // CORRECTED 2026-10-02.  This used to rest on act_min = act_max = 0 with
    // AI 1 "attack random", reading MonsterUnit::initTurnChild @0x1159098
    // (min + one roll per step in [min, max)) as "0/0 never acts".  It does
    // act: the first action of a turn is not gated by that count, which only
    // grants extra ones (BattleUnit::initMove, loopEndWait).  In the client
    // the dummies attacked with no attack art -- frozen for the attack, then
    // snapping back from the player's line (player report 2026-10-02).
    act_min/act_max stay 0; nothing reads them for a monster whose only AI row
    ends its turn.
  * HP 100,000,000 (the wiki's lowest HP setting) and DEF 0 are placeholders:
    entryMonstersSandbag overwrites base HP and DEF from the player's settings.
  * Positions: the six-enemy formation five archived stages already use.
  * No drops, no capture (unit_id 0, authored_capture), no boss stage.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MISSIONS = ROOT / 'deploy/archive/mission.json'
AIS = ROOT / 'deploy/archive/ai.json'
MISSION_ID = 6000000
FIRST_DUMMY = 10000000
DUMMY_AI = 60000001
POSITIONS = ('160:222', '245:258', '245:342', '160:378', '75:342', '75:258')
ELEMENTS = ('Fire', 'Water', 'Earth', 'Thunder', 'Light', 'Dark')


def dummy(slot):
    element = slot + 1
    return {
        'id': FIRST_DUMMY + slot,
        'name': f'Enemy {element}',
        'unit_id': 0,
        'visuals': {
            'img_a': f'unit_anime_{element}0000.png',
            'anm_cgg': 'unit_cgg_10000.csv',
            'cgs_idle': 'unit_idle_cgs_10000.csv',
            'cgs_move': '',
            'cgs_atk': '',
            'cgs_skill': '',
            'element': element,
            'effect_frame': '24:124:1',
            'damage_frame': '30:100:2:1',
            'drop_check_count': 1,
            'move_speed': 0,
            'attack_move_type': 0,
            'back_move_type': 0,
            'after_image': 0,
            'skill_move_type': 0,
            # The target reticle: PlayerParty::lockOn centres it on
            # BattleUnit::getTargetCursorDispPos = the touch rect's centre
            # plus this offset, and GameSprite::setPosition flips with
            # layerHeight - y, so battle coordinates are y-DOWN: positive
            # moves the reticle down.  Calibrated in-client 2026-10-02 from two
            # screenshots: at "0,-40" (a default every archived special
            # monster carries) the reticle sat ~110 px ABOVE the dummy's body,
            # and at "0,70" ~101 px BELOW it.  These are battle units, not
            # screen pixels -- the layer is 320 units wide, ~1.9 px per unit
            # on the player's 609-px window -- so both shots agree on ~+17.
            # ("0,70" came from adding the 110 px to -40 one-for-one.)
            # Production rows are "0,0" or small positives ("0,10".."0,20").
            'cursor_disp_pos': '0,17',
            'hp_disp_pos': '0,-60',
        },
        'position': POSITIONS[slot],
        'hp': 100_000_000,
        'atk': 0,
        'def': 0,
        'ai_id': DUMMY_AI,
        'act_min': 0,
        'act_max': 0,
        'wait': 5,
        'unit_drop_id': 0,
        'unit_drop_level': 0,
        'unit_drop_type': 0,
        'unit_drop_chance': 0,
        'authored_capture': True,
        'zel_max_drop': 0,
        'zel_drop_count': 0,
        'karma_max_drop': 0,
        'karma_drop_count': 0,
        'treasure_chest_chance': 0,
        'treasure_drops': [],
    }


def record():
    return {
        'id': MISSION_ID,
        'name': "Summoners' Training Ground",
        'zel': 0,
        'karma': 0,
        'exp': 0,
        'energy_cost': 0,
        'stages': [{
            'is_boss': False,
            'first_attack_rate': 0,
            'battle_monsters': [dummy(slot) for slot in range(len(ELEMENTS))],
        }],
    }


def ai_record():
    """The dummies' AI: end the turn, every turn (see the module note)."""
    return {
        'id': DUMMY_AI,
        'name': 'Training dummy - never acts',
        'actions': [{
            'priority': 1,
            'percent': 100.0,
            'act_target': 2,
            'search_term': 'random',
            'self_conditions': [],
            'party_conditions': [],
            'action': {'type': 'turn_end', 'flag_changes': [], 'unknown_bool': True,
                       'unknown_int_1': 0, 'unknown_int_2': 0},
        }],
    }


def dump(data, indent):
    """Each archive's own layout: CRLF, and mission.json at 1 space, ai.json at 4."""
    text = json.dumps(data, indent=indent, ensure_ascii=False).replace('\n', '\r\n') + '\r\n'
    return text.encode('utf-8')


def updated(path, entry, indent):
    """(old bytes, new bytes) for an id-keyed archive list with `entry` replaced."""
    raw = path.read_bytes()
    by_id = {m['id']: m for m in json.loads(raw)}
    by_id[entry['id']] = entry
    return raw, dump([by_id[k] for k in sorted(by_id)], indent)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    changes = [(MISSIONS, *updated(MISSIONS, record(), 1)), (AIS, *updated(AIS, ai_record(), 4))]
    stale = [path.name for path, raw, new in changes if new != raw]
    if args.check:
        print('up to date' if not stale else f'stale: {", ".join(stale)}')
        return 1 if stale else 0
    for path, raw, new in changes:
        if new != raw:
            path.write_bytes(new)
            print(f'wrote {path.name}')
    if not stale:
        print('nothing to write (already current)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
