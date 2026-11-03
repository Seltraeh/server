"""Author the per-hit material drops (BattleMonster.material_drops) from the Global wiki.

    python scripts/gen_material_drops.py --check [--archive DIR]   # exit 1 if DIR/mission.json differs
    python scripts/gen_material_drops.py --out PATH [--archive DIR] # write the result to PATH
    python scripts/gen_material_drops.py extract                    # rebuild the sources file from tools/

Idempotent: only `material_drops` is written, every other field of every mission is kept byte for byte, and
a monster the policy gives nothing loses the key.  There is no default write target: --out names one.

RELEASE ORDER.  The server only understands `material_drops` from candidate 92f41576 (round 8) on; the
live f72310df's strict archive LoadJson refuses a mission.json carrying it.  Write deploy/archive/mission.json
with this ONLY after that server (or a later one) is installed.

THE CHANNEL (round 8, docs/CLAUDE_FRESH_ROUND8_HANDOFF_2026-10-31.md section 3b).  Each monster's list rides
in its BattleMonsterGroupMst row as i30R8TAs, `per:itemId:maxCount`, and MissionEnd reports what dropped,
which the server credits.  What the client does with it (arm64 libgame.so):
  * Each hit picks ONE entry at random, GameUtils::getBattleRandom(0, getDropMaretialCnt() - 1) in
    DamageData::update @0x103CBC0, and rolls only that entry's percent (BattleUnit::getDropMaterialPer
    @0x111F7B0: less the monster's resist, times a mission bonus, plus drop-rate buffs such as Fuu's; doubled
    by the same per-hit flag that doubles the crystal/Zel/Karma rolls).  So a list of n entries gives each
    entry about per/n per hit.
  * `maxCount` is the ONLY limit, and it is per monster and per entry: setDropRest @0x114A784 stores the
    parsed int as is, decDropRest @0x114A81C floors it at 0, and getDropMaterialPer returns 0 once it runs
    out.  The field (BattleItemList::setMaterial), the pickup (BattleItemMaterial::exitItemMove) and the
    reward list (BattleRewardList::addItem) add no cap of their own, and BattleGroupDataMst's
    maxItemDrop has no reader.
  * Storage: GameUtils::incWarehouseItem @0x118A0A8 fills a stack up to ItemMst::getMaxNum (999 for
    materials) and opens a new stack for the rest; gme/common/Warehouse.cpp does the same.  A full item box
    only blocks STARTING a quest (GameScene::isOverItemBox @0x163B490); it never stops a drop.

EVIDENCE
  * One live MissionStart survives: deploy/log/http_log_jE6Sp0q4_20260513_184738.log (27 more logs replay
    the same body).  It is mission 10, Start of Adventure: every regular monster carries
    "7:10000:3,7:10300:3" (Green Grass, Green Drop) and the boss "7:10301:5,3:10603:1" (Blue Drop, Thunder
    Eye).  Mission 10 is served exactly that.  The RATES below are that capture's, the only ones known.
  * The wiki's dungeon pages ({{Quest}} and {{Zone:Footer}}, cached by tools/wiki_zone_scrape.py): the
    footer's `drops` is the dungeon's pool, its `dropsrare` the dungeon's rare drops, and a quest's own
    `drops` names that mission's special drop.  Mission 10 matches the capture on all three: its quest drop
    is Thunder Eye (the boss's 3 % entry), and Green Grass, Green Drop and Blue Drop are in the pool.
  * The wiki's material pages (`howToObtain`, cached by tools/wiki_item_scrape.py) name the missions an item
    drops in, sometimes the enemy that drops it ("Sword of Fire - Lava", "Douglas (up to 2)").  Mission 10's
    only entry is Blue Drop, which the capture puts on the boss.  Entries that are not battle drops (clear
    and completion rewards, CBP and other chests, exchanges, the Raid Battle and Event Bazaar regions) are
    skipped.  An item with any enemy or chest entry is a BOSS ITEM: rare, and never part of an area's
    random drops.
  * The area tables are the union of an area's dungeon pools and of its material-page items that are not
    boss items: what drops at random in that area.  They stand in for a dungeon the wiki gives no pool.

POLICY (authored from that evidence; the wiki names items, never rates)
  * Regular monsters: two pool items at 7 %, up to 3 each.  All regulars of a mission share the pair, as in
    the capture; the pair rotates with the mission's place in its dungeon so the dungeon covers its pool.
  * The boss (the highest-HP monster of the boss wave; its escorts are regulars): the mission's
    material-page items, or else the next pool item, at 7 %, up to 5; then the quest's special drops and one
    rotating dungeon rare at 3 %, up to 1.  A boss item listed for the mission goes on the enemy its entry
    names when the mission fields it (else on the boss) at 3 %, up to 1 or the page's "up to N".
  * Only materials (ItemMst kind 2) that exist in deploy/mst/item_mst.json: MissionEnd refuses an unknown
    item, and consumables such as Divine Light belong in chests.
  * No lists for Mimics (they are the treasure), script monsters, Mimic chests, or the Battle Simulator.
    Missions the sources say nothing about are left alone.  Treasure chests are not touched.

THE CAPS ARE THE LIVE SERVER'S, ON PURPOSE (Evan, 2026-11-01: "Match whatever the client dictates as its
limit").  The client has no limit of its own beyond maxCount, so the truth is what the live server sent it,
and the capture is the only sample: 3 per regular entry, 5 for the boss's common entry, 1 for a rare.  Do not
lower them.  With a level-capped squad they decide the yield (client run v7: mission 10 paid Green Grass 28,
Green Drop 25, Blue Drop 5; mission 11 Green Claw 35, Don Nut 33, Blue Drop 5, Light Eye 1), as they did live.
"""
import argparse
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MISSIONS = ROOT / 'deploy' / 'archive' / 'mission.json'
ITEM_MST = ROOT / 'deploy' / 'mst' / 'item_mst.json'
MISSION_MST = ROOT / 'deploy' / 'mst' / 'mission_mst.json'
SOURCES = ROOT / 'scripts' / 'data' / 'material_drop_sources.json'
TOOLS = ROOT / 'tools'
CAPTURE_LOG = ROOT / 'deploy' / 'log' / 'http_log_jE6Sp0q4_20260513_184738.log'

