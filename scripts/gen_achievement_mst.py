"""Port the three Randall Achievement tables out of the decoded MST dump.

The Achievement screen (GetAchievementInfo, YPBU7MD8) answered with the signal
key alone because none of its master data had ever been ported.  All three
tables are in the decoded dump and none of them was in deploy/mst.

Output (wrapper keys from GameResponseParser::parseBodyTag, which is the channel
that carries them -- see the handbook: these tags are NOT in getResponseObject,
they are written to the client's own MST file by DataMstManager::save*):

    deploy/mst/achievement_subject_mst.json       H9ATfJ38
    deploy/mst/achievement_trade_mst.json         82CcMZhp
    deploy/mst/achievement_deliver_rate_mst.json  1tJiqKgZ

Two filters, both deliberate:

  * ROW 0 IS A HEADER.  The shipped tables open with a row whose every cell is
    the literal column name ("SUBJECT_NAME", "NEED_MISSION_ID", ...).  It is
    data-dump residue, not a record.
  * EXPIRED SUBJECT ROWS ARE DROPPED.  15,173 subjects and 32,500 trade offers is nine
    years of seasonal events; `SzV0Nps7` (end date) cuts that to what a player
    today could actually see.  The client checks these dates itself, so leaving
    the rest in would only cost bandwidth -- but it would also make the tables
    unreadable to anyone auditing them later.

Trade offers instead use the newest row per reward, with a fixed offline
availability window. Original prices and limits are retained.

The trade shop is additionally curated: an offer whose reward cannot be granted
by this server is dropped rather than listed, so nothing in the shop takes
Merit Points for something that will not arrive.  Reward shape is the shared
present vocabulary, `qBAb07rh` = "<type>:<id>:<count>:<a>:<b>".

The alternate-art offers (type 15) are in that shop and ARE granted -- see the
CHECKED/GRANTABLE note below; they were dropped until 2026-09-13 only because
the unlock had not been traced.

usage: python tools/gen_achievement_mst.py [--dry]
"""
from pathlib import Path
import json
import sys
import argparse

ROOT = Path(__file__).resolve().parent.parent
SRC = None  # Set by --source; decoded dumps are external inputs.
DST = ROOT / 'deploy' / 'mst'

# Anything still open on this date survives.  Fixed rather than "today" so the
# generated tables are reproducible and a re-run does not silently shrink them.
CUTOFF = '2026-09-13 00:00:00'

SUBJECT_KEY = 'H9ATfJ38'
TRADE_KEY = '82CcMZhp'
RATE_KEY = '1tJiqKgZ'

# Reward types the AchievementTrade handler can actually grant, and where this
# server checks the id.  3 zel, 8 gem and 11 karma have no id to check.
#
# Type 15 is ALTERNATE ART -- the "Alternate Art: Shion" family, 25 offers.  It
# is a per-species unlock, not a unit: the art itself already ships with the
# client as unit_ills_full_<id>_2.png / unit_ills_thum_<id>_2.png, and what the
# server grants is permission to use it.  UserUnitDictionaryList::isUnitImgType
# @0x12B1D80 is the entire gate -- it returns getImgTypeFlg() == 1, and that
# member is written by exactly one thing, `2pAyFjmZ` on the unit-dictionary row
# (UserUnitDictionaryResponse::readParam @0x1404D60).  WHICH variant is showing
# is then a client-local preference (UserUnitDictionaryList::unitImgTypeSave /
# unitImgTypeLoad), so the server never needs to store or send it.
#
# So these offers are checked against the ART rather than against a unit pool:
# an id with no _2 files is dropped, because the toggle would appear and swap
# to a texture that does not exist.
CHECKED = {'4': 'item', '5': 'item', '6': 'unit', '7': 'item', '15': 'art'}
GRANTABLE = {'3', '4', '5', '6', '7', '8', '11', '15'}

# The 991/992/993 id bands are ACCOUNT RECOVERY, not shop stock: 146 rows all
# named "Recover: <thing>", all priced 5,000, covering the starter units and
# their evolution materials.  They are what a GM hands back to a player who lost
# an account, and putting them on sale would both bury the seven real offers and
# turn Merit Points into a shortcut to any unit in the starter tree.  Set
# INCLUDE_RECOVERY = True if that catalogue is ever wanted.
INCLUDE_RECOVERY = False
RECOVERY_BANDS = ('991', '992', '993')


def load(glob):
    rows = []
    for p in sorted(SRC.glob(glob)):
        rows += json.loads(p.read_text(encoding='utf-8'))
    return rows


def live(rows, idkey):
    """Drop the header row and everything already expired."""
    out = []
    for r in rows:
        if r[idkey] == '0':
            continue
        if r.get('SzV0Nps7', '') < CUTOFF:
            continue
        out.append(r)
    return out


