"""Fresh-install master-table bootstrap (Session 19): scripts/check_mst_bootstrap.py's rules on synthetic
deliveries, the real server data against its recorded inventory, and -- given SERVER_EXE -- a running
isolated server's own Initialize and /mst/ answers against the offline mirror.

    python scripts/test_mst_bootstrap.py [SERVER_EXE] [--record]

  synthetic   a small server tree under out/qa/current/session19/mst_bootstrap (rows made here, encoded with
              the client's own keys; nothing from a player's LocalState): every delivery state the audit
              names -- served from game_content/mst, staged, found by name, partial (a part missing), a
              staged table with a missing part never announced, the version a staged table raises, a client
              holding an older version (a version change) or one part of two (an interrupted download), a
              record count that differs, a malformed part, a version the server does not hold, a required
              table announced empty, rows readParam ignores, the feature gate off, a broken initial-unit
              definition, a missing text dictionary
  real data   deploy/ (read only) with an empty LocalState: the inventory must equal
              scripts/qa_baselines/mst_bootstrap_deploy_20261022.json (--record rewrites it after a reviewed
              content change)
  server      (SERVER_EXE) a fresh save on the real roots: guest login, Initialize, every announced part and
              every dictionary fetched -- the same inventory as offline, the 29 response tables present; then
              the synthetic tree served live, changed while running (a part completed, a version raised, a
              part removed): each Initialize re-reads the staged manifests and the audit follows it
"""
from __future__ import annotations

import base64
import datetime
import json
import shutil
import sys
from pathlib import Path

from Crypto.Cipher import AES

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Fixture, IsolatedServer  # noqa: E402
import check_mst_bootstrap as mb  # noqa: E402

WORK = ROOT / 'out' / 'qa' / 'current' / 'session19' / 'mst_bootstrap'
BASELINE = ROOT / 'scripts' / 'qa_baselines' / 'mst_bootstrap_deploy_20261022.json'
PORT = 19972
FACTS = mb.load_facts()
KEYS = {t['table']: (t['file_key'], t['encode_key']) for t in FACTS['version_channel']['tables']}
UNITS = '1-2@0@10011:0@1@10030,2-2@0@20011,3-2@0@30011,4-2@0@40011'


def encode(rows: list, table: str) -> bytes:
    key = KEYS[table][1].encode()[:16].ljust(16, b'\0')
    raw = json.dumps(rows, separators=(',', ':')).encode()
    pad = 16 - len(raw) % 16
    return base64.b64encode(AES.new(key, AES.MODE_ECB).encrypt(raw + bytes([pad]) * pad))


def put(path: Path, data: bytes) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path


def name(table: str, version: int, part: int | None = None) -> str:
    return f'Ver{version}_{KEYS[table][0]}' + (f'_{part}' if part else '') + '.dat'


def rows_of(n: int, column: str = 'id') -> list[dict]:
    return [{column: str(i)} for i in range(n)]


