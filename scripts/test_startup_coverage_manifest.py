"""Offline tests of startup-manifest derivation and evaluation (Session 16, QC corrections).

    python scripts/test_startup_coverage_manifest.py

No server and no real content: small trees under out/qa/current/session16/manifest_regen/.

  positive      a tree whose every required file exists and a policy owning all of it: complete
                (the control every negative below is measured against)
  derivation    every dependency a file NAMES is required whether or not it exists -- SuperAnim
                pages, script params, font pages, sprite-plist textures, the texture of a particle
                without embedded data; QC's missing_atlas.plist probe yields its PNG and a problem
                (QC finding 2).  A particle WITH embedded data needs its named texture only when
                some copy exists: cocos2d-x draws the embedded copy when it cannot read the file
  regeneration  each required file removed, the manifest AND a policy owning everything left
                regenerated from the smaller tree: the removed name is still required (or, for a
                root, the root is) and the evaluation is incomplete -- coverage can never become
                complete by forgetting a file
  categories    a list enumerated from existing files is an unknown list: never complete
  absent        a required name in no source is a problem, not an exemption; QC's evaluator probe
                (an absent file in both sets plus an unknown category) is incomplete (QC finding 3,
                correction-review finding 2)
  schema        validate_manifest refuses what the broker's loadManifest refuses (SCHEMA_CASES,
                shared with test_content_coverage_wire.py, which runs them against the server):
                QC's login-only and empty-set manifests, unknown / malformed / duplicate sets,
                reasonless or contradictory exclusions, stray absent notes, malformed client and
                server files
  server tables the manifest bound to the response tables a server serves (Session 18 correction):
                matching -> complete; a changed digest, a served table not named, a named table no
                response serves, none named, nothing served -> incomplete with the broker's reason;
                restored -> complete again.  A response table is read from the folder it is given
"""
from __future__ import annotations

import copy
import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker  # noqa: E402
import startup_coverage as sc  # noqa: E402
from test_gen_effect_family_policy import particle, sam, write  # noqa: E402

WORK = ROOT / 'out' / 'qa' / 'current' / 'session16' / 'manifest_regen'
QC_PROBE = ROOT / 'out' / 'qa' / 'current' / 'session16' / 'qc_dependency_probe'
BUILD = 'TEST1234-00ABCDEF-00123456'

# rel path -> bytes.  Login roots: a_login.png, anim.sam (2 pages), script.txt (2 params),
# font.fnt (1 page), atlas.plist (sprite: atlas.png), fx.plist (particle: fx_tex.png).
# Background roots: b_bg.png, bg.txt (1 param).
FILES = {
    'login/a_login.png': b'\x89PNG\r\n\x1a\nlogin',
    'sam/anim/anim.sam': sam(['anim_p1.png', 'anim_p2.png']),
    'sam/anim/anim_p1.png': b'\x89PNG\r\n\x1a\np1',
    'sam/anim/anim_p2.png': b'\x89PNG\r\n\x1a\np2',
    'event/script.txt': b'*,type=PARAM,id=1,param=bg1:pic.png,#\r\n*,type=PARAM,id=6,param=/sound/bgm.mp3,#\r\n',
    'event/pic.png': b'\x89PNG\r\n\x1a\npic',
    'sound/bgm.mp3': b'ID3bgm',
    'font/font.fnt': b'info face="x"\npage id=0 file="font_0.png"\n',
    'font/font_0.png': b'\x89PNG\r\n\x1a\nfont',
    'ui/atlas.plist': b'<plist><dict><key>frames</key><dict/></dict></plist>',
    'ui/atlas.png': b'\x89PNG\r\n\x1a\natlas',
    'effect/fx.plist': particle('fx_tex.png', embedded=False),
    'effect/fx_tex.png': b'\x89PNG\r\n\x1a\nfx',
    'bg/b_bg.png': b'\x89PNG\r\n\x1a\nbg',
    'bg/bg.txt': b'*,type=PARAM,id=6,param=bg_music.mp3,#\r\n',
    'sound/bg_music.mp3': b'ID3bgmusic',
}
SCENE = ['a_login.png', 'anim.sam', 'script.txt', 'font.fnt', 'atlas.plist', 'fx.plist']
RES_PATHS = ['bg/b_bg.png', 'bg/bg.txt']
ROOTS = {'a_login.png', 'anim.sam', 'script.txt', 'font.fnt', 'atlas.plist', 'fx.plist', 'b_bg.png', 'bg.txt'}
DEPENDS = {'anim_p1.png': 'anim.sam', 'anim_p2.png': 'anim.sam', 'pic.png': 'script.txt', 'bgm.mp3': 'script.txt',
           'font_0.png': 'font.fnt', 'atlas.png': 'atlas.plist', 'fx_tex.png': 'fx.plist',
           'bg_music.mp3': 'bg.txt'}


def make_tree(base: Path, drop: str | None = None) -> Path:
    if base.exists():
        shutil.rmtree(base)
    content = base / 'content'
    for rel, data in FILES.items():
        if drop is None or rel.rsplit('/', 1)[-1] != drop:
            write(content, rel, data)
    return content


def policy_owning(content: Path) -> dict:
    """A policy owning every file of the tree, one bundle each (what a generator could do at best)."""
    rels = sorted(p.relative_to(content).as_posix() for p in content.rglob('*') if p.is_file())
    return {'enabled': True, 'scope': rels, 'aliases': {}, 'exclude': [],
            'extensions': ['.png', '.sam', '.txt', '.fnt', '.plist', '.mp3'],
            'bundles': {f'b_{r.rsplit("/", 1)[-1]}': [r.rsplit('/', 1)[-1]] for r in rels}}


