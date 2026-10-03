"""#15 missing units: every Global unit this client ships is grantable, JP-only ones stay out.

python scripts/test_unit_roster_wire.py PATH_TO_DEBUG_EXE [--port 19994]
       [--out out/qa/current/unit_roster]

A unit can only be granted, evolved into or pooled if deploy/archive/unit.json
has its row (fromArchivedUnit returns nothing otherwise).  tools/gen_unit_archive.py
builds rows from the client's own unit_mst and names them from the Global wiki,
falling back to the client's own string table.  It had skipped 114 units the
client names in plain English -- 113 because the wiki titles same-named rarities
"Deemo and the Girl (4★)" and the ★ failed its English-name filter, one because
the client spells "Titan Wing Blaze" with a no-break space.  "Deemo and the girl"
in the report is that ONE unit, Deemo and the Girl: Light 50563/50564/50565
(4-6★) and Dark 60864/60865 (5-6★), per the wiki's unit pages
(tools/wiki_units/cache/Deemo_and_the_Girl__4__.wikitext etc.).

The 31 still unarchived are named only in Japanese (MEIKO/KAITO, 吉田くん, ...) or
not at all; they are JP-only releases and stay out rather than be invented.
30645 Young Sheep Ullah evolves into one of them (30646, no name anywhere) --
the one evolution link left pointing outside the archive.

Static: the archive rule over all 2,291 client units, identities (rarity,
element, skills) against unit_mst, art on disk, evolution links.
Wire: a present grants Deemo and the Girl, it evolves into its 5★ form with the
destination's skills, and both survive a restart.
"""
import argparse
import json
import re
import sys
from pathlib import Path
from bf_testkit import ROOT, Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir  # noqa: E402

ENGLISH = re.compile(r"^[A-Za-z0-9À-ɏ ×,.'&:!?\-’()/+]+$")
UNIT_ART = [('img', 'unit_ills_full', 'png'), ('img', 'unit_anime', 'png'), ('cgg', 'unit_cgg', 'csv')]
DEEMO = {50563: (4, 5), 50564: (5, 5), 50565: (6, 5), 60864: (5, 6), 60865: (6, 6)}
KNOWN_UNNAMED_EVOLUTION = {(30645, 30646)}