class Tree:
    """A server's three roots: mst (version_info_mst.json, defines_mst.json), the document root (game_content
    with mst/ and content/) and system (features.json)."""

    def __init__(self, base: Path, mst_root: Path | None = None):
        self.base = base
        if base.exists():
            shutil.rmtree(base)
        self.mst = mst_root or base / 'mst'
        self.doc = base / 'game_content'
        self.system = base / 'system'
        self.baseline = []
        self.defines = {mb.INITIAL_UNIT_COLUMN: UNITS}
        self.features = {'corrupted_mst_check': 1}

    def announce(self, table: str, version: int, file_count: int, record_num: int) -> None:
        self.baseline.append({'moWQ30GH': table, 'e3QNsuZ8': '', 'd2RFtP8T': str(version),
                              'H6k1LIxC': str(file_count), '5kbnkTp0': str(record_num)})

    def stage(self, table: str, version: int, parts: list[list], present: list[int] | None = None,
              record_num: int | None = None) -> None:
        folder = self.doc / 'content' / table
        if folder.exists():
            shutil.rmtree(folder)
        n = len(parts)
        for i, rows in enumerate(parts, start=1):
            if present is None or i in present:
                put(folder / name(table, version, i if n > 1 else None), encode(rows, table))
        manifest = {'table': table, 'file_key': KEYS[table][0], 'version': version, 'file_count': n,
                    'record_num': sum(len(r) for r in parts) if record_num is None else record_num}
        put(folder / 'manifest.json', json.dumps(manifest).encode())

    def stage_manifest(self, table: str, text: bytes) -> None:
        """A valid one-part v2 table whose manifest.json is replaced by these bytes: the manifest is the only
        defect."""
        self.stage(table, 2, [rows_of(1)])
        put(self.doc / 'content' / table / 'manifest.json', text)

    def write(self) -> 'Tree':
        put(self.mst / 'version_info_mst.json', json.dumps({'KeC10fuL': self.baseline}).encode())
        put(self.mst / 'defines_mst.json', json.dumps(self.defines).encode())
        put(self.system / 'features.json', json.dumps(self.features).encode())
        (self.doc / 'mst').mkdir(parents=True, exist_ok=True)
        (self.doc / 'content').mkdir(parents=True, exist_ok=True)
        return self

    def audit(self, localstate: Path | None = None) -> dict:
        return mb.audit(FACTS, localstate=localstate, mst_root=self.mst, document_root=self.doc,
                        system_root=self.system)


def table(result: dict, t: str) -> dict:
    return next(e for e in result['tables'] if e['table'] == t)


def synthetic(base: Path, mst_root: Path | None = None) -> Tree:
    """The base tree: one table per delivery path, a required table announced empty, rows readParam ignores,
    a dictionary present and one missing."""
    tree = Tree(base, mst_root)
    unit = [rows_of(2, 'unit_id'), rows_of(2, 'unit_id')]
    tree.announce('F_UNIT_MST', 5, 2, 4)                       # served from game_content/mst, two parts
    for i, rows in enumerate(unit, start=1):
        put(tree.doc / 'mst' / name('F_UNIT_MST', 5, i), encode(rows, 'F_UNIT_MST'))
    tree.announce('F_ITEM_MST', 3, 1, 9)                       # the staged v4 below replaces this row
    tree.stage('F_ITEM_MST', 4, [rows_of(3, 'item_id')])
    tree.announce('F_RESOURCE_MST', 7, 1, 3)                   # found by name, one folder deeper than findMstPart looks
    put(tree.doc / 'content' / 'a' / 'b' / name('F_RESOURCE_MST', 7), encode(
        [{'ZwstRU92': '/sgtext/sgtext_A_1.zip'}, {'ZwstRU92': '/monitor.zip'}, {'ZwstRU92': '/unit/u.png'}],
        'F_RESOURCE_MST'))
    put(tree.doc / 'content' / 'sgtext' / 'sgtext_A_1.zip', b'PK\x05\x06' + bytes(18))
    tree.announce('F_DUNGEON_MST', 2, 1, 0)                    # required (M_DUNGEON_MST's list), announced empty
    put(tree.doc / 'mst' / name('F_DUNGEON_MST', 2), encode([], 'F_DUNGEON_MST'))
    tree.announce('F_CHRONOLOGY_MST', 1, 1, 0)                 # unchecked, legitimately empty
    put(tree.doc / 'mst' / name('F_CHRONOLOGY_MST', 1), encode([], 'F_CHRONOLOGY_MST'))
    tree.announce('F_MISSION_MST', 9, 1, 1)                    # the server holds v8 only
    put(tree.doc / 'mst' / name('F_MISSION_MST', 8), encode(rows_of(1), 'F_MISSION_MST'))
    tree.announce('F_SKILL_MST', 4, 1, 5)                      # rows differ from the record count
    put(tree.doc / 'mst' / name('F_SKILL_MST', 4), encode(rows_of(4), 'F_SKILL_MST'))
    tree.announce('F_LEADER_SKILL_MST', 2, 1, 1)               # not this encoding
    put(tree.doc / 'mst' / name('F_LEADER_SKILL_MST', 2), b'not base64 of an AES block')
    tree.announce('F_EFFECT_MST', 6, 3, 3)                     # partial delivery: part 3 absent
    for i in (1, 2):
        put(tree.doc / 'mst' / name('F_EFFECT_MST', 6, i), encode(rows_of(1), 'F_EFFECT_MST'))
    tree.stage('F_MEDAL_MST', 2,
               [rows_of(1), rows_of(1)], present=[1])          # staged with a part missing: never announced
    tree.announce('F_AI_MST', 1, 1, 1)                         # compared and dropped by readParam
    tree.announce('M_LEVEL_MST', 19, 1, 0)                     # a response table's version row
    for t, text in MANIFEST_SKIPPED.items():                   # never announced (mstVersions skips them)
        tree.stage_manifest(t, text)
    for t, text in MANIFEST_ACCEPTED.items():
        tree.stage_manifest(t, text)
    return tree.write()