COMMON, COMMON_MAX, BOSS_COMMON_MAX = 7, 3, 5    # the capture's pool entries
RARE, RARE_MAX = 3, 1                            # the capture's special entry
MATERIAL = '2'                                   # ItemMst h0K7wjeH
SIMULATOR = 6000000


# ---------------------------------------------------------------------------------------------------------
# Generation (reads only the committed sources file and deploy/)
# ---------------------------------------------------------------------------------------------------------

def materials():
    rows = next(iter(json.loads(ITEM_MST.read_text(encoding='utf-8-sig')).values()))
    return {int(r['kixHbe54']) for r in rows if r.get('h0K7wjeH') == MATERIAL}


def dungeon_of():
    rows = next(iter(json.loads(MISSION_MST.read_text(encoding='utf-8-sig')).values()))
    return {int(r['j28VNcUW']): int(r['MHx05sXt']) for r in rows}, \
        {int(r['j28VNcUW']): int(r['VjCY7rX4']) for r in rows}


def entry(per, item_id, max_count):
    return {'per': per, 'item_id': item_id, 'max_count': max_count}


def dedupe(entries):
    seen, out = set(), []
    for e in entries:
        if e['item_id'] not in seen:
            seen.add(e['item_id'])
            out.append(e)
    return out


def is_mimic(monster):
    return 'mimic' in monster['name'].lower()


def boss_of(mission):
    """(boss monster, its stage): the highest-HP monster of the boss wave (the last wave when none is)."""
    stages = [s for s in mission['stages'] if s['is_boss']] or mission['stages'][-1:]
    best = None
    for s in stages:
        for b in s['battle_monsters']:
            if not is_mimic(b) and (best is None or b['hp'] > best['hp']):
                best = b
    return best


def named(monsters, enemy):
    """The mission's monster an annotation names: exact name, else the name ending in it ("Flame King Goltovah")."""
    want = enemy.strip().lower()
    for b in monsters:
        if b['name'].strip().lower() == want:
            return b
    for b in monsters:
        if b['name'].strip().lower().endswith(' ' + want):
            return b
    return None