def offline_trades(rows):
    """Keep the newest offer per reward, including expired monthly stock.

    This is an offline catalogue: original prices and purchase limits survive,
    but one fixed offer replaces years of repeated dated rotations. Recovery
    entries are kept separate so they cannot displace regular shop stock.
    """
    latest = {}
    for row in rows:
        if row['Mdgsh04u'] == '0':
            continue
        reward = tuple(row['qBAb07rh'].split(':')[:3])
        recovery = row['Mdgsh04u'].startswith(RECOVERY_BANDS)
        key = (reward, recovery)
        rank = (row.get('qA7M9EjP', ''), int(row['Mdgsh04u']))
        old = latest.get(key)
        if old is None or rank > (old.get('qA7M9EjP', ''), int(old['Mdgsh04u'])):
            latest[key] = row
    return [dict(row, qA7M9EjP='2020-01-01 00:00:00',
                 SzV0Nps7='2037-12-31 23:59:59') for row in latest.values()]


def known_ids():
    units = {str(u['id']) for u in json.loads(
        (ROOT / 'deploy' / 'archive' / 'unit.json').read_text(encoding='utf-8'))}
    items_doc = json.loads((DST / 'item_mst.json').read_text(encoding='utf-8'))
    items = {r['kixHbe54'] for r in next(v for v in items_doc.values() if isinstance(v, list))}
    return units, items


def art_ids():
    """Species whose alternate illustration actually ships in game_content.

    Both files matter: the detail page swaps unit_ills_full_<id>_2.png and the
    guide list swaps unit_ills_thum_<id>_2.png, and a missing one is a blank
    sprite rather than an error.  60 species have them; the shop sells 25.
    """
    img = ROOT / 'deploy' / 'game_content' / 'content' / 'unit' / 'img'
    full = {p.stem[len('unit_ills_full_'):-len('_2')]
            for p in img.glob('unit_ills_full_*_2.png')}
    thum = {p.stem[len('unit_ills_thum_'):-len('_2')]
            for p in img.glob('unit_ills_thum_*_2.png')}
    return full & thum


def main():
    global SRC
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Decoded MST directory')
    parser.add_argument('--dry', action='store_true')
    args = parser.parse_args()
    SRC, dry = args.source, args.dry
    if not SRC.is_dir():
        parser.error('Decoded MST directory does not exist')
    units, items = known_ids()
    art = art_ids()

    subjects = live(load('F_ACHIEVEMENT_SUBJECT_MST_*.json'), 'M7SXoc31')
    subjects.sort(key=lambda r: int(r['M7SXoc31']))

    trades = offline_trades(load('F_ACHIEVEMENT_TRADE_MST_*.json'))
    kept, dropped = [], {}
    for r in trades:
        parts = r['qBAb07rh'].split(':')
        kind = parts[0] if parts else ''
        target = parts[1] if len(parts) > 1 else ''
        if not INCLUDE_RECOVERY and r['Mdgsh04u'][:3] in RECOVERY_BANDS:
            dropped.setdefault('account-recovery band', 0)
            dropped['account-recovery band'] += 1
            continue
        # A nameless offer draws a blank tile.
        if not r['hQ1SJZU9'].strip():
            dropped.setdefault('no name', 0)
            dropped['no name'] += 1
            continue
        if kind not in GRANTABLE:
            dropped.setdefault(f'reward type {kind}', 0)
            dropped[f'reward type {kind}'] += 1
            continue
        # Two rows price a unit at 0 Merit Points ("Haido", "Recover: Haido");
        # they are account-recovery entries, not shop stock, and listing them
        # would put a free unit in the shop.
        if r['3EWLm0sA'] == '0':
            dropped.setdefault('free (price 0)', 0)
            dropped['free (price 0)'] += 1
            continue
        pool = CHECKED.get(kind)
        if pool == 'unit' and target not in units:
            dropped.setdefault(f'unit {target}', 0)
            dropped[f'unit {target}'] += 1
            continue
        if pool == 'art' and target not in art:
            dropped.setdefault(f'no alternate art for unit {target}', 0)
            dropped[f'no alternate art for unit {target}'] += 1
            continue
        if pool == 'item' and target not in items:
            dropped.setdefault(f'item {target}', 0)
            dropped[f'item {target}'] += 1
            continue
        kept.append(r)
    kept.sort(key=lambda r: int(r['Mdgsh04u']))

    rates = [r for r in load('F_ACHIEVEMENT_DELIVER_RATE_MST_*.json')
             if r['232id0qQ'] != 'PARAM']

    out = {
        'achievement_subject_mst.json': (SUBJECT_KEY, subjects),
        'achievement_trade_mst.json': (TRADE_KEY, kept),
        'achievement_deliver_rate_mst.json': (RATE_KEY, rates),
    }
    for name, (key, rows) in out.items():
        print(f'{name:38s} {key}  {len(rows)} rows')
        if not dry:
            (DST / name).write_text(
                json.dumps({key: rows}, ensure_ascii=False, indent=1) + '\n',
                encoding='utf-8', newline='\n')

    print(f'\ntrade offers dropped as unsellable here: '
          f'{sum(dropped.values())} across {len(dropped)} target(s)')
    for target, n in sorted(dropped.items(), key=lambda kv: -kv[1])[:10]:
        print(f'    {target}  x{n}')
    if dry:
        print('dry run: nothing written')


if __name__ == '__main__':
    main()
