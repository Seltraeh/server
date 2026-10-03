"""Writes deploy/archive/brave_slots.json -- the Brave Slots payout table (#27).

python scripts/gen_brave_slots_table.py          # write
python scripts/gen_brave_slots_table.py --check  # exit 1 if the file differs

PRIZE LIST: the Global wiki "Slots" page, revision 658840 (2020-07-27),
https://bravefrontierglobal.fandom.com/wiki/Slots -- twelve matchings, several
"Either (one)" of a named set.  Each variant is its own row here, sharing the
matching's reels.  Every id is resolved by name from the client's own text
(tools/sgtext) and exists in deploy/mst.

REEL PICTURES: the machine's picture list (deploy/system/brave_slots.json
rY6j0Jvs): 81 j_god, 1 s_flog, 7 allup_unit, 2 b_flog, 8 c_god, 6 pup_unit,
3 b_emperor, 18 sphere, 9 g_god, 80 m_god, 13/12/15 item, 82 medal -- the same
file names the wiki's matchings show.  Two wiki rows use a "Secret" picture this
machine does not have: Jewel King stays on g_god (9) and the Raid Medal on a
medal in the third reel, both AUTHORED.

WEIGHTS ARE AUTHORED.  Neither the wiki nor any client table carries the odds;
these keep the old table's tiers (rarest first) and are not a claim about the
live game's rates.  Removed from the old table because the wiki does not list
them: Elementum Tome x5 and the Ignis Shard consolation.  The machine cannot
express "no prize" (loadBraveSlotArchive), so every pull pays one of these.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'deploy' / 'archive' / 'brave_slots.json'

UNIT, ITEM, SPHERE, MEDAL = 6, 4, 7, 12   # present types (4/5/7 item/material/sphere)


def row(key, reels, rank, present_type, target_id, count, description, weight):
    return {'key': key, 'weight': weight, 'reels': reels, 'prize_rank': str(rank),
            'present_type': present_type, 'target_id': target_id, 'target_cnt': count,
            'description': description}


def table():
    rows = [row('jewel_god', [81, 81, 81], 4, UNIT, 50133, 1, 'Jewel God', 1),
            row('jewel_king', [9, 9, 9], 3, UNIT, 50132, 1, 'Jewel King', 3)]
    for uid, name in ((10204, 'Fire God'), (20204, 'Water God'), (30204, 'Earth God'),
                      (40204, 'Thunder God'), (50204, 'Light God'), (60134, 'Metal God')):
        rows.append(row('god_' + name.split()[0].lower(), [80, 80, 80], 3, UNIT, uid, 1, name, 1))
    rows += [row('almighty_imp', [7, 7, 7], 3, UNIT, 50612, 1, 'Almighty Imp Arton', 5),
             row('sphere_frog', [1, 1, 1], 2, UNIT, 20302, 1, 'Sphere Frog', 10),
             row('burst_emperor', [3, 3, 3], 2, UNIT, 10313, 1, 'Burst Emperor', 16),
             row('burst_frog', [2, 2, 2], 2, UNIT, 10312, 1, 'Burst Frog', 25)]
    for uid, name in ((10452, 'Power Imp Pakpak'), (20442, 'Guard Imp Ganju'),
                      (30432, 'Healing Imp Fwahl'), (40432, 'Vigor Imp Molin')):
        rows.append(row('imp_' + name.split()[0].lower(), [6, 6, 6], 2, UNIT, uid, 1, name, 5))
    for uid, name in ((10344, 'Fire Crystal'), (20334, 'Water Crystal'), (30324, 'Earth Crystal'),
                      (40324, 'Thunder Crystal'), (50364, 'Light Crystal'), (60334, 'Dark Crystal')):
        rows.append(row('crystal_' + name.split()[0].lower(), [8, 8, 8], 2, UNIT, uid, 1, name, 5))
    spheres = ((30008, 'Sparkle Edge'), (30108, 'Sky Shield'), (30208, 'Ruler Staff'), (30308, 'Cosmos Armor'),
               (30007, 'Carnage Edge'), (30107, 'Ruler Shield'), (30207, 'Nature Staff'), (30307, 'Odd Armor'),
               (30006, 'Chosen Blade'), (30106, 'Grand Shield'), (30206, 'Worship Cane'), (30306, 'Dark Armor'),
               (30005, 'God Sword'), (30105, 'Dogma Shield'), (30205, 'Order Staff'), (30305, 'Ember Armor'),
               (30004, 'Divine Sword'), (30104, 'King Shield'), (30204, 'Godly Staff'), (30304, 'God Armor'),
               (30003, 'Dragon Blade'), (30103, 'Star Shield'), (30203, 'Sky Staff'), (30303, 'Shiny Armor'))
    for iid, name in spheres:
        rows.append(row('sphere_' + str(iid), [18, 18, 18], 2, SPHERE, iid, 1, name, 1))
    for iid, count, name in ((70000, 10, 'Beacon'), (70100, 3, 'Smoke Bomb'), (70200, 1, 'Teleport'),
                             (70300, 3, 'God Crystal'), (70400, 2, 'Atk Crystal'), (70500, 2, 'Def Crystal'),
                             (70600, 5, 'Detector')):
        rows.append(row('item_' + str(iid), [13, 13, 13], 1, ITEM, iid, count, f'{name} x{count}', 6))
    rows.append(row('raid_medal', [1, 7, 82], 1, MEDAL, 1, 1, '1 Raid Medal', 200))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    text = json.dumps(table(), indent=4, ensure_ascii=False) + '\n'
    if args.check:
        same = OUT.read_text(encoding='utf-8') == text
        print('brave_slots.json is ' + ('current' if same else 'OUT OF DATE'))
        return 0 if same else 1
    OUT.write_text(text, encoding='utf-8', newline='\n')
    print(f'wrote {OUT} ({len(table())} rows)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