def manifest(name: str, **fields) -> bytes:
    m = {'table': name, 'file_key': KEYS[name][0], 'version': 2, 'file_count': 1, 'record_num': 1}
    m.update(fields)
    return json.dumps({k: v for k, v in m.items() if v is not ...}).encode()


# Staged manifests that are not a manifest (the reproduced AttributeError: the audit read .get from a list,
# null, a number or a string) or carry a field of the wrong type for MstVersions.hpp's glz::read.  Each
# table's part is present, so the manifest is the only defect.  The server's own skip is checked against
# these in server mode (agree), not assumed.
MANIFEST_SKIPPED = {
    'F_RAID_WORLD_MST': b'[]',
    'F_RAID_MAP_MST': b'null',
    'F_RAID_POINT_MST': b'1',
    'F_RAID_USER_ROUTE_MST': b'"F_RAID_USER_ROUTE_MST"',
    'F_STAMP_MST': b'{"table": "F_STAMP_MST", ',                                  # not JSON
    'F_RAID_BOSS_ROUTE_MST': manifest('F_RAID_BOSS_ROUTE_MST', record_num='1'),  # quoted number
    'F_RAID_BOSS_MST': manifest('F_RAID_BOSS_MST', record_num=None),            # null
    'F_RAID_MISSION_MST': manifest('F_RAID_MISSION_MST', record_num=True),      # bool
    'F_RAID_BATTLE_GROUP_MST': manifest('F_RAID_BATTLE_GROUP_MST', version=2.0),  # a fraction's syntax
    'F_RAID_PLAYSTYLE_MST': manifest('F_RAID_PLAYSTYLE_MST', record_num=2 ** 31),  # outside int32
    'F_RAID_RC_MST': manifest('F_RAID_RC_MST', table=['F_RAID_RC_MST']),        # a list for a string
    'F_STAMP_SET_MST': manifest('F_STAMP_SET_MST', file_count=2_000_000_000),    # part 2 of 2e9 absent
    'F_RAID_MISSION_CLEAR_CND_MST': b'\xef\xbb\xbf' + manifest('F_RAID_MISSION_CLEAR_CND_MST'),  # UTF-8 BOM
}
# Controls: still announced.  An unknown key is ignored and a missing record_num reads as 0.
MANIFEST_ACCEPTED = {
    'F_RAID_MISSION_POINT_MST': manifest('F_RAID_MISSION_POINT_MST', record_num=..., _note='kept'),
}