def mst(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19994)
    ap.add_argument('--out', default=str(qa_dir('unit_roster')))
    args = ap.parse_args()
    check = Checker()

    units = {int(r['pn16CNah']): r for r in mst('unit')}
    skills = {str(next(iter(r.values()))) for r in mst('skill')}
    evo = {int(r['pn16CNah']): r for r in mst('unit_evo')}
    archive = {r['id']: r for r in json.loads((ROOT / 'deploy/archive/unit.json').read_text(encoding='utf-8'))}
    wiki = json.loads((ROOT / 'tools/wiki_units/units.json').read_text(encoding='utf-8'))
    client = {}
    for line in (ROOT / 'tools/sgtext/units.tsv').read_text(encoding='utf-8').splitlines():
        uid, _, name = line.partition('\t')
        client[uid] = name.replace(' ', ' ')

    def english_name(uid):
        for name in ((wiki.get(str(uid)) or {}).get('name'), client.get(str(uid))):
            if name and ENGLISH.match(name):
                return name
        return None

    # ---- the rule, over every unit the client ships ---------------------------------
    should = {u for u, r in units.items() if u != 1 and english_name(u)
              and str(r.get('eyUo6a8c', '')).strip()}
    check('every Global unit with an English name and frames is archived', should <= set(archive),
          sorted(should - set(archive))[:10])
    no_english = sorted(u for u in archive if not english_name(u))
    check('no archived unit is named only in Japanese (no JP-only unit slipped in)', not no_english,
          no_english[:10])
    left = sorted(set(units) - set(archive) - {1})
    check('what stays out is named only in Japanese or not at all', all(not english_name(u) for u in left),
          [u for u in left if english_name(u)][:10])
    print(f'{len(archive)} archived of {len(units)} client units; {len(left)} left out (JP-only or unnamed)')

    # ---- identities -----------------------------------------------------------------
    wrong = [u for u, a in archive.items() if u in units and
             (int(units[u]['7ofj5xa1']) != a['rarity'] or int(units[u]['iNy0ZU5M']) != a['element'])]
    check('every archived rarity and element matches the client MST', not wrong, wrong[:10])
    dangling = [u for u, a in archive.items() if (a.get('bb_id') and a['bb_id'] not in skills)
                or (a.get('sbb_id') and a['sbb_id'] not in skills)]
    check('every archived BB/SBB id is a real skill', not dangling, dangling[:10])
    for u, (rarity, element) in DEEMO.items():
        a = archive.get(u)
        check(f'{u} {client.get(str(u))}: archived as {rarity}★ element {element}',
              a is not None and (a['rarity'], a['element']) == (rarity, element)
              and a['name'] == client.get(str(u)), a and (a['name'], a['rarity'], a['element']))
    content = ROOT / 'deploy/game_content/content/unit'
    no_art = sorted(u for u in archive if u >= 10000 and not all(
        (content / sub / f'{stem}_{u}.{ext}').is_file() for sub, stem, ext in UNIT_ART))
    added_no_art = [u for u in no_art if u in should and u not in (10011,)]
    print(f'archived units without the full art set on disk: {len(no_art)}')
    check('every Deemo form has its art on disk', not set(DEEMO) & set(no_art), sorted(set(DEEMO) & set(no_art)))

    # ---- evolution links ------------------------------------------------------------
    out = {(u, int(r['74VFwuTd'])) for u, r in evo.items()
           if u in archive and int(r['74VFwuTd']) and int(r['74VFwuTd']) not in archive}
    check('every archived unit evolves into an archived unit (but the one unnamed form)',
          out == KNOWN_UNNAMED_EVOLUTION, sorted(out))
    check('the Deemo chains are whole', all(int(evo[u]['74VFwuTd']) in archive for u in (50563, 50564, 60864)))

    # ---- wire: grant, evolve, restart ----------------------------------------------
    fx = Fixture.create(args.out, port=args.port)
    state = {}
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        pid = db.execute("INSERT INTO user_presents(user_id,present_type,target_id,target_cnt) VALUES (?,6,'50563',1)",
                         (c.user,)).lastrowid
        db.commit()
        reply = c.call('PresentReceipt', body={'o6uWU0Z7': [{'i1WQkh4G': '0', 'S1B82FHK': str(pid)}]})
        granted = [u for u in reply.get('qC2tJs4E', []) if str(u.get('pn16CNah')) == '50563']
        check('present: Deemo and the Girl 50563 is granted and rides the reply', 'error' not in reply
              and len(granted) == 1, (reply.get('error'), [u.get('pn16CNah') for u in reply.get('qC2tJs4E', [])]))
        row = db.execute("SELECT * FROM user_units WHERE user_id=? AND unit_id='50563'", (c.user,)).fetchone()
        check('present: stored with its own skills', row is not None and str(row['bb_id']) == units[50563]['nj9Lw7mV'],
              row and dict(row).get('bb_id'))

        # Evolve 50563 -> 50564 with the recipe's own materials.
        keys = ['85X6JHQA', 'wh3YRU08', '7MxucW2J', 'j7fTS3ca', 'Hb8yfmv7', 'Voht18AP', '3g8brFoq', 'agp4CKEV', 'eKPWNoLn']
        kinds = ['Xyt6rhx2', '0tna4Idu', '6GwnugW3', 'hdF8ND2H', 'nB7pFdR0', 'IZUvR489', 'bNRUuatB', '2BFgYLjg', '18Oz7z8k']
        base = row['user_unit_id']
        db.execute('UPDATE user_units SET unit_lvl=? WHERE user_unit_id=?', (int(units[50563]['EI1DF8Yt']), base))
        template = dict(row)
        template.pop('user_unit_id')
        recipe = evo[50563]
        nodes = []
        for k, kind in zip(keys, kinds):
            material = int(recipe[k])
            if not material:
                continue
            if recipe[kind] == '2':
                db.execute('INSERT INTO user_items(user_id,item_id,item_num) VALUES (?,?,1)'
                           ' ON CONFLICT(user_id,item_id) DO UPDATE SET item_num=item_num+1', (c.user, material))
                nodes.append({'inU8Q4gL': str(material), 'mnZ5K4Ii': '2', '29MgiJIQ': '2'})
            else:
                mrow = dict(template, unit_id=str(material), favorite_flg=0, bb_id=units[material]['nj9Lw7mV'],
                            sbb_id=units[material]['iEFZ6H19'], eqip_item_id=0, eqip_item_id2=0)
                uid = db.execute(f"INSERT INTO user_units ({','.join(mrow)}) VALUES ({','.join('?' for _ in mrow)})",
                                 tuple(mrow.values())).lastrowid
                nodes.append({'inU8Q4gL': str(uid), 'mnZ5K4Ii': '2', '29MgiJIQ': '1'})
        db.execute('UPDATE user_info SET zel=99999999, karma=99999999 WHERE id=?', (c.user,))
        db.commit()
        reply = c.call('UnitEvo', body={'8Z2NQrx1': [{'inU8Q4gL': str(base), 'mnZ5K4Ii': '1', '29MgiJIQ': '1'}] + nodes,
                                        'I82p0wCL': [{'pn16CNah': recipe['74VFwuTd']}],
                                        'mCE3rUu5': [{'Rs7bCE3t': recipe['Rs7bCE3t']}]})
        evolved = db.execute('SELECT unit_id,bb_id,sbb_id,leader_skill_id FROM user_units WHERE user_unit_id=?',
                             (base,)).fetchone()
        target = units[50564]
        check('evolution: 50563 becomes 50564 with the 5★ form\'s skills', 'error' not in reply and evolved is not None
              and str(evolved['unit_id']).split('_')[0] == '50564'
              and (str(evolved['bb_id']), str(evolved['leader_skill_id'])) == (target['nj9Lw7mV'], target['oS3kTZ2W']),
              (reply.get('error'), evolved and tuple(evolved)))
        state['base'] = base
        db.close()

    with IsolatedServer(args.exe, fx, log_name='restart.log'):
        c = Client(fx)
        db = fx.db()
        kept = db.execute('SELECT unit_id FROM user_units WHERE user_unit_id=?', (state['base'],)).fetchone()
        check('restart: the evolved Deemo and the Girl is still owned', kept is not None
              and str(kept['unit_id']).split('_')[0] == '50564', kept and tuple(kept))
        listed = [u for u in c.call('UserInfo').get('4ceMWH6k', []) if str(u.get('edy7fq3L')) == str(state['base'])]
        check('restart: UserInfo lists it as 50564', len(listed) == 1 and str(listed[0].get('pn16CNah')) == '50564',
              listed[:1])
        db.close()
    return check.summary('unit roster')


if __name__ == '__main__':
    sys.exit(main())