#: The client files the test manifests name (Session 18): the offline evaluation, like the broker,
#: never calls a manifest complete that names none (the DLL hashes them; these tests do not).
CLIENT_FILES = [{'root': 'localstate', 'path': 'Ver1_Test0001.dat', 'sha256': 'a' * 64}]
#: The response tables the test server serves (file -> sha256), and the manifests' inputs.server_files
#: naming exactly them (Session 18 correction): the evaluation binds one to the other.
SERVED = {'banner_info_mst.json': 'b' * 64, 'dbb_mst.json': 'c' * 64}
SERVER_FILES = [{'root': 'mst', 'path': name, 'sha256': digest} for name, digest in SERVED.items()]


def evaluate(policy: dict, manifest: dict, content: sc.Content) -> dict:
    """The offline verdict for a server serving SERVED."""
    return sc.evaluate(policy, manifest, content, SERVED)


def derive(content: Path, categories=(), packaged=frozenset()) -> dict:
    manifest, _report = sc.build_manifest(sc.Content(content), scene=SCENE, res_paths=RES_PATHS,
                                          packaged=set(packaged), categories=list(categories), build=BUILD,
                                          inputs={'client_files': CLIENT_FILES, 'server_files': SERVER_FILES})
    return manifest


def required(manifest: dict) -> set:
    return set(manifest['sets']['login']) | set(manifest['sets']['background'])


def schema_cases(base: dict) -> list[tuple[str, dict]]:
    """Manifests the broker must refuse (each with a manifest_error, complete 0) -- and so must
    validate_manifest.  `base` is a valid manifest whose login and background sets are non-empty."""
    login, background = list(base['sets']['login']), list(base['sets']['background'])
    cases = [
        ('QC: a login set only', dict(base, sets={'login': login})),
        ('QC: empty login and background sets', dict(base, sets={'login': [], 'background': []})),
        ('an empty background set', dict(base, sets={'login': login, 'background': []})),
        ('no sets at all', {k: v for k, v in base.items() if k != 'sets'}),
        ('sets is not an object', dict(base, sets=[login, background])),
        ('a set is not an array', dict(base, sets={'login': login, 'background': 'b_bg.png'})),
        ('an unknown extra set', dict(base, sets={'login': login, 'background': background, 'extra': ['x.png']})),
        ('a non-flat name', dict(base, sets={'login': login + ['event/x.png'], 'background': background})),
        ('a non-string name', dict(base, sets={'login': login + [7], 'background': background})),
        ('a name twice (case-insensitively)',
         dict(base, sets={'login': login + [login[0].upper()], 'background': background})),
        ('an exclusion without its reason', dict(base, excluded={'thumbs.db': ''})),
        ('a name both required and excluded', dict(base, excluded={login[0]: 'a documented reason'})),
        ('an absent note for a name no set requires', dict(base, absent={'never_required.png': 'nowhere'})),
        ('an unknown top-level key', dict(base, surprise=1)),
        ('another format', dict(base, format='bfcp-startup 2')),
        ('no client build', dict(base, build='')),
        ('an unknown list holding a non-string', dict(base, unknown=[3])),
    ]
    # Session 18: inputs.client_files names the client's own files; the broker refuses a malformed one.
    good = {'root': 'localstate', 'path': 'Ver1_Fixture1.dat', 'sha256': 'a' * 64}
    inputs = dict(base.get('inputs') or {})
    bad_files = [
        ('client_files not an array', {'root': 'localstate'}),
        ('a client file without its digest', [{'root': 'localstate', 'path': 'Ver1_Fixture1.dat'}]),
        ('a client file with an extra key', [dict(good, size=3)]),
        ('a client file under an unknown root', [dict(good, root='system')]),
        ('a client file path leaving its root', [dict(good, root='package', path='Assets/../../x.dat')]),
        ('a LocalState client file in a folder', [dict(good, path='sub/Ver1_Fixture1.dat')]),
        ('a client file path with a reserved character', [dict(good, root='package', path='Assets/x*.dat')]),
        ('a client file digest too short', [dict(good, sha256='abcd')]),
        ('a client file digest in upper case', [dict(good, sha256='A' * 64)]),
        ('a client file named twice (case-insensitively)', [good, dict(good, path='VER1_FIXTURE1.DAT')]),
    ]
    cases += [(label, dict(base, inputs=dict(inputs, client_files=files))) for label, files in bad_files]
    # Session 18 correction: inputs.server_files names the server's response tables, each a file directly
    # under its mst_root; the broker refuses a malformed or repeated one.
    table = {'root': 'mst', 'path': 'banner_info_mst.json', 'sha256': 'b' * 64}
    bad_tables = [
        ('server_files not an array', {'root': 'mst'}),
        ('a server file without its digest', [{'root': 'mst', 'path': 'banner_info_mst.json'}]),
        ('a server file with an extra key', [dict(table, size=3)]),
        ('a server file under another root', [dict(table, root='localstate')]),
        ('a server file in a folder', [dict(table, path='sub/banner_info_mst.json')]),
        ('a server file path leaving its root', [dict(table, path='..')]),
        ('a server file path with a reserved character', [dict(table, path='banner*.json')]),
        ('a server file digest too short', [dict(table, sha256='abcd')]),
        ('a server file digest in upper case', [dict(table, sha256='B' * 64)]),
        ('a server file named twice (case-insensitively)', [table, dict(table, path='BANNER_INFO_MST.JSON')]),
        ('a server file that is not an object', ['banner_info_mst.json']),
        ('a served projection in a folder', [dict(table, root='served', path='x/3SG2wX0R')]),
        ('a served projection named twice', [dict(table, root='served', path='3SG2wX0R'),
                                             dict(table, root='served', path='3sg2wx0r')]),
    ]
    cases += [(label, dict(base, inputs=dict(inputs, server_files=files))) for label, files in bad_tables]
    # Session 19: a client table the derivation recorded reading must be bound (an omitted binding).
    cases += [
        ('inputs.tables records a client part client_files does not bind',
         dict(base, inputs=dict(inputs, tables={'F_ITEM_MST': {'version': 1028, 'parts': ['Ver1028_83JWTCGy.dat']}}))),
        ('inputs.tables records an omitted part of a joined table',
         dict(base, inputs=dict(inputs, tables={'M_AREA_MST': {'served': '3SG2wX0R', 'joined': [
             {'version': 1003, 'parts': ['Ver1003_B57MJYoF.dat']}]}}))),
        ('inputs.tables is not an object', dict(base, inputs=dict(inputs, tables=['F_ITEM_MST']))),
    ]
    return cases