def check_synthetic(check: Checker) -> None:
    base = WORK / 'synthetic'
    tree = synthetic(base)
    r = tree.audit()
    unit, item = table(r, 'F_UNIT_MST'), table(r, 'F_ITEM_MST')
    check('synthetic: a two-part table served from game_content/mst is available with its four rows',
          unit['status'] == 'available' and unit['rows'] == 4
          and [Path(p['source']).parent.name for p in unit['parts']] == ['mst', 'mst'], unit)
    check('synthetic: the staged v4 replaces the baseline row v3 (Initialize\'s merge) and is served from its '
          'table folder', item['status'] == 'available' and item['announced'] == {'version': 4, 'file_count': 1,
                                                                                  'record_num': 3}
          and Path(item['parts'][0]['source']).parent.name == 'F_ITEM_MST', item)
    res = table(r, 'F_RESOURCE_MST')
    check('synthetic: a part two folders deep is found by name (findContentByName)',
          res['status'] == 'available' and Path(res['parts'][0]['source']).parent.name == 'b', res)
    zips = {z['path']: z['status'] for z in r['dictionaries']}
    check('synthetic: the delivered F_RESOURCE_MST names two dictionaries: one served, /monitor.zip missing '
          '(a blocker); its .png is no dictionary', zips == {'/sgtext/sgtext_A_1.zip': 'available', '/monitor.zip': 'missing'}
          and 'dictionary /monitor.zip' in r['blockers'], zips)
    d = table(r, 'F_DUNGEON_MST')
    check('synthetic: F_DUNGEON_MST announced with no rows is required (M_DUNGEON_MST\'s login list) -> blocked',
          d['status'] == 'empty' and d['requirement'] == 'required' and 'F_DUNGEON_MST' in r['blockers'], d)
    c = table(r, 'F_CHRONOLOGY_MST')
    check('synthetic: an unchecked table announced with 0 records loads empty and blocks nothing',
          c['status'] == 'empty' and c['requirement'] == 'unchecked' and 'F_CHRONOLOGY_MST' not in r['blockers'], c)
    m = table(r, 'F_MISSION_MST')
    check('synthetic: announced v9, the server holds v8 only -> wrong-version, blocked',
          m['status'] == 'wrong-version' and list(m['other_versions']) == [8] and 'F_MISSION_MST' in r['blockers'], m)
    s = table(r, 'F_SKILL_MST')
    check('synthetic: 4 rows against an announced 5 -> incomplete (loadFile re-queues it each login), blocked',
          s['status'] == 'incomplete' and s['rows'] == 4 and '0x543030' in s['why'] and 'F_SKILL_MST' in r['blockers'], s)
    ls = table(r, 'F_LEADER_SKILL_MST')
    check('synthetic: a part that is not this encoding -> malformed, blocked',
          ls['status'] == 'malformed' and 'F_LEADER_SKILL_MST' in r['blockers'], ls)
    e = table(r, 'F_EFFECT_MST')
    check('synthetic: two of three parts served -> incomplete (a partial delivery), blocked',
          e['status'] == 'incomplete' and 'partial delivery' in e['why'] and 'F_EFFECT_MST' in r['blockers'], e)
    partial = 'F_MEDAL_MST'
    p = table(r, partial)
    check('synthetic: a staged table with a part missing is never announced (mstVersions) -> missing',
          p['status'] == 'missing' and p['announced'] is None, p)
    npc = table(r, 'F_NPC_MESSAGE_MST')
    check('synthetic: F_NPC_MESSAGE_MST not announced is tolerated (resetFileVersion) -> missing, not blocked',
          npc['status'] == 'missing' and npc['requirement'] == 'tolerated' and 'F_NPC_MESSAGE_MST' not in r['blockers'], npc)
    ignored = {e['table'] for e in r['tables'] if e['status'] == 'not-registered'}
    check('synthetic: rows readParam does not register (F_AI_MST, a response table\'s M_LEVEL_MST) are listed, '
          'never downloads', ignored == {'F_AI_MST', 'M_LEVEL_MST'}, ignored)
    check('synthetic: a required table never announced (F_UNIT_TYPE_MST) is missing and blocked',
          table(r, 'F_UNIT_TYPE_MST')['status'] == 'missing' and 'F_UNIT_TYPE_MST' in r['blockers'])
    check('synthetic: the initial units and the corruption-check feature pass',
          r['initial_units'].startswith('elements') and r['feature'] == 'corrupted_mst_check = 1')
    skipped = {t: table(r, t)['announced'] for t in MANIFEST_SKIPPED}
    check(f'synthetic: {len(skipped)} staged manifests that are not an object or carry a wrongly typed field '
          '(list, null, number, string, broken JSON, quoted/null/bool/out-of-range record_num, 2.0 version, '
          'list table, 2e9 parts, BOM) are never announced, and the audit completes',
          all(a is None for a in skipped.values()), {t: a for t, a in skipped.items() if a is not None})
    accepted = {t: table(r, t) for t in MANIFEST_ACCEPTED}
    check('synthetic: a manifest with an unknown key and no record_num is still announced (record 0) and served',
          all(e['status'] == 'available' and e['announced'] == {'version': 2, 'file_count': 1, 'record_num': 0}
              for e in accepted.values()), accepted)
    for text in (b'[]', b'null', b'1', b'"x"', b'{', b'{"table": 1}', manifest('F_RAID_MAP_MST', version=True)):
        check(f'read_manifest({text[:24]!r}) skips it without raising', mb.read_manifest(text.decode()) is None)
    check('read_manifest: a valid manifest with defaults for missing fields and unknown keys ignored',
          mb.read_manifest('{"table": "T", "x": [1]}') == {'table': 'T', 'file_key': '', 'version': 0,
                                                          'file_count': 0, 'record_num': 0})

    # The client side: an empty LocalState asks for everything; held parts are not fetched.
    ls_dir = base / 'client' / 'LocalState'
    put(ls_dir / name('F_UNIT_MST', 5, 1), encode(rows_of(2, 'unit_id'), 'F_UNIT_MST'))
    (tree.doc / 'mst' / name('F_UNIT_MST', 5, 1)).unlink()
    r2 = tree.audit(ls_dir)
    u2 = table(r2, 'F_UNIT_MST')
    check('interrupted download: the client holds part 1 of 2, the server only part 2 -> available (the held '
          'part is not fetched again)', u2['status'] == 'available' and u2['rows'] == 4
          and 'held in LocalState' in u2['parts'][0]['source'], u2)
    r3 = tree.audit()
    check('... the same server with an empty LocalState: part 1 is not served -> incomplete',
          table(r3, 'F_UNIT_MST')['status'] == 'incomplete')
    put(ls_dir / name('F_UNIT_MST', 5, 1), b'a truncated download')
    u4 = table(tree.audit(ls_dir), 'F_UNIT_MST')
    check('interrupted download: a held part that does not decode -> malformed (loadFile deletes and re-queues it)',
          u4['status'] == 'malformed' and 'held in LocalState' in u4['parts'][0]['source'], u4)

    # A version change: the server raises F_UNIT_MST to v6; the client holds v5.
    tree.stage('F_UNIT_MST', 6, [rows_of(3, 'unit_id'), rows_of(2, 'unit_id')])
    put(ls_dir / name('F_UNIT_MST', 5, 1), encode(rows_of(2, 'unit_id'), 'F_UNIT_MST'))
    put(ls_dir / name('F_UNIT_MST', 5, 2), encode(rows_of(2, 'unit_id'), 'F_UNIT_MST'))
    u5 = table(tree.audit(ls_dir), 'F_UNIT_MST')
    check('version change: staged v6 announced, its parts downloaded (not held), the held v5 reported stale '
          '(saveVersion deletes it after the login check)', u5['status'] == 'available' and u5['rows'] == 5
          and u5['announced']['version'] == 6 and u5['local_versions'] == [5] and 'stale_local' in u5
          and all('held' not in p['source'] for p in u5['parts']), u5)
    tree.stage('F_UNIT_MST', 7, [rows_of(3, 'unit_id'), rows_of(2, 'unit_id')], present=[1])
    u6 = table(tree.audit(ls_dir), 'F_UNIT_MST')
    check('version change: a v7 staged with a part missing is not announced; the baseline v5 stands, held by '
          'the client', u6['announced']['version'] == 5 and u6['status'] == 'available'
          and all('held' in p['source'] for p in u6['parts']), u6)

    # The gates.
    tree.features = {'corrupted_mst_check': 0}
    tree.defines = {mb.INITIAL_UNIT_COLUMN: '1-2@0@10011,2-2@0@20011,3-2@0@30011'}
    tree.write()
    r7 = tree.audit()
    check('the corruption-check feature off is a blocker (the check must keep running)',
          any(b.startswith('features:') for b in r7['blockers']), r7['blockers'][-3:])
    check('an initial-unit definition without element 4 is a blocker (setInitialUnits false: NETWORK_SERVER_ERROR)',
          any('element(s) [4]' in b for b in r7['blockers']), r7['blockers'][-3:])