def plan(mission, k, src, mats, report):
    """{id(monster dict): [entries]} for one mission, or None when the sources say nothing."""
    mid = str(mission['id'])
    m = src['missions'].get(mid, {})
    d = src['dungeons'].get(str(src['_dungeon'].get(mission['id'])), {})
    a = src['areas'].get(str(src['_area'].get(mission['id'])), {})
    ok = lambda ids: [i for i in ids if i in mats]   # noqa: E731
    for i in (m.get('special', []) + m.get('page_items', []) + d.get('common', []) + d.get('rare', [])
              + a.get('common', []) + [e[0] for e in m.get('enemy_items', [])]):
        if i not in mats:
            report['not_material'].add(i)
    pool = ok(d.get('common', [])) or ok(a.get('common', []))
    page, special, rare = ok(m.get('page_items', [])), ok(m.get('special', [])), ok(d.get('rare', []))
    enemy_items = [e for e in m.get('enemy_items', []) if e[0] in mats]
    monsters = [b for s in mission['stages'] for b in s['battle_monsters'] if not is_mimic(b)]
    boss = boss_of(mission)
    capture = src['capture']
    if mission['id'] == capture['mission_id']:
        regular = [entry(*e) for e in capture['regular']]
        lists = {id(b): list(regular) for b in monsters}
        lists[id(boss)] = [entry(*e) for e in capture['boss']]
        report['capture'] += 1
        return lists
    if not (pool or page or special or rare or enemy_items):
        return None
    n = len(pool)
    regular = [entry(COMMON, pool[(2 * k + j) % n], COMMON_MAX) for j in range(min(2, n))] if n else []
    boss_list = [entry(COMMON, i, BOSS_COMMON_MAX) for i in page]
    if not boss_list and n:
        boss_list = [entry(COMMON, pool[(2 * k + 2) % n], BOSS_COMMON_MAX)]
    boss_list += [entry(RARE, i, RARE_MAX) for i in special]
    if rare:
        boss_list.append(entry(RARE, rare[k % len(rare)], RARE_MAX))
    lists = {id(b): list(regular) for b in monsters}
    if boss is not None:
        lists[id(boss)] = boss_list
    for item_id, enemy, max_count in enemy_items:
        target = named(monsters, enemy) if enemy else None
        if enemy and target is None:
            report['enemy_missing'].add((mission['id'], enemy))
        target = target or boss
        if target is not None:
            lists[id(target)] = lists[id(target)] + [entry(RARE, item_id, max_count or RARE_MAX)]
    return {key: dedupe(v) for key, v in lists.items()}


def generate(raw, src=None, mats=None):
    """New mission.json bytes for archive bytes `raw`, and a report dict."""
    src = dict(src or json.loads(SOURCES.read_text(encoding='utf-8')))
    src['_dungeon'], src['_area'] = dungeon_of()
    mats = materials() if mats is None else mats
    missions = json.loads(raw)
    report = {'missions': 0, 'monsters': 0, 'capture': 0, 'not_material': set(), 'enemy_missing': set()}
    by_dungeon = {}
    for mission in sorted(missions, key=lambda x: x['id']):
        by_dungeon.setdefault(src['_dungeon'].get(mission['id']), []).append(mission['id'])
    rank = {mid: k for ids in by_dungeon.values() for k, mid in enumerate(ids)}
    for mission in missions:
        lists = None if mission['id'] == SIMULATOR else plan(mission, rank[mission['id']], src, mats, report)
        written = 0
        for stage in mission['stages']:
            for b in stage['battle_monsters']:
                drops = (lists or {}).get(id(b))
                if drops:
                    b['material_drops'] = drops
                    written += 1
                else:
                    b.pop('material_drops', None)
        if written:
            report['missions'] += 1
            report['monsters'] += written
    text = json.dumps(missions, indent=1, ensure_ascii=False).replace('\n', '\r\n') + '\r\n'
    return text.encode('utf-8'), report


def main_generate(args):
    path = args.archive / MISSIONS.name
    raw = path.read_bytes()
    new, report = generate(raw)
    print(f"{report['missions']} mission(s), {report['monsters']} monster list(s) "
          f"(mission {SIMULATOR} and Mimics excluded; {report['capture']} from the live capture)")
    if report['not_material']:
        print(f"  left out (not a material in item_mst): {sorted(report['not_material'])}")
    if report['enemy_missing']:
        print(f"  {len(report['enemy_missing'])} named enemy entr(ies) not fielded; given to the boss")
    if args.check:
        print('up to date' if new == raw else f'stale: {path}')
        return 0 if new == raw else 1
    if args.out is None:
        sys.exit('nothing written: pass --out PATH (see RELEASE ORDER in the docstring)')
    if args.out.exists() and args.out.read_bytes() == new:
        print(f'nothing to write ({args.out} already current)')
        return 0
    # temp + rename: a QA mirror's files are hard links into deploy/, and an in-place write goes through them
    tmp = args.out.with_name(f'.{args.out.name}.tmp')
    tmp.write_bytes(new)
    os.replace(tmp, args.out)
    print(f'wrote {args.out}')
    return 0


# ---------------------------------------------------------------------------------------------------------
# Extraction (reads the git-ignored tools/ caches; run when they are refreshed)
# ---------------------------------------------------------------------------------------------------------

def norm(s):
    return re.sub(r'\s+', ' ', s.replace("'''", '').replace('’', "'")).strip().lower()