def main() -> int:
    check = Checker()
    WORK.mkdir(parents=True, exist_ok=True)

    # positive control
    content = make_tree(WORK / 'full')
    manifest = derive(content)
    ev = evaluate(policy_owning(content), manifest, sc.Content(content))
    check('positive: every root and every dependency it names is required',
          required(manifest) == ROOTS | set(DEPENDS), sorted((ROOTS | set(DEPENDS)) ^ required(manifest)))
    check('positive: the manifest is one the broker would load', not sc.validate_manifest(manifest),
          sc.validate_manifest(manifest))
    check('positive: a policy owning everything makes it complete (the control)',
          ev['complete'] and not ev['unknown'] and all(not v['problems'] for v in ev['sets'].values())
          and ev['server_binding'] == '', ev)
    unbound = dict(manifest, inputs={'server_files': SERVER_FILES})
    ev = evaluate(policy_owning(content), unbound, sc.Content(content))
    check('positive without client files named: incomplete, as the broker says (nothing ties it to a client)',
          not ev['complete'] and ev['inputs'] == 0 and not sc.validate_manifest(unbound), ev)

    # Session 18 correction: the manifest bound to the response tables the server serves.
    policy, tree = policy_owning(content), sc.Content(content)

    def bound(served: dict, server_files) -> dict:
        inputs = {'client_files': CLIENT_FILES}
        if server_files is not None:
            inputs['server_files'] = server_files
        return sc.evaluate(policy, dict(manifest, inputs=inputs), tree, served)
    changed = dict(SERVED, **{'banner_info_mst.json': 'd' * 64})
    cases = [
        ('no server files named (an old or unbound manifest)', SERVED, None,
         "the manifest names none of the server's response tables (inputs.server_files): it cannot be bound to "
         "what this server's responses serve"),
        ('an empty server_files list', SERVED, [],
         "the manifest names none of the server's response tables (inputs.server_files): it cannot be bound to "
         "what this server's responses serve"),
        ('a served table changed since the manifest was derived', changed, SERVER_FILES,
         'the manifest does not match the response tables this server serves: banner_info_mst.json: derived from '
         f'sha256:{"b" * 16}..., served sha256:{"d" * 16}...'),
        ('a served table the manifest does not name', SERVED, SERVER_FILES[:1],
         'the manifest does not match the response tables this server serves: dbb_mst.json: not named (served '
         f'sha256:{"c" * 16}...)'),
        ('a named table no response serves', SERVED, SERVER_FILES + [{'root': 'mst', 'path': 'raid_point_mst.json',
                                                                      'sha256': 'e' * 64}],
         'the manifest does not match the response tables this server serves: raid_point_mst.json is not a table '
         'any response serves to a startup list'),
        ('several at once, in name order', dict(changed, **{'dbb_mst.json': 'f' * 64}),
         [SERVER_FILES[1], {'root': 'mst', 'path': 'Banner_Info_Mst.json', 'sha256': 'b' * 64},
          {'root': 'mst', 'path': 'aaa_mst.json', 'sha256': 'e' * 64}],
         'the manifest does not match the response tables this server serves: aaa_mst.json is not a table any '
         f'response serves to a startup list; Banner_Info_Mst.json: derived from sha256:{"b" * 16}..., served '
         f'sha256:{"d" * 16}...; dbb_mst.json: derived from sha256:{"c" * 16}..., served sha256:{"f" * 16}...'),
        ('a server that serves no table', {}, SERVER_FILES,
         'the server recorded none of the response tables the startup lists read: there is nothing to bind the '
         'manifest to'),
    ]
    for label, served, files, reason in cases:
        ev = bound(served, files)
        check(f'server tables: {label} -> incomplete with the broker\'s reason', ev['complete'] is False
              and ev['server_binding'] == reason and not ev['unknown']
              and all(not v['problems'] for v in ev['sets'].values()), (ev['server_binding'], reason))
    ev = bound(dict(SERVED), [dict(f, path=f['path'].upper()) for f in SERVER_FILES])
    check('server tables: restored, named in another case (one file on Windows) -> bound and complete again',
          ev['complete'] and ev['server_binding'] == '', ev['server_binding'])
    mst = WORK / 'served_mst'
    if mst.exists():
        shutil.rmtree(mst)
    for name in sc.response_tables()[1:]:
        write(mst, name, f'{{"{name}": []}}'.encode())
    write(mst, 'raid_point_mst.json', b'{"x": []}')
    got = sc.served_tables(mst)
    check('server tables: served_tables reads exactly the response tables a folder holds (a missing one left '
          'out, an unrelated one ignored), each hashed', list(got) == sc.response_tables()[1:]
          and all(got[n] == sc.sha256_file(mst / n) for n in got), got)
    check('server tables: the response tables are the DERIVED response sources -- six files',
          sc.response_tables() == ['dungeon_key_mst.json', 'grand_mission_mst.json', 'banner_info_mst.json',
                                   'grand_mission_map_mst.json', 'general_event_mst.json', 'dbb_mst.json'],
          sc.response_tables())

    # derivation of missing dependencies
    c = sc.Content(QC_PROBE)
    names, problems = c.closure(['missing_atlas.plist'])
    check("QC probe: missing_atlas.plist requires missing_atlas.png, and it is a problem",
          names == {'missing_atlas.plist', 'missing_atlas.png'} and problems.get('missing_atlas.png') == 'missing',
          (names, problems))

    # particles: cocos2d-x draws the named texture when it can read it, else the embedded data
    emb = make_tree(WORK / 'particles')
    write(emb, 'effect/emb.plist', particle('emb_tex.png', embedded=True))
    write(emb, 'effect/noemb.plist', particle('noemb_tex.png', embedded=False))
    names, problems = sc.Content(emb).closure(['emb.plist'])
    check('embedded particle, its named texture in no source: the embedded copy is drawn -> nothing more required',
          names == {'emb.plist'} and not problems, (names, problems))
    write(emb, 'effect/emb_tex.png', b'\x89PNG\r\n\x1a\nemb')
    names, problems = sc.Content(emb).closure(['emb.plist'])
    check('embedded particle, its named texture present: that file is what the game draws -> required',
          names == {'emb.plist', 'emb_tex.png'} and not problems, (names, problems))
    names, problems = sc.Content(emb).closure(['noemb.plist'])
    check('particle without embedded data: its named texture is required although no source has it -> a problem',
          'noemb_tex.png' in names and problems.get('noemb_tex.png') == 'missing', (names, problems))

    # regeneration: no removed file is ever forgotten
    for name in sorted(ROOTS | set(DEPENDS)):
        reduced = make_tree(WORK / f'without_{name}', drop=name)
        m = derive(reduced)
        ev = evaluate(policy_owning(reduced), m, sc.Content(reduced))
        classes = {cls for v in ev['sets'].values() for cls, ns in v['problems'].items() if name in ns}
        # Regenerated from the smaller tree, the manifest itself notes the name as in no source.
        check(f'regenerated without {name}: still required, noted absent, a problem -> incomplete',
              name in required(m) and name in m['absent'] and not ev['complete'] and classes == {'absent'},
              (name in required(m), name in m['absent'], ev['complete'], classes))

    # an enumerated category is an unknown list
    cat = make_tree(WORK / 'category')
    write(cat, 'item/item_1.png', b'\x89PNG\r\n\x1a\nitem')
    m = derive(cat, categories=[('item', 'ItemMst', 'GameScene::requestItemFiles', ('item',))])
    ev = evaluate(policy_owning(cat), m, sc.Content(cat))
    check('category: its existing files are required, the list itself unknown, never complete',
          'item_1.png' in m['sets']['background'] and len(m['unknown']) == 1 and not ev['complete']
          and ev['unknown'] == m['unknown'], (m['unknown'], ev['complete']))

    # absent names are gaps
    absent_tree = make_tree(WORK / 'absent')
    m = derive(absent_tree)
    m = dict(m, sets={'login': sorted(m['sets']['login'] + ['gone_login.png']),
                      'background': sorted(m['sets']['background'] + ['gone_bg.png'])},
             absent={'gone_login.png': 'absent from every source', 'gone_bg.png': 'absent from every source'})
    ev = evaluate(policy_owning(absent_tree), m, sc.Content(absent_tree))
    check('absent: a required name in no source is a problem in each set, incomplete',
          not ev['complete'] and ev['sets']['login']['problems'].get('absent') == ['gone_login.png']
          and ev['sets']['background']['problems'].get('absent') == ['gone_bg.png'], ev['sets'])
    probe = dict(m, unknown=['category x: rules not recovered'])
    ev = evaluate(policy_owning(absent_tree), probe, sc.Content(absent_tree))
    check('QC correction probe: absent files in both sets and an unknown category -> incomplete',
          ev['complete'] is False and ev['unknown'] == ['category x: rules not recovered'], ev)
    derived = derive(make_tree(WORK / 'absent_derived', drop='pic.png'), packaged={'pic.png'})
    check('absent (derived): a required name only the app package ships is noted, still required',
          'pic.png' in required(derived) and 'pic.png' in derived['absent']
          and 'app package' in derived['absent']['pic.png'], derived['absent'])

    # Session 18: the client's resourceMap exclusion table (exact case) and derived category lists
    appx = WORK / 'appx_fixture'
    write(appx, 'Assets/Resources/resourceMap', b'999999:a_login.png,Cutin.sam,cutin.sam,\r\n40:layout_x.csv,x.png,\r\n')
    kept, dropped = sc.bundle_map(appx)
    check('exclusions: a table name is dropped, its case variant kept, the trailing "\\r" token never a name',
          kept['999999'] == ['a_login.png', 'cutin.sam'] and dropped['999999'] == ['Cutin.sam']
          and kept['40'] == ['layout_x.csv', 'x.png'], (kept, dropped))
    m, rep = sc.build_manifest(sc.Content(content), scene=SCENE, res_paths=RES_PATHS, packaged=set(), categories=[],
                               scene_dropped=['Cutin.sam', 'b_bg.png'], build=BUILD)
    check('exclusions: a dropped login name no list requires is excluded with the reason; one another list '
          'requires stays required', 'cutin.sam' in m['excluded'] and 'b_bg.png' in m['sets']['background']
          and 'b_bg.png' not in m['excluded'] and not sc.validate_manifest(m), (m['excluded'], sc.validate_manifest(m)))
    medal = next(r for r in sc.DERIVED if r.key == 'medal')
    key = next(r for r in sc.DERIVED if r.key == 'dungeonkey')
    dk = make_tree(WORK / 'derived')
    write(dk, 'medal/m1.png', b'\x89PNG\r\n\x1a\nm1')
    for n in ('k.png', 'k_s.png', 'open.png', 'seal.png'):
        write(dk, f'dungeon_key/{n}', b'\x89PNG\r\n\x1a\n' + n.encode())

    def derived_manifest(rule, rows):
        return sc.build_manifest(sc.Content(dk), scene=SCENE, res_paths=RES_PATHS, packaged=set(), categories=[],
                                 derived=[(rule, rows)], build=BUILD,
                                 inputs={'client_files': CLIENT_FILES, 'server_files': SERVER_FILES})[0]
    m = derived_manifest(medal, [])
    check('derived, empty table: no names, and the list is exact (not unknown)', not m['unknown']
          and required(m) == ROOTS | set(DEPENDS), m['unknown'])
    m = derived_manifest(medal, [{'M2cv6dum': 'm1.png'}, {'M2cv6dum': ''}, {}])
    ev = evaluate(policy_owning(dk), m, sc.Content(dk))
    check('derived, populated: the field is required; an empty field or a missing column asks for nothing; '
          'owned -> complete', 'm1.png' in m['sets']['background'] and not m['unknown'] and ev['complete'], ev)
    m = derived_manifest(medal, [{'M2cv6dum': 'gone.png'}])
    ev = evaluate(policy_owning(dk), m, sc.Content(dk))
    check('derived, missing required name: still required, noted absent, incomplete',
          'gone.png' in m['sets']['background'] and 'gone.png' in m['absent'] and not ev['complete'], ev['sets'])
    m = derived_manifest(key, [{'M2cv6dum': 'k.png,k_s.png', 'VX0j1fni': 'open.png', '9unNZ6b0': 'seal.png'},
                               {'M2cv6dum': 'only.png'}])
    check('derived, split column: tokens 0 and 1 by the parser\'s own rule; a single token asks for one name',
          {'k.png', 'k_s.png', 'open.png', 'seal.png', 'only.png'} <= set(m['sets']['background'])
          and not m['unknown'], m['unknown'])
    m = derived_manifest(key, [{'M2cv6dum': ' ', 'VX0j1fni': 'open.png'}])
    check('derived, unguarded token missing (the parser reads token 0 unchecked): the list stays unknown',
          any(u.startswith('category dungeonkey') for u in m['unknown']), m['unknown'])
    m = derived_manifest(medal, None)
    check('derived, no copy of the table: the list stays unknown',
          any(u.startswith('category medal') for u in m['unknown']), m['unknown'])
    boss = next(r for r in sc.DERIVED if r.key == 'raid_boss_bgm')
    m = derived_manifest(boss, sc.derived_rows(boss)[0])
    check('derived, a list no check-list key reaches (raid_boss_bgm): exactly empty, not unknown',
          not m['unknown'] and required(m) == ROOTS | set(DEPENDS), m['unknown'])
    write(dk, 'sound/bgm_x.mp3', b'ID3x')
    bgm = next(r for r in sc.DERIVED if r.key == 'raid_point_bgm')
    m = derived_manifest(bgm, [{'3sRN9BPS': 'bgm_x.mp3'}])
    check('derived, a sound column: the sound is required (a superset of what 0x238870 lets through)',
          'bgm_x.mp3' in m['sets']['background'] and not m['unknown'], m['unknown'])
    m = derived_manifest(medal, [{'M2cv6dum': 'bgm_x.mp3'}])
    check('derived, a sound where the rule expects none: the list stays unknown',
          any(u.startswith('category medal') for u in m['unknown']), m['unknown'])
    gev = next(r for r in sc.DERIVED if r.key == 'general_event')
    m = derived_manifest(gev, [{'b4D63uav': '1', 't5R47iwj': 'bgm_x.mp3,3,100'},
                               {'b4D63uav': '3', 't5R47iwj': 'not_asked.mp3'},
                               {'b4D63uav': '01', 't5R47iwj': 'bgm_x.mp3'}])
    check('derived, general_event: token 0 of rows whose b4D63uav reads as 1 (stream >> int); other rows '
          'ask for nothing', 'bgm_x.mp3' in m['sets']['background'] and 'not_asked.mp3' not in required(m)
          and not m['unknown'], (m['unknown'], sorted(required(m) - ROOTS - set(DEPENDS))))
    m = derived_manifest(gev, [{'b4D63uav': '1', 't5R47iwj': ''}])
    check('derived, general_event with an empty parameter: the client asks for the folder itself -> unknown',
          any(u.startswith('category general_event') for u in m['unknown']), m['unknown'])
    raid_map = next(r for r in sc.DERIVED if r.key == 'raidMap')
    named, why = sc.derived_names(raid_map, [{'b5VveUd8': 'm1.jpg:0:0,m2.jpg:0:960,two:parts,,:0:0,x.jpg:1:2:3'}])
    check('derived, map tiles: the first part of each piece with exactly three parts; others and empty names '
          'ask for nothing', [n for n, _f in named] == ['m1.jpg', 'm2.jpg'] and not why, (named, why))
    # A response table is read from the server's mst_root the generator is given -- never another folder.
    fixture_mst = WORK / 'response_mst'
    write(fixture_mst, 'a.json', b'{"K1": [{"c": "a1.png"}]}')
    write(fixture_mst, 'b.json', b'{"K2": [{"c": "b1.png"}]}')
    both = sc.DerivedList('x', 'M_X', ('response', 'a.json', 'K1'), (sc.Field('c', None, '/x/', ''),), None, '',
                          also=(('response', 'b.json', 'K2'),))
    rows, files, identity = sc.derived_rows(both, fixture_mst)
    check('derived, a list another response can replace in a session: both sources\' rows, both files as '
          'inputs (root mst, the SHA-256 of the file read)', rows == [{'c': 'a1.png'}, {'c': 'b1.png'}]
          and files == [{'root': 'mst', 'path': n, 'sha256': sc.sha256_file(fixture_mst / n)} for n in ('a.json', 'b.json')]
          and identity.get('also'), (rows, files, identity))
    gone = sc.DerivedList('x', 'M_X', ('response', 'a.json', 'K1'), (sc.Field('c', None, '/x/', ''),), None, '',
                          also=(('response', 'missing.json', 'K2'),))
    check('derived, a missing extra source: no rows (the list stays unknown)',
          sc.derived_rows(gone, fixture_mst)[0] is None, sc.derived_rows(gone, fixture_mst))
    check('derived, the generator reads the folder it is given: the default (deploy/mst) holds no a.json',
          sc.derived_rows(both)[0] is None, sc.derived_rows(both)[1])

    # Session 19: item and area, derived exactly
    item = next(r for r in sc.DERIVED if r.key == 'item')
    area = next(r for r in sc.DERIVED if r.key == 'area')
    frames = [  # ItemMst::getItemFrameName 0x5E4D40, branch by branch
        ({'h0K7wjeH': '1', 'rNmJkH6K': '1'}, 'item_frame_raid.png'),
        ({'h0K7wjeH': '1', 'rNmJkH6K': '1', 'ZKQ1X69a': '1'}, 'item_frame_raid.png'),
        ({'h0K7wjeH': '2', 'rNmJkH6K': '1'}, 'item_frame_2.png'),
        ({'h0K7wjeH': '2', 'ZKQ1X69a': '1'}, 'item_frame_5.png'),
        ({'h0K7wjeH': '1', 'ZKQ1X69a': '1'}, 'item_frame_raid.png'),
        ({'h0K7wjeH': '4', 'ZKQ1X69a': '1'}, 'item_frame_raid.png'),
        ({'h0K7wjeH': '3', 'ZKQ1X69a': '1'}, 'item_frame_3.png'),
        ({'h0K7wjeH': '5', 'ZKQ1X69a': '1'}, 'item_frame_6.png'),
        ({'h0K7wjeH': '888001'}, 'item_frame_4.png'),
        ({'h0K7wjeH': '888002', 'ZKQ1X69a': '1'}, 'item_frame_raid.png'),
        ({'h0K7wjeH': '4'}, 'item_frame_4.png'),
        ({'h0K7wjeH': '0'}, 'item_frame_0.png'),
        ({'h0K7wjeH': '-1'}, 'item_frame_-1.png'),
        ({'h0K7wjeH': '5'}, 'item_frame_6.png'),
        ({'h0K7wjeH': '6'}, 'item_frame_7.png'),
        ({'h0K7wjeH': '7'}, '.png'),
        ({'h0K7wjeH': 'abc'}, 'item_frame_0.png'),
        ({'h0K7wjeH': ' 3x'}, 'item_frame_3.png'),
        ({}, 'item_frame_0.png'),
    ]
    got = [(row, sc.item_frame_name(row), want) for row, want in frames]
    check('item: the frame name follows every branch of getItemFrameName (a raid flag, the raid types, 888001 / '
          '888002, the number itself below 5, 6 and 7 for 5 and 6, a bare ".png" above; stream >> int)',
          all(g == w for _r, g, w in got), [(r, g, w) for r, g, w in got if g != w])
    named, why = sc.derived_names(item, [])
    check('item: an empty table asks for nothing (and the list is exact)', named == [] and why == [], (named, why))
    named, why = sc.derived_names(item, [{'kixHbe54': '1', 'Mt3Y0bo5': 'item_thum_1.png', 'h0K7wjeH': '3'},
                                         {'kixHbe54': '2', 'Mt3Y0bo5': '', 'h0K7wjeH': '6'},
                                         {'kixHbe54': '3', 'Mt3Y0bo5': 'sub/item_thum_3.png', 'h0K7wjeH': '7'}])
    check('item: the thumbnail under /item/ (an empty one asks for nothing, a path its basename) and the frame under '
          '/common/ for every row -- never empty', not why and [(n, f.folder) for n, f in named] == [
              ('item_thum_1.png', '/item/'), ('item_frame_3.png', '/common/'), ('item_frame_7.png', '/common/'),
              ('item_thum_3.png', '/item/'), ('.png', '/common/')], [(n, f.folder) for n, f in named])
    named, why = sc.derived_names(item, [{'kixHbe54': '5', 'Mt3Y0bo5': 'a.png'}, {'kixHbe54': '5', 'Mt3Y0bo5': 'b.png'}])
    check('item: an id twice is a problem (which row the client keeps is not modelled): the list stays unknown',
          len(why) == 2 and all('appears 2 times' in w for w in why), why)
    # Session 19: deriving item exactly must not drop what the still-unknown weapon list asks for under /item/.
    weapon = next(c for c in sc.CATEGORIES if c[0] == 'weapon, summoner_img')
    wt = make_tree(WORK / 'weapon_standin')
    write(wt, 'item/item_thum_1.png', b'\x89PNG\r\n\x1a\nitem')
    write(wt, 'item/weapon_thum_0160.png', b'\x89PNG\r\n\x1a\nweapon')
    m, rep = sc.build_manifest(sc.Content(wt), scene=SCENE, res_paths=RES_PATHS, packaged=set(), categories=[weapon],
                               derived=[(item, [{'kixHbe54': '1', 'Mt3Y0bo5': 'item_thum_1.png', 'h0K7wjeH': '3'}])],
                               build=BUILD, inputs={'client_files': CLIENT_FILES, 'server_files': SERVER_FILES})
    prov = rep['provenance']
    check('weapon stand-in: an item/ file no F_ITEM_MST row names (weapon_thum_0160.png) stays required through the '
          'unknown weapon list (x86 0x7F6CE0 asks for /item/); the exact item rule names only its own row',
          'weapon_thum_0160.png' in m['sets']['background']
          and 'category weapon, summoner_img' in prov.get('weapon_thum_0160.png', [])
          and not any(s.startswith('category item (') for s in prov.get('weapon_thum_0160.png', []))
          and any(s.startswith('category item (') for s in prov.get('item_thum_1.png', []))
          and any(u.startswith('category weapon, summoner_img') for u in m['unknown']),
          {n: prov.get(n) for n in ('weapon_thum_0160.png', 'item_thum_1.png')})
    served = [{'VjCY7rX4': '10', '3v1qg7Uj': '1', 'b5VveUd8': 'area_10.png', 'H0UB8pqm': 'plate_10.png'},
              {'VjCY7rX4': '20', '3v1qg7Uj': '3', 'b5VveUd8': '', 'H0UB8pqm': 'plate_20.png'},
              {'VjCY7rX4': '30', '3v1qg7Uj': '10', 'b5VveUd8': 'area_30.png', 'H0UB8pqm': 'plate_30.png'},
              {'VjCY7rX4': '40', '3v1qg7Uj': '0', 'b5VveUd8': 'area_40.png', 'H0UB8pqm': ''}]
    dungeons = [{'VjCY7rX4': '10', '21VKZo0E': 'banner_a.png'}, {'VjCY7rX4': '10', '21VKZo0E': ''},
                {'VjCY7rX4': '30', '21VKZo0E': 'banner_c.png'}, {'VjCY7rX4': '99', '21VKZo0E': 'banner_x.png'},
                {'VjCY7rX4': '010', '21VKZo0E': 'banner_y.png'}]
    named, why = sc.derived_names(area, served, {0: dungeons})
    check('area: img under /area/; the plate under /dungeon/ for types 1 and 3, /area/ otherwise; empty fields ask '
          'for nothing; each area\'s dungeons (area id equal as a string -- "010" is not 10; an empty room_asset '
          'asks for nothing; another area\'s never) under /dungeon/', not why and [(n, f.folder) for n, f in named] == [
              ('area_10.png', '/area/'), ('plate_10.png', '/dungeon/'), ('banner_a.png', '/dungeon/'),
              ('plate_20.png', '/dungeon/'), ('area_30.png', '/area/'), ('plate_30.png', '/area/'),
              ('banner_c.png', '/dungeon/'), ('area_40.png', '/area/')], [(n, f.folder) for n, f in named])
    m, _r = sc.build_manifest(sc.Content(content), scene=SCENE, res_paths=RES_PATHS, packaged=set(), categories=[],
                              derived=[(area, served, {0: None})], build=BUILD,
                              inputs={'client_files': CLIENT_FILES, 'server_files': SERVER_FILES})
    check('area: without a copy of the table it walks (the client\'s F_DUNGEON_MST) the list stays unknown',
          any(u.startswith('category area') and 'a table it walks' in u for u in m['unknown']), m['unknown'])
    # The server's curation, ported (ServerCache.cpp clientAreaMst): fixture tables under a fake mst_root.
    cur = WORK / 'served_area'
    if cur.exists():
        shutil.rmtree(cur)

    def vortex_area(area_id, order, name='Readable', parent=None, plate='plate.png'):
        return {'VjCY7rX4': str(area_id), '9C64Qwe0': '99', 'XuJL4pc5': str(order), 'V84mzqoX': name,
                '3v1qg7Uj': '1', 'b5VveUd8': f'img_{area_id}.png', 'H0UB8pqm': plate,
                **({'sHMai2ZG': parent} if parent is not None else {})}
    areas = [vortex_area(100100, 5, plate='frog.png'),                                    # kept
             vortex_area(100200, 1),                                       # a '???' dungeon: hidden
             vortex_area(100300, 2),                                                       # no dungeon: hidden
             vortex_area(100400, 3, name='天上'),                                  # untranslated: hidden
             vortex_area(100500, 9), vortex_area(100510, 4),                               # duplicate banners
             vortex_area(100700, 6), vortex_area(100701, 7, parent='100700'),              # parent kept, empty
             vortex_area(100600, 8),                                                       # Parade Garden
             {'VjCY7rX4': '10', '9C64Qwe0': '1', 'XuJL4pc5': '1', 'V84mzqoX': 'Grand Gaia', '3v1qg7Uj': '1',
              'b5VveUd8': 'gg.png', 'H0UB8pqm': 'gg_plate.png'}]
    dgs = [{'MHx05sXt': str(i), '9C64Qwe0': '99', 'VjCY7rX4': str(a), 'bWsLFP96': n, '21VKZo0E': b}
           for i, (a, n, b) in enumerate([(100100, 'Frog', 'b1.png'), (100200, 'Jewel', 'b2.png'),
                                          (100400, 'Kami', 'b4.png'), (100500, 'Same', 'dup.png'),
                                          (100510, 'Same', 'dup.png'), (100701, 'Child', 'b7.png'),
                                          (100600, 'Parade', 'b6.png'), (100200, '???Mojibake', 'b9.png')], 1)]
    write(cur, 'mst/area_mst.json', json.dumps({'3SG2wX0R': areas}).encode())
    write(cur, 'mst/dungeon_mst.json', json.dumps({'01pzrTCm': dgs}).encode())
    write(cur, 'banners/frog_EN.png', b'\x89PNG\r\n\x1a\nEN')
    rows = sc.served_area_rows(cur / 'mst', None)
    by_id = {r['VjCY7rX4']: r for r in rows}
    check('served areas: a Vortex area without a dungeon, with an undisplayable name (its own or a dungeon\'s) or a '
          'banner set already seen (in display order) is hidden; a parent category and every non-Vortex row stay, '
          'in file order', [r['VjCY7rX4'] for r in rows] == ['100100', '100510', '100700', '100701', '100600', '10'],
          [r['VjCY7rX4'] for r in rows])
    check('served areas: Parade Garden floated to 99999; without a banner folder (an isolated server) no plate is '
          'translated', by_id['100600']['XuJL4pc5'] == '99999' and by_id['100100']['H0UB8pqm'] == 'frog.png', by_id)
    localised = sc.served_area_rows(cur / 'mst', cur / 'banners')
    check('served areas: with the server\'s banner folder holding frog_EN.png that plate is served translated, the '
          'others untouched', [r['H0UB8pqm'] for r in localised if r['VjCY7rX4'] == '100100'] == ['frog_EN.png']
          and [r['H0UB8pqm'] for r in localised if r['VjCY7rX4'] == '10'] == ['gg_plate.png'], localised)
    text = sc.served_projection('3SG2wX0R', [{'VjCY7rX4': '7', '3v1qg7Uj': '3', 'b5VveUd8': 'a\tb.png',
                                              'H0UB8pqm': 'c\\d\n.png'}])
    check('served projection: a header naming the key and the columns, one tab-separated line per served row, '
          '\\ tab and newline escaped', text == b'3SG2wX0R VjCY7rX4,3v1qg7Uj,b5VveUd8,H0UB8pqm\n7\t3\ta\\tb.png\tc\\\\d\\n.png\n',
          text)
    proj = {**SERVED, 'served:3SG2wX0R': 'f' * 64}
    bound_files = SERVER_FILES + [{'root': 'served', 'path': '3SG2wX0R', 'sha256': 'f' * 64}]
    check('served source: bound when the manifest names the projection digest the server serves',
          sc.server_binding(dict(manifest, inputs={'server_files': bound_files}), proj) == '',
          sc.server_binding(dict(manifest, inputs={'server_files': bound_files}), proj))
    check('served source: another projection (rows changed after the manifest) -> the broker\'s reason names it',
          sc.server_binding(dict(manifest, inputs={'server_files': bound_files}), dict(proj, **{'served:3SG2wX0R': 'e' * 64}))
          == 'the manifest does not match the response tables this server serves: served:3SG2wX0R: derived from '
          f'sha256:{"f" * 16}..., served sha256:{"e" * 16}...', '')
    check('served source: omitted -> not named', sc.server_binding(dict(manifest, inputs={'server_files': SERVER_FILES}),
                                                                   proj).endswith(f'served:3SG2wX0R: not named (served '
                                                                                  f'sha256:{"f" * 16}...)'), '')
    check('server sources: the six response tables and the served area table',
          sc.server_sources() == sc.response_tables() + ['served:3SG2wX0R'], sc.server_sources())
    # A required item / area asset removed: regenerated, it stays required, noted absent, incomplete.
    ia = make_tree(WORK / 'item_area')
    for rel in ('item/item_thum_1.png', 'common/item_frame_3.png', 'area/area_10.png', 'dungeon/plate_10.png',
                'dungeon/banner_a.png'):
        write(ia, rel, b'\x89PNG\r\n\x1a\n' + rel.encode())
    item_rows = [{'kixHbe54': '1', 'Mt3Y0bo5': 'item_thum_1.png', 'h0K7wjeH': '3'}]
    area_rows = [served[0]]
    for drop in ('item_thum_1.png', 'item_frame_3.png', 'banner_a.png'):
        (next(p for p in ia.rglob(drop))).unlink()
        m, _r = sc.build_manifest(sc.Content(ia), scene=SCENE, res_paths=RES_PATHS, packaged=set(), categories=[],
                                  derived=[(item, item_rows, {}), (area, area_rows, {0: dungeons})], build=BUILD,
                                  inputs={'client_files': CLIENT_FILES, 'server_files': SERVER_FILES})
        ev = evaluate(policy_owning(ia), m, sc.Content(ia))
        check(f'item/area: {drop} removed, manifest and policy regenerated: still required, noted absent, incomplete',
              drop in m['sets']['background'] and drop in m['absent'] and not ev['complete'] and not m['unknown'],
              (drop in m['sets']['background'], m['absent'], ev['complete']))
        write(ia, 'restored/' + drop, b'\x89PNG\r\n\x1a\n' + drop.encode())

    # schema parity
    base = derive(make_tree(WORK / 'schema'))
    for label, bad in schema_cases(copy.deepcopy(base)):
        errors = sc.validate_manifest(bad)
        ev = evaluate(policy_owning(WORK / 'schema' / 'content'), bad, sc.Content(WORK / 'schema' / 'content'))
        check(f'schema: {label} -> refused, never complete', bool(errors) and ev['complete'] is False
              and ev['manifest_errors'] == errors, errors)
    return check.summary('test_startup_coverage_manifest')


if __name__ == '__main__':
    raise SystemExit(main())