def projection(result: dict) -> dict:
    """The parts of an inventory both modes and every run must agree on (not where a part came from)."""
    return {'tables': {t['table']: [t['requirement'], t['status'], t.get('announced'), t.get('rows'),
                                    [p['name'] for p in t.get('parts', [])]] for t in result['tables']},
            'dictionaries': {z['path']: z['status'] for z in result['dictionaries']},
            'initial_units': result['initial_units'], 'feature': result['feature'],
            'blockers': sorted(b for b in result['blockers'] if '(' not in b)}


def check_real(check: Checker, record: bool) -> dict:
    result = mb.audit(FACTS)
    current = projection(result)
    counts = {}
    for req, status, *_ in current['tables'].values():
        counts[f'{req}/{status}'] = counts.get(f'{req}/{status}', 0) + 1
    if record:
        BASELINE.write_text(json.dumps({
            'recorded': datetime.datetime.now().astimezone().isoformat(timespec='seconds'),
            'note': 'deploy/ read only, an empty LocalState, offline (scripts/check_mst_bootstrap.py). A change here '
                    'is a change in what a fresh install receives: review it, then re-record with --record.',
            'facts_sha256': FACTS['exe_sha256'], 'counts': counts, **current}, indent=1), encoding='utf-8')
        print(f'recorded {BASELINE.relative_to(ROOT)}: {counts}')
    base = json.loads(BASELINE.read_text(encoding='utf-8')) if BASELINE.is_file() else None
    check(f'real data: the recorded inventory exists ({BASELINE.name})', base is not None)
    if base:
        changed = sorted(t for t in set(base['tables']) | set(current['tables'])
                         if base['tables'].get(t) != current['tables'].get(t))
        check('real data: every table\'s requirement, delivery, version, rows and parts equal the recorded '
              'inventory', not changed, {t: (base['tables'].get(t), current['tables'].get(t)) for t in changed[:4]})
        check('real data: the dictionaries, the initial units, the feature and the blockers equal it',
              all(base[k] == current[k] for k in ('dictionaries', 'initial_units', 'feature', 'blockers')),
              {k: (base[k], current[k]) for k in ('initial_units', 'feature', 'blockers') if base[k] != current[k]})
    check('real data: no required delivery is blocked for a fresh install (offline)', not result['blockers'],
          result['blockers'][:8])
    print(f'real data counts: {counts}')
    return result