def tsv(name):
    out = {}
    for line in (TOOLS / 'sgtext' / name).read_text(encoding='utf-8').splitlines():
        parts = line.split('\t')
        if len(parts) >= 2 and parts[0].isdigit():
            out[int(parts[0])] = parts[1]
    return out


def item_index():
    """Lower-cased English name -> material id (client strings, then ItemMst English, then the wiki overlay)."""
    index = {}
    for iid, name in tsv('material.tsv').items():
        index.setdefault(norm(name), iid)
    rows = next(iter(json.loads(ITEM_MST.read_text(encoding='utf-8-sig')).values()))
    for r in rows:
        if r.get('h0K7wjeH') == MATERIAL:
            index.setdefault(norm(r.get('kP3zJ9Ra', '')), int(r['kixHbe54']))
    overlay = TOOLS / 'wiki_items' / 'item_names.json'
    if overlay.is_file():
        for iid, name in json.loads(overlay.read_text(encoding='utf-8')).items():
            index.setdefault(norm(name), int(iid))
    return index


NOT_A_DROP = re.compile(r'reward|clear|chest|exchange|\bcbp\b|achiev|complet', re.I)
# Raid Battle and the Event Bazaar are their own modes (raids switch per-hit drops off client-side); a story
# mission may still be CALLED a raid ("Dragon Disciple Raid"), so the wiki's region decides, not the wording.
NOT_A_DROP_REGION = {'raid', 'event bazaar'}


def parse_obtain(text):
    """'Sword of Fire - Lava' -> [(mission, enemy|None, up_to|None)] candidates, most literal first."""
    up_to = re.search(r'up to (\d+)', text, re.I)
    up_to = int(up_to.group(1)) if up_to else None
    base = re.sub(r'\(.*?\)', '', text.replace("'''", '')).strip()
    out = [(base, None, up_to)]
    for sep in (' - ', ': '):
        if sep in base:
            mission, enemy = base.split(sep, 1)
            out.append((mission.strip(), enemy.strip(), up_to))
    return out


def enemies_of(text):
    """'Mega, Emilia, Lava, and Douglas' -> ['Mega', 'Emilia', 'Lava', 'Douglas']."""
    parts = re.split(r',\s*(?:and\s+)?|\s+and\s+', text)
    return [p.strip() for p in parts if p.strip() and len(p.strip()) < 40]


def capture():
    """The surviving live MissionStart's two lists, read from the log itself."""
    for line in CAPTURE_LOG.read_text(encoding='utf-8').splitlines():
        if not line.startswith('RESPONSE'):
            continue
        body = json.loads(line[line.index('{'):])
        rows = next(v for v in body.values()
                    if isinstance(v, list) and v and isinstance(v[0], dict) and 'i30R8TAs' in v[0])
        lists = [r['i30R8TAs'] for r in rows]
        regular, boss = lists[0], lists[-1]
        assert all(x == regular for x in lists[:-1]) and boss != regular, lists
        triple = lambda s: [[float(p), int(i), int(c)] for p, i, c in (e.split(':') for e in s.split(','))]  # noqa: E731
        return {'mission_id': 10, 'log': str(CAPTURE_LOG.relative_to(ROOT)).replace('\\', '/'),
                'regular': [[int(p) if p.is_integer() else p, i, c] for p, i, c in triple(regular)],
                'boss': [[int(p) if p.is_integer() else p, i, c] for p, i, c in triple(boss)]}
    raise SystemExit(f'no RESPONSE line in {CAPTURE_LOG}')


