"""#22A Lukroar gate / #24 double-gate overlay: everything the summon reveal
depends on that the SERVER controls, plus the client cache when it is present.

python scripts/test_summon_reveal_wire.py PATH_TO_DEBUG_EXE [--port 19985] [--no-client-cache]

What the reveal is made of (libgame.so, arm64):
  * GachaAction's Km35HAXv rows carry u0vkt9yH, an EFFECT id.  The client looks
    it up in GachaEffectMstList::getObjectWithEffectID (SummonsAnimeScene2::
    downloadFiles @0x169BFB0) and plays that row's three scripts (Wait, the
    optional Change that makes a double gate, Open) out of content/gacha/.
  * Gacha.cpp picks the effect by the unit's archive RARITY only, so every
    rarity a pool can hand out needs a row.  Omni (rarity 8) has two, 900/901,
    authored with the test gate 99008 on 2026-09-13 (commit 70e82f7); both
    reuse the 7-star scripts.
  * The scripts are split by AdventureSystem::advSet @0xFD4198 into commands
    between "*" and "#" (the delimiter strings the static initializer at
    0xCAD8D4/0xCAD8FC builds), using AdventureSystem::search -- so bytes
    between commands, e.g. the stock UTF-8 byte-order mark at the top of
    gOpenGold(Rare7).txt, are skipped.  A mark INSIDE a command is not.
  * id=42 draws a named SuperAnim (content/sam/<Name>/<Name>.sam plus the PNG
    pages it names), id=46 a particle plist (effect/plist/, texture embedded),
    id=34 a sound (sound/).
  * The reveal itself shows unit/img/unit_ills_full_<id>.png and animates
    unit_anime_<id>.png with unit_cgg_<id>.csv (GachaActionScene::updateEvent
    @0x16994A0 builds the /unit/img/ path through getUnitIllsImageDef).

A. Lukroar's variant and metadata, with known-good comparisons (Omni Amadream
   51337, Divine Dragon 51246).
B. Every rarity any archive pool can summon has a weighted effect row, and every
   effect row's scripts exist.
C. Every file every script names exists; every SAM's PNG pages exist.
D. #24's patched shape (tools/patch_gacha_scripts.py): no id=20 clearScreen;
   each Wait/Change draws one named touch prompt and, after its MSGWAIT, takes
   down the prompt, the message mask (id=18 all) and the talk window
   (id=26 0); each starts by clearing what the previous Open left; each Open's
   particles have distinct, removable names; no byte-order mark inside a command.
E. Lukroar's reveal art decodes and every sprite frame lies inside its sheet.
F. (Read-only, skipped when absent) the client's LocalState copies equal the
   served files.  Unit art is cached obfuscated as byte[i] + i*i (mod 256) --
   confirmed by decoding the cached copies back to the served bytes -- while
   SAM pages, scripts and CSVs are cached as served.
G. Wire: with the Omni test gate's pool narrowed to Lukroar, every pull grants
   51247 and names an effect row of rarity 8 whose scripts exist.

Visual acceptance (gate draws, no black void, the white flash clears between the
two gates of a double gate) is a CLIENT check and is not claimed here.
"""
import argparse
import json
import os
import re
import shutil
import sys
from collections import Counter
from pathlib import Path

from PIL import Image

from bf_testkit import ROOT, Fixture, IsolatedServer, Client, Checker
from bf_testkit import qa_dir  # noqa: E402

CONTENT = ROOT / 'deploy' / 'game_content' / 'content'
LOCALSTATE = Path(os.environ.get('LOCALAPPDATA', '')) / 'Packages' / 'gumi.BraveFrontier_99p3jr0gh0z6w' / 'LocalState'
LUKROAR, AMADREAM, DIVINE_DRAGON = 51247, 51337, 51246
OMNI_TEST_GATE = 99008
PARTICLE_NAMES = {'light', 'light2', 'light3', 'light4'}
PARAM = re.compile(r'type=PARAM,id=(\d+),param=([^,#]*)')