def check_server(check: Checker, exe: Path, offline: dict) -> None:
    # 1. A fresh save on the real roots (read only): the server's own answers.
    fx = Fixture.create(WORK / 'server_real', port=PORT, fresh=True)
    with IsolatedServer(exe, fx, log_name='server.log'):
        wire = mb.audit(FACTS, port=PORT)
    a, b = projection(wire), projection(offline)
    changed = sorted(t for t in set(a['tables']) | set(b['tables']) if a['tables'].get(t) != b['tables'].get(t))
    check('server (real roots, fresh save, guest login): Initialize announces and /mst/ serves exactly what the '
          'offline mirror computes, table by table', not changed and wire['announced'] == offline['announced'],
          {t: (a['tables'].get(t), b['tables'].get(t)) for t in changed[:4]})
    check('server: every dictionary the delivered F_RESOURCE_MST names is served over /content',
          a['dictionaries'] == b['dictionaries'] and all(v == 'available' for v in a['dictionaries'].values()),
          [k for k, v in a['dictionaries'].items() if v != 'available'][:6])
    missing = [r for r in wire['responses'] if r['status'] != 'available']
    check(f'server: the {len(wire["responses"])} checked response tables all carry rows in a fresh Initialize',
          len(wire['responses']) == 29 and not missing, missing)
    check('server: the initial units and corrupted_mst_check (featureCheck.php) pass, nothing is blocked',
          not wire['blockers'] and wire['feature'] == 'corrupted_mst_check = 1', wire['blockers'][:6])

    # 2. The synthetic tree served live, changed while it runs.
    base = WORK / 'server_synthetic'
    mst = base / 'mst'
    if base.exists():
        shutil.rmtree(base)
    # A COPY (new files): deploy/mst's tables are hard-linked elsewhere.  The server loads every table at boot.
    shutil.copytree(mb.MST_ROOT, mst, copy_function=shutil.copyfile)
    defines = json.loads((mst / 'defines_mst.json').read_text(encoding='utf-8'))
    tree = synthetic(base / 'tree', mst_root=mst)
    tree.defines = defines
    tree.write()
    for real in mb.SYSTEM_ROOT.glob('*.json'):
        if real.name not in ('content_provider.json', 'content_startup.json'):
            shutil.copyfile(real, tree.system / real.name)
    sfx = Fixture.create(base / 'server', port=PORT, fresh=True, mst_root=mst, document_root=tree.doc,
                         system_root=tree.system)

    def agree(label: str) -> dict:
        w, o = mb.audit(FACTS, port=PORT), tree.audit()
        pw, po = projection(w), projection(o)
        diff = sorted(t for t in set(pw['tables']) | set(po['tables']) if pw['tables'].get(t) != po['tables'].get(t))
        check(f'server (synthetic, {label}): the running server and the offline mirror agree',
              not diff and pw['dictionaries'] == po['dictionaries'],
              {t: (pw['tables'].get(t), po['tables'].get(t)) for t in diff[:4]})
        return w

    partial = 'F_MEDAL_MST'
    with IsolatedServer(exe, sfx, log_name='server.log'):
        w = agree('as built')
        live = {t: table(w, t)['announced'] for t in (*MANIFEST_SKIPPED, *MANIFEST_ACCEPTED)}
        check('server (synthetic): Initialize announces none of the malformed or wrongly typed staged manifests '
              '(glz::read fails or the row is incomplete) and still announces the control',
              all(live[t] is None for t in MANIFEST_SKIPPED) and all(live[t] for t in MANIFEST_ACCEPTED), live)
        check('server (synthetic): wrong-version, incomplete (record count and partial), malformed and empty '
              'required tables all reach the client exactly as the audit classified them',
              [table(w, t)['status'] for t in ('F_MISSION_MST', 'F_SKILL_MST', 'F_EFFECT_MST', 'F_LEADER_SKILL_MST',
                                                'F_DUNGEON_MST')]
              == ['wrong-version', 'incomplete', 'incomplete', 'malformed', 'empty'])
        put(tree.doc / 'content' / partial / name(partial, 2, 2), encode(rows_of(1), partial))
        w = agree('the staged table\'s missing part added while running')
        check('server (synthetic): the next Initialize announces the completed staged table (mstVersions runs per '
              'Initialize)', table(w, partial)['status'] == 'available' and table(w, partial)['announced']['version'] == 2)
        tree.stage('F_ITEM_MST', 5, [rows_of(4, 'item_id')])
        w = agree('F_ITEM_MST raised to v5 while running')
        check('server (synthetic): a version change takes effect on the next Initialize (v5, its part served)',
              table(w, 'F_ITEM_MST')['announced']['version'] == 5 and table(w, 'F_ITEM_MST')['rows'] == 4)
        (tree.doc / 'mst' / name('F_UNIT_MST', 5, 2)).unlink()
        w = agree('a served part removed while running')
        check('server (synthetic): an announced table whose parts are gone is no longer available',
              table(w, 'F_UNIT_MST')['status'] in ('missing', 'incomplete'))


def main() -> int:
    args = [a for a in sys.argv[1:] if a != '--record']
    check = Checker()
    WORK.mkdir(parents=True, exist_ok=True)
    check_synthetic(check)
    offline = check_real(check, '--record' in sys.argv)
    if args:
        check_server(check, Path(args[0]), offline)
    else:
        print('note: no SERVER_EXE: the running-server checks were skipped')
    return check.summary('test_mst_bootstrap')


if __name__ == '__main__':
    raise SystemExit(main())