def main_extract(_args):
    templates = json.loads((TOOLS / 'wiki_zones' / 'mission_templates.json').read_text(encoding='utf-8'))
    items = json.loads((TOOLS / 'wiki_items' / 'items.json').read_text(encoding='utf-8'))['items']
    dungeon, area = dungeon_of()
    names = item_index()
    dnames, mnames = tsv('dungeons.tsv'), tsv('missions.tsv')
    region_id = {}
    for aid, name in sorted(tsv('areas.tsv').items()):
        region_id.setdefault(norm(name), aid)
    by_name = {}
    for mid, did in dungeon.items():
        if mid in mnames and did in dnames:
            by_name.setdefault((norm(dnames[did]), norm(mnames[mid])), []).append(mid)

    dungeons, missions, areas, unresolved = {}, {}, {}, {}

    def mission(mid):
        return missions.setdefault(str(mid), {'special': [], 'page_items': [], 'enemy_items': []})

    for key in sorted(templates, key=int):
        t = templates[key]
        did = dungeon.get(int(key))
        if did is not None and (t['dungeon_drops'] or t['dungeon_drops_rare']):
            dungeons.setdefault(str(did), {'page': t['dungeon'],
                                           'common': [d['id'] for d in t['dungeon_drops']],
                                           'rare': [d['id'] for d in t['dungeon_drops_rare']]})
        for name in t['drops']:
            iid = names.get(norm(name))
            if iid is None:
                unresolved[name] = unresolved.get(name, 0) + 1
            elif iid not in mission(key)['special']:
                mission(key)['quest'] = t['quest']
                mission(key)['special'].append(iid)

    def entries(iid):
        """[(region, mission ids | None, enemy | None, up_to)] for one material's battle-drop entries."""
        out = []
        for o in items[iid].get('obtain', []):
            dname, text = o['dungeon'], o['mission']
            if NOT_A_DROP.search(dname + ' ' + text) or norm(o.get('region', '')) in NOT_A_DROP_REGION:
                continue
            if ': ' in dname and norm(text) == norm(o.get('region', '')):
                dname, text = dname.split(': ', 1)          # "Kagan Desert: Bandit of the Desert" / "Morgan"
            candidates = parse_obtain(text)
            hit = next(((by_name[(norm(dname), norm(m))], e, u) for m, e, u in candidates
                        if (norm(dname), norm(m)) in by_name), None)
            if hit is None:   # an unplaced entry still says whether it names an enemy
                hit = (None, candidates[-1][1] if len(candidates) > 1 else None, candidates[0][2])
            out.append((o.get('region', ''),) + hit)
        return out

    boss_items = {int(iid) for iid in items
                  if any(enemy for _, _, enemy, _ in entries(iid))
                  or any('chest' in o['mission'].lower() for o in items[iid].get('obtain', []))}
    for iid in sorted(items, key=int):
        for region, mids, enemy, up_to in entries(iid):
            rid = region_id.get(norm(region))
            if rid is not None and int(iid) not in boss_items:
                pool = areas.setdefault(str(rid), {'name': region, 'common': []})['common']
                if int(iid) not in pool:
                    pool.append(int(iid))
            for mid in mids or ():
                rec = mission(mid)
                if enemy or int(iid) in boss_items:
                    for e in (enemies_of(enemy) if enemy else [None]):
                        if [int(iid), e, up_to] not in rec['enemy_items']:
                            rec['enemy_items'].append([int(iid), e, up_to])
                elif int(iid) not in rec['page_items']:
                    rec['page_items'].append(int(iid))

    # An area's random drops also include every pool of its dungeons, lowest dungeon id first.
    for did in sorted(dungeons, key=int):
        aids = {area[mid] for mid, d in dungeon.items() if d == int(did) and mid in area}
        for aid in sorted(aids):
            pool = areas.setdefault(str(aid), {'name': dungeons[did]['page'], 'common': []})['common']
            for iid in dungeons[did]['common']:
                if iid not in pool:
                    pool.append(iid)

    missions = {k: {f: v for f, v in rec.items() if v} for k, rec in missions.items()}
    out = {
        'about': 'Inputs of scripts/gen_material_drops.py, extracted from the git-ignored tools/ wiki caches '
                 '(wiki_zones/mission_templates.json, wiki_items/items.json) and the client strings '
                 '(tools/sgtext). Rebuild with `python scripts/gen_material_drops.py extract`.',
        'capture': capture(),
        'dungeons': dict(sorted(dungeons.items(), key=lambda kv: int(kv[0]))),
        'areas': dict(sorted(areas.items(), key=lambda kv: int(kv[0]))),
        'missions': dict(sorted(((k, v) for k, v in missions.items() if v), key=lambda kv: int(kv[0]))),
    }
    SOURCES.parent.mkdir(parents=True, exist_ok=True)
    SOURCES.write_text(json.dumps(out, indent=1, ensure_ascii=False).replace('\n', '\r\n') + '\r\n',
                       encoding='utf-8', newline='')
    print(f"wrote {SOURCES.relative_to(ROOT)}: {len(out['dungeons'])} dungeon pool(s), {len(out['areas'])} "
          f"area table(s), {len(out['missions'])} mission record(s)")
    if unresolved:
        print(f'  quest drop names with no material id: {dict(sorted(unresolved.items()))}')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('command', nargs='?', choices=['extract'])
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--archive', type=Path, default=MISSIONS.parent,
                    help='the archive folder to read (default deploy/archive)')
    ap.add_argument('--out', type=Path, help='where to write the regenerated mission.json')
    args = ap.parse_args()
    return main_extract(args) if args.command == 'extract' else main_generate(args)


if __name__ == '__main__':
    sys.exit(main())