def mst(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


def commands(raw):
    """The script as the client splits it: the text between each '*' and '#'."""
    text = raw.decode('utf-8', errors='replace')
    out, pos = [], 0
    while True:
        start = text.find('*', pos)
        if start < 0:
            return out
        end = text.find('#', start)
        if end < 0:
            return out
        out.append(text[start:end + 1])
        pos = end + 1


def sam_pages(path):
    return sorted({m.decode() for m in re.findall(rb'[A-Za-z0-9_~\-.]+\.png', path.read_bytes())})


def script_refs(cmds):
    """(kind, relative path) for every file a script's PARAM ops name."""
    refs = []
    for c in cmds:
        m = PARAM.search(c)
        if not m:
            continue
        op, param = int(m.group(1)), m.group(2)
        if op == 42:
            for sam in param.split(':')[3:5]:
                if sam and sam != 'none':
                    name = sam[:-4]
                    refs.append(('sam', f'sam/{name}/{sam}'))
        elif op == 46:
            refs.append(('plist', f'effect/plist/{param.split(":")[4]}'))
        elif op == 34:
            refs.append(('sound', f'sound/{param}'))
    return refs


def ops(cmds):
    return [(int(m.group(1)), m.group(2)) for m in (PARAM.search(c) for c in cmds) if m]


def decode_cached_png(data):
    return bytes((b - i * i) & 0xFF for i, b in enumerate(data))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19985)
    ap.add_argument('--no-client-cache', action='store_true', help='skip section F')
    args = ap.parse_args()
    check = Checker()

    units = {int(r['pn16CNah']): r for r in mst('unit')}
    archive_units = {r['id']: r for r in json.loads((ROOT / 'deploy/archive/unit.json').read_text(encoding='utf-8'))}
    sgtext = dict(line.rstrip('\n').split('\t', 1) for line in
                  (ROOT / 'tools/sgtext/all.tsv').read_text(encoding='utf-8').splitlines() if '\t' in line)

    # ---- A. the variant -----------------------------------------------------------
    named = sorted(k for k, v in sgtext.items() if re.fullmatch(r'MST_UNIT_\d+_NAME', k) and 'Lukroar' in v)
    check('A: the client text names exactly one Lukroar, unit 51247', named == [f'MST_UNIT_{LUKROAR}_NAME'], named)
    for uid, rare, element in ((LUKROAR, 8, 5), (AMADREAM, 8, 5), (DIVINE_DRAGON, 7, 5)):
        row, arc = units.get(uid), archive_units.get(uid)
        check(f'A: {uid} {sgtext.get(f"MST_UNIT_{uid}_NAME")}: MST rarity {rare} element {element}',
              row is not None and int(row['7ofj5xa1']) == rare and int(row['iNy0ZU5M']) == element,
              row and (row['7ofj5xa1'], row['iNy0ZU5M']))
        check(f'A: {uid}: archive agrees (the rarity Gacha.cpp picks the effect by)',
              arc is not None and arc['rarity'] == rare and arc['element'] == element,
              arc and (arc['rarity'], arc['element']))

    # ---- B. effect rows ---------------------------------------------------------------
    effects = mst('gacha_effect')
    by_rarity = Counter(int(e['7ofj5xa1']) for e in effects if int(e['ug9xV4Fz']) > 0)
    gates = json.loads((ROOT / 'deploy/archive/gacha.json').read_text(encoding='utf-8'))
    summonable = Counter()
    for gate in gates:
        for p in gate.get('pool', []):
            arc = archive_units.get(p['unit_id'])
            summonable[arc['rarity'] if arc else None] += 1
    check('B: every pooled unit has an archive record', None not in summonable, summonable.get(None))
    for rarity in sorted(r for r in summonable if r is not None):
        check(f'B: rarity {rarity} ({summonable[rarity]} pool entries) has a weighted effect row',
              by_rarity[rarity] > 0, dict(by_rarity))
    check('B: Omni (rarity 8) rows are 900/901',
          sorted(e['u0vkt9yH'] for e in effects if e['7ofj5xa1'] == '8') == ['900', '901'])
    scripts = {}
    for e in effects:
        for col in ('7ZNcmYS2', 'YTx3c1jQ', 'tj0i9JhC'):
            if e[col]:
                scripts.setdefault(e[col], []).append(e['u0vkt9yH'])
    for name, rows in sorted(scripts.items()):
        check(f'B: script {name} (effects {",".join(rows)}) exists', (CONTENT / 'gacha' / name).is_file())

    # ---- C/D. every script, shape and references -------------------------------------
    parsed = {}
    for name in sorted(scripts):
        path = CONTENT / 'gacha' / name
        if not path.is_file():
            continue
        cmds = commands(path.read_bytes())
        parsed[name] = cmds
        for kind, rel in script_refs(cmds):
            target = CONTENT / rel
            check(f'C: {name}: {kind} {rel} exists', target.is_file())
            if kind == 'sam' and target.is_file():
                missing = [p for p in sam_pages(target) if not (target.parent / p).is_file()]
                check(f'C: {rel}: every PNG page exists', not missing, missing)
            if kind == 'plist' and target.is_file():
                text = target.read_text(encoding='utf-8', errors='replace')
                check(f'C: {rel}: texture embedded', '<key>textureImageData</key>' in text)
        check(f'D: {name}: no byte-order mark inside a command', not any('﻿' in c for c in cmds))
        o = ops(cmds)
        check(f'D: {name}: no id=20 clearScreen (the white screen)', (20, '0') not in o and not any(i == 20 for i, _ in o))
        if name.startswith('gOpen'):
            lights = [p.split(':')[0] for i, p in o if i == 46]
            check(f'D: {name}: particle names distinct and removable', len(lights) == len(set(lights))
                  and set(lights) <= PARTICLE_NAMES, lights)
        else:
            head = o[:5]
            check(f"D: {name}: opens by clearing the previous Open's gate and particles",
                  head == [(43, 'door_open')] + [(47, n) for n in ('light', 'light2', 'light3', 'light4')], head)
            prompts = [p for i, p in o if i == 42 and p.startswith('touch_prompt:')]
            check(f'D: {name}: exactly one named touch prompt', len(prompts) == 1, prompts)
            wait = next((k for k, c in enumerate(cmds) if 'type=MSGWAIT' in c), None)
            after = ops(cmds[wait + 1:]) if wait is not None else []
            for op in ((26, '0'), (18, 'all'), (43, 'touch_prompt')):
                check(f'D: {name}: after the tap, id={op[0]} {op[1]}', op in after, after)
            masks = sum(1 for i, _ in o if i == 13)
            check(f'D: {name}: every message mask (id=13) is taken down (id=18 all)',
                  masks == sum(1 for op in o if op == (18, 'all')), masks)
            drawn = {p.split(':')[0] for i, p in o if i == 42}
            removed = {p for i, p in o if i == 43}
            check(f'D: {name}: every anime it draws is removed again', drawn <= removed, drawn - removed)

    # ---- E. the reveal art -------------------------------------------------------------
    for uid in (LUKROAR, AMADREAM, DIVINE_DRAGON):
        files = {k: CONTENT / 'unit' / sub / f'{stem}_{uid}.{ext}' for k, sub, stem, ext in (
            ('full', 'img', 'unit_ills_full', 'png'), ('anime', 'img', 'unit_anime', 'png'),
            ('thum', 'img', 'unit_ills_thum', 'png'), ('battle', 'img', 'unit_ills_battle', 'png'),
            ('cgg', 'cgg', 'unit_cgg', 'csv'), ('idle', 'cgs', 'unit_idle_cgs', 'csv'))}
        missing = [k for k, p in files.items() if not p.is_file()]
        check(f'E: {uid}: reveal files present', not missing, missing)
        if missing:
            continue
        for k in ('full', 'anime', 'thum', 'battle'):
            try:
                with Image.open(files[k]) as im:
                    im.load()
                    ok = im.size[0] > 0 and im.size[1] > 0
            except Exception as exc:  # noqa: BLE001 - any decode failure is the finding
                ok = exc
            check(f'E: {uid}: {files[k].name} decodes', ok is True, ok)
        with Image.open(files['anime']) as im:
            w, h = im.size
        frames, outside = 0, []
        for line in files['cgg'].read_text().splitlines():
            v = [x for x in line.strip().split(',') if x != '']
            if len(v) < 2:
                continue
            vals = list(map(int, v[2:]))
            for i in range(int(v[1])):
                part = vals[i * 11:(i + 1) * 11]
                x, y, pw, ph = part[6:10]
                frames += 1
                if x < 0 or y < 0 or x + pw > w or y + ph > h:
                    outside.append(part)
        rows = sum(1 for line in files['cgg'].read_text().splitlines() if line.strip())
        idle = [int(line.split(',')[0]) for line in files['idle'].read_text().splitlines() if line.strip()]
        check(f'E: {uid}: all {frames} sprite parts inside the {w}x{h} sheet', frames > 0 and not outside, outside[:3])
        check(f'E: {uid}: idle frames name existing cgg rows', idle and max(idle) < rows, (max(idle), rows))

    # ---- F. the client's cached copies (read-only) ------------------------------------
    if args.no_client_cache or not LOCALSTATE.is_dir():
        print(f'SKIP F: client cache ({"disabled" if args.no_client_cache else "no LocalState on this machine"})')
    else:
        wanted = [CONTENT / 'gacha' / n for n in parsed]
        for cmds in parsed.values():
            for kind, rel in script_refs(cmds):
                wanted.append(CONTENT / rel)
                if kind == 'sam' and (CONTENT / rel).is_file():
                    wanted += [(CONTENT / rel).parent / p for p in sam_pages(CONTENT / rel)]
        for sub, stem, ext in (('img', 'unit_ills_full', 'png'), ('img', 'unit_anime', 'png'),
                               ('cgg', 'unit_cgg', 'csv'), ('cgs', 'unit_idle_cgs', 'csv')):
            wanted.append(CONTENT / 'unit' / sub / f'{stem}_{LUKROAR}.{ext}')
        stale, cached = [], 0
        for served in dict.fromkeys(wanted):
            copy = LOCALSTATE / served.name
            if not copy.is_file() or not served.is_file():
                continue
            cached += 1
            data, want = copy.read_bytes(), served.read_bytes()
            if data != want and not (served.suffix == '.png' and decode_cached_png(data) == want):
                stale.append(served.name)
        print(f'F: {cached} of {len(set(wanted))} dependencies are in the client cache')
        check('F: every cached dependency equals the served file', not stale, stale)

    # ---- G. wire: the Omni gate narrowed to Lukroar ----------------------------------
    archive = qa_dir('summon_archive')
    archive.mkdir(parents=True, exist_ok=True)
    for f in (ROOT / 'deploy/archive').iterdir():
        if f.is_file() and '.bak' not in f.name and f.name != 'gacha.json':
            shutil.copy2(f, archive / f.name)
    narrowed = json.loads(json.dumps(gates))
    gate = next(g for g in narrowed if g['id'] == OMNI_TEST_GATE)
    gate['pool'] = [p for p in gate['pool'] if p['unit_id'] == LUKROAR]
    check('G: the Omni test gate pools Lukroar', len(gate['pool']) == 1)
    (archive / 'gacha.json').write_text(json.dumps(narrowed, indent=1, ensure_ascii=False), encoding='utf-8')
    effect_rows = {e['u0vkt9yH']: e for e in effects}
    fx = Fixture.create(qa_dir('summon_reveal'), port=args.port, archive_root=archive)
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        db.execute('UPDATE user_info SET gems=100 WHERE id=?', (c.user,))
        db.commit()
        seen = Counter()
        for pull in range(8):
            before = db.execute("SELECT COUNT(*) FROM user_units WHERE user_id=? AND unit_id LIKE ?",
                                (c.user, f'{LUKROAR}%')).fetchone()[0]
            reply = c.call('GachaAction', body={'6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
                                                 '1IR86sAv': [{'7Ffmi96v': str(OMNI_TEST_GATE),
                                                               'a329kbl8': '1', '324b023k': '0'}]})
            ope = reply.get('Km35HAXv', [])
            units_out = reply.get('qC2tJs4E', [])
            check(f'G: pull {pull + 1}: one result, Lukroar', 'error' not in reply and len(ope) == 1
                  and len(units_out) == 1 and str(units_out[0].get('pn16CNah')) == str(LUKROAR),
                  (reply.get('error'), [u.get('pn16CNah') for u in units_out]))
            if not ope:
                continue
            row = effect_rows.get(str(ope[0].get('u0vkt9yH')))
            seen[ope[0].get('u0vkt9yH')] += 1
            check(f'G: pull {pull + 1}: effect {ope[0].get("u0vkt9yH")} is a rarity-8 row whose scripts exist',
                  row is not None and row['7ofj5xa1'] == '8' and all(
                      (CONTENT / 'gacha' / row[col]).is_file() for col in ('7ZNcmYS2', 'YTx3c1jQ', 'tj0i9JhC') if row[col]))
            after = db.execute("SELECT COUNT(*) FROM user_units WHERE user_id=? AND unit_id LIKE ?",
                               (c.user, f'{LUKROAR}%')).fetchone()[0]
            check(f'G: pull {pull + 1}: exactly one Lukroar stored', after == before + 1, (before, after))
        print(f'G: effects served {dict(seen)} (900 = Rainbow->black double gate, 901 = black gate)')
        db.close()
    return check.summary('summon reveal')


if __name__ == '__main__':
    sys.exit(main())
